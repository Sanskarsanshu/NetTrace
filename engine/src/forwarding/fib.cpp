#include "nettrace/forwarding/fib.hpp"

#include <algorithm>

namespace nettrace {

bool ForwardingTable::addRoute(Route route) {
    routes_.push_back(std::move(route));
    return true;
}

void ForwardingTable::clear() {
    routes_.clear();
}

ForwardingTable ForwardingTable::build(const TopologySnapshot& snap, const NodeId& nodeId,
                                         const std::vector<Route>& extraRoutes) {
    ForwardingTable table;
    const auto* node = snap.findNode(nodeId);
    if (!node) {
        return table;
    }

    // 1. Auto-install connected routes for each interface with an IPv4 address (M2-30)
    for (const auto& iface : node->interfaces) {
        if (!iface.ipv4.has_value()) {
            continue;
        }
        Route conn;
        conn.prefix = IPv4Subnet(iface.ipv4->network(), iface.ipv4->prefixLength());
        conn.nextHop = std::nullopt;
        conn.outInterface = iface.id;
        conn.source = RouteSource::Connected;
        conn.adminPreference = 0;
        conn.metric = 0;
        conn.valid = true;
        conn.displayId = makeDisplayId(node->name, conn);
        table.addRoute(std::move(conn));
    }

    // 2. Default route 0.0.0.0/0 from host defaultGateway (M2-31)
    if ((node->type == NodeType::Host || node->type == NodeType::CloudEndpoint) &&
        node->defaultGateway.has_value()) {
        const auto& gw = *node->defaultGateway;
        for (const auto& iface : node->interfaces) {
            if (iface.ipv4.has_value() && iface.ipv4->contains(gw)) {
                Route def;
                def.prefix = IPv4Subnet(IPv4Address(0), 0); // 0.0.0.0/0
                def.nextHop = gw;
                def.outInterface = iface.id;
                def.source = RouteSource::Default;
                def.adminPreference = 1; // Default preference per Task-4 specification
                def.metric = 0;
                def.valid = true;
                def.displayId = makeDisplayId(node->name, def);
                table.addRoute(std::move(def));
                break;
            }
        }
    }

    // 3. Static routes configured on the node (M2-32)
    for (const auto& sr : node->staticRoutes) {
        Route r;
        r.prefix = sr.prefix;
        r.nextHop = sr.nextHop;
        r.source = RouteSource::Static;
        r.adminPreference = sr.adminPreference;
        r.metric = sr.metric;
        r.valid = true;

        if (sr.outInterface.has_value()) {
            r.outInterface = *sr.outInterface;
        } else if (sr.nextHop.has_value()) {
            // If only nextHop is given, egress = interface whose subnet contains it (lowest InterfaceId)
            std::vector<InterfaceId> matchingIfaces;
            for (const auto& iface : node->interfaces) {
                if (iface.ipv4.has_value() && iface.ipv4->contains(*sr.nextHop)) {
                    matchingIfaces.push_back(iface.id);
                }
            }
            std::sort(matchingIfaces.begin(), matchingIfaces.end());
            if (!matchingIfaces.empty()) {
                r.outInterface = matchingIfaces.front();
            }
        }

        r.displayId = makeDisplayId(node->name, r);
        table.addRoute(std::move(r));
    }

    // 4. Extra (policy) routes
    for (const auto& er : extraRoutes) {
        table.addRoute(er);
    }

    return table;
}

bool ForwardingTable::isRouteUsable(
    const Route& route, const NodeId& nodeId,
    const TopologySnapshot& snap, const StateView& stateView,
    std::string* unusableReasonOut) {

    // 1. Node must be Up
    if (!stateView.isNodeUp(nodeId)) {
        if (unusableReasonOut) *unusableReasonOut = "UNUSABLE_NODE_DOWN";
        return false;
    }

    // 2. Egress interface must be Up
    if (!stateView.isInterfaceUp(nodeId, route.outInterface)) {
        if (unusableReasonOut) *unusableReasonOut = "UNUSABLE_INTERFACE_DOWN";
        return false;
    }

    // 3. Link attached to that interface must be Up
    for (const auto& link : snap.links()) {
        if ((link.a.nodeId == nodeId && link.a.interfaceId == route.outInterface) ||
            (link.b.nodeId == nodeId && link.b.interfaceId == route.outInterface)) {
            if (!stateView.isLinkUp(link.id)) {
                if (unusableReasonOut) *unusableReasonOut = "UNUSABLE_LINK_DOWN";
                return false;
            }
        }
    }

    return true;
}

std::optional<Route> ForwardingTable::lookup(
    const IPv4Address& dst,
    const NodeId& nodeId,
    const TopologySnapshot& snap,
    const StateView* stateView) const {
    return lookupWithEvidence(dst, nodeId, snap, stateView, nullptr, nullptr);
}

std::optional<Route> ForwardingTable::lookupWithEvidence(
    const IPv4Address& dst,
    const NodeId& nodeId,
    const TopologySnapshot& snap,
    const StateView* stateView,
    std::vector<Route>* candidatesOut,
    std::vector<std::pair<Route, std::string>>* rejectedOut) const {

    SnapshotStateView defaultView(snap);
    const StateView& view = stateView ? *stateView : defaultView;

    // 1. Collect all routes whose prefix contains dst
    std::vector<Route> matching;
    for (const auto& r : routes_) {
        if (r.valid && r.prefix.contains(dst)) {
            matching.push_back(r);
        }
    }

    if (candidatesOut) {
        *candidatesOut = matching;
    }

    if (matching.empty()) {
        return std::nullopt;
    }

    // 2. Separate usable and unusable routes (unusable routes removed BEFORE longest-prefix selection)
    std::vector<Route> usable;
    for (const auto& r : matching) {
        std::string unusableReason;
        if (isRouteUsable(r, nodeId, snap, view, &unusableReason)) {
            usable.push_back(r);
        } else {
            if (rejectedOut) {
                rejectedOut->emplace_back(r, unusableReason);
            }
        }
    }

    if (usable.empty()) {
        return std::nullopt;
    }

    // 3. Sort usable candidates by D-19 hierarchy
    std::sort(usable.begin(), usable.end(), [](const Route& a, const Route& b) {
        // Longest Prefix Match
        if (a.prefix.prefixLength() != b.prefix.prefixLength()) {
            return a.prefix.prefixLength() > b.prefix.prefixLength();
        }
        // Lowest Administrative Preference
        if (a.adminPreference != b.adminPreference) {
            return a.adminPreference < b.adminPreference;
        }
        // Lowest Metric
        if (a.metric != b.metric) {
            return a.metric < b.metric;
        }
        // Numeric tie-break
        return a.numericTieBreakLessThan(b);
    });

    Route chosen = usable.front();

    // 4. Record rejection reasons for remaining usable candidates
    if (rejectedOut) {
        for (size_t i = 1; i < usable.size(); ++i) {
            const auto& loser = usable[i];
            std::string reason;
            if (loser.prefix.prefixLength() < chosen.prefix.prefixLength()) {
                reason = "SHORTER_PREFIX";
            } else if (loser.adminPreference > chosen.adminPreference) {
                reason = "HIGHER_PREFERENCE";
            } else if (loser.metric > chosen.metric) {
                reason = "HIGHER_METRIC";
            } else {
                reason = "TIE_BREAK";
            }
            rejectedOut->emplace_back(loser, reason);
        }
    }

    return chosen;
}

} // namespace nettrace

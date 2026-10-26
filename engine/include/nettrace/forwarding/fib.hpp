#pragma once

#include "nettrace/forwarding/route.hpp"
#include "nettrace/model/topology.hpp"
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nettrace {

/**
 * @brief Usability StateView abstraction allowing runtime fault state injection (M7).
 */
class StateView {
public:
    virtual ~StateView() = default;
    virtual bool isNodeUp(const NodeId& id) const = 0;
    virtual bool isInterfaceUp(const NodeId& nodeId, const InterfaceId& ifaceId) const = 0;
    virtual bool isLinkUp(const LinkId& linkId) const = 0;
};

/**
 * @brief Default StateView reading administrative states directly from the TopologySnapshot.
 */
class SnapshotStateView : public StateView {
public:
    explicit SnapshotStateView(const TopologySnapshot& snap) : snap_(snap) {}

    bool isNodeUp(const NodeId& id) const override {
        const auto* n = snap_.findNode(id);
        return n && n->adminState == AdminState::Up;
    }

    bool isInterfaceUp(const NodeId& nodeId, const InterfaceId& ifaceId) const override {
        const auto* iface = snap_.findInterface(nodeId, ifaceId);
        return iface && iface->adminState == AdminState::Up;
    }

    bool isLinkUp(const LinkId& linkId) const override {
        const auto* l = snap_.findLink(linkId);
        return l && l->adminState == AdminState::Up;
    }

private:
    const TopologySnapshot& snap_;
};

/**
 * @brief Forwarding Information Base (FIB) implementing Longest-Prefix Match (LPM)
 * with D-19 hierarchical numeric tie-breaking and operational usability checks.
 */
class ForwardingTable {
public:
    ForwardingTable() = default;

    // Adds a route to the table.
    bool addRoute(Route route);

    // Clears all routes.
    void clear();

    size_t size() const { return routes_.size(); }
    bool empty() const { return routes_.empty(); }

    const std::vector<Route>& routes() const { return routes_; }

    // Auto-populates FIB for a node from the immutable snapshot:
    // 1. Connected routes for every active interface with an IPv4 address.
    // 2. Default route 0.0.0.0/0 from host defaultGateway (egress = host interface on-link to gateway).
    // 3. Static routes configured on the node.
    static ForwardingTable build(const TopologySnapshot& snap, const NodeId& nodeId);

    // Determines if a route on nodeId is operationally usable according to stateView and snapshot.
    // If unusable, outputs the exact reason code (UNUSABLE_NODE_DOWN, UNUSABLE_INTERFACE_DOWN, UNUSABLE_LINK_DOWN).
    static bool isRouteUsable(const Route& route, const NodeId& nodeId,
                              const TopologySnapshot& snap, const StateView& stateView,
                              std::string* unusableReasonOut = nullptr);

    // Performs Longest-Prefix Match (LPM) lookup for a destination address.
    std::optional<Route> lookup(
        const IPv4Address& dst,
        const NodeId& nodeId,
        const TopologySnapshot& snap,
        const StateView* stateView = nullptr) const;

    // Performs LPM lookup while recording all matching candidates and rejected routes with reasons.
    std::optional<Route> lookupWithEvidence(
        const IPv4Address& dst,
        const NodeId& nodeId,
        const TopologySnapshot& snap,
        const StateView* stateView = nullptr,
        std::vector<Route>* candidatesOut = nullptr,
        std::vector<std::pair<Route, std::string>>* rejectedOut = nullptr) const;

private:
    std::vector<Route> routes_;
};

using Fib = ForwardingTable;

} // namespace nettrace

#include "nettrace/forwarding/forwarding_engine.hpp"

namespace nettrace {

ForwardingDecision ForwardingEngine::decideHostSend(
    const TopologySnapshot& snap,
    const NodeId& hostId,
    const IPv4Address& destIp,
    const StateView* stateView) {

    ForwardingDecision d;
    d.nodeId = hostId;
    d.destinationIp = destIp;

    const auto* host = snap.findNode(hostId);
    if (!host) {
        d.hostOutcome = HostSendOutcome::NoRoute;
        d.dropReason = ErrorCode::NoRoute;
        return d;
    }

    // Build FIB for the host
    ForwardingTable table = ForwardingTable::build(snap, hostId);

    // Lookup route with evidence
    auto routeOpt = table.lookupWithEvidence(destIp, hostId, snap, stateView,
                                             &d.candidateRoutes, &d.rejectedRoutes);

    if (routeOpt.has_value()) {
        d.matchedRoute = *routeOpt;
        d.outInterface = routeOpt->outInterface;

        if (routeOpt->source == RouteSource::Connected) {
            d.hostOutcome = HostSendOutcome::Direct;
            d.nextHopIp = destIp; // ARP-1: On-link destination is the next hop
            d.needsArp = true;
        } else {
            d.hostOutcome = HostSendOutcome::ViaGateway;
            d.nextHopIp = routeOpt->nextHop; // Next hop is gateway / configured next-hop
            d.needsArp = true;
        }
    } else {
        d.hostOutcome = HostSendOutcome::NoRoute;
        d.dropReason = ErrorCode::NoRoute;
        if (!d.candidateRoutes.empty()) {
            d.detail = "ALL_MATCHING_ROUTES_UNUSABLE";
        }
    }

    return d;
}

ForwardingDecision ForwardingEngine::decideRouterForward(
    const TopologySnapshot& snap,
    const NodeId& routerId,
    const IPv4Address& destIp,
    const StateView* stateView) {

    ForwardingDecision d;
    d.nodeId = routerId;
    d.destinationIp = destIp;

    const auto* router = snap.findNode(routerId);
    if (!router) {
        d.routerOutcome = RouterForwardOutcome::NoRoute;
        d.dropReason = ErrorCode::NoRoute;
        return d;
    }

    // 1. Check if router forwarding is enabled
    if (!router->forwardingEnabled) {
        d.routerOutcome = RouterForwardOutcome::ForwardingDisabled;
        d.dropReason = ErrorCode::ForwardingDisabled;
        return d;
    }

    // 2. Check if destination IP matches any of router's own active interfaces (LocalDelivery)
    for (const auto& iface : router->interfaces) {
        if (iface.adminState == AdminState::Up && iface.ipv4.has_value() &&
            iface.ipv4->address() == destIp) {
            d.routerOutcome = RouterForwardOutcome::LocalDelivery;
            d.outInterface = iface.id;
            d.nextHopIp = destIp;
            d.needsArp = false;
            return d;
        }
    }

    // 3. Perform LPM lookup in router FIB
    ForwardingTable table = ForwardingTable::build(snap, routerId);
    auto routeOpt = table.lookupWithEvidence(destIp, routerId, snap, stateView,
                                             &d.candidateRoutes, &d.rejectedRoutes);

    if (routeOpt.has_value()) {
        d.routerOutcome = RouterForwardOutcome::Forward;
        d.matchedRoute = *routeOpt;
        d.outInterface = routeOpt->outInterface;
        d.nextHopIp = routeOpt->isConnected() ? destIp : *routeOpt->nextHop;
        d.needsArp = true;
    } else {
        d.routerOutcome = RouterForwardOutcome::NoRoute;
        d.dropReason = ErrorCode::NoRoute;
        if (!d.candidateRoutes.empty()) {
            d.detail = "ALL_MATCHING_ROUTES_UNUSABLE";
        }
    }

    return d;
}

} // namespace nettrace

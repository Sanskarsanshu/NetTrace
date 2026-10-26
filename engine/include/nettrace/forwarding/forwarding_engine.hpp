#pragma once

#include "nettrace/forwarding/decision.hpp"
#include "nettrace/forwarding/fib.hpp"
#include "nettrace/model/topology.hpp"

namespace nettrace {

/**
 * @brief Forwarding Decision Engine for Hosts and Routers (M2-34, M2-35).
 */
class ForwardingEngine {
public:
    // Host send decision (M2-34)
    static ForwardingDecision decideHostSend(
        const TopologySnapshot& snap,
        const NodeId& hostId,
        const IPv4Address& destIp,
        const StateView* stateView = nullptr);

    // Router forwarding decision (M2-35)
    static ForwardingDecision decideRouterForward(
        const TopologySnapshot& snap,
        const NodeId& routerId,
        const IPv4Address& destIp,
        const StateView* stateView = nullptr);
};

// Convenience free function aliases
inline ForwardingDecision decideHostSend(
    const TopologySnapshot& snap,
    const NodeId& hostId,
    const IPv4Address& destIp,
    const StateView* stateView = nullptr) {
    return ForwardingEngine::decideHostSend(snap, hostId, destIp, stateView);
}

inline ForwardingDecision decideRouterForward(
    const TopologySnapshot& snap,
    const NodeId& routerId,
    const IPv4Address& destIp,
    const StateView* stateView = nullptr) {
    return ForwardingEngine::decideRouterForward(snap, routerId, destIp, stateView);
}

} // namespace nettrace

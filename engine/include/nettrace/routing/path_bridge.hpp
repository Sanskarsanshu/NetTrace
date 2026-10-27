#pragma once

#include "nettrace/forwarding/route.hpp"
#include "nettrace/model/topology.hpp"
#include "nettrace/routing/route_result.hpp"

#include <map>
#include <string>
#include <vector>

namespace nettrace {

/**
 * @brief Result of policy route generation bridging planned path into node FIBs (M2-39, D-04).
 */
struct PolicyPlanResult {
    bool success = true;
    std::vector<NodeId> plannedPath;
    std::map<NodeId, std::vector<Route>> policyRoutes;          // Policy routes per node along path
    std::map<NodeId, std::vector<Route>> overriddenUserRoutes;    // Existing user routes matching destIp with different next hop/egress
    std::vector<std::string> notes;
    ErrorCode errorCode = ErrorCode::None;
    std::string errorMessage;

    size_t totalPolicyRoutesCount() const {
        size_t count = 0;
        for (const auto& kv : policyRoutes) {
            count += kv.second.size();
        }
        return count;
    }

    const std::vector<Route>& routesForNode(const NodeId& id) const {
        static const std::vector<Route> empty;
        auto it = policyRoutes.find(id);
        return it != policyRoutes.end() ? it->second : empty;
    }

    const std::vector<Route>& overriddenForNode(const NodeId& id) const {
        static const std::vector<Route> empty;
        auto it = overriddenUserRoutes.find(id);
        return it != overriddenUserRoutes.end() ? it->second : empty;
    }
};

/**
 * @brief Translates an end-to-end planned RouteResult into hop-by-hop /32 policy routes (M2-39).
 * The TopologySnapshot and user configuration are never mutated.
 */
PolicyPlanResult planPolicyRoutes(
    const TopologySnapshot& snap,
    const RouteResult& routeResult,
    const IPv4Address& destIp);

} // namespace nettrace

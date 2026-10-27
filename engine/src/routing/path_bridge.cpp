#include "nettrace/routing/path_bridge.hpp"
#include "nettrace/model/broadcast_domain.hpp"
#include "nettrace/forwarding/fib.hpp"
#include <cassert>

namespace nettrace {

PolicyPlanResult planPolicyRoutes(
    const TopologySnapshot& snap,
    const RouteResult& routeResult,
    const IPv4Address& destIp) {

    PolicyPlanResult res;

    // Snapshot content hash must remain invariant
    const std::string initialHash = snap.contentHash();

    // 1. Error check: unreachable path -> NO_ROUTE and no routes
    if (!routeResult.reachable || routeResult.pathNodeIds.size() < 2) {
        res.success = false;
        res.errorCode = ErrorCode::NoRoute;
        res.errorMessage = "Path is unreachable or contains fewer than 2 nodes";
        return res;
    }

    res.plannedPath = routeResult.pathNodeIds;
    NodeId destNodeId = routeResult.pathNodeIds.back();
    const auto* destNode = snap.findNode(destNodeId);
    if (!destNode) {
        res.success = false;
        res.errorCode = ErrorCode::InvalidConfiguration;
        res.errorMessage = "Destination node not found in topology snapshot";
        return res;
    }

    // 2. Error check: destIp not owned by the last node of the path -> INVALID_CONFIGURATION
    bool destOwnsIp = false;
    for (const auto& iface : destNode->interfaces) {
        if (iface.ipv4.has_value() && iface.ipv4->address() == destIp) {
            destOwnsIp = true;
            break;
        }
    }

    if (!destOwnsIp) {
        res.success = false;
        res.errorCode = ErrorCode::InvalidConfiguration;
        res.errorMessage = "destIp " + destIp.toString() + " is not owned by the destination node '" + destNodeId.str() + "'";
        return res;
    }

    // Precompute broadcast domains
    auto domains = computeBroadcastDomains(snap);

    // 3. For each hop along the path (on EVERY node except the destination):
    for (size_t i = 0; i + 1 < routeResult.pathNodeIds.size(); ++i) {
        const NodeId& currId = routeResult.pathNodeIds[i];
        const NodeId& nextId = routeResult.pathNodeIds[i + 1];

        const auto* currNode = snap.findNode(currId);
        const auto* nextNode = snap.findNode(nextId);
        if (!currNode || !nextNode) continue;

        // Find the shared broadcast domain between currNode and nextNode
        const BroadcastDomain* sharedDomain = nullptr;
        InterfaceId currIfaceId;
        InterfaceId nextIfaceId;
        IPv4Address nextIfaceIp;

        for (const auto& bd : domains) {
            bool foundCurr = false;
            bool foundNext = false;
            InterfaceId cIf, nIf;
            IPv4Address nIp;

            for (const auto& ifRef : bd.interfaces) {
                if (ifRef.nodeId == currId) {
                    foundCurr = true;
                    cIf = ifRef.interfaceId;
                } else if (ifRef.nodeId == nextId) {
                    foundNext = true;
                    nIf = ifRef.interfaceId;
                    const auto* nIface = snap.findInterface(nextId, nIf);
                    if (nIface && nIface->ipv4.has_value()) {
                        nIp = nIface->ipv4->address();
                    }
                }
            }

            if (foundCurr && foundNext) {
                sharedDomain = &bd;
                currIfaceId = cIf;
                nextIfaceId = nIf;
                nextIfaceIp = nIp;
                break;
            }
        }

        if (!sharedDomain) {
            res.notes.push_back("Warning: no shared domain found between " + currId.str() + " and " + nextId.str());
            continue;
        }

        // Policy route: /32 host route with source Policy on EVERY node except destination
        Route polRoute;
        polRoute.prefix = IPv4Subnet(destIp, 32);
        polRoute.outInterface = currIfaceId;
        polRoute.source = RouteSource::Policy;
        polRoute.adminPreference = 1;
        polRoute.metric = 0;
        polRoute.valid = true;

        if (nextId == destNodeId) {
            // Next node is destination in directly shared domain -> no next hop (direct)
            polRoute.nextHop = std::nullopt;
        } else {
            // Next node is intermediate router -> next hop is next node's interface IP in shared domain
            polRoute.nextHop = nextIfaceIp;
        }

        polRoute.displayId = makeDisplayId(currNode->name, polRoute);
        res.policyRoutes[currId].push_back(polRoute);

        // 4. Overridden user routes check:
        // User routes that contain destination and have a DIFFERENT next hop or egress than the policy route
        // Check static routes
        for (const auto& sr : currNode->staticRoutes) {
            if (sr.prefix.contains(destIp)) {
                // Determine effective egress interface of the static route
                InterfaceId srOut = sr.outInterface.value_or(InterfaceId());
                if (!sr.outInterface.has_value() && sr.nextHop.has_value()) {
                    for (const auto& iface : currNode->interfaces) {
                        if (iface.ipv4.has_value() && iface.ipv4->contains(*sr.nextHop)) {
                            srOut = iface.id;
                            break;
                        }
                    }
                }

                bool differentEgress = (srOut != polRoute.outInterface);
                bool differentNextHop = (sr.nextHop != polRoute.nextHop);

                if (differentEgress || differentNextHop) {
                    Route overridden;
                    overridden.prefix = sr.prefix;
                    overridden.nextHop = sr.nextHop;
                    overridden.outInterface = srOut;
                    overridden.source = RouteSource::Static;
                    overridden.adminPreference = sr.adminPreference;
                    overridden.metric = sr.metric;
                    overridden.valid = true;
                    overridden.displayId = makeDisplayId(currNode->name, overridden);
                    res.overriddenUserRoutes[currId].push_back(std::move(overridden));
                }
            }
        }

        // Check default gateway if host
        if (currNode->defaultGateway.has_value()) {
            IPv4Subnet defaultSub(IPv4Address(0), 0);
            if (defaultSub.contains(destIp)) {
                InterfaceId defOut;
                for (const auto& iface : currNode->interfaces) {
                    if (iface.ipv4.has_value() && iface.ipv4->contains(*currNode->defaultGateway)) {
                        defOut = iface.id;
                        break;
                    }
                }
                bool differentEgress = (defOut != polRoute.outInterface);
                bool differentNextHop = (currNode->defaultGateway != polRoute.nextHop);

                if (differentEgress || differentNextHop) {
                    Route overridden;
                    overridden.prefix = defaultSub;
                    overridden.nextHop = currNode->defaultGateway;
                    overridden.outInterface = defOut;
                    overridden.source = RouteSource::Default;
                    overridden.adminPreference = 1;
                    overridden.metric = 0;
                    overridden.valid = true;
                    overridden.displayId = makeDisplayId(currNode->name, overridden);
                    res.overriddenUserRoutes[currId].push_back(std::move(overridden));
                }
            }
        }
    }

    // Verify snapshot hash was never altered
    assert(snap.contentHash() == initialHash);

    return res;
}

} // namespace nettrace

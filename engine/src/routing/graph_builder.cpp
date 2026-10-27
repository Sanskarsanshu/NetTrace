#include "nettrace/routing/graph_builder.hpp"
#include <algorithm>
#include <queue>
#include <set>

namespace nettrace {

void L3Graph::addL3Arc(L3Arc arc) {
    // Add synthetic edge to base Graph
    Edge edge;
    edge.linkId = arc.syntheticEdgeId;
    edge.from = arc.fromNode;
    edge.to = arc.toNode;
    edge.fromInterface = arc.fromInterface;
    edge.toInterface = arc.toInterface;
    edge.cost = arc.cost;
    edge.delayMs = arc.propagationDelayMs;
    edge.bandwidthBps = arc.bandwidthBps;
    edge.lossProbability = arc.packetLossProbability;
    edge.mtuBytes = arc.mtuBytes;
    edge.active = true;

    // Directed arc on the L3 topology
    addLink(edge, Directionality::Directed);

    arcsById_[arc.syntheticEdgeId] = arc;
    l3Arcs_.push_back(std::move(arc));
}

const L3Arc* L3Graph::findL3Arc(const LinkId& syntheticId) const {
    auto it = arcsById_.find(syntheticId);
    if (it != arcsById_.end()) {
        return &it->second;
    }
    return nullptr;
}

const L3Arc* L3Graph::findL3Arc(const NodeId& from, const NodeId& to) const {
    for (const auto& arc : l3Arcs_) {
        if (arc.fromNode == from && arc.toNode == to) {
            return &arc;
        }
    }
    return nullptr;
}

L3Graph::ExpandedPath L3Graph::expandPath(const RouteResult& routeResult, const TopologySnapshot& snap) const {
    ExpandedPath ep;

    if (!routeResult.reachable || routeResult.pathNodeIds.empty()) {
        return ep;
    }

    ep.physicalNodeIds.push_back(routeResult.pathNodeIds.front());

    for (size_t i = 0; i + 1 < routeResult.pathNodeIds.size(); ++i) {
        const auto& u = routeResult.pathNodeIds[i];
        const auto& v = routeResult.pathNodeIds[i + 1];

        const L3Arc* arc = nullptr;
        if (i < routeResult.pathLinkIds.size()) {
            arc = findL3Arc(routeResult.pathLinkIds[i]);
        }
        if (!arc) {
            arc = findL3Arc(u, v);
        }

        if (arc) {
            for (const auto& sw : arc->switchesTraversed) {
                ep.switchesTraversed.push_back(sw);
            }

            NodeId curr = u;
            for (const auto& linkId : arc->underlyingLinkIds) {
                ep.physicalLinkIds.push_back(linkId);
                const auto* l = snap.findLink(linkId);
                if (l) {
                    NodeId nextNode = (l->a.nodeId == curr) ? l->b.nodeId : l->a.nodeId;
                    ep.physicalNodeIds.push_back(nextNode);
                    curr = nextNode;
                }
            }
        }
    }

    ep.linkHopCount = static_cast<uint32_t>(ep.physicalLinkIds.size());

    // routerHopCount: routers strictly between source and destination (D-05)
    uint32_t rHops = 0;
    if (routeResult.pathNodeIds.size() > 2) {
        for (size_t i = 1; i + 1 < routeResult.pathNodeIds.size(); ++i) {
            const auto* n = snap.findNode(routeResult.pathNodeIds[i]);
            if (n && n->type == NodeType::Router) {
                rHops++;
            }
        }
    }
    ep.routerHopCount = rHops;

    return ep;
}

// Helper struct for BFS search inside a broadcast domain
struct BfsState {
    NodeId currentNode;
    InterfaceId currentInterface;
    std::vector<LinkId> pathLinks;
    std::vector<NodeId> pathNodes; // Includes intermediate switches
    std::vector<NodeId> switchesTraversed;
};

L3Graph GraphBuilder::build(const TopologySnapshot& snap) {
    L3Graph l3;

    // 1. Record Inactive Component Exclusions per M2-F / ADR-0031
    // Elements whose OWN adminState is Down (node, interface, link)
    for (const auto& node : snap.nodes()) {
        if (node.adminState == AdminState::Down) {
            l3.addExclusion("node:" + node.id.str());
        }
        for (const auto& iface : node.interfaces) {
            if (iface.adminState == AdminState::Down) {
                l3.addExclusion("interface:" + node.id.str() + ":" + iface.id.str());
            }
        }
    }
    for (const auto& link : snap.links()) {
        if (link.adminState == AdminState::Down) {
            l3.addExclusion("link:" + link.id.str());
        }
    }

    // 2. Add Layer-3 nodes (Hosts, Routers, CloudEndpoints). Switches are NEVER L3 nodes.
    for (const auto& node : snap.nodes()) {
        if (node.type == NodeType::Host || node.type == NodeType::Router ||
            node.type == NodeType::CloudEndpoint) {
            bool isActive = (node.adminState == AdminState::Up);
            l3.addNode(node.id, node.name, isActive);
        }
    }

    // 3. Compute broadcast domains
    auto domains = computeBroadcastDomains(snap);

    // 4. For each broadcast domain, aggregate paths between every ordered pair of L3 nodes
    for (const auto& bd : domains) {
        // Collect L3 endpoint interfaces in this domain
        std::vector<InterfaceRef> l3Endpoints;
        for (const auto& ifaceRef : bd.interfaces) {
            const auto* n = snap.findNode(ifaceRef.nodeId);
            if (n && (n->type == NodeType::Host || n->type == NodeType::Router ||
                      n->type == NodeType::CloudEndpoint)) {
                l3Endpoints.push_back(ifaceRef);
            }
        }

        // Collect links belonging to this broadcast domain
        // A link belongs to bd if both endpoints are in bd (either in bd.interfaces or switch ports)
        std::vector<const Link*> domainLinks;
        for (const auto& link : snap.links()) {
            if (link.adminState != AdminState::Up) continue;

            const auto* nodeA = snap.findNode(link.a.nodeId);
            const auto* nodeB = snap.findNode(link.b.nodeId);
            if (!nodeA || !nodeB) continue;
            if (nodeA->adminState != AdminState::Up || nodeB->adminState != AdminState::Up) continue;

            const auto* ifA = snap.findInterface(link.a.nodeId, link.a.interfaceId);
            const auto* ifB = snap.findInterface(link.b.nodeId, link.b.interfaceId);
            if (!ifA || !ifB) continue;
            if (ifA->adminState != AdminState::Up || ifB->adminState != AdminState::Up) continue;

            bool aInDomain = bd.contains(link.a.nodeId, link.a.interfaceId) ||
                             (nodeA->type == NodeType::Switch &&
                              std::find(bd.switchIds.begin(), bd.switchIds.end(), nodeA->id) != bd.switchIds.end());

            bool bInDomain = bd.contains(link.b.nodeId, link.b.interfaceId) ||
                             (nodeB->type == NodeType::Switch &&
                              std::find(bd.switchIds.begin(), bd.switchIds.end(), nodeB->id) != bd.switchIds.end());

            if (aInDomain && bInDomain) {
                domainLinks.push_back(&link);
            }
        }

        // For every ordered pair of L3 endpoints (uRef, vRef) in this domain:
        for (const auto& uRef : l3Endpoints) {
            for (const auto& vRef : l3Endpoints) {
                if (uRef.nodeId == vRef.nodeId) continue;

                // Find L2 path from uRef to vRef inside domain using BFS (fewest links, tie-break by node ID)
                std::queue<BfsState> q;
                std::set<std::pair<NodeId, size_t>> visited; // (node, hops) to allow tie-breaking at same depth

                BfsState initial;
                initial.currentNode = uRef.nodeId;
                initial.currentInterface = uRef.interfaceId;
                initial.pathNodes.push_back(uRef.nodeId);
                q.push(initial);

                std::vector<BfsState> validPathsToV;
                size_t minHopFound = std::numeric_limits<size_t>::max();

                while (!q.empty()) {
                    auto curr = q.front();
                    q.pop();

                    if (curr.pathLinks.size() > minHopFound) {
                        break;
                    }

                    if (curr.currentNode == vRef.nodeId && curr.currentInterface == vRef.interfaceId) {
                        if (curr.pathLinks.size() < minHopFound) {
                            minHopFound = curr.pathLinks.size();
                            validPathsToV.clear();
                        }
                        validPathsToV.push_back(curr);
                        continue;
                    }

                    // Collect and sort possible next steps from curr.currentNode
                    struct NextHop {
                        NodeId nextNode;
                        InterfaceId nextIface;
                        const Link* link;
                    };
                    std::vector<NextHop> nextSteps;

                    for (const auto* l : domainLinks) {
                        // Check if link connects from curr
                        if (l->a.nodeId == curr.currentNode && (curr.pathLinks.empty() ? (l->a.interfaceId == curr.currentInterface) : true)) {
                            // Traversable from a -> b?
                            // Directed means a -> b only.
                            nextSteps.push_back({l->b.nodeId, l->b.interfaceId, l});
                        } else if (l->b.nodeId == curr.currentNode && (curr.pathLinks.empty() ? (l->b.interfaceId == curr.currentInterface) : true)) {
                            // Traversable from b -> a only if Bidirectional
                            if (l->directionality == Directionality::Bidirectional) {
                                nextSteps.push_back({l->a.nodeId, l->a.interfaceId, l});
                            }
                        }
                    }

                    // Sort next steps by nextNode ID ascending for deterministic lexicographic tie-breaking
                    std::sort(nextSteps.begin(), nextSteps.end(), [](const NextHop& a, const NextHop& b) {
                        if (a.nextNode != b.nextNode) return a.nextNode < b.nextNode;
                        return a.link->id < b.link->id;
                    });

                    for (const auto& step : nextSteps) {
                        // Don't visit an L3 node as an intermediate node!
                        const auto* n = snap.findNode(step.nextNode);
                        if (!n) continue;
                        if (step.nextNode != vRef.nodeId && n->type != NodeType::Switch) {
                            continue; // Cannot traverse through another L3 host/router inside L2 domain
                        }

                        // Cycle avoidance: don't revisit nodes already in path
                        if (std::find(curr.pathNodes.begin(), curr.pathNodes.end(), step.nextNode) != curr.pathNodes.end()) {
                            continue;
                        }

                        BfsState nextState = curr;
                        nextState.currentNode = step.nextNode;
                        nextState.currentInterface = step.nextIface;
                        nextState.pathLinks.push_back(step.link->id);
                        nextState.pathNodes.push_back(step.nextNode);
                        if (n->type == NodeType::Switch) {
                            nextState.switchesTraversed.push_back(step.nextNode);
                        }

                        q.push(nextState);
                    }
                }

                if (validPathsToV.empty()) {
                    continue; // No traversable path from u to v in this domain
                }

                // Tie-breaking: lexicographically lowest sequence of node IDs
                std::sort(validPathsToV.begin(), validPathsToV.end(), [](const BfsState& a, const BfsState& b) {
                    return a.pathNodes < b.pathNodes;
                });

                const auto& best = validPathsToV.front();

                // 5. Aggregate metrics along best.pathLinks
                Cost totalCost = 0.0;
                DelayMs totalDelay = 0.0;
                BandwidthBps minBandwidth = std::numeric_limits<BandwidthBps>::max();
                std::optional<BandwidthBps> minAvailableBandwidth = std::numeric_limits<BandwidthBps>::max();
                bool anyMissingAvailable = false;
                uint32_t minMtu = std::numeric_limits<uint32_t>::max();
                double notLossProduct = 1.0;

                for (const auto& linkId : best.pathLinks) {
                    const auto* l = snap.findLink(linkId);
                    if (!l) continue;

                    totalCost += l->cost;
                    totalDelay += l->propagationDelayMs;
                    minBandwidth = std::min(minBandwidth, l->bandwidthBps);

                    if (l->availableBandwidthBps.has_value()) {
                        if (minAvailableBandwidth.has_value()) {
                            minAvailableBandwidth = std::min(*minAvailableBandwidth, *l->availableBandwidthBps);
                        }
                    } else {
                        anyMissingAvailable = true;
                    }

                    minMtu = std::min(minMtu, effectiveMtu(*l, snap));

                    notLossProduct *= (1.0 - l->packetLossProbability);
                }

                if (anyMissingAvailable) {
                    minAvailableBandwidth = std::nullopt;
                }

                double totalLoss = 1.0 - notLossProduct;

                // 6. Build L3Arc
                L3Arc arc;
                arc.syntheticEdgeId = LinkId("l3:" + uRef.nodeId.str() + ":" + uRef.interfaceId.str() + "->" +
                                             vRef.nodeId.str() + ":" + vRef.interfaceId.str());
                arc.fromNode = uRef.nodeId;
                arc.toNode = vRef.nodeId;
                arc.fromInterface = uRef.interfaceId;
                arc.toInterface = vRef.interfaceId;
                arc.domainId = bd.id;
                arc.underlyingLinkIds = best.pathLinks;
                arc.switchesTraversed = best.switchesTraversed;
                arc.cost = totalCost;
                arc.propagationDelayMs = totalDelay;
                arc.bandwidthBps = minBandwidth;
                arc.availableBandwidthBps = minAvailableBandwidth;
                arc.packetLossProbability = totalLoss;
                arc.mtuBytes = minMtu;

                l3.addL3Arc(std::move(arc));
            }
        }
    }

    return l3;
}

} // namespace nettrace

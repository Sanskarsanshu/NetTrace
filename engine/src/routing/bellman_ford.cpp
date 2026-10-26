#include "nettrace/routing/bellman_ford.hpp"
#include <chrono>
#include <limits>
#include <map>
#include <set>
#include <algorithm>

namespace nettrace {

RouteResult BellmanFord::findPath(const Graph& graph, 
                                 const NodeId& source, 
                                 const NodeId& destination, 
                                 Options options) {
    auto startTime = std::chrono::high_resolution_clock::now();
    const std::string objName = "cost";
    const std::string objUnit = "cost_units";

    if (!graph.hasNode(source)) {
        return RouteResult::unreachable(objName, objUnit, ErrorCode::UnknownNode, 
            "Source node '" + source.str() + "' does not exist in graph");
    }
    if (!graph.hasNode(destination)) {
        return RouteResult::unreachable(objName, objUnit, ErrorCode::UnknownNode, 
            "Destination node '" + destination.str() + "' does not exist in graph");
    }

    const NodeInfo* srcNode = graph.getNode(source);
    const NodeInfo* dstNode = graph.getNode(destination);
    if (options.excludeInactive) {
        if (!srcNode || !srcNode->active) {
            return RouteResult::unreachable(objName, objUnit, ErrorCode::Unreachable, 
                "Source node '" + source.str() + "' is inactive");
        }
        if (!dstNode || !dstNode->active) {
            return RouteResult::unreachable(objName, objUnit, ErrorCode::Unreachable, 
                "Destination node '" + destination.str() + "' is inactive");
        }
    }

    // Source equals destination
    if (source == destination) {
        RouteResult res;
        res.reachable = true;
        res.pathNodeIds = {source};
        res.pathLinkIds = {};
        res.objectiveName = objName;
        res.objectiveValue = 0.0;
        res.objectiveUnit = objUnit;
        res.totalCost = 0.0;
        res.linkHopCount = 0;
        res.routerHopCount = 0;
        res.bottleneckBandwidthBps = 0;
        res.estimatedDelayMs = 0.0;
        res.estimatedLossProbability = 0.0;
        res.exploredNodeCount = 1;
        res.relaxationCount = 0;
        auto endTime = std::chrono::high_resolution_clock::now();
        res.algorithmRuntimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return res;
    }

    const auto& allNodes = graph.nodeIds();
    const size_t V = allNodes.size();

    std::map<NodeId, Cost> dist;
    std::map<NodeId, uint32_t> hops;
    std::map<NodeId, std::pair<NodeId, Edge>> pred;
    std::vector<std::string> warnings;

    for (const auto& nid : allNodes) {
        dist[nid] = std::numeric_limits<Cost>::infinity();
        hops[nid] = std::numeric_limits<uint32_t>::max();
    }
    dist[source] = 0.0;
    hops[source] = 0;

    // Collect all active directed edges
    std::vector<Edge> allEdges;
    for (const auto& u : allNodes) {
        for (const auto& edge : graph.getOutgoingEdges(u, options.excludeInactive)) {
            allEdges.push_back(edge);
        }
    }

    // Check for bidirectional negative edges (immediate 2-cycle warning)
    for (const auto& edge : allEdges) {
        if (edge.cost < 0.0) {
            for (const auto& back : allEdges) {
                if (back.from == edge.to && back.to == edge.from && back.linkId == edge.linkId && back.cost < 0.0) {
                    warnings.push_back("Bidirectional link '" + edge.linkId.str() + "' has negative cost (" + 
                        std::to_string(edge.cost) + "), creating a 2-node negative cycle between '" + 
                        edge.from.str() + "' and '" + edge.to.str() + "'");
                    break;
                }
            }
        }
    }

    uint32_t totalRelaxations = 0;
    uint32_t completedRounds = 0;

    // Relax edges up to V - 1 times
    for (size_t i = 1; i < V; ++i) {
        bool anyRelaxation = false;
        completedRounds++;

        for (const auto& edge : allEdges) {
            NodeId u = edge.from;
            NodeId v = edge.to;

            // Only relax from reachable nodes
            if (dist[u] == std::numeric_limits<Cost>::infinity()) {
                continue;
            }

            Cost newDist = dist[u] + edge.cost;
            uint32_t newHops = hops[u] + 1;

            bool isBetter = false;
            if (newDist < dist[v]) {
                isBetter = true;
            } else if (std::abs(newDist - dist[v]) < 1e-9) {
                if (newHops < hops[v]) {
                    isBetter = true;
                } else if (newHops == hops[v]) {
                    if (u < pred[v].first) {
                        isBetter = true;
                    }
                }
            }

            if (isBetter) {
                dist[v] = newDist;
                hops[v] = newHops;
                pred[v] = {u, edge};
                totalRelaxations++;
                anyRelaxation = true;
            }
        }

        if (!anyRelaxation) {
            break;
        }
    }

    // V-th round: detect negative cycles reachable from source
    NodeId cycleStartNode;
    bool hasNegativeCycle = false;

    for (const auto& edge : allEdges) {
        NodeId u = edge.from;
        NodeId v = edge.to;

        if (dist[u] == std::numeric_limits<Cost>::infinity()) {
            continue; // Ignore unreachable components
        }

        if (dist[u] + edge.cost < dist[v] - 1e-9) {
            hasNegativeCycle = true;
            cycleStartNode = v;
            pred[v] = {u, edge};
            break;
        }
    }

    if (hasNegativeCycle) {
        // Trace back V times to find a node guaranteed to be in the cycle
        NodeId cycleNode = cycleStartNode;
        for (size_t i = 0; i < V; ++i) {
            cycleNode = pred[cycleNode].first;
        }

        // Now collect the cycle
        std::vector<NodeId> cycle;
        NodeId curr = cycleNode;
        do {
            cycle.push_back(curr);
            curr = pred[curr].first;
        } while (curr != cycleNode && cycle.size() <= V + 1);
        cycle.push_back(cycleNode);
        std::reverse(cycle.begin(), cycle.end());

        auto endTime = std::chrono::high_resolution_clock::now();
        RouteResult res = RouteResult::unreachable(objName, objUnit, ErrorCode::NegativeCycle,
            "Reachable negative-weight cycle detected from source '" + source.str() + "'");
        res.cycleNodeIds = std::move(cycle);
        res.warnings = std::move(warnings);
        res.relaxationCount = totalRelaxations;
        res.exploredNodeCount = static_cast<uint32_t>(V);
        res.algorithmRuntimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return res;
    }

    // If destination was not reached
    if (dist[destination] == std::numeric_limits<Cost>::infinity()) {
        auto endTime = std::chrono::high_resolution_clock::now();
        RouteResult res = RouteResult::unreachable(objName, objUnit, ErrorCode::Unreachable,
            "No path exists between source '" + source.str() + "' and destination '" + destination.str() + "'");
        res.warnings = std::move(warnings);
        res.relaxationCount = totalRelaxations;
        res.exploredNodeCount = static_cast<uint32_t>(completedRounds);
        res.algorithmRuntimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return res;
    }

    // Reconstruct path
    std::vector<NodeId> pathNodes;
    std::vector<LinkId> pathLinks;
    std::vector<Edge> pathEdges;

    NodeId curr = destination;
    while (curr != source) {
        pathNodes.push_back(curr);
        auto it = pred.find(curr);
        if (it == pred.end()) break;
        pathLinks.push_back(it->second.second.linkId);
        pathEdges.push_back(it->second.second);
        curr = it->second.first;
    }
    pathNodes.push_back(source);

    std::reverse(pathNodes.begin(), pathNodes.end());
    std::reverse(pathLinks.begin(), pathLinks.end());
    std::reverse(pathEdges.begin(), pathEdges.end());

    // Metrics along path
    Cost totalCost = 0.0;
    DelayMs totalDelay = 0.0;
    BandwidthBps bottleneckBps = std::numeric_limits<BandwidthBps>::max();
    double survivalProb = 1.0;

    for (const auto& edge : pathEdges) {
        totalCost += edge.cost;
        totalDelay += edge.delayMs;
        bottleneckBps = std::min(bottleneckBps, edge.bandwidthBps);
        survivalProb *= (1.0 - edge.lossProbability);
    }
    if (pathEdges.empty()) {
        bottleneckBps = 0;
    }

    RouteResult res;
    res.reachable = true;
    res.pathNodeIds = std::move(pathNodes);
    res.pathLinkIds = std::move(pathLinks);
    res.objectiveName = objName;
    res.objectiveValue = totalCost;
    res.objectiveUnit = objUnit;
    res.totalCost = totalCost;
    res.linkHopCount = static_cast<uint32_t>(res.pathLinkIds.size());
    res.routerHopCount = 0;
    res.bottleneckBandwidthBps = bottleneckBps;
    res.estimatedDelayMs = totalDelay;
    res.estimatedLossProbability = 1.0 - survivalProb;
    res.exploredNodeCount = static_cast<uint32_t>(completedRounds);
    res.relaxationCount = totalRelaxations;
    res.warnings = std::move(warnings);

    auto endTime = std::chrono::high_resolution_clock::now();
    res.algorithmRuntimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return res;
}

} // namespace nettrace

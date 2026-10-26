#include "nettrace/routing/bfs.hpp"
#include <chrono>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <algorithm>

namespace nettrace {

RouteResult BFS::findPath(const Graph& graph, 
                         const NodeId& source, 
                         const NodeId& destination, 
                         Options options) {
    auto startTime = std::chrono::high_resolution_clock::now();
    const std::string objName = "fewest_hops";
    const std::string objUnit = "hops";

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

    // Source equals destination edge case
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

    std::queue<NodeId> q;
    std::set<NodeId> visited;
    std::map<NodeId, std::pair<NodeId, Edge>> pred;

    q.push(source);
    visited.insert(source);

    uint32_t exploredCount = 0;
    bool found = false;

    while (!q.empty()) {
        NodeId u = q.front();
        q.pop();
        exploredCount++;

        if (u == destination) {
            found = true;
            break;
        }

        // Neighbors are returned in stable deterministic sorted order
        for (const auto& edge : graph.getOutgoingEdges(u, options.excludeInactive)) {
            NodeId v = edge.to;
            if (visited.find(v) == visited.end()) {
                visited.insert(v);
                pred[v] = {u, edge};
                q.push(v);
                if (v == destination) {
                    found = true;
                    break;
                }
            }
        }
        if (found) break;
    }

    if (!found) {
        auto endTime = std::chrono::high_resolution_clock::now();
        RouteResult res = RouteResult::unreachable(objName, objUnit, ErrorCode::Unreachable,
            "No path exists between source '" + source.str() + "' and destination '" + destination.str() + "'");
        res.exploredNodeCount = exploredCount;
        res.algorithmRuntimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return res;
    }

    // Path reconstruction
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

    // Calculate metrics along reconstructed path
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
    res.objectiveValue = static_cast<double>(res.pathLinkIds.size());
    res.objectiveUnit = objUnit;
    res.totalCost = totalCost; // Reconstructed post-hoc cost
    res.linkHopCount = static_cast<uint32_t>(res.pathLinkIds.size());
    res.routerHopCount = 0;
    res.bottleneckBandwidthBps = bottleneckBps;
    res.estimatedDelayMs = totalDelay;
    res.estimatedLossProbability = 1.0 - survivalProb;
    res.exploredNodeCount = exploredCount;
    res.relaxationCount = 0;

    auto endTime = std::chrono::high_resolution_clock::now();
    res.algorithmRuntimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return res;
}

} // namespace nettrace

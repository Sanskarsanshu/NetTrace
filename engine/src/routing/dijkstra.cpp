#include "nettrace/routing/dijkstra.hpp"
#include <chrono>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <algorithm>

namespace nettrace {

namespace {

struct QueueItem {
    Cost dist;
    uint32_t hops;
    NodeId u;

    // Greater operator for min-heap (lower dist first, then fewer hops, then lexicographical u)
    bool operator>(const QueueItem& other) const {
        if (dist != other.dist) return dist > other.dist;
        if (hops != other.hops) return hops > other.hops;
        return u > other.u;
    }
};

} // namespace

RouteResult Dijkstra::findPath(const Graph& graph, 
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

    // Source equals destination edge case (TASKS M1-22)
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

    std::map<NodeId, Cost> dist;
    std::map<NodeId, uint32_t> hops;
    std::map<NodeId, std::pair<NodeId, Edge>> pred;
    std::set<NodeId> settled;

    for (const auto& nid : graph.nodeIds()) {
        dist[nid] = std::numeric_limits<Cost>::infinity();
        hops[nid] = std::numeric_limits<uint32_t>::max();
    }

    dist[source] = 0.0;
    hops[source] = 0;

    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
    pq.push({0.0, 0, source});

    uint32_t exploredCount = 0;
    uint32_t relaxationCount = 0;

    while (!pq.empty()) {
        QueueItem top = pq.top();
        pq.pop();
        NodeId u = top.u;

        if (settled.find(u) != settled.end()) {
            continue;
        }
        settled.insert(u);
        exploredCount++;

        if (u == destination) {
            break;
        }

        for (const auto& edge : graph.getOutgoingEdges(u, options.excludeInactive)) {
            if (edge.cost < 0.0) {
                return RouteResult::unreachable(objName, objUnit, ErrorCode::NegativeWeight,
                    "Negative weight encountered on link '" + edge.linkId.str() + 
                    "'; Dijkstra does not support negative weights");
            }

            NodeId v = edge.to;
            if (settled.find(v) != settled.end()) {
                continue;
            }

            Cost newDist = dist[u] + edge.cost;
            uint32_t newHops = hops[u] + 1;

            bool isBetter = false;
            if (newDist < dist[v]) {
                isBetter = true;
            } else if (std::abs(newDist - dist[v]) < 1e-9) {
                // Tie-break: fewer hops
                if (newHops < hops[v]) {
                    isBetter = true;
                } else if (newHops == hops[v]) {
                    // Tie-break: compare predecessor node IDs lexicographically
                    if (u < pred[v].first) {
                        isBetter = true;
                    }
                }
            }

            if (isBetter) {
                dist[v] = newDist;
                hops[v] = newHops;
                pred[v] = {u, edge};
                relaxationCount++;
                pq.push({newDist, newHops, v});
            }
        }
    }

    if (dist[destination] == std::numeric_limits<Cost>::infinity()) {
        auto endTime = std::chrono::high_resolution_clock::now();
        RouteResult res = RouteResult::unreachable(objName, objUnit, ErrorCode::Unreachable,
            "No path exists between source '" + source.str() + "' and destination '" + destination.str() + "'");
        res.exploredNodeCount = exploredCount;
        res.relaxationCount = relaxationCount;
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
        if (it == pred.end()) {
            break; // Sanity check
        }
        pathLinks.push_back(it->second.second.linkId);
        pathEdges.push_back(it->second.second);
        curr = it->second.first;
    }
    pathNodes.push_back(source);

    std::reverse(pathNodes.begin(), pathNodes.end());
    std::reverse(pathLinks.begin(), pathLinks.end());
    std::reverse(pathEdges.begin(), pathEdges.end());

    // Calculate metrics along path
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
    res.exploredNodeCount = exploredCount;
    res.relaxationCount = relaxationCount;

    auto endTime = std::chrono::high_resolution_clock::now();
    res.algorithmRuntimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return res;
}

} // namespace nettrace

#include "nettrace/routing/widest_path.hpp"
#include <chrono>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <algorithm>

namespace nettrace {

namespace {

struct WidestQueueItem {
    BandwidthBps width;
    Cost cost;
    uint32_t hops;
    NodeId u;

    // Less operator for max-heap (higher width first, then lower cost, then fewer hops, then lexicographical u)
    bool operator<(const WidestQueueItem& other) const {
        if (width != other.width) return width < other.width;
        if (cost != other.cost) return cost > other.cost;
        if (hops != other.hops) return hops > other.hops;
        return u > other.u;
    }
};

} // namespace

RouteResult WidestPath::findPath(const Graph& graph, 
                                 const NodeId& source, 
                                 const NodeId& destination, 
                                 Options options) {
    auto startTime = std::chrono::high_resolution_clock::now();
    const std::string objName = "bottleneck_bandwidth";
    const std::string objUnit = "bps";

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

    std::map<NodeId, BandwidthBps> width;
    std::map<NodeId, Cost> cost;
    std::map<NodeId, uint32_t> hops;
    std::map<NodeId, std::pair<NodeId, Edge>> pred;
    std::set<NodeId> settled;

    for (const auto& nid : graph.nodeIds()) {
        width[nid] = 0;
        cost[nid] = std::numeric_limits<Cost>::infinity();
        hops[nid] = std::numeric_limits<uint32_t>::max();
    }

    width[source] = std::numeric_limits<BandwidthBps>::max();
    cost[source] = 0.0;
    hops[source] = 0;

    std::priority_queue<WidestQueueItem> pq;
    pq.push({width[source], 0.0, 0, source});

    uint32_t exploredCount = 0;
    uint32_t relaxationCount = 0;

    while (!pq.empty()) {
        WidestQueueItem top = pq.top();
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
            NodeId v = edge.to;
            if (settled.find(v) != settled.end()) {
                continue;
            }

            BandwidthBps candidateWidth = std::min(width[u], edge.bandwidthBps);
            Cost candidateCost = cost[u] + edge.cost;
            uint32_t candidateHops = hops[u] + 1;

            bool isBetter = false;
            if (candidateWidth > width[v]) {
                isBetter = true;
            } else if (candidateWidth == width[v]) {
                // Tie-break: lower total cost
                if (candidateCost < cost[v]) {
                    isBetter = true;
                } else if (std::abs(candidateCost - cost[v]) < 1e-9) {
                    // Tie-break: fewer hops
                    if (candidateHops < hops[v]) {
                        isBetter = true;
                    } else if (candidateHops == hops[v]) {
                        // Tie-break: lexicographical predecessor
                        if (u < pred[v].first) {
                            isBetter = true;
                        }
                    }
                }
            }

            if (isBetter) {
                width[v] = candidateWidth;
                cost[v] = candidateCost;
                hops[v] = candidateHops;
                pred[v] = {u, edge};
                relaxationCount++;
                pq.push({candidateWidth, candidateCost, candidateHops, v});
            }
        }
    }

    if (width[destination] == 0) {
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
        if (it == pred.end()) break;
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
    res.objectiveValue = static_cast<double>(bottleneckBps);
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

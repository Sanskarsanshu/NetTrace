#include "test_framework.hpp"
#include "nettrace/routing/graph.hpp"
#include "nettrace/routing/dijkstra.hpp"
#include "nettrace/routing/bfs.hpp"
#include "nettrace/routing/bellman_ford.hpp"
#include "nettrace/routing/widest_path.hpp"

#include <random>
#include <algorithm>
#include <vector>
#include <cmath>

using namespace nettrace;

namespace {

// Helper to recursively find all simple paths from u to target
void dfsAllSimplePaths(const Graph& g, const NodeId& current, const NodeId& target,
                        std::vector<NodeId>& visited,
                        std::vector<std::vector<NodeId>>& allPaths) {
    visited.push_back(current);
    if (current == target) {
        allPaths.push_back(visited);
    } else {
        for (const auto& edge : g.getOutgoingEdges(current, true)) {
            if (std::find(visited.begin(), visited.end(), edge.to) == visited.end()) {
                dfsAllSimplePaths(g, edge.to, target, visited, allPaths);
            }
        }
    }
    visited.pop_back();
}

} // namespace

TEST_CASE("T-1-13: Determinism: 100 repeated runs with shuffled insertion order produce identical path") {
    // Construct a diamond graph with multiple equal-cost alternative routes:
    // node-0 -> node-1 -> node-3 -> node-5 (cost 10 + 10 + 10 = 30)
    // node-0 -> node-2 -> node-4 -> node-5 (cost 10 + 10 + 10 = 30)
    // node-0 -> node-1 -> node-4 -> node-5 (cost 10 + 10 + 10 = 30)
    // node-0 -> node-2 -> node-3 -> node-5 (cost 10 + 10 + 10 = 30)
    struct RawEdge {
        std::string linkId;
        std::string from;
        std::string to;
        double cost;
    };

    std::vector<RawEdge> edgeDefs = {
        {"l_01", "node-0", "node-1", 10.0},
        {"l_02", "node-0", "node-2", 10.0},
        {"l_13", "node-1", "node-3", 10.0},
        {"l_14", "node-1", "node-4", 10.0},
        {"l_23", "node-2", "node-3", 10.0},
        {"l_24", "node-2", "node-4", 10.0},
        {"l_35", "node-3", "node-5", 10.0},
        {"l_45", "node-4", "node-5", 10.0}
    };

    std::vector<NodeId> baselineNodes;
    std::vector<LinkId> baselineLinks;

    for (int iter = 0; iter < 100; ++iter) {
        Graph g;
        for (int i = 0; i <= 5; ++i) {
            REQUIRE(g.addNode(NodeId("node-" + std::to_string(i))).isOk());
        }

        auto shuffledEdges = edgeDefs;
        std::mt19937_64 rng(12345ULL + iter);
        std::shuffle(shuffledEdges.begin(), shuffledEdges.end(), rng);

        for (const auto& re : shuffledEdges) {
            Edge e{LinkId(re.linkId), NodeId(re.from), NodeId(re.to), InterfaceId("e1"), InterfaceId("e1"), re.cost};
            REQUIRE(g.addLink(e, Directionality::Directed).isOk());
        }

        auto result = Dijkstra::findPath(g, NodeId("node-0"), NodeId("node-5"));
        REQUIRE(result.reachable);

        if (iter == 0) {
            baselineNodes = result.pathNodeIds;
            baselineLinks = result.pathLinkIds;
        } else {
            REQUIRE_EQ(result.pathNodeIds.size(), baselineNodes.size());
            for (size_t k = 0; k < baselineNodes.size(); ++k) {
                REQUIRE_EQ(result.pathNodeIds[k].str(), baselineNodes[k].str());
            }
            REQUIRE_EQ(result.pathLinkIds.size(), baselineLinks.size());
            for (size_t k = 0; k < baselineLinks.size(); ++k) {
                REQUIRE_EQ(result.pathLinkIds[k].str(), baselineLinks[k].str());
            }
        }
    }
}

TEST_CASE("T-1-14: Property test: Dijkstra == Bellman-Ford == Floyd-Warshall reference on seeded random graphs") {
    constexpr double INF = 1e12;
    std::mt19937_64 rng(99991ULL);

    for (int graphIdx = 0; graphIdx < 20; ++graphIdx) {
        int n = 8;
        Graph g;
        for (int i = 0; i < n; ++i) {
            REQUIRE(g.addNode(NodeId("n" + std::to_string(i))).isOk());
        }

        std::vector<std::vector<double>> dist(n, std::vector<double>(n, INF));
        for (int i = 0; i < n; ++i) dist[i][i] = 0.0;

        int edgeCount = 14 + (rng() % 10);
        int linkNum = 0;
        for (int e = 0; e < edgeCount; ++e) {
            int u = rng() % n;
            int v = rng() % n;
            if (u == v) continue; // Skip self loops

            double cost = static_cast<double>(1 + (rng() % 20));
            std::string linkId = "l_" + std::to_string(linkNum++);
            Edge edge{LinkId(linkId), NodeId("n" + std::to_string(u)), NodeId("n" + std::to_string(v)),
                     InterfaceId("e1"), InterfaceId("e1"), cost};

            REQUIRE(g.addLink(edge, Directionality::Directed).isOk());
            dist[u][v] = std::min(dist[u][v], cost);
        }

        // Floyd-Warshall all-pairs shortest paths reference
        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    if (dist[i][k] < INF && dist[k][j] < INF) {
                        dist[i][j] = std::min(dist[i][j], dist[i][k] + dist[k][j]);
                    }
                }
            }
        }

        int src = 0;
        int dst = n - 1;
        auto dijkstraRes = Dijkstra::findPath(g, NodeId("n" + std::to_string(src)), NodeId("n" + std::to_string(dst)));
        auto bfRes = BellmanFord::findPath(g, NodeId("n" + std::to_string(src)), NodeId("n" + std::to_string(dst)));

        if (dist[src][dst] < INF) {
            REQUIRE(dijkstraRes.reachable);
            REQUIRE(bfRes.reachable);
            REQUIRE(std::abs(dijkstraRes.totalCost - dist[src][dst]) < 1e-9);
            REQUIRE(std::abs(bfRes.totalCost - dist[src][dst]) < 1e-9);
        } else {
            REQUIRE_FALSE(dijkstraRes.reachable);
            REQUIRE_FALSE(bfRes.reachable);
        }
    }
}

TEST_CASE("T-1-15: Property test: BFS length <= any other path edge count on seeded random graphs") {
    std::mt19937_64 rng(88883ULL);

    for (int graphIdx = 0; graphIdx < 20; ++graphIdx) {
        int n = 7;
        Graph g;
        for (int i = 0; i < n; ++i) {
            REQUIRE(g.addNode(NodeId("n" + std::to_string(i))).isOk());
        }

        int edgeCount = 12 + (rng() % 6);
        int linkNum = 0;
        for (int e = 0; e < edgeCount; ++e) {
            int u = rng() % n;
            int v = rng() % n;
            if (u == v) continue;

            double cost = static_cast<double>(1 + (rng() % 10));
            std::string linkId = "l_" + std::to_string(linkNum++);
            Edge edge{LinkId(linkId), NodeId("n" + std::to_string(u)), NodeId("n" + std::to_string(v)),
                     InterfaceId("e1"), InterfaceId("e1"), cost};
            REQUIRE(g.addLink(edge, Directionality::Directed).isOk());
        }

        NodeId src("n0");
        NodeId dst("n" + std::to_string(n - 1));
        auto bfsRes = BFS::findPath(g, src, dst);

        std::vector<NodeId> visited;
        std::vector<std::vector<NodeId>> allPaths;
        dfsAllSimplePaths(g, src, dst, visited, allPaths);

        if (!allPaths.empty()) {
            REQUIRE(bfsRes.reachable);
            for (const auto& path : allPaths) {
                uint32_t pathEdgeCount = static_cast<uint32_t>(path.size() - 1);
                REQUIRE(bfsRes.linkHopCount <= pathEdgeCount);
            }
        } else {
            REQUIRE_FALSE(bfsRes.reachable);
        }
    }
}

TEST_CASE("T-1-16: Property test: Widest-Path bottleneck >= any other path bottleneck on seeded random graphs") {
    std::mt19937_64 rng(77773ULL);

    for (int graphIdx = 0; graphIdx < 20; ++graphIdx) {
        int n = 6;
        Graph g;
        for (int i = 0; i < n; ++i) {
            REQUIRE(g.addNode(NodeId("n" + std::to_string(i))).isOk());
        }

        int edgeCount = 10 + (rng() % 6);
        int linkNum = 0;
        for (int e = 0; e < edgeCount; ++e) {
            int u = rng() % n;
            int v = rng() % n;
            if (u == v) continue;

            uint64_t bw = (1 + (rng() % 10)) * 100000000ULL; // 100 Mbps .. 1 Gbps
            std::string linkId = "l_" + std::to_string(linkNum++);
            Edge edge{LinkId(linkId), NodeId("n" + std::to_string(u)), NodeId("n" + std::to_string(v)),
                     InterfaceId("e1"), InterfaceId("e1"), 1.0, bw};
            REQUIRE(g.addLink(edge, Directionality::Directed).isOk());
        }

        NodeId src("n0");
        NodeId dst("n" + std::to_string(n - 1));
        auto widestRes = WidestPath::findPath(g, src, dst);

        std::vector<NodeId> visited;
        std::vector<std::vector<NodeId>> allPaths;
        dfsAllSimplePaths(g, src, dst, visited, allPaths);

        if (!allPaths.empty()) {
            REQUIRE(widestRes.reachable);
            for (const auto& path : allPaths) {
                uint64_t pathBottleneck = std::numeric_limits<uint64_t>::max();
                for (size_t i = 0; i + 1 < path.size(); ++i) {
                    uint64_t maxDirectEdgeBw = 0;
                    for (const auto& edge : g.getOutgoingEdges(path[i], true)) {
                        if (edge.to == path[i + 1]) {
                            maxDirectEdgeBw = std::max(maxDirectEdgeBw, edge.bandwidthBps);
                        }
                    }
                    pathBottleneck = std::min(pathBottleneck, maxDirectEdgeBw);
                }
                REQUIRE(widestRes.bottleneckBandwidthBps >= pathBottleneck);
            }
        } else {
            REQUIRE_FALSE(widestRes.reachable);
        }
    }
}

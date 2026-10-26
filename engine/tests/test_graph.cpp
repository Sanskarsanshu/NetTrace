#include "test_framework.hpp"
#include "nettrace/routing/graph.hpp"
#include "nettrace/routing/dijkstra.hpp"
#include "nettrace/routing/bfs.hpp"
#include "nettrace/routing/bellman_ford.hpp"
#include "nettrace/routing/widest_path.hpp"
#include <cmath>
#include <limits>

using namespace nettrace;

TEST_CASE("T-1-08: Unreachable destination returns reachable=false for all four algorithms") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk()); // Disconnected

    auto bfsRes = BFS::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE_FALSE(bfsRes.reachable);
    REQUIRE_EQ(bfsRes.failureCode, ErrorCode::Unreachable);

    auto dijkstraRes = Dijkstra::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE_FALSE(dijkstraRes.reachable);
    REQUIRE_EQ(dijkstraRes.failureCode, ErrorCode::Unreachable);

    auto bfRes = BellmanFord::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE_FALSE(bfRes.reachable);
    REQUIRE_EQ(bfRes.failureCode, ErrorCode::Unreachable);

    auto widestRes = WidestPath::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE_FALSE(widestRes.reachable);
    REQUIRE_EQ(widestRes.failureCode, ErrorCode::Unreachable);
}

TEST_CASE("T-1-09: Source equals destination returns single-node path for all four algorithms") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());

    auto bfsRes = BFS::findPath(g, NodeId("A"), NodeId("A"));
    REQUIRE(bfsRes.reachable);
    REQUIRE_EQ(bfsRes.pathNodeIds.size(), 1);
    REQUIRE_EQ(bfsRes.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(bfsRes.pathLinkIds.size(), 0);
    REQUIRE_EQ(bfsRes.linkHopCount, 0);
    REQUIRE_EQ(bfsRes.totalCost, 0.0);

    auto dijkstraRes = Dijkstra::findPath(g, NodeId("A"), NodeId("A"));
    REQUIRE(dijkstraRes.reachable);
    REQUIRE_EQ(dijkstraRes.pathNodeIds.size(), 1);
    REQUIRE_EQ(dijkstraRes.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(dijkstraRes.pathLinkIds.size(), 0);
    REQUIRE_EQ(dijkstraRes.totalCost, 0.0);

    auto bfRes = BellmanFord::findPath(g, NodeId("A"), NodeId("A"));
    REQUIRE(bfRes.reachable);
    REQUIRE_EQ(bfRes.pathNodeIds.size(), 1);
    REQUIRE_EQ(bfRes.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(bfRes.pathLinkIds.size(), 0);
    REQUIRE_EQ(bfRes.totalCost, 0.0);

    auto widestRes = WidestPath::findPath(g, NodeId("A"), NodeId("A"));
    REQUIRE(widestRes.reachable);
    REQUIRE_EQ(widestRes.pathNodeIds.size(), 1);
    REQUIRE_EQ(widestRes.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(widestRes.pathLinkIds.size(), 0);
    REQUIRE_EQ(widestRes.totalCost, 0.0);
}

TEST_CASE("T-1-10: Parallel edges: correct edge chosen per algorithm and identified by pathLinkIds") {
    Graph g;
    REQUIRE(g.addNode(NodeId("node-1")).isOk());
    REQUIRE(g.addNode(NodeId("node-2")).isOk());

    // Parallel edge 1: cheap (cost 1.0), low bandwidth (10 Mbps)
    Edge p1{LinkId("link-cheap"), NodeId("node-1"), NodeId("node-2"), InterfaceId("e1"), InterfaceId("e1"), 1.0, 10000000ULL};
    // Parallel edge 2: fast (cost 10.0), high bandwidth (1 Gbps)
    Edge p2{LinkId("link-fast"), NodeId("node-1"), NodeId("node-2"), InterfaceId("e2"), InterfaceId("e2"), 10.0, 1000000000ULL};

    REQUIRE(g.addLink(p1, Directionality::Directed).isOk());
    REQUIRE(g.addLink(p2, Directionality::Directed).isOk());

    // Dijkstra chooses the cheaper edge: link-cheap
    auto dijkstraRes = Dijkstra::findPath(g, NodeId("node-1"), NodeId("node-2"));
    REQUIRE(dijkstraRes.reachable);
    REQUIRE_EQ(dijkstraRes.totalCost, 1.0);
    REQUIRE_EQ(dijkstraRes.pathLinkIds.size(), 1);
    REQUIRE_EQ(dijkstraRes.pathLinkIds[0].str(), "link-cheap");

    // Widest-Path chooses the higher bandwidth edge: link-fast
    auto widestRes = WidestPath::findPath(g, NodeId("node-1"), NodeId("node-2"));
    REQUIRE(widestRes.reachable);
    REQUIRE_EQ(widestRes.bottleneckBandwidthBps, 1000000000ULL);
    REQUIRE_EQ(widestRes.pathLinkIds.size(), 1);
    REQUIRE_EQ(widestRes.pathLinkIds[0].str(), "link-fast");
}

TEST_CASE("T-1-11: Directed link usable only one way; bidirectional usable both ways") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());

    // Directed link A -> B
    REQUIRE(g.addLink({LinkId("l_dir"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0}, Directionality::Directed).isOk());
    // Bidirectional link B <-> C
    REQUIRE(g.addLink({LinkId("l_bi"), NodeId("B"), NodeId("C"), InterfaceId("e2"), InterfaceId("e1"), 2.0}, Directionality::Bidirectional).isOk());

    // Can go A -> B, but CANNOT go B -> A
    auto aToB = Dijkstra::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE(aToB.reachable);

    auto bToA = Dijkstra::findPath(g, NodeId("B"), NodeId("A"));
    REQUIRE_FALSE(bToA.reachable);

    // Can go B -> C AND C -> B
    auto bToC = Dijkstra::findPath(g, NodeId("B"), NodeId("C"));
    REQUIRE(bToC.reachable);
    REQUIRE_EQ(bToC.totalCost, 2.0);

    auto cToB = Dijkstra::findPath(g, NodeId("C"), NodeId("B"));
    REQUIRE(cToB.reachable);
    REQUIRE_EQ(cToB.totalCost, 2.0);
}

TEST_CASE("T-1-12: Inactive link and node excluded from path") {
    Graph g;
    REQUIRE(g.addNode(NodeId("node-1")).isOk());
    REQUIRE(g.addNode(NodeId("node-2"), "Node 2", false).isOk()); // Inactive node
    REQUIRE(g.addNode(NodeId("node-3")).isOk());
    REQUIRE(g.addNode(NodeId("node-4")).isOk());

    // Path through inactive node-2
    REQUIRE(g.addLink({LinkId("l1"), NodeId("node-1"), NodeId("node-2"), InterfaceId("e1"), InterfaceId("e1"), 1.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l2"), NodeId("node-2"), NodeId("node-4"), InterfaceId("e2"), InterfaceId("e1"), 1.0}, Directionality::Directed).isOk());

    // Path through active node-3, but link l4 is inactive
    REQUIRE(g.addLink({LinkId("l3"), NodeId("node-1"), NodeId("node-3"), InterfaceId("e3"), InterfaceId("e1"), 2.0}, Directionality::Directed).isOk());
    Edge l4{LinkId("l4"), NodeId("node-3"), NodeId("node-4"), InterfaceId("e2"), InterfaceId("e2"), 2.0};
    l4.active = false;
    REQUIRE(g.addLink(l4, Directionality::Directed).isOk());

    auto result = Dijkstra::findPath(g, NodeId("node-1"), NodeId("node-4"));
    REQUIRE_FALSE(result.reachable);
}

TEST_CASE("T-1-17: Graph validation rejects malformed graphs") {
    // 1. Duplicate node ID rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("dup-node")).isOk());
        auto res = g.addNode(NodeId("dup-node"));
        REQUIRE_FALSE(res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::DuplicateId);
    }

    // 2. Duplicate link ID rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        REQUIRE(g.addNode(NodeId("B")).isOk());
        REQUIRE(g.addNode(NodeId("C")).isOk());
        REQUIRE(g.addLink({LinkId("l-dup"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0}).isOk());
        auto res = g.addLink({LinkId("l-dup"), NodeId("B"), NodeId("C"), InterfaceId("e2"), InterfaceId("e1"), 2.0});
        REQUIRE_FALSE(res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::DuplicateId);
    }

    // 3. Non-finite cost (NaN and Inf) rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        REQUIRE(g.addNode(NodeId("B")).isOk());
        auto resNaN = g.addLink({LinkId("l-nan"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), std::numeric_limits<double>::quiet_NaN()});
        REQUIRE_FALSE(resNaN.isOk());
        REQUIRE_EQ(resNaN.error().code, ErrorCode::InvalidWeight);

        auto resInf = g.addLink({LinkId("l-inf"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), std::numeric_limits<double>::infinity()});
        REQUIRE_FALSE(resInf.isOk());
        REQUIRE_EQ(resInf.error().code, ErrorCode::InvalidWeight);
    }

    // 4. Non-finite or negative delay rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        REQUIRE(g.addNode(NodeId("B")).isOk());
        Edge eNegDelay{LinkId("l-neg-d"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0};
        eNegDelay.delayMs = -1.0;
        auto res = g.addLink(eNegDelay);
        REQUIRE_FALSE(res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::InvalidWeight);
    }

    // 5. Probability outside [0.0, 1.0] rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        REQUIRE(g.addNode(NodeId("B")).isOk());
        Edge eBadProb1{LinkId("l-prob1"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0};
        eBadProb1.lossProbability = -0.05;
        auto res1 = g.addLink(eBadProb1);
        REQUIRE_FALSE(res1.isOk());
        REQUIRE_EQ(res1.error().code, ErrorCode::InvalidProbability);

        Edge eBadProb2{LinkId("l-prob2"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0};
        eBadProb2.lossProbability = 1.05;
        auto res2 = g.addLink(eBadProb2);
        REQUIRE_FALSE(res2.isOk());
        REQUIRE_EQ(res2.error().code, ErrorCode::InvalidProbability);
    }

    // 6. Non-positive bandwidth (0) rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        REQUIRE(g.addNode(NodeId("B")).isOk());
        Edge eZeroBw{LinkId("l-zero-bw"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0, 0ULL};
        auto res = g.addLink(eZeroBw);
        REQUIRE_FALSE(res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::NonPositiveBandwidth);
    }

    // 7. Dangling endpoints rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        auto resSrc = g.addLink({LinkId("l-dang1"), NodeId("missing-src"), NodeId("A"), InterfaceId("e1"), InterfaceId("e1"), 1.0});
        REQUIRE_FALSE(resSrc.isOk());
        REQUIRE_EQ(resSrc.error().code, ErrorCode::DanglingEndpoint);

        auto resDst = g.addLink({LinkId("l-dang2"), NodeId("A"), NodeId("missing-dst"), InterfaceId("e1"), InterfaceId("e1"), 1.0});
        REQUIRE_FALSE(resDst.isOk());
        REQUIRE_EQ(resDst.error().code, ErrorCode::DanglingEndpoint);
    }

    // 8. Self-loops rejected
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        auto resSelf = g.addLink({LinkId("l-self"), NodeId("A"), NodeId("A"), InterfaceId("e1"), InterfaceId("e2"), 1.0});
        REQUIRE_FALSE(resSelf.isOk());
        REQUIRE_EQ(resSelf.error().code, ErrorCode::SelfLoopRejected);
    }
}

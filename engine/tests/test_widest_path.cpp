#include "test_framework.hpp"
#include "nettrace/routing/widest_path.hpp"
#include "nettrace/routing/dijkstra.hpp"

using namespace nettrace;

TEST_CASE("T-1-09: Widest-Path maximizes bottleneck bandwidth") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());
    REQUIRE(g.addNode(NodeId("D")).isOk());

    // Path 1: A -> B -> D (bandwidth: min(100 Mbps, 100 Mbps) = 100 Mbps)
    // Path 2: A -> C -> D (bandwidth: min(1000 Mbps, 500 Mbps) = 500 Mbps)
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0, 100000000ULL}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_bd"), NodeId("B"), NodeId("D"), InterfaceId("e2"), InterfaceId("e1"), 1.0, 100000000ULL}, Directionality::Directed).isOk());

    REQUIRE(g.addLink({LinkId("l_ac"), NodeId("A"), NodeId("C"), InterfaceId("e3"), InterfaceId("e1"), 10.0, 1000000000ULL}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_cd"), NodeId("C"), NodeId("D"), InterfaceId("e2"), InterfaceId("e2"), 10.0, 500000000ULL}, Directionality::Directed).isOk());

    auto result = WidestPath::findPath(g, NodeId("A"), NodeId("D"));
    REQUIRE(result.reachable);
    // Widest path must choose A -> C -> D with 500 Mbps bottleneck
    REQUIRE_EQ(result.bottleneckBandwidthBps, 500000000ULL);
    REQUIRE_EQ(result.objectiveValue, 500000000.0);
    REQUIRE_EQ(result.pathNodeIds.size(), 3);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathNodeIds[1].str(), "C");
    REQUIRE_EQ(result.pathNodeIds[2].str(), "D");
}

TEST_CASE("T-1-10: Divergent path between Dijkstra and Widest-Path on same graph") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());
    REQUIRE(g.addNode(NodeId("D")).isOk());

    // Path 1 (Low cost 2.0, low bandwidth 10 Mbps): A -> B -> D
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0, 10000000ULL}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_bd"), NodeId("B"), NodeId("D"), InterfaceId("e2"), InterfaceId("e1"), 1.0, 10000000ULL}, Directionality::Directed).isOk());

    // Path 2 (High cost 50.0, high bandwidth 1000 Mbps): A -> C -> D
    REQUIRE(g.addLink({LinkId("l_ac"), NodeId("A"), NodeId("C"), InterfaceId("e3"), InterfaceId("e1"), 25.0, 1000000000ULL}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_cd"), NodeId("C"), NodeId("D"), InterfaceId("e2"), InterfaceId("e2"), 25.0, 1000000000ULL}, Directionality::Directed).isOk());

    // Dijkstra optimizes cost: picks A -> B -> D
    auto dijkstraRes = Dijkstra::findPath(g, NodeId("A"), NodeId("D"));
    REQUIRE(dijkstraRes.reachable);
    REQUIRE_EQ(dijkstraRes.totalCost, 2.0);
    REQUIRE_EQ(dijkstraRes.pathNodeIds[1].str(), "B");

    // Widest-Path optimizes bandwidth: picks A -> C -> D
    auto widestRes = WidestPath::findPath(g, NodeId("A"), NodeId("D"));
    REQUIRE(widestRes.reachable);
    REQUIRE_EQ(widestRes.bottleneckBandwidthBps, 1000000000ULL);
    REQUIRE_EQ(widestRes.pathNodeIds[1].str(), "C");
}

TEST_CASE("T-1-11: Source equals destination handling in Widest-Path") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());

    auto result = WidestPath::findPath(g, NodeId("A"), NodeId("A"));
    REQUIRE(result.reachable);
    REQUIRE_EQ(result.pathNodeIds.size(), 1);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathLinkIds.size(), 0);
    REQUIRE_EQ(result.bottleneckBandwidthBps, 0);
    REQUIRE_EQ(result.totalCost, 0.0);
}

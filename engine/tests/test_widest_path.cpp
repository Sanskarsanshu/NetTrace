#include "test_framework.hpp"
#include "nettrace/routing/widest_path.hpp"

using namespace nettrace;

TEST_CASE("T-1-07: Widest-Path maximizes bottleneck bandwidth") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());
    REQUIRE(g.addNode(NodeId("D")).isOk());

    // Path 1 (Low cost 2.0, low bandwidth 100 Mbps): A -> B -> D
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 1.0, 100000000ULL}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_bd"), NodeId("B"), NodeId("D"), InterfaceId("e2"), InterfaceId("e1"), 1.0, 100000000ULL}, Directionality::Directed).isOk());

    // Path 2 (Higher cost 20.0, high bandwidth 500 Mbps bottleneck): A -> C -> D
    REQUIRE(g.addLink({LinkId("l_ac"), NodeId("A"), NodeId("C"), InterfaceId("e3"), InterfaceId("e1"), 10.0, 1000000000ULL}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_cd"), NodeId("C"), NodeId("D"), InterfaceId("e2"), InterfaceId("e2"), 10.0, 500000000ULL}, Directionality::Directed).isOk());

    auto result = WidestPath::findPath(g, NodeId("A"), NodeId("D"));
    REQUIRE(result.reachable);
    // Widest path must choose A -> C -> D with 500 Mbps bottleneck, despite higher cost
    REQUIRE_EQ(result.bottleneckBandwidthBps, 500000000ULL);
    REQUIRE_EQ(result.objectiveValue, 500000000.0);
    REQUIRE_EQ(result.pathNodeIds.size(), 3);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathNodeIds[1].str(), "C");
    REQUIRE_EQ(result.pathNodeIds[2].str(), "D");
    REQUIRE_EQ(result.totalCost, 20.0);
}

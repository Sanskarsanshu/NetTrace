#include "test_framework.hpp"
#include "nettrace/routing/dijkstra.hpp"

using namespace nettrace;

TEST_CASE("T-1-01: Dijkstra picks minimum-cost path over fewer hops") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("D")).isOk());

    // Path 1 (1 hop, high cost): A -> D (cost 20.0)
    REQUIRE(g.addLink({LinkId("l_ad"), NodeId("A"), NodeId("D"), InterfaceId("e1"), InterfaceId("e1"), 20.0}, Directionality::Directed).isOk());
    // Path 2 (2 hops, low cost): A -> B -> D (cost 5.0 + 5.0 = 10.0)
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e2"), InterfaceId("e1"), 5.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_bd"), NodeId("B"), NodeId("D"), InterfaceId("e2"), InterfaceId("e2"), 5.0}, Directionality::Directed).isOk());

    auto result = Dijkstra::findPath(g, NodeId("A"), NodeId("D"));
    REQUIRE(result.reachable);
    // Dijkstra must pick Path 2 (A -> B -> D) because cost 10.0 < 20.0, despite having more hops
    REQUIRE_EQ(result.totalCost, 10.0);
    REQUIRE_EQ(result.linkHopCount, 2);
    REQUIRE_EQ(result.pathNodeIds.size(), 3);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathNodeIds[1].str(), "B");
    REQUIRE_EQ(result.pathNodeIds[2].str(), "D");
    REQUIRE_EQ(result.pathLinkIds.size(), 2);
    REQUIRE_EQ(result.pathLinkIds[0].str(), "l_ab");
    REQUIRE_EQ(result.pathLinkIds[1].str(), "l_bd");
}

TEST_CASE("T-1-02: Dijkstra rejects negative edge with NEGATIVE_WEIGHT") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());

    Edge negEdge{LinkId("l_neg"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), -2.0};
    REQUIRE(g.addLink(negEdge, Directionality::Directed).isOk());

    auto result = Dijkstra::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE_FALSE(result.reachable);
    REQUIRE_EQ(result.failureCode, ErrorCode::NegativeWeight);
}

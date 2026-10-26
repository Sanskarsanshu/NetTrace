#include "test_framework.hpp"
#include "nettrace/routing/dijkstra.hpp"

using namespace nettrace;

TEST_CASE("T-1-02: Dijkstra minimum cost path") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());
    REQUIRE(g.addNode(NodeId("D")).isOk());

    // Path 1: A -> B -> D (cost 10 + 10 = 20)
    // Path 2: A -> C -> D (cost 5 + 5 = 10)
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 10.0}).isOk());
    REQUIRE(g.addLink({LinkId("l_bd"), NodeId("B"), NodeId("D"), InterfaceId("e2"), InterfaceId("e1"), 10.0}).isOk());
    REQUIRE(g.addLink({LinkId("l_ac"), NodeId("A"), NodeId("C"), InterfaceId("e3"), InterfaceId("e1"), 5.0}).isOk());
    REQUIRE(g.addLink({LinkId("l_cd"), NodeId("C"), NodeId("D"), InterfaceId("e2"), InterfaceId("e2"), 5.0}).isOk());

    auto result = Dijkstra::findPath(g, NodeId("A"), NodeId("D"));
    REQUIRE(result.reachable);
    REQUIRE_EQ(result.totalCost, 10.0);
    REQUIRE_EQ(result.objectiveValue, 10.0);
    REQUIRE_EQ(result.pathNodeIds.size(), 3);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathNodeIds[1].str(), "C");
    REQUIRE_EQ(result.pathNodeIds[2].str(), "D");
    REQUIRE_EQ(result.pathLinkIds.size(), 2);
    REQUIRE_EQ(result.pathLinkIds[0].str(), "l_ac");
    REQUIRE_EQ(result.pathLinkIds[1].str(), "l_cd");
}

TEST_CASE("T-1-03: Dijkstra tie-breaking (equal cost prefers fewer hops)") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());
    REQUIRE(g.addNode(NodeId("D")).isOk());

    // Path 1 (2 hops): A -> B -> D (cost 5 + 5 = 10)
    // Path 2 (3 hops): A -> C -> B -> D (cost 2 + 3 + 5 = 10)
    // Direct path (1 hop): A -> D with cost 10
    REQUIRE(g.addLink({LinkId("l_ad"), NodeId("A"), NodeId("D"), InterfaceId("e1"), InterfaceId("e1"), 10.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e2"), InterfaceId("e1"), 5.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_bd"), NodeId("B"), NodeId("D"), InterfaceId("e2"), InterfaceId("e2"), 5.0}, Directionality::Directed).isOk());

    auto result = Dijkstra::findPath(g, NodeId("A"), NodeId("D"));
    REQUIRE(result.reachable);
    REQUIRE_EQ(result.totalCost, 10.0);
    // 1-hop path A -> D must be preferred over 2-hop A -> B -> D
    REQUIRE_EQ(result.pathNodeIds.size(), 2);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathNodeIds[1].str(), "D");
    REQUIRE_EQ(result.linkHopCount, 1);
}

TEST_CASE("T-1-04: Dijkstra negative weight rejection") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());

    Edge negEdge{LinkId("l_neg"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), -2.0};
    REQUIRE(g.addLink(negEdge, Directionality::Directed).isOk());

    auto result = Dijkstra::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE_FALSE(result.reachable);
    REQUIRE_EQ(result.failureCode, ErrorCode::NegativeWeight);
}

TEST_CASE("T-1-11: Source equals destination handling in Dijkstra") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());

    auto result = Dijkstra::findPath(g, NodeId("A"), NodeId("A"));
    REQUIRE(result.reachable);
    REQUIRE_EQ(result.pathNodeIds.size(), 1);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathLinkIds.size(), 0);
    REQUIRE_EQ(result.totalCost, 0.0);
    REQUIRE_EQ(result.linkHopCount, 0);
    REQUIRE_EQ(result.objectiveValue, 0.0);
}

TEST_CASE("Dijkstra unreachable destination") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk()); // Disconnected

    auto result = Dijkstra::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE_FALSE(result.reachable);
    REQUIRE_EQ(result.failureCode, ErrorCode::Unreachable);
}

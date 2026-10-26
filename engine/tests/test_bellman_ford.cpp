#include "test_framework.hpp"
#include "nettrace/routing/bellman_ford.hpp"

using namespace nettrace;

TEST_CASE("T-1-04: Bellman-Ford handles negative edges without cycles") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());

    // Directed edges: A -> B (cost 5), B -> C (cost -2), A -> C (cost 4)
    // Shortest path A -> B -> C has cost 5 + (-2) = 3 < 4
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 5.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_bc"), NodeId("B"), NodeId("C"), InterfaceId("e2"), InterfaceId("e1"), -2.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_ac"), NodeId("A"), NodeId("C"), InterfaceId("e3"), InterfaceId("e2"), 4.0}, Directionality::Directed).isOk());

    auto result = BellmanFord::findPath(g, NodeId("A"), NodeId("C"));
    REQUIRE(result.reachable);
    REQUIRE_EQ(result.totalCost, 3.0);
    REQUIRE_EQ(result.pathNodeIds.size(), 3);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathNodeIds[1].str(), "B");
    REQUIRE_EQ(result.pathNodeIds[2].str(), "C");
}

TEST_CASE("T-1-05: Bellman-Ford detects reachable negative cycle") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    REQUIRE(g.addNode(NodeId("C")).isOk());

    // Cycle: B -> C (cost 1), C -> B (cost -2) => total cycle cost -1
    // Connected from A -> B (cost 2)
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 2.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_bc"), NodeId("B"), NodeId("C"), InterfaceId("e2"), InterfaceId("e1"), 1.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_cb"), NodeId("C"), NodeId("B"), InterfaceId("e2"), InterfaceId("e3"), -2.0}, Directionality::Directed).isOk());

    auto result = BellmanFord::findPath(g, NodeId("A"), NodeId("C"));
    REQUIRE_FALSE(result.reachable);
    REQUIRE_EQ(result.failureCode, ErrorCode::NegativeCycle);
    REQUIRE(result.cycleNodeIds.size() >= 2);
}

TEST_CASE("T-1-06: Bellman-Ford ignores unreachable negative cycle") {
    Graph g;
    REQUIRE(g.addNode(NodeId("A")).isOk());
    REQUIRE(g.addNode(NodeId("B")).isOk());
    // Disconnected negative cycle component: X -> Y -> X
    REQUIRE(g.addNode(NodeId("X")).isOk());
    REQUIRE(g.addNode(NodeId("Y")).isOk());

    // Path A -> B
    REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e1"), InterfaceId("e1"), 10.0}, Directionality::Directed).isOk());

    // Negative cycle between X and Y
    REQUIRE(g.addLink({LinkId("l_xy"), NodeId("X"), NodeId("Y"), InterfaceId("e1"), InterfaceId("e1"), 1.0}, Directionality::Directed).isOk());
    REQUIRE(g.addLink({LinkId("l_yx"), NodeId("Y"), NodeId("X"), InterfaceId("e2"), InterfaceId("e2"), -3.0}, Directionality::Directed).isOk());

    // Query from A to B: should succeed despite disconnected cycle between X and Y!
    auto result = BellmanFord::findPath(g, NodeId("A"), NodeId("B"));
    REQUIRE(result.reachable);
    REQUIRE_EQ(result.totalCost, 10.0);
    REQUIRE_EQ(result.pathNodeIds.size(), 2);
    REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
    REQUIRE_EQ(result.pathNodeIds[1].str(), "B");
}

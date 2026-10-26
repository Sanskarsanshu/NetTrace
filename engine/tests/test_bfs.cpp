#include "test_framework.hpp"
#include "nettrace/routing/bfs.hpp"

using namespace nettrace;

TEST_CASE("T-1-03: BFS picks fewest edges even when costlier; reports totalCost") {
    // 1. BFS selects 1-hop path over 2-hop or 3-hop cheaper paths
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        REQUIRE(g.addNode(NodeId("B")).isOk());
        REQUIRE(g.addNode(NodeId("C")).isOk());
        REQUIRE(g.addNode(NodeId("D")).isOk());

        // Path 1 (1 hop, high cost): A -> D with cost 100.0
        REQUIRE(g.addLink({LinkId("l_direct"), NodeId("A"), NodeId("D"), InterfaceId("e1"), InterfaceId("e1"), 100.0}, Directionality::Directed).isOk());
        // Path 2 (3 hops, low cost): A -> B -> C -> D with total cost 1 + 1 + 1 = 3.0
        REQUIRE(g.addLink({LinkId("l_ab"), NodeId("A"), NodeId("B"), InterfaceId("e2"), InterfaceId("e1"), 1.0}, Directionality::Directed).isOk());
        REQUIRE(g.addLink({LinkId("l_bc"), NodeId("B"), NodeId("C"), InterfaceId("e2"), InterfaceId("e1"), 1.0}, Directionality::Directed).isOk());
        REQUIRE(g.addLink({LinkId("l_cd"), NodeId("C"), NodeId("D"), InterfaceId("e2"), InterfaceId("e2"), 1.0}, Directionality::Directed).isOk());

        auto result = BFS::findPath(g, NodeId("A"), NodeId("D"));
        REQUIRE(result.reachable);
        // BFS must select the 1-hop path A -> D
        REQUIRE_EQ(result.linkHopCount, 1);
        REQUIRE_EQ(result.pathNodeIds.size(), 2);
        REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
        REQUIRE_EQ(result.pathNodeIds[1].str(), "D");
        REQUIRE_EQ(result.pathLinkIds.size(), 1);
        REQUIRE_EQ(result.pathLinkIds[0].str(), "l_direct");

        // Reconstructed total cost must be computed and reported accurately
        REQUIRE_EQ(result.totalCost, 100.0);
        REQUIRE_EQ(result.objectiveName, "fewest_hops");
        REQUIRE_EQ(result.objectiveValue, 1.0);
        REQUIRE_EQ(result.objectiveUnit, "hops");
    }

    // 2. Source equals destination handling in BFS
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());

        auto result = BFS::findPath(g, NodeId("A"), NodeId("A"));
        REQUIRE(result.reachable);
        REQUIRE_EQ(result.pathNodeIds.size(), 1);
        REQUIRE_EQ(result.pathNodeIds[0].str(), "A");
        REQUIRE_EQ(result.linkHopCount, 0);
        REQUIRE_EQ(result.objectiveValue, 0.0);
        REQUIRE_EQ(result.totalCost, 0.0);
    }

    // 3. Unreachable destination in BFS
    {
        Graph g;
        REQUIRE(g.addNode(NodeId("A")).isOk());
        REQUIRE(g.addNode(NodeId("B")).isOk());

        auto result = BFS::findPath(g, NodeId("A"), NodeId("B"));
        REQUIRE_FALSE(result.reachable);
        REQUIRE_EQ(result.failureCode, ErrorCode::Unreachable);
    }
}

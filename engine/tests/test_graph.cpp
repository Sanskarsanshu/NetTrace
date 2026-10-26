#include "test_framework.hpp"
#include "nettrace/routing/graph.hpp"

using namespace nettrace;

TEST_CASE("T-1-01: Graph deterministic node and edge iteration order") {
    Graph g;
    // Add nodes in reverse alphabetical order
    REQUIRE(g.addNode(NodeId("node-Z")).isOk());
    REQUIRE(g.addNode(NodeId("node-A")).isOk());
    REQUIRE(g.addNode(NodeId("node-M")).isOk());

    const auto& nodeIds = g.nodeIds();
    REQUIRE_EQ(nodeIds.size(), 3);
    REQUIRE_EQ(nodeIds[0].str(), "node-A");
    REQUIRE_EQ(nodeIds[1].str(), "node-M");
    REQUIRE_EQ(nodeIds[2].str(), "node-Z");

    // Add edges from node-A to multiple targets in unsorted order
    Edge e1{LinkId("link-3"), NodeId("node-A"), NodeId("node-Z"), InterfaceId("e1"), InterfaceId("e1"), 10.0};
    Edge e2{LinkId("link-1"), NodeId("node-A"), NodeId("node-M"), InterfaceId("e2"), InterfaceId("e2"), 5.0};
    Edge e3{LinkId("link-2"), NodeId("node-A"), NodeId("node-M"), InterfaceId("e3"), InterfaceId("e3"), 2.0};

    REQUIRE(g.addLink(e1, Directionality::Directed).isOk());
    REQUIRE(g.addLink(e2, Directionality::Directed).isOk());
    REQUIRE(g.addLink(e3, Directionality::Directed).isOk());

    auto edges = g.getOutgoingEdges(NodeId("node-A"));
    REQUIRE_EQ(edges.size(), 3);
    // Target node-M comes before node-Z; for node-M, cost 2.0 comes before cost 5.0
    REQUIRE_EQ(edges[0].to.str(), "node-M");
    REQUIRE_EQ(edges[0].cost, 2.0);
    REQUIRE_EQ(edges[1].to.str(), "node-M");
    REQUIRE_EQ(edges[1].cost, 5.0);
    REQUIRE_EQ(edges[2].to.str(), "node-Z");
}

TEST_CASE("T-1-12: Parallel edges and self-loops") {
    Graph g;
    REQUIRE(g.addNode(NodeId("node-1")).isOk());
    REQUIRE(g.addNode(NodeId("node-2")).isOk());

    // Self loop must be rejected
    Edge selfLoop{LinkId("link-self"), NodeId("node-1"), NodeId("node-1"), InterfaceId("e1"), InterfaceId("e2"), 1.0};
    auto selfLoopRes = g.addLink(selfLoop);
    REQUIRE_FALSE(selfLoopRes.isOk());
    REQUIRE_EQ(selfLoopRes.error().code, ErrorCode::SelfLoopRejected);

    // Parallel edges between node-1 and node-2 with different link IDs
    Edge p1{LinkId("link-fast"), NodeId("node-1"), NodeId("node-2"), InterfaceId("e1"), InterfaceId("e1"), 5.0, 1000000000};
    Edge p2{LinkId("link-slow"), NodeId("node-1"), NodeId("node-2"), InterfaceId("e2"), InterfaceId("e2"), 2.0, 10000000};
    REQUIRE(g.addLink(p1, Directionality::Directed).isOk());
    REQUIRE(g.addLink(p2, Directionality::Directed).isOk());

    auto edges = g.getOutgoingEdges(NodeId("node-1"));
    REQUIRE_EQ(edges.size(), 2);
    REQUIRE_EQ(edges[0].linkId.str(), "link-slow");
    REQUIRE_EQ(edges[1].linkId.str(), "link-fast");
}

TEST_CASE("T-1-13: Inactive link and node exclusion") {
    Graph g;
    REQUIRE(g.addNode(NodeId("node-1")).isOk());
    REQUIRE(g.addNode(NodeId("node-2"), "Node 2", false).isOk()); // Inactive node
    REQUIRE(g.addNode(NodeId("node-3")).isOk());

    Edge e1{LinkId("link-1-2"), NodeId("node-1"), NodeId("node-2"), InterfaceId("e1"), InterfaceId("e1"), 1.0};
    Edge e2{LinkId("link-1-3"), NodeId("node-1"), NodeId("node-3"), InterfaceId("e2"), InterfaceId("e1"), 1.0};
    e2.active = false; // Inactive edge

    REQUIRE(g.addLink(e1, Directionality::Directed).isOk());
    REQUIRE(g.addLink(e2, Directionality::Directed).isOk());

    // When excludeInactive = true, both links should be excluded
    auto activeEdges = g.getOutgoingEdges(NodeId("node-1"), true);
    REQUIRE_EQ(activeEdges.size(), 0);

    // When excludeInactive = false, both edges are returned
    auto allEdges = g.getOutgoingEdges(NodeId("node-1"), false);
    REQUIRE_EQ(allEdges.size(), 2);
}

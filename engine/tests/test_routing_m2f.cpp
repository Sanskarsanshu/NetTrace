#include "test_framework.hpp"
#include "nettrace/routing/graph_builder.hpp"
#include "nettrace/routing/path_bridge.hpp"
#include "nettrace/routing/dijkstra.hpp"
#include "nettrace/routing/bfs.hpp"
#include "nettrace/routing/widest_path.hpp"
#include "nettrace/forwarding/fib.hpp"
#include "nettrace/model/topology.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace nettrace;

inline RouteResult dijkstra(const Graph& g, const NodeId& src, const NodeId& dst) {
    return Dijkstra::findPath(g, src, dst);
}
inline RouteResult bfs(const Graph& g, const NodeId& src, const NodeId& dst) {
    return BFS::findPath(g, src, dst);
}
inline RouteResult widestPath(const Graph& g, const NodeId& src, const NodeId& dst) {
    return WidestPath::findPath(g, src, dst);
}

// -----------------------------------------------------------------------------
// Helper to construct Fixture A (T-2-14)
// host1.eth0 (192.168.1.10/24) <-> sw1.p1
// sw1.p2 <-> host2.eth0 (192.168.1.20/24)
// Link 1: cost 1.0, delay 1.5, bw 100M, avail 50M, loss 0.1, mtu 1500
// Link 2: cost 2.0, delay 2.0, bw 50M,  avail 30M, loss 0.2, mtu 1400
// Aggregated: cost 3, delay 3.5, bw 50M, avail 30M, loss 0.28, mtu 1400, switches [sw1]
// -----------------------------------------------------------------------------
static TopologyDraft makeFixtureA() {
    TopologyDraft d;

    Node h1;
    h1.id = NodeId("host1");
    h1.name = "Host 1";
    h1.type = NodeType::Host;
    Interface if1;
    if1.id = InterfaceId("eth0");
    if1.name = "eth0";
    if1.mac = MacAddress::parse("00:00:5e:00:53:01").value();
    if1.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
    h1.interfaces.push_back(if1);
    d.nodes.push_back(h1);

    Node sw1;
    sw1.id = NodeId("sw1");
    sw1.name = "Switch 1";
    sw1.type = NodeType::Switch;
    Interface p1;
    p1.id = InterfaceId("p1");
    p1.name = "p1";
    sw1.interfaces.push_back(p1);
    Interface p2;
    p2.id = InterfaceId("p2");
    p2.name = "p2";
    sw1.interfaces.push_back(p2);
    d.nodes.push_back(sw1);

    Node h2;
    h2.id = NodeId("host2");
    h2.name = "Host 2";
    h2.type = NodeType::Host;
    Interface if2;
    if2.id = InterfaceId("eth0");
    if2.name = "eth0";
    if2.mac = MacAddress::parse("00:00:5e:00:53:02").value();
    if2.ipv4 = IPv4Subnet::parse("192.168.1.20/24").value();
    h2.interfaces.push_back(if2);
    d.nodes.push_back(h2);

    Link l1;
    l1.id = LinkId("l1");
    l1.a = {NodeId("host1"), InterfaceId("eth0")};
    l1.b = {NodeId("sw1"), InterfaceId("p1")};
    l1.cost = 1.0;
    l1.propagationDelayMs = 1.5;
    l1.bandwidthBps = 100000000;
    l1.availableBandwidthBps = 50000000;
    l1.packetLossProbability = 0.1;
    l1.mtuBytes = 1500;
    d.links.push_back(l1);

    Link l2;
    l2.id = LinkId("l2");
    l2.a = {NodeId("sw1"), InterfaceId("p2")};
    l2.b = {NodeId("host2"), InterfaceId("eth0")};
    l2.cost = 2.0;
    l2.propagationDelayMs = 2.0;
    l2.bandwidthBps = 50000000;
    l2.availableBandwidthBps = 30000000;
    l2.packetLossProbability = 0.2;
    l2.mtuBytes = 1400;
    d.links.push_back(l2);

    return d;
}

// -----------------------------------------------------------------------------
// Helper to construct Fixture M (T-2-27, T-2-28, T-2-30)
// 5 L3 nodes: host-a, r1, r2, r3, host-b
// Links:
// l_a_r1 : host-a (192.168.1.10/24) <-> r1.eth0 (192.168.1.1/24) : cost 1, delay 1.0, bw 1000M
// l_r1_r2: r1.eth1 (10.0.12.1/30)   <-> r2.eth0 (10.0.12.2/30)   : cost 1, delay 1.0, bw 500M
// l_r2_r3: r2.eth1 (10.0.23.1/30)   <-> r3.eth0 (10.0.23.2/30)   : cost 1, delay 1.0, bw 500M
// l_r1_r3: r1.eth2 (10.0.13.1/30)   <-> r3.eth1 (10.0.13.2/30)   : cost 10, delay 5.0, bw 1000M
// l_r3_b : r3.eth2 (10.0.35.1/24)   <-> host-b (10.0.35.2/24)    : cost 1, delay 1.0, bw 1000M
// -----------------------------------------------------------------------------
static TopologyDraft makeFixtureM() {
    TopologyDraft d;

    // host-a
    Node hostA;
    hostA.id = NodeId("host-a");
    hostA.name = "Host A";
    hostA.type = NodeType::Host;
    hostA.defaultGateway = IPv4Address::parse("192.168.1.1").value();
    Interface ifA;
    ifA.id = InterfaceId("eth0");
    ifA.name = "eth0";
    ifA.mac = MacAddress::parse("00:00:5e:00:53:01").value();
    ifA.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
    hostA.interfaces.push_back(ifA);
    d.nodes.push_back(hostA);

    // r1
    Node r1;
    r1.id = NodeId("r1");
    r1.name = "Router 1";
    r1.type = NodeType::Router;
    r1.forwardingEnabled = true;

    Interface r1_0;
    r1_0.id = InterfaceId("eth0");
    r1_0.name = "eth0";
    r1_0.mac = MacAddress::parse("00:00:5e:00:53:11").value();
    r1_0.ipv4 = IPv4Subnet::parse("192.168.1.1/24").value();
    r1.interfaces.push_back(r1_0);

    Interface r1_1;
    r1_1.id = InterfaceId("eth1");
    r1_1.name = "eth1";
    r1_1.mac = MacAddress::parse("00:00:5e:00:53:12").value();
    r1_1.ipv4 = IPv4Subnet::parse("10.0.12.1/30").value();
    r1.interfaces.push_back(r1_1);

    Interface r1_2;
    r1_2.id = InterfaceId("eth2");
    r1_2.name = "eth2";
    r1_2.mac = MacAddress::parse("00:00:5e:00:53:13").value();
    r1_2.ipv4 = IPv4Subnet::parse("10.0.13.1/30").value();
    r1.interfaces.push_back(r1_2);

    d.nodes.push_back(r1);

    // r2
    Node r2;
    r2.id = NodeId("r2");
    r2.name = "Router 2";
    r2.type = NodeType::Router;
    r2.forwardingEnabled = true;

    Interface r2_0;
    r2_0.id = InterfaceId("eth0");
    r2_0.name = "eth0";
    r2_0.mac = MacAddress::parse("00:00:5e:00:53:21").value();
    r2_0.ipv4 = IPv4Subnet::parse("10.0.12.2/30").value();
    r2.interfaces.push_back(r2_0);

    Interface r2_1;
    r2_1.id = InterfaceId("eth1");
    r2_1.name = "eth1";
    r2_1.mac = MacAddress::parse("00:00:5e:00:53:22").value();
    r2_1.ipv4 = IPv4Subnet::parse("10.0.23.1/30").value();
    r2.interfaces.push_back(r2_1);

    d.nodes.push_back(r2);

    // r3
    Node r3;
    r3.id = NodeId("r3");
    r3.name = "Router 3";
    r3.type = NodeType::Router;
    r3.forwardingEnabled = true;

    Interface r3_0;
    r3_0.id = InterfaceId("eth0");
    r3_0.name = "eth0";
    r3_0.mac = MacAddress::parse("00:00:5e:00:53:31").value();
    r3_0.ipv4 = IPv4Subnet::parse("10.0.23.2/30").value();
    r3.interfaces.push_back(r3_0);

    Interface r3_1;
    r3_1.id = InterfaceId("eth1");
    r3_1.name = "eth1";
    r3_1.mac = MacAddress::parse("00:00:5e:00:53:32").value();
    r3_1.ipv4 = IPv4Subnet::parse("10.0.13.2/30").value();
    r3.interfaces.push_back(r3_1);

    Interface r3_2;
    r3_2.id = InterfaceId("eth2");
    r3_2.name = "eth2";
    r3_2.mac = MacAddress::parse("00:00:5e:00:53:33").value();
    r3_2.ipv4 = IPv4Subnet::parse("10.0.35.1/24").value();
    r3.interfaces.push_back(r3_2);

    d.nodes.push_back(r3);

    // host-b
    Node hostB;
    hostB.id = NodeId("host-b");
    hostB.name = "Host B";
    hostB.type = NodeType::Host;
    hostB.defaultGateway = IPv4Address::parse("10.0.35.1").value();
    Interface ifB;
    ifB.id = InterfaceId("eth0");
    ifB.name = "eth0";
    ifB.mac = MacAddress::parse("00:00:5e:00:53:51").value();
    ifB.ipv4 = IPv4Subnet::parse("10.0.35.2/24").value();
    hostB.interfaces.push_back(ifB);
    d.nodes.push_back(hostB);

    // 5 physical links
    Link l_a_r1;
    l_a_r1.id = LinkId("link-a-r1");
    l_a_r1.a = {NodeId("host-a"), InterfaceId("eth0")};
    l_a_r1.b = {NodeId("r1"), InterfaceId("eth0")};
    l_a_r1.cost = 1.0;
    l_a_r1.propagationDelayMs = 1.0;
    l_a_r1.bandwidthBps = 1000000000;
    l_a_r1.packetLossProbability = 0.0;
    l_a_r1.mtuBytes = 1500;
    d.links.push_back(l_a_r1);

    Link l_r1_r2;
    l_r1_r2.id = LinkId("link-r1-r2");
    l_r1_r2.a = {NodeId("r1"), InterfaceId("eth1")};
    l_r1_r2.b = {NodeId("r2"), InterfaceId("eth0")};
    l_r1_r2.cost = 1.0;
    l_r1_r2.propagationDelayMs = 1.0;
    l_r1_r2.bandwidthBps = 500000000;
    l_r1_r2.packetLossProbability = 0.0;
    l_r1_r2.mtuBytes = 1500;
    d.links.push_back(l_r1_r2);

    Link l_r2_r3;
    l_r2_r3.id = LinkId("link-r2-r3");
    l_r2_r3.a = {NodeId("r2"), InterfaceId("eth1")};
    l_r2_r3.b = {NodeId("r3"), InterfaceId("eth0")};
    l_r2_r3.cost = 1.0;
    l_r2_r3.propagationDelayMs = 1.0;
    l_r2_r3.bandwidthBps = 500000000;
    l_r2_r3.packetLossProbability = 0.0;
    l_r2_r3.mtuBytes = 1500;
    d.links.push_back(l_r2_r3);

    Link l_r1_r3;
    l_r1_r3.id = LinkId("link-r1-r3");
    l_r1_r3.a = {NodeId("r1"), InterfaceId("eth2")};
    l_r1_r3.b = {NodeId("r3"), InterfaceId("eth1")};
    l_r1_r3.cost = 10.0;
    l_r1_r3.propagationDelayMs = 5.0;
    l_r1_r3.bandwidthBps = 1000000000;
    l_r1_r3.packetLossProbability = 0.0;
    l_r1_r3.mtuBytes = 1500;
    d.links.push_back(l_r1_r3);

    Link l_r3_b;
    l_r3_b.id = LinkId("link-r3-b");
    l_r3_b.a = {NodeId("r3"), InterfaceId("eth2")};
    l_r3_b.b = {NodeId("host-b"), InterfaceId("eth0")};
    l_r3_b.cost = 1.0;
    l_r3_b.propagationDelayMs = 1.0;
    l_r3_b.bandwidthBps = 1000000000;
    l_r3_b.packetLossProbability = 0.0;
    l_r3_b.mtuBytes = 1500;
    d.links.push_back(l_r3_b);

    return d;
}

// -----------------------------------------------------------------------------
// T-2-14: Fixture A aggregation
// -----------------------------------------------------------------------------
TEST_CASE("T-2-14 Fixture A aggregation and variations") {
    // 1. Baseline Fixture A
    auto draft = makeFixtureA();
    auto snap = TopologySnapshot::freeze(draft).value();
    auto l3 = GraphBuilder::build(snap);

    // 2 L3 nodes (host1, host2) and 2 directed arcs (host1->host2, host2->host1)
    REQUIRE_EQ(l3.nodeCount(), 2);
    REQUIRE_EQ(l3.l3Arcs().size(), 2);

    const auto* arcForward = l3.findL3Arc(NodeId("host1"), NodeId("host2"));
    REQUIRE(arcForward != nullptr);
    REQUIRE_EQ(arcForward->cost, 3.0);
    REQUIRE_EQ(arcForward->bandwidthBps, 50000000);
    REQUIRE(arcForward->availableBandwidthBps.has_value());
    REQUIRE_EQ(*arcForward->availableBandwidthBps, 30000000);
    REQUIRE_EQ(arcForward->propagationDelayMs, 3.5);
    // loss: 1 - (1 - 0.1)*(1 - 0.2) = 1 - 0.72 = 0.28
    REQUIRE(std::abs(arcForward->packetLossProbability - 0.28) < 1e-6);
    REQUIRE_EQ(arcForward->mtuBytes, 1400);

    // Underlying link order per direction
    REQUIRE_EQ(arcForward->underlyingLinkIds.size(), 2);
    REQUIRE_EQ(arcForward->underlyingLinkIds[0].str(), "l1");
    REQUIRE_EQ(arcForward->underlyingLinkIds[1].str(), "l2");
    REQUIRE_EQ(arcForward->switchesTraversed.size(), 1);
    REQUIRE_EQ(arcForward->switchesTraversed[0].str(), "sw1");

    const auto* arcReverse = l3.findL3Arc(NodeId("host2"), NodeId("host1"));
    REQUIRE(arcReverse != nullptr);
    REQUIRE_EQ(arcReverse->underlyingLinkIds.size(), 2);
    REQUIRE_EQ(arcReverse->underlyingLinkIds[0].str(), "l2");
    REQUIRE_EQ(arcReverse->underlyingLinkIds[1].str(), "l1");
    REQUIRE_EQ(arcReverse->switchesTraversed.size(), 1);
    REQUIRE_EQ(arcReverse->switchesTraversed[0].str(), "sw1");

    // Variation 1: Missing availability on one link -> aggregated availableBandwidth is nullopt
    auto draftVar1 = makeFixtureA();
    draftVar1.links[0].availableBandwidthBps = std::nullopt;
    auto snapVar1 = TopologySnapshot::freeze(draftVar1).value();
    auto l3Var1 = GraphBuilder::build(snapVar1);
    const auto* arcVar1 = l3Var1.findL3Arc(NodeId("host1"), NodeId("host2"));
    REQUIRE(arcVar1 != nullptr);
    REQUIRE(!arcVar1->availableBandwidthBps.has_value());

    // Variation 2: MTU override 1280 on an interface -> aggregated MTU is 1280
    auto draftVar2 = makeFixtureA();
    draftVar2.nodes[0].interfaces[0].mtuBytes = 1280;
    auto snapVar2 = TopologySnapshot::freeze(draftVar2).value();
    auto l3Var2 = GraphBuilder::build(snapVar2);
    const auto* arcVar2 = l3Var2.findL3Arc(NodeId("host1"), NodeId("host2"));
    REQUIRE(arcVar2 != nullptr);
    REQUIRE_EQ(arcVar2->mtuBytes, 1280);

    // Variation 3: Three hosts in one domain -> 6 arcs (ordered pairs)
    auto draftVar3 = makeFixtureA();
    Node h3;
    h3.id = NodeId("host3");
    h3.name = "Host 3";
    h3.type = NodeType::Host;
    Interface if3;
    if3.id = InterfaceId("eth0");
    if3.name = "eth0";
    if3.mac = MacAddress::parse("00:00:5e:00:53:03").value();
    if3.ipv4 = IPv4Subnet::parse("192.168.1.30/24").value();
    h3.interfaces.push_back(if3);
    draftVar3.nodes.push_back(h3);

    Interface p3;
    p3.id = InterfaceId("p3");
    p3.name = "p3";
    draftVar3.nodes[1].interfaces.push_back(p3); // sw1.p3

    Link l3Link;
    l3Link.id = LinkId("l3Link");
    l3Link.a = {NodeId("sw1"), InterfaceId("p3")};
    l3Link.b = {NodeId("host3"), InterfaceId("eth0")};
    draftVar3.links.push_back(l3Link);

    auto snapVar3 = TopologySnapshot::freeze(draftVar3).value();
    auto l3Var3 = GraphBuilder::build(snapVar3);
    REQUIRE_EQ(l3Var3.nodeCount(), 3);
    // 3 hosts in one domain: 3 * 2 = 6 directed arcs
    REQUIRE_EQ(l3Var3.l3Arcs().size(), 6);
}

// -----------------------------------------------------------------------------
// T-2-27: Fixture M (Path metrics, hop counts, 5 L3 nodes, 10 arcs)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-27 Fixture M algorithms and dual hop counts") {
    auto draft = makeFixtureM();
    auto snap = TopologySnapshot::freeze(draft).value();
    auto l3 = GraphBuilder::build(snap);

    // Exactly 5 L3 nodes and 10 arcs
    REQUIRE_EQ(l3.nodeCount(), 5);
    REQUIRE_EQ(l3.l3Arcs().size(), 10);

    // 1. Dijkstra from host-a to host-b
    auto dijRes = dijkstra(l3, NodeId("host-a"), NodeId("host-b"));
    REQUIRE(dijRes.reachable);
    // Cheaper cost: host-a -> r1 -> r2 -> r3 -> host-b (cost 1+1+1+1 = 4.0)
    REQUIRE_EQ(dijRes.totalCost, 4.0);
    REQUIRE_EQ(dijRes.pathNodeIds.size(), 5);
    REQUIRE_EQ(dijRes.pathNodeIds[0].str(), "host-a");
    REQUIRE_EQ(dijRes.pathNodeIds[1].str(), "r1");
    REQUIRE_EQ(dijRes.pathNodeIds[2].str(), "r2");
    REQUIRE_EQ(dijRes.pathNodeIds[3].str(), "r3");
    REQUIRE_EQ(dijRes.pathNodeIds[4].str(), "host-b");

    auto expDij = l3.expandPath(dijRes, snap);
    REQUIRE_EQ(expDij.routerHopCount, 3); // r1, r2, r3 strictly between host-a and host-b
    REQUIRE_EQ(expDij.linkHopCount, 4);
    REQUIRE(expDij.switchesTraversed.empty());

    // 2. BFS from host-a to host-b
    auto bfsRes = bfs(l3, NodeId("host-a"), NodeId("host-b"));
    REQUIRE(bfsRes.reachable);
    // Fewest L3 edges: host-a -> r1 -> r3 -> host-b (3 edges, cost 1+10+1 = 12.0)
    REQUIRE_EQ(bfsRes.totalCost, 12.0);
    REQUIRE_EQ(bfsRes.pathNodeIds.size(), 4);
    REQUIRE_EQ(bfsRes.pathNodeIds[0].str(), "host-a");
    REQUIRE_EQ(bfsRes.pathNodeIds[1].str(), "r1");
    REQUIRE_EQ(bfsRes.pathNodeIds[2].str(), "r3");
    REQUIRE_EQ(bfsRes.pathNodeIds[3].str(), "host-b");

    auto expBfs = l3.expandPath(bfsRes, snap);
    REQUIRE_EQ(expBfs.routerHopCount, 2); // r1, r3
    REQUIRE_EQ(expBfs.linkHopCount, 3);
    REQUIRE(expBfs.switchesTraversed.empty());

    // 3. Widest Path from host-a to host-b
    auto wideRes = widestPath(l3, NodeId("host-a"), NodeId("host-b"));
    REQUIRE(wideRes.reachable);
    // Path through r1-r3 has bottleneck min(1G, 1G, 1G) = 1G
    // Path through r2 has bottleneck min(1G, 500M, 500M, 1G) = 500M
    // Widest path maximizes bottleneck -> picks host-a -> r1 -> r3 -> host-b
    REQUIRE_EQ(wideRes.bottleneckBandwidthBps, 1000000000);
    REQUIRE_EQ(wideRes.pathNodeIds.size(), 4);
    REQUIRE_EQ(wideRes.pathNodeIds[1].str(), "r1");
    REQUIRE_EQ(wideRes.pathNodeIds[2].str(), "r3");

    auto expWide = l3.expandPath(wideRes, snap);
    REQUIRE_EQ(expWide.routerHopCount, 2);
    REQUIRE_EQ(expWide.linkHopCount, 3);
}

// -----------------------------------------------------------------------------
// T-2-28: Directed link, inactive components, and exclusions
// -----------------------------------------------------------------------------
TEST_CASE("T-2-28 directed links and exclusions") {
    // 1. Directed link: link-r1-r2 directed r1 -> r2
    auto draftDir = makeFixtureM();
    draftDir.links[1].directionality = Directionality::Directed; // link-r1-r2
    auto snapDir = TopologySnapshot::freeze(draftDir).value();
    auto l3Dir = GraphBuilder::build(snapDir);

    // Forward arc exists: r1 -> r2
    REQUIRE(l3Dir.findL3Arc(NodeId("r1"), NodeId("r2")) != nullptr);
    // Reverse arc r2 -> r1 must NOT exist
    REQUIRE(l3Dir.findL3Arc(NodeId("r2"), NodeId("r1")) == nullptr);

    // 2. Inactive components in switched fixture
    auto draftInact = makeFixtureA();
    draftInact.nodes[1].adminState = AdminState::Down;      // sw1 Down
    draftInact.links[1].adminState = AdminState::Down;      // l2 Down
    draftInact.nodes[0].interfaces[0].adminState = AdminState::Down; // host1.eth0 Down

    auto snapInact = TopologySnapshot::freeze(draftInact).value();
    auto l3Inact = GraphBuilder::build(snapInact);

    const auto& ex = l3Inact.exclusions();
    REQUIRE(std::find(ex.begin(), ex.end(), "node:sw1") != ex.end());
    REQUIRE(std::find(ex.begin(), ex.end(), "link:l2") != ex.end());
    REQUIRE(std::find(ex.begin(), ex.end(), "interface:host1:eth0") != ex.end());

    // No composite arcs generated when sw1 is Down
    REQUIRE(l3Inact.l3Arcs().empty());
}

// -----------------------------------------------------------------------------
// T-2-29: Determinism (build twice and shuffled insertion order)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-29 GraphBuilder determinism") {
    auto draft1 = makeFixtureM();
    auto snap1 = TopologySnapshot::freeze(draft1).value();
    auto l3_1 = GraphBuilder::build(snap1);

    // Build twice from same snapshot
    auto l3_2 = GraphBuilder::build(snap1);
    REQUIRE_EQ(l3_1.l3Arcs().size(), l3_2.l3Arcs().size());
    for (size_t i = 0; i < l3_1.l3Arcs().size(); ++i) {
        REQUIRE_EQ(l3_1.l3Arcs()[i].syntheticEdgeId.str(), l3_2.l3Arcs()[i].syntheticEdgeId.str());
        REQUIRE_EQ(l3_1.l3Arcs()[i].cost, l3_2.l3Arcs()[i].cost);
    }

    // Shuffled insertion order in draft
    auto draftShuffled = makeFixtureM();
    std::reverse(draftShuffled.nodes.begin(), draftShuffled.nodes.end());
    std::reverse(draftShuffled.links.begin(), draftShuffled.links.end());
    auto snapShuffled = TopologySnapshot::freeze(draftShuffled).value();
    REQUIRE_EQ(snap1.contentHash(), snapShuffled.contentHash());

    auto l3_shuffled = GraphBuilder::build(snapShuffled);
    REQUIRE_EQ(l3_1.l3Arcs().size(), l3_shuffled.l3Arcs().size());
    for (size_t i = 0; i < l3_1.l3Arcs().size(); ++i) {
        REQUIRE_EQ(l3_1.l3Arcs()[i].syntheticEdgeId.str(), l3_shuffled.l3Arcs()[i].syntheticEdgeId.str());
    }
}

// -----------------------------------------------------------------------------
// T-2-30: Policy routes bridge (D-04)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-30 policy routes and user route override") {
    auto draft = makeFixtureM();

    // User configures a static route on r1: 10.0.35.0/24 via 10.0.13.2 (r3, the BFS path)
    draft.nodes[1].staticRoutes.push_back({
        IPv4Subnet::parse("10.0.35.0/24").value(),
        IPv4Address::parse("10.0.13.2").value(), // Next hop is r3 on eth2
        InterfaceId("eth2"),
        1,
        0
    });

    auto snap = TopologySnapshot::freeze(draft).value();
    const std::string initialHash = snap.contentHash();
    auto l3 = GraphBuilder::build(snap);

    IPv4Address destIp = IPv4Address::parse("10.0.35.2").value(); // Owned by host-b

    // 1. Dijkstra Path: host-a -> r1 -> r2 -> r3 -> host-b (5 nodes -> 4 policy routes)
    auto dijRes = dijkstra(l3, NodeId("host-a"), NodeId("host-b"));
    auto polDij = planPolicyRoutes(snap, dijRes, destIp);
    REQUIRE(polDij.success);
    REQUIRE_EQ(polDij.totalPolicyRoutesCount(), 4);
    REQUIRE_EQ(polDij.plannedPath.size(), 5);

    // On r1, policy route chooses r2 (10.0.12.2 on eth1)
    auto r1PolRoutes = polDij.routesForNode(NodeId("r1"));
    REQUIRE_EQ(r1PolRoutes.size(), 1);
    REQUIRE_EQ(r1PolRoutes[0].outInterface.str(), "eth1");
    REQUIRE(r1PolRoutes[0].nextHop.has_value());
    REQUIRE_EQ(r1PolRoutes[0].nextHop->toString(), "10.0.12.2");

    // Overridden user route on r1: the static route via r3 (10.0.13.2 on eth2) is overridden!
    auto r1OverriddenDij = polDij.overriddenForNode(NodeId("r1"));
    REQUIRE_EQ(r1OverriddenDij.size(), 1);
    REQUIRE_EQ(r1OverriddenDij[0].prefix.toString(), "10.0.35.0/24");
    REQUIRE_EQ(r1OverriddenDij[0].outInterface.str(), "eth2");

    // Before policy route, r1 lookup for 10.0.35.2 resolves via static route (eth2, 10.0.13.2)
    auto tableBefore = ForwardingTable::build(snap, NodeId("r1"));
    auto lkpBefore = tableBefore.lookup(destIp, NodeId("r1"), snap);
    REQUIRE(lkpBefore.has_value());
    REQUIRE_EQ(lkpBefore->outInterface.str(), "eth2");
    REQUIRE_EQ(lkpBefore->nextHop->toString(), "10.0.13.2");

    // After policy route is installed, r1 lookup for 10.0.35.2 changes to policy route (eth1, 10.0.12.2)!
    auto tableAfter = ForwardingTable::build(snap, NodeId("r1"), r1PolRoutes);
    auto lkpAfter = tableAfter.lookup(destIp, NodeId("r1"), snap);
    REQUIRE(lkpAfter.has_value());
    REQUIRE_EQ(lkpAfter->source, RouteSource::Policy);
    REQUIRE_EQ(lkpAfter->prefix.toString(), "10.0.35.2/32");
    REQUIRE_EQ(lkpAfter->outInterface.str(), "eth1");
    REQUIRE_EQ(lkpAfter->nextHop->toString(), "10.0.12.2");

    // 2. BFS Path: host-a -> r1 -> r3 -> host-b (4 nodes -> 3 policy routes)
    auto bfsRes = bfs(l3, NodeId("host-a"), NodeId("host-b"));
    auto polBfs = planPolicyRoutes(snap, bfsRes, destIp);
    REQUIRE(polBfs.success);
    REQUIRE_EQ(polBfs.totalPolicyRoutesCount(), 3);
    REQUIRE_EQ(polBfs.plannedPath.size(), 4);

    // On r1, BFS policy route chooses r3 (10.0.13.2 on eth2)
    // Since this matches the existing user static route, overriddenUserRoutes for r1 is EMPTY!
    auto r1OverriddenBfs = polBfs.overriddenForNode(NodeId("r1"));
    REQUIRE(r1OverriddenBfs.empty());

    // 3. Snapshot hash is invariant
    REQUIRE_EQ(snap.contentHash(), initialHash);

    // 4. Error cases
    // Unreachable path -> NO_ROUTE
    RouteResult unreachRes = RouteResult::unreachable("dijkstra", "cost", ErrorCode::NoRoute, "No path");
    auto polUnreach = planPolicyRoutes(snap, unreachRes, destIp);
    REQUIRE(!polUnreach.success);
    REQUIRE_EQ(polUnreach.errorCode, ErrorCode::NoRoute);
    REQUIRE_EQ(polUnreach.totalPolicyRoutesCount(), 0);

    // destIp not owned by destination node -> INVALID_CONFIGURATION
    IPv4Address wrongIp = IPv4Address::parse("172.16.99.1").value();
    auto polWrong = planPolicyRoutes(snap, dijRes, wrongIp);
    REQUIRE(!polWrong.success);
    REQUIRE_EQ(polWrong.errorCode, ErrorCode::InvalidConfiguration);
}

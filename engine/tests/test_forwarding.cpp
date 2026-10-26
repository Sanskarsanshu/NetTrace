#include "test_framework.hpp"
#include "nettrace/forwarding/route.hpp"
#include "nettrace/forwarding/fib.hpp"
#include "nettrace/forwarding/decision.hpp"
#include "nettrace/forwarding/forwarding_engine.hpp"
#include "nettrace/forwarding/next_hop.hpp"
#include "nettrace/forwarding/explain.hpp"
#include "nettrace/forwarding/ephemeral_port.hpp"
#include "nettrace/model/topology.hpp"
#include "nettrace/model/validate.hpp"
#include "nettrace/model/arp_cache.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace nettrace;

// Helper to construct baseline Fixture S (Scenario B)
// host-a eth0 192.168.1.10/24, defaultGateway 192.168.1.1, MAC 00:00:5e:00:53:01
// r1     eth0 192.168.1.1/24,  MAC 00:00:5e:00:53:11
//        eth1 10.0.0.1/24,     MAC 00:00:5e:00:53:12
// host-b eth0 10.0.0.20/24,    defaultGateway 10.0.0.1,    MAC 00:00:5e:00:53:02
// link l1: host-a.eth0 <-> r1.eth0
// link l2: host-b.eth0 <-> r1.eth1
static TopologyDraft makeFixtureS() {
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
    Interface rIf0;
    rIf0.id = InterfaceId("eth0");
    rIf0.name = "eth0";
    rIf0.mac = MacAddress::parse("00:00:5e:00:53:11").value();
    rIf0.ipv4 = IPv4Subnet::parse("192.168.1.1/24").value();
    r1.interfaces.push_back(rIf0);
    Interface rIf1;
    rIf1.id = InterfaceId("eth1");
    rIf1.name = "eth1";
    rIf1.mac = MacAddress::parse("00:00:5e:00:53:12").value();
    rIf1.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
    r1.interfaces.push_back(rIf1);
    d.nodes.push_back(r1);

    // host-b
    Node hostB;
    hostB.id = NodeId("host-b");
    hostB.name = "Host B";
    hostB.type = NodeType::Host;
    hostB.defaultGateway = IPv4Address::parse("10.0.0.1").value();
    Interface ifB;
    ifB.id = InterfaceId("eth0");
    ifB.name = "eth0";
    ifB.mac = MacAddress::parse("00:00:5e:00:53:02").value();
    ifB.ipv4 = IPv4Subnet::parse("10.0.0.20/24").value();
    hostB.interfaces.push_back(ifB);
    d.nodes.push_back(hostB);

    // Links
    Link l1;
    l1.id = LinkId("l1");
    l1.a = {NodeId("host-a"), InterfaceId("eth0")};
    l1.b = {NodeId("r1"), InterfaceId("eth0")};
    d.links.push_back(l1);

    Link l2;
    l2.id = LinkId("l2");
    l2.a = {NodeId("host-b"), InterfaceId("eth0")};
    l2.b = {NodeId("r1"), InterfaceId("eth1")};
    d.links.push_back(l2);

    return d;
}

// Custom StateView for dynamic fault tests
class MockStateView : public StateView {
public:
    std::vector<std::string> downNodes;
    std::vector<std::pair<std::string, std::string>> downInterfaces; // (nodeId, ifaceId)
    std::vector<std::string> downLinks;

    bool isNodeUp(const NodeId& id) const override {
        return std::find(downNodes.begin(), downNodes.end(), id.str()) == downNodes.end();
    }

    bool isInterfaceUp(const NodeId& nodeId, const InterfaceId& ifaceId) const override {
        for (const auto& p : downInterfaces) {
            if (p.first == nodeId.str() && p.second == ifaceId.str()) return false;
        }
        return true;
    }

    bool isLinkUp(const LinkId& linkId) const override {
        return std::find(downLinks.begin(), downLinks.end(), linkId.str()) == downLinks.end();
    }
};

// -----------------------------------------------------------------------------
// T-2-04: Same-subnet does not use a gateway
// -----------------------------------------------------------------------------
TEST_CASE("T-2-04 same-subnet does not use gateway") {
    auto draft = makeFixtureS();
    auto snap = TopologySnapshot::freeze(draft).value();

    // host-a FIB has exactly 2 routes: Connected 192.168.1.0/24 and Default 0.0.0.0/0
    auto tableA = ForwardingTable::build(snap, NodeId("host-a"));
    REQUIRE_EQ(tableA.routes().size(), 2);
    REQUIRE(tableA.routes()[0].source == RouteSource::Connected);
    REQUIRE_EQ(tableA.routes()[0].prefix.toString(), "192.168.1.0/24");
    REQUIRE_EQ(tableA.routes()[0].adminPreference, 0);
    REQUIRE(!tableA.routes()[0].nextHop.has_value());
    REQUIRE(tableA.routes()[1].source == RouteSource::Default);
    REQUIRE_EQ(tableA.routes()[1].prefix.toString(), "0.0.0.0/0");
    REQUIRE_EQ(tableA.routes()[1].adminPreference, 1);
    REQUIRE_EQ(tableA.routes()[1].nextHop->toString(), "192.168.1.1");

    // host-a -> 192.168.1.1 : Direct, nextHop 192.168.1.1, matched 192.168.1.0/24
    auto d1 = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("192.168.1.1").value());
    REQUIRE(d1.hostOutcome == HostSendOutcome::Direct);
    REQUIRE(d1.nextHopIp.has_value());
    REQUIRE_EQ(d1.nextHopIp->toString(), "192.168.1.1");
    REQUIRE_EQ(d1.outInterface->str(), "eth0");
    REQUIRE(d1.matchedRoute.has_value());
    REQUIRE_EQ(d1.matchedRoute->prefix.toString(), "192.168.1.0/24");

    // host-a -> 192.168.1.77 : Direct, nextHop 192.168.1.77
    auto d2 = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("192.168.1.77").value());
    REQUIRE(d2.hostOutcome == HostSendOutcome::Direct);
    REQUIRE(d2.nextHopIp.has_value());
    REQUIRE_EQ(d2.nextHopIp->toString(), "192.168.1.77");
    REQUIRE_EQ(d2.outInterface->str(), "eth0");
    REQUIRE(d2.matchedRoute.has_value());
    REQUIRE_EQ(d2.matchedRoute->prefix.toString(), "192.168.1.0/24");
}

// -----------------------------------------------------------------------------
// T-2-05: Off-subnet uses gateway and keeps destination IP
// -----------------------------------------------------------------------------
TEST_CASE("T-2-05 off-subnet uses gateway and keeps destination IP") {
    auto draft = makeFixtureS();
    auto snap = TopologySnapshot::freeze(draft).value();

    // host-a -> 10.0.0.20 : ViaGateway, nextHop 192.168.1.1, matched 0.0.0.0/0, egress eth0, destination stays 10.0.0.20
    auto d = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("10.0.0.20").value());
    REQUIRE(d.hostOutcome == HostSendOutcome::ViaGateway);
    REQUIRE(d.nextHopIp.has_value());
    REQUIRE_EQ(d.nextHopIp->toString(), "192.168.1.1");
    REQUIRE(d.outInterface.has_value());
    REQUIRE_EQ(d.outInterface->str(), "eth0");
    REQUIRE_EQ(d.destinationIp.toString(), "10.0.0.20");
    REQUIRE(d.matchedRoute.has_value());
    REQUIRE_EQ(d.matchedRoute->prefix.toString(), "0.0.0.0/0");
}

// -----------------------------------------------------------------------------
// T-2-06: Host with no gateway gives NO_ROUTE
// -----------------------------------------------------------------------------
TEST_CASE("T-2-06 no gateway gives NO_ROUTE") {
    auto draft = makeFixtureS();
    // Remove host-b's gateway
    draft.nodes[2].defaultGateway = std::nullopt;
    auto snap = TopologySnapshot::freeze(draft).value();

    // host-b has only Connected 10.0.0.0/24
    auto tableB = ForwardingTable::build(snap, NodeId("host-b"));
    REQUIRE_EQ(tableB.routes().size(), 1);

    // host-b -> 192.168.1.10 : NoRoute (reason NO_ROUTE)
    auto d1 = decideHostSend(snap, NodeId("host-b"), IPv4Address::parse("192.168.1.10").value());
    REQUIRE(d1.hostOutcome == HostSendOutcome::NoRoute);
    REQUIRE(d1.dropReason.has_value());
    REQUIRE(*d1.dropReason == ErrorCode::NoRoute);

    // host-b -> 10.0.0.1 : Direct, nextHop 10.0.0.1
    auto d2 = decideHostSend(snap, NodeId("host-b"), IPv4Address::parse("10.0.0.1").value());
    REQUIRE(d2.hostOutcome == HostSendOutcome::Direct);
    REQUIRE(d2.nextHopIp.has_value());
    REQUIRE_EQ(d2.nextHopIp->toString(), "10.0.0.1");
    REQUIRE_EQ(d2.outInterface->str(), "eth0");
}

// -----------------------------------------------------------------------------
// T-2-07: Fixture L (LPM Table)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-07 LPM table lookup") {
    // Router r with eth0 10.0.0.1/24 and eth1 10.1.0.1/24 and static routes:
    // 0.0.0.0/0   via 10.0.0.254 metric 0
    // 10.0.0.0/8  via 10.0.0.2   metric 5
    // 10.4.0.0/16 via 10.0.0.3   metric 5
    // 10.4.7.0/24 via 10.0.0.4   metric 5
    TopologyDraft draft;
    Node r;
    r.id = NodeId("r");
    r.name = "Router R";
    r.type = NodeType::Router;
    r.forwardingEnabled = true;

    Interface eth0;
    eth0.id = InterfaceId("eth0");
    eth0.name = "eth0";
    eth0.mac = MacAddress::parse("00:00:5e:00:53:21").value();
    eth0.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
    r.interfaces.push_back(eth0);

    Interface eth1;
    eth1.id = InterfaceId("eth1");
    eth1.name = "eth1";
    eth1.mac = MacAddress::parse("00:00:5e:00:53:22").value();
    eth1.ipv4 = IPv4Subnet::parse("10.1.0.1/24").value();
    r.interfaces.push_back(eth1);

    r.staticRoutes.push_back({IPv4Subnet::parse("0.0.0.0/0").value(), IPv4Address::parse("10.0.0.254").value(), std::nullopt, 1, 0});
    r.staticRoutes.push_back({IPv4Subnet::parse("10.0.0.0/8").value(), IPv4Address::parse("10.0.0.2").value(), std::nullopt, 1, 5});
    r.staticRoutes.push_back({IPv4Subnet::parse("10.4.0.0/16").value(), IPv4Address::parse("10.0.0.3").value(), std::nullopt, 1, 5});
    r.staticRoutes.push_back({IPv4Subnet::parse("10.4.7.0/24").value(), IPv4Address::parse("10.0.0.4").value(), std::nullopt, 1, 5});

    draft.nodes.push_back(r);
    auto snap = TopologySnapshot::freeze(draft).value();

    auto table = ForwardingTable::build(snap, NodeId("r"));

    // 10.4.7.9 -> 10.4.7.0/24 via 10.0.0.4
    auto r1 = table.lookup(IPv4Address::parse("10.4.7.9").value(), NodeId("r"), snap);
    REQUIRE(r1.has_value());
    REQUIRE_EQ(r1->prefix.toString(), "10.4.7.0/24");
    REQUIRE_EQ(r1->nextHop->toString(), "10.0.0.4");

    // 10.4.8.9 -> 10.4.0.0/16 via 10.0.0.3
    auto r2 = table.lookup(IPv4Address::parse("10.4.8.9").value(), NodeId("r"), snap);
    REQUIRE(r2.has_value());
    REQUIRE_EQ(r2->prefix.toString(), "10.4.0.0/16");
    REQUIRE_EQ(r2->nextHop->toString(), "10.0.0.3");

    // 10.200.1.1 -> 10.0.0.0/8 via 10.0.0.2 (default metric 0 must NOT beat /8)
    auto r3 = table.lookup(IPv4Address::parse("10.200.1.1").value(), NodeId("r"), snap);
    REQUIRE(r3.has_value());
    REQUIRE_EQ(r3->prefix.toString(), "10.0.0.0/8");
    REQUIRE_EQ(r3->nextHop->toString(), "10.0.0.2");

    // 11.1.1.1 -> 0.0.0.0/0 via 10.0.0.254
    auto r4 = table.lookup(IPv4Address::parse("11.1.1.1").value(), NodeId("r"), snap);
    REQUIRE(r4.has_value());
    REQUIRE_EQ(r4->prefix.toString(), "0.0.0.0/0");
    REQUIRE_EQ(r4->nextHop->toString(), "10.0.0.254");

    // 10.0.0.55 -> Connected 10.0.0.0/24 (a /24 beats /8 and default)
    auto r5 = table.lookup(IPv4Address::parse("10.0.0.55").value(), NodeId("r"), snap);
    REQUIRE(r5.has_value());
    REQUIRE_EQ(r5->prefix.toString(), "10.0.0.0/24");
    REQUIRE(r5->source == RouteSource::Connected);
}

// -----------------------------------------------------------------------------
// T-2-08: Deterministic tie-breaking (including the 9 vs 10 numeric trap)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-08 deterministic tie-breaking and numeric trap") {
    TopologyDraft draft;
    Node r;
    r.id = NodeId("r");
    r.name = "Router R";
    r.type = NodeType::Router;
    r.forwardingEnabled = true;

    Interface eth0;
    eth0.id = InterfaceId("eth0");
    eth0.name = "eth0";
    eth0.mac = MacAddress::parse("00:00:5e:00:53:21").value();
    eth0.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
    r.interfaces.push_back(eth0);

    // 1. 10.9.0.0/16 via 10.0.0.2 metric 10 vs 10.9.0.0/16 via 10.0.0.3 metric 5 -> metric 5 wins
    r.staticRoutes.push_back({IPv4Subnet::parse("10.9.0.0/16").value(), IPv4Address::parse("10.0.0.2").value(), std::nullopt, 1, 10});
    r.staticRoutes.push_back({IPv4Subnet::parse("10.9.0.0/16").value(), IPv4Address::parse("10.0.0.3").value(), std::nullopt, 1, 5});

    draft.nodes.push_back(r);
    auto snap1 = TopologySnapshot::freeze(draft).value();
    auto table1 = ForwardingTable::build(snap1, NodeId("r"));
    auto res1 = table1.lookup(IPv4Address::parse("10.9.1.1").value(), NodeId("r"), snap1);
    REQUIRE(res1.has_value());
    REQUIRE_EQ(res1->nextHop->toString(), "10.0.0.3");
    REQUIRE_EQ(res1->metric, 5);

    // 2. Add 10.9.0.0/16 via 10.0.0.5 adminPreference 0 metric 100 -> wins over both (preference beats metric)
    draft.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.9.0.0/16").value(), IPv4Address::parse("10.0.0.5").value(), std::nullopt, 0, 100});
    auto snap2 = TopologySnapshot::freeze(draft).value();
    auto table2 = ForwardingTable::build(snap2, NodeId("r"));
    auto res2 = table2.lookup(IPv4Address::parse("10.9.1.1").value(), NodeId("r"), snap2);
    REQUIRE(res2.has_value());
    REQUIRE_EQ(res2->nextHop->toString(), "10.0.0.5");
    REQUIRE_EQ(res2->adminPreference, 0);

    // 3. Numeric trap: 10.8.0.0/16 via 10.0.0.9 vs 10.8.0.0/16 via 10.0.0.10
    // String comparison: "10.0.0.10" < "10.0.0.9" (incorrect string order)
    // Numeric 32-bit uint: 10.0.0.9 (167772169) < 10.0.0.10 (167772170) -> 10.0.0.9 wins!
    draft.nodes[0].staticRoutes.clear();
    draft.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.8.0.0/16").value(), IPv4Address::parse("10.0.0.10").value(), std::nullopt, 1, 0});
    draft.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.8.0.0/16").value(), IPv4Address::parse("10.0.0.9").value(), std::nullopt, 1, 0});

    auto snap3 = TopologySnapshot::freeze(draft).value();
    auto table3 = ForwardingTable::build(snap3, NodeId("r"));
    auto res3 = table3.lookup(IPv4Address::parse("10.8.1.1").value(), NodeId("r"), snap3);
    REQUIRE(res3.has_value());
    REQUIRE_EQ(res3->nextHop->toString(), "10.0.0.9");
}

// -----------------------------------------------------------------------------
// T-2-09: Router egress interface and next hop on Fixture S
// -----------------------------------------------------------------------------
TEST_CASE("T-2-09 router egress interface and next hop") {
    auto draft = makeFixtureS();
    auto snap = TopologySnapshot::freeze(draft).value();

    // r1 FIB has exactly 2 routes: Connected 192.168.1.0/24 via eth0 and Connected 10.0.0.0/24 via eth1
    auto tableR = ForwardingTable::build(snap, NodeId("r1"));
    REQUIRE_EQ(tableR.routes().size(), 2);

    // r1 -> 10.0.0.20 : Forward, egress eth1, nextHop 10.0.0.20, matched 10.0.0.0/24
    auto d1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.0.0.20").value());
    REQUIRE(d1.routerOutcome == RouterForwardOutcome::Forward);
    REQUIRE(d1.outInterface.has_value());
    REQUIRE_EQ(d1.outInterface->str(), "eth1");
    REQUIRE(d1.nextHopIp.has_value());
    REQUIRE_EQ(d1.nextHopIp->toString(), "10.0.0.20");
    REQUIRE(d1.matchedRoute.has_value());
    REQUIRE_EQ(d1.matchedRoute->prefix.toString(), "10.0.0.0/24");

    // r1 -> 192.168.1.10 : Forward, egress eth0, nextHop 192.168.1.10
    auto d2 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("192.168.1.10").value());
    REQUIRE(d2.routerOutcome == RouterForwardOutcome::Forward);
    REQUIRE(d2.outInterface.has_value());
    REQUIRE_EQ(d2.outInterface->str(), "eth0");
    REQUIRE(d2.nextHopIp.has_value());
    REQUIRE_EQ(d2.nextHopIp->toString(), "192.168.1.10");

    // r1 -> 172.16.0.5 : NoRoute (reason NO_ROUTE)
    auto d3 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("172.16.0.5").value());
    REQUIRE(d3.routerOutcome == RouterForwardOutcome::NoRoute);
    REQUIRE(d3.dropReason.has_value());
    REQUIRE(*d3.dropReason == ErrorCode::NoRoute);
}

// -----------------------------------------------------------------------------
// T-2-10: Router local delivery and forwarding disabled
// -----------------------------------------------------------------------------
TEST_CASE("T-2-10 router local delivery") {
    auto draft = makeFixtureS();
    auto snap = TopologySnapshot::freeze(draft).value();

    // r1 -> 10.0.0.1 : LocalDelivery (matches active eth1)
    auto d1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.0.0.1").value());
    REQUIRE(d1.routerOutcome == RouterForwardOutcome::LocalDelivery);
    REQUIRE(d1.outInterface.has_value());
    REQUIRE_EQ(d1.outInterface->str(), "eth1");

    // r1 -> 192.168.1.1 : LocalDelivery (matches active eth0)
    auto d2 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("192.168.1.1").value());
    REQUIRE(d2.routerOutcome == RouterForwardOutcome::LocalDelivery);
    REQUIRE(d2.outInterface.has_value());
    REQUIRE_EQ(d2.outInterface->str(), "eth0");

    // If router forwarding is disabled -> ForwardingDisabled
    draft.nodes[1].forwardingEnabled = false;
    auto snapDisabled = TopologySnapshot::freeze(draft).value();
    auto d3 = decideRouterForward(snapDisabled, NodeId("r1"), IPv4Address::parse("10.0.0.20").value());
    REQUIRE(d3.routerOutcome == RouterForwardOutcome::ForwardingDisabled);
    REQUIRE(d3.dropReason.has_value());
    REQUIRE(*d3.dropReason == ErrorCode::ForwardingDisabled);
}

// -----------------------------------------------------------------------------
// T-2-11: Usability fallback (down interface/link/node makes routes unusable)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-11 usability fallback") {
    // 1. Fixture S with host-a eth0 Down: host-a -> 10.0.0.20 gives NoRoute with detail ALL_MATCHING_ROUTES_UNUSABLE
    auto draftS = makeFixtureS();
    auto snapS = TopologySnapshot::freeze(draftS).value();
    MockStateView viewHostDown;
    viewHostDown.downInterfaces.push_back({"host-a", "eth0"});

    auto dHost = decideHostSend(snapS, NodeId("host-a"), IPv4Address::parse("10.0.0.20").value(), &viewHostDown);
    REQUIRE(dHost.hostOutcome == HostSendOutcome::NoRoute);
    REQUIRE_EQ(dHost.detail, "ALL_MATCHING_ROUTES_UNUSABLE");
    REQUIRE(!dHost.candidateRoutes.empty());
    REQUIRE(!dHost.rejectedRoutes.empty());
    REQUIRE_EQ(dHost.rejectedRoutes[0].second, "UNUSABLE_INTERFACE_DOWN");

    // 2. Usability fallback router:
    // Routes 10.4.0.0/16 via 10.0.0.3 metric 5 (eth0) and 10.4.0.0/16 via 10.1.0.2 metric 50 (eth1)
    TopologyDraft draft;
    Node r;
    r.id = NodeId("r");
    r.name = "Router R";
    r.type = NodeType::Router;
    r.forwardingEnabled = true;

    Interface eth0;
    eth0.id = InterfaceId("eth0");
    eth0.name = "eth0";
    eth0.mac = MacAddress::parse("00:00:5e:00:53:21").value();
    eth0.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
    r.interfaces.push_back(eth0);

    Interface eth1;
    eth1.id = InterfaceId("eth1");
    eth1.name = "eth1";
    eth1.mac = MacAddress::parse("00:00:5e:00:53:22").value();
    eth1.ipv4 = IPv4Subnet::parse("10.1.0.1/24").value();
    r.interfaces.push_back(eth1);

    r.staticRoutes.push_back({IPv4Subnet::parse("10.4.0.0/16").value(), IPv4Address::parse("10.0.0.3").value(), InterfaceId("eth0"), 1, 5});
    r.staticRoutes.push_back({IPv4Subnet::parse("10.4.0.0/16").value(), IPv4Address::parse("10.1.0.2").value(), InterfaceId("eth1"), 1, 50});

    draft.nodes.push_back(r);
    auto snap = TopologySnapshot::freeze(draft).value();
    auto table = ForwardingTable::build(snap, NodeId("r"));

    // Normally eth0 wins
    auto normal = table.lookup(IPv4Address::parse("10.4.1.1").value(), NodeId("r"), snap);
    REQUIRE(normal.has_value());
    REQUIRE_EQ(normal->outInterface.str(), "eth0");
    REQUIRE_EQ(normal->metric, 5);

    // With eth0 Down -> eth1 route is chosen, eth0 route rejected as UNUSABLE_INTERFACE_DOWN
    MockStateView viewEth0Down;
    viewEth0Down.downInterfaces.push_back({"r", "eth0"});

    std::vector<Route> candidates;
    std::vector<std::pair<Route, std::string>> rejected;
    auto fallback = table.lookupWithEvidence(IPv4Address::parse("10.4.1.1").value(), NodeId("r"), snap,
                                            &viewEth0Down, &candidates, &rejected);
    REQUIRE(fallback.has_value());
    REQUIRE_EQ(fallback->outInterface.str(), "eth1");
    REQUIRE_EQ(fallback->metric, 50);

    bool foundRejectedEth0 = false;
    for (const auto& rej : rejected) {
        if (rej.first.outInterface.str() == "eth0" && rej.second == "UNUSABLE_INTERFACE_DOWN") {
            foundRejectedEth0 = true;
        }
    }
    REQUIRE(foundRejectedEth0);

    // With both eth0 and eth1 Down -> NoRoute
    MockStateView viewBothDown;
    viewBothDown.downInterfaces.push_back({"r", "eth0"});
    viewBothDown.downInterfaces.push_back({"r", "eth1"});
    auto noRoute = table.lookup(IPv4Address::parse("10.4.1.1").value(), NodeId("r"), snap, &viewBothDown);
    REQUIRE(!noRoute.has_value());
}

// -----------------------------------------------------------------------------
// T-2-15: Structured evidence and address-substitution guard
// -----------------------------------------------------------------------------
TEST_CASE("T-2-15 explanation structured evidence and substitution guard") {
    // 1. Evidence keys on Fixture S: host-a -> 10.0.0.20
    auto draftS = makeFixtureS();
    auto snapS = TopologySnapshot::freeze(draftS).value();

    auto dS = decideHostSend(snapS, NodeId("host-a"), IPv4Address::parse("10.0.0.20").value());
    auto expS = explain(dS);

    REQUIRE_EQ(expS.getEvidence("destinationIp"), "10.0.0.20");
    REQUIRE_EQ(expS.getEvidence("matchedPrefix"), "0.0.0.0/0");
    REQUIRE_EQ(expS.getEvidence("routeSource"), "Default");
    REQUIRE_EQ(expS.getEvidence("nextHopIp"), "192.168.1.1");
    REQUIRE_EQ(expS.getEvidence("egressInterface"), "eth0");
    REQUIRE_EQ(expS.getEvidence("onLink"), "false");
    REQUIRE(!expS.getEvidence("candidateCount").empty());

    // 2. Address-substitution guard:
    // Create copy with 192.168.1.x -> 172.20.1.x and 10.0.0.x -> 10.50.0.x
    TopologyDraft draftSub;

    Node hostA;
    hostA.id = NodeId("host-a");
    hostA.name = "Host A";
    hostA.type = NodeType::Host;
    hostA.defaultGateway = IPv4Address::parse("172.20.1.1").value();
    Interface ifA;
    ifA.id = InterfaceId("eth0");
    ifA.name = "eth0";
    ifA.mac = MacAddress::parse("00:00:5e:00:53:01").value();
    ifA.ipv4 = IPv4Subnet::parse("172.20.1.10/24").value();
    hostA.interfaces.push_back(ifA);
    draftSub.nodes.push_back(hostA);

    Node r1;
    r1.id = NodeId("r1");
    r1.name = "Router 1";
    r1.type = NodeType::Router;
    r1.forwardingEnabled = true;
    Interface rIf0;
    rIf0.id = InterfaceId("eth0");
    rIf0.name = "eth0";
    rIf0.mac = MacAddress::parse("00:00:5e:00:53:11").value();
    rIf0.ipv4 = IPv4Subnet::parse("172.20.1.1/24").value();
    r1.interfaces.push_back(rIf0);
    Interface rIf1;
    rIf1.id = InterfaceId("eth1");
    rIf1.name = "eth1";
    rIf1.mac = MacAddress::parse("00:00:5e:00:53:12").value();
    rIf1.ipv4 = IPv4Subnet::parse("10.50.0.1/24").value();
    r1.interfaces.push_back(rIf1);
    draftSub.nodes.push_back(r1);

    Node hostB;
    hostB.id = NodeId("host-b");
    hostB.name = "Host B";
    hostB.type = NodeType::Host;
    hostB.defaultGateway = IPv4Address::parse("10.50.0.1").value();
    Interface ifB;
    ifB.id = InterfaceId("eth0");
    ifB.name = "eth0";
    ifB.mac = MacAddress::parse("00:00:5e:00:53:02").value();
    ifB.ipv4 = IPv4Subnet::parse("10.50.0.20/24").value();
    hostB.interfaces.push_back(ifB);
    draftSub.nodes.push_back(hostB);

    Link l1;
    l1.id = LinkId("l1");
    l1.a = {NodeId("host-a"), InterfaceId("eth0")};
    l1.b = {NodeId("r1"), InterfaceId("eth0")};
    draftSub.links.push_back(l1);

    Link l2;
    l2.id = LinkId("l2");
    l2.a = {NodeId("host-b"), InterfaceId("eth0")};
    l2.b = {NodeId("r1"), InterfaceId("eth1")};
    draftSub.links.push_back(l2);

    auto snapSub = TopologySnapshot::freeze(draftSub).value();
    auto dSub = decideHostSend(snapSub, NodeId("host-a"), IPv4Address::parse("10.50.0.20").value());
    auto expSub = explain(dSub);

    std::string allText = expSub.what + " " + expSub.where + " " + expSub.why + " " + expSub.whatNext;
    for (const auto& kv : expSub.evidence) {
        allText += " " + kv.first + "=" + kv.second;
    }

    // Must contain the new substituted addresses
    REQUIRE(allText.find("172.20.1.") != std::string::npos);
    REQUIRE(allText.find("10.50.0.") != std::string::npos);

    // Must NOT contain ANY of the old hardcoded addresses
    REQUIRE(allText.find("192.168.1.") == std::string::npos);
    REQUIRE(allText.find("10.0.0.") == std::string::npos);
}

// -----------------------------------------------------------------------------
// T-2-20: NextHopResolution
// -----------------------------------------------------------------------------
TEST_CASE("T-2-20 next hop resolution with ArpCache") {
    auto draft = makeFixtureS();
    auto snap = TopologySnapshot::freeze(draft).value();

    // host-a -> 10.0.0.20
    auto d = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("10.0.0.20").value());
    REQUIRE(d.nextHopIp.has_value());
    REQUIRE_EQ(d.nextHopIp->toString(), "192.168.1.1");

    // 1. Empty ArpCache -> needsArp is true
    ArpCache emptyCache;
    auto res1 = resolveNextHop(d, emptyCache);
    REQUIRE_EQ(res1.nextHopIp.toString(), "192.168.1.1");
    REQUIRE_EQ(res1.egressInterface.str(), "eth0");
    REQUIRE(res1.needsArp);
    REQUIRE(!res1.cachedMac.has_value());

    // 2. Populated ArpCache with gateway MAC (r1.eth0 = 00:00:5e:00:53:11) -> needsArp is false
    ArpCache populatedCache;
    populatedCache.insertOrUpdate(IPv4Address::parse("192.168.1.1").value(),
                                  MacAddress::parse("00:00:5e:00:53:11").value(),
                                  ArpEntrySource::Preloaded);
    auto res2 = resolveNextHop(d, populatedCache);
    REQUIRE_EQ(res2.nextHopIp.toString(), "192.168.1.1");
    REQUIRE(!res2.needsArp);
    REQUIRE(res2.cachedMac.has_value());
    REQUIRE_EQ(res2.cachedMac->toString(), "00:00:5e:00:53:11");

    // 3. Direct send: host-a -> 192.168.1.77 -> nextHop is 192.168.1.77
    auto dDirect = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("192.168.1.77").value());
    auto res3 = resolveNextHop(dDirect, emptyCache);
    REQUIRE_EQ(res3.nextHopIp.toString(), "192.168.1.77");
    REQUIRE(res3.needsArp);
}

// -----------------------------------------------------------------------------
// T-2-22: EphemeralPortAllocator
// -----------------------------------------------------------------------------
TEST_CASE("T-2-22 ephemeral port allocator sequence and wrap-around") {
    EphemeralPortAllocator alloc1;

    // First three values are 49152, 49153, 49154
    REQUIRE_EQ(alloc1.next(), 49152);
    REQUIRE_EQ(alloc1.next(), 49153);
    REQUIRE_EQ(alloc1.next(), 49154);

    // Advance to 16384th value (16381 more iterations)
    for (int i = 0; i < 16380; ++i) {
        alloc1.next();
    }
    // 16384th value must be 65535
    REQUIRE_EQ(alloc1.next(), 65535);

    // 16385th value wraps back to 49152
    REQUIRE_EQ(alloc1.next(), 49152);

    // Independent instances per host
    EphemeralPortAllocator alloc2;
    REQUIRE_EQ(alloc2.next(), 49152);
    REQUIRE_EQ(alloc1.next(), 49153);
    REQUIRE_EQ(alloc2.next(), 49153);
}

static bool hasIssueWithCode(const ValidationReport& rep, const std::string& code) {
    for (const auto& issue : rep.issues) {
        if (issue.code == code) return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// T-2-26: Static route validation
// -----------------------------------------------------------------------------
TEST_CASE("T-2-26 static route validation") {
    // Router with eth0 10.0.0.1/24
    auto makeBaseRouter = []() {
        TopologyDraft d;
        Node r;
        r.id = NodeId("r");
        r.name = "Router R";
        r.type = NodeType::Router;
        r.forwardingEnabled = true;
        Interface eth0;
        eth0.id = InterfaceId("eth0");
        eth0.name = "eth0";
        eth0.mac = MacAddress::parse("00:00:5e:00:53:21").value();
        eth0.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
        r.interfaces.push_back(eth0);
        d.nodes.push_back(r);
        return d;
    };

    // 1. 10.5.0.0/16 via 10.0.0.2 -> valid
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), IPv4Address::parse("10.0.0.2").value(), std::nullopt, 1, 0});
        auto rep = validate(d);
        REQUIRE(rep.ok());
    }

    // 2. 10.5.0.0/16 via 172.16.0.1 -> ROUTE_NEXT_HOP_NOT_ON_LINK
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), IPv4Address::parse("172.16.0.1").value(), std::nullopt, 1, 0});
        auto rep = validate(d);
        REQUIRE(!rep.ok());
        REQUIRE(hasIssueWithCode(rep, "ROUTE_NEXT_HOP_NOT_ON_LINK"));
    }

    // 3. 10.5.0.0/16 via 10.0.0.1 (own IP) -> INVALID_ROUTE
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), IPv4Address::parse("10.0.0.1").value(), std::nullopt, 1, 0});
        auto rep = validate(d);
        REQUIRE(!rep.ok());
        REQUIRE(hasIssueWithCode(rep, "INVALID_ROUTE"));
    }

    // 4. 10.5.0.1/16 via 10.0.0.2 (host bits set) -> INVALID_ROUTE
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.1/16").value(), IPv4Address::parse("10.0.0.2").value(), std::nullopt, 1, 0});
        auto rep = validate(d);
        REQUIRE(!rep.ok());
        REQUIRE(hasIssueWithCode(rep, "INVALID_ROUTE"));
    }

    // 5. 10.5.0.0/16 with outInterface eth9 -> ROUTE_INTERFACE_NOT_FOUND
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), std::nullopt, InterfaceId("eth9"), 1, 0});
        auto rep = validate(d);
        REQUIRE(!rep.ok());
        REQUIRE(hasIssueWithCode(rep, "ROUTE_INTERFACE_NOT_FOUND"));
    }

    // 6. 10.5.0.0/16 with neither next hop nor interface -> INVALID_ROUTE
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), std::nullopt, std::nullopt, 1, 0});
        auto rep = validate(d);
        REQUIRE(!rep.ok());
        REQUIRE(hasIssueWithCode(rep, "INVALID_ROUTE"));
    }

    // 7. 10.5.0.0/16 outInterface eth0, no next hop -> valid (direct)
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), std::nullopt, InterfaceId("eth0"), 1, 0});
        auto rep = validate(d);
        REQUIRE(rep.ok());
    }

    // 8. 0.0.0.0/0 via 10.0.0.254 -> valid
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("0.0.0.0/0").value(), IPv4Address::parse("10.0.0.254").value(), std::nullopt, 1, 0});
        auto rep = validate(d);
        REQUIRE(rep.ok());
    }

    // 9. The same route listed twice -> warning DUPLICATE_ROUTE
    {
        auto d = makeBaseRouter();
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), IPv4Address::parse("10.0.0.2").value(), std::nullopt, 1, 0});
        d.nodes[0].staticRoutes.push_back({IPv4Subnet::parse("10.5.0.0/16").value(), IPv4Address::parse("10.0.0.2").value(), std::nullopt, 1, 0});
        auto rep = validate(d);
        REQUIRE(rep.ok()); // warnings do not make topology invalid
        REQUIRE(hasIssueWithCode(rep, "DUPLICATE_ROUTE"));
    }
}


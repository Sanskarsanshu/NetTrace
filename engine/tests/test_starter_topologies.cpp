#include "test_framework.hpp"
#include "nettrace/model/topology_io.hpp"
#include "nettrace/model/validate.hpp"
#include "nettrace/routing/graph_builder.hpp"
#include "nettrace/routing/path_bridge.hpp"
#include "nettrace/routing/dijkstra.hpp"
#include "nettrace/routing/bfs.hpp"
#include "nettrace/routing/widest_path.hpp"
#include "nettrace/forwarding/forwarding_engine.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace nettrace;

static std::string loadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string findExampleDir() {
    std::vector<std::string> prefixes = {"examples/", "../examples/", "../../examples/"};
    for (const auto& p : prefixes) {
        if (!loadFile(p + "same-subnet.json").empty()) {
            return p;
        }
    }
    return "";
}

// Custom StateView for dynamic link/node fault simulation
class FaultStateView : public StateView {
public:
    std::vector<std::string> downLinks;

    bool isLinkUp(const LinkId& linkId) const override {
        for (const auto& d : downLinks) {
            if (d == linkId.str()) return false;
        }
        return true;
    }
    bool isNodeUp(const NodeId&) const override { return true; }
    bool isInterfaceUp(const NodeId&, const InterfaceId&) const override { return true; }
};

// -----------------------------------------------------------------------------
// T-2-33: Starter topologies validation, stability, and scenarios (TEST_VECTORS.md section 4)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-33 starter topologies scenarios and assertions") {
    std::string exDir = findExampleDir();
    REQUIRE(!exDir.empty());

    const std::vector<std::string> allFiles = {
        "same-subnet.json",
        "gateway-routing.json",
        "multi-router-cloud.json",
        "ttl-expiry.json",
        "no-route.json",
        "bandwidth-vs-cost.json",
        "link-failover.json",
        "mtu-df.json",
        "fragmentation.json",
        "lossy.json",
        "loop.json",
        "shortcut-cost-vs-hops.json"
    };

    // 1. Generic check across all 12 files: loads, 0 errors, 0 warnings, hash stable
    for (const auto& fname : allFiles) {
        std::string raw = loadFile(exDir + fname);
        REQUIRE(!raw.empty());

        auto d1Res = loadTopologyJson(raw);
        REQUIRE(d1Res.isOk());
        auto d1 = d1Res.value();

        auto rep = validate(d1);
        REQUIRE(rep.ok());
        REQUIRE_EQ(rep.errorCount(), 0);
        REQUIRE_EQ(rep.warningCount(), 0);

        auto s1Res = TopologySnapshot::freeze(d1);
        REQUIRE(s1Res.isOk());
        auto s1 = s1Res.value();

        // Second load produces identical hash
        auto d2 = loadTopologyJson(raw).value();
        auto s2 = TopologySnapshot::freeze(d2).value();
        REQUIRE_EQ(s1.contentHash(), s2.contentHash());
    }

    // -------------------------------------------------------------------------
    // 4.1 same-subnet.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "same-subnet.json")).value()).value();
        auto d = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("192.168.1.20").value());
        REQUIRE(d.hostOutcome == HostSendOutcome::Direct);
        REQUIRE(d.nextHopIp.has_value());
        REQUIRE_EQ(d.nextHopIp->toString(), "192.168.1.20");
        REQUIRE(d.matchedRoute.has_value());
        REQUIRE_EQ(d.matchedRoute->prefix.toString(), "192.168.1.0/24");

        auto l3 = GraphBuilder::build(snap);
        REQUIRE_EQ(l3.nodeCount(), 2);
        REQUIRE_EQ(l3.l3Arcs().size(), 2);
        const auto* arc = l3.findL3Arc(NodeId("host-a"), NodeId("host-b"));
        REQUIRE(arc != nullptr);
        REQUIRE_EQ(arc->cost, 2.0);
        REQUIRE_EQ(arc->propagationDelayMs, 2.0);
        REQUIRE_EQ(arc->bandwidthBps, 100000000);
        REQUIRE_EQ(arc->mtuBytes, 1500);
        REQUIRE_EQ(arc->switchesTraversed.size(), 1);
        REQUIRE_EQ(arc->switchesTraversed[0].str(), "sw1");
        REQUIRE_EQ(arc->underlyingLinkIds.size(), 2);
    }

    // -------------------------------------------------------------------------
    // 4.2 gateway-routing.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "gateway-routing.json")).value()).value();
        auto dA1 = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("192.168.1.1").value());
        REQUIRE(dA1.hostOutcome == HostSendOutcome::Direct);
        REQUIRE_EQ(dA1.nextHopIp->toString(), "192.168.1.1");
        REQUIRE_EQ(dA1.outInterface->str(), "eth0");

        auto dA2 = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("10.0.0.20").value());
        REQUIRE(dA2.hostOutcome == HostSendOutcome::ViaGateway);
        REQUIRE_EQ(dA2.nextHopIp->toString(), "192.168.1.1");
        REQUIRE_EQ(dA2.outInterface->str(), "eth0");

        auto dR1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.0.0.20").value());
        REQUIRE(dR1.routerOutcome == RouterForwardOutcome::Forward);
        REQUIRE_EQ(dR1.nextHopIp->toString(), "10.0.0.20");
        REQUIRE_EQ(dR1.outInterface->str(), "eth1");

        auto dR2 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("192.168.1.10").value());
        REQUIRE(dR2.routerOutcome == RouterForwardOutcome::Forward);
        REQUIRE_EQ(dR2.nextHopIp->toString(), "192.168.1.10");
        REQUIRE_EQ(dR2.outInterface->str(), "eth0");

        auto dRLoc1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.0.0.1").value());
        REQUIRE(dRLoc1.routerOutcome == RouterForwardOutcome::LocalDelivery);

        auto dRLoc2 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("192.168.1.1").value());
        REQUIRE(dRLoc2.routerOutcome == RouterForwardOutcome::LocalDelivery);

        auto dRNo = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("172.16.0.5").value());
        REQUIRE(dRNo.routerOutcome == RouterForwardOutcome::NoRoute);
        REQUIRE_EQ(*dRNo.dropReason, ErrorCode::NoRoute);

        auto l3 = GraphBuilder::build(snap);
        REQUIRE_EQ(l3.nodeCount(), 3);
        REQUIRE_EQ(l3.l3Arcs().size(), 4);
        auto dij = Dijkstra::findPath(l3, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(dij.reachable);
        REQUIRE_EQ(dij.totalCost, 2.0);
        auto expDij = l3.expandPath(dij, snap);
        REQUIRE_EQ(expDij.routerHopCount, 1);
        REQUIRE_EQ(expDij.linkHopCount, 2);
    }

    // -------------------------------------------------------------------------
    // 4.3 multi-router-cloud.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "multi-router-cloud.json")).value()).value();
        auto l3 = GraphBuilder::build(snap);

        auto dij = Dijkstra::findPath(l3, NodeId("host-a"), NodeId("cloud"));
        REQUIRE(dij.reachable);
        REQUIRE_EQ(dij.totalCost, 17.0);
        auto expDij = l3.expandPath(dij, snap);
        REQUIRE_EQ(expDij.routerHopCount, 3);
        REQUIRE_EQ(expDij.linkHopCount, 5);
        REQUIRE_EQ(expDij.switchesTraversed.size(), 1);
        REQUIRE_EQ(expDij.switchesTraversed[0].str(), "sw1");

        // Forwarding toward 10.0.35.2
        auto dR1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.0.35.2").value());
        REQUIRE_EQ(dR1.nextHopIp->toString(), "10.0.12.2");
        REQUIRE_EQ(dR1.outInterface->str(), "eth1");

        auto dR2 = decideRouterForward(snap, NodeId("r2"), IPv4Address::parse("10.0.35.2").value());
        REQUIRE_EQ(dR2.nextHopIp->toString(), "10.0.23.2");
        REQUIRE_EQ(dR2.outInterface->str(), "eth1");

        auto dR3 = decideRouterForward(snap, NodeId("r3"), IPv4Address::parse("10.0.35.2").value());
        REQUIRE(dR3.routerOutcome == RouterForwardOutcome::Forward);
        REQUIRE_EQ(dR3.nextHopIp->toString(), "10.0.35.2");
        REQUIRE_EQ(dR3.outInterface->str(), "eth1");

        // Return toward 192.168.1.10
        auto dRet3 = decideRouterForward(snap, NodeId("r3"), IPv4Address::parse("192.168.1.10").value());
        REQUIRE_EQ(dRet3.nextHopIp->toString(), "10.0.23.1");
        REQUIRE_EQ(dRet3.outInterface->str(), "eth0");

        auto dRet2 = decideRouterForward(snap, NodeId("r2"), IPv4Address::parse("192.168.1.10").value());
        REQUIRE_EQ(dRet2.nextHopIp->toString(), "10.0.12.1");
        REQUIRE_EQ(dRet2.outInterface->str(), "eth0");

        auto dRet1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("192.168.1.10").value());
        REQUIRE_EQ(dRet1.nextHopIp->toString(), "192.168.1.10");
        REQUIRE_EQ(dRet1.outInterface->str(), "eth0");
    }

    // -------------------------------------------------------------------------
    // 4.4 ttl-expiry.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "ttl-expiry.json")).value()).value();
        auto l3 = GraphBuilder::build(snap);

        auto dij = Dijkstra::findPath(l3, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(dij.reachable);
        REQUIRE_EQ(dij.totalCost, 4.0);
        auto expDij = l3.expandPath(dij, snap);
        REQUIRE_EQ(expDij.routerHopCount, 3);
        REQUIRE_EQ(expDij.linkHopCount, 4);

        // Forwarding toward 10.2.0.20
        auto dR1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.2.0.20").value());
        REQUIRE_EQ(dR1.nextHopIp->toString(), "10.12.0.2");
        REQUIRE_EQ(dR1.outInterface->str(), "eth1");

        auto dR2 = decideRouterForward(snap, NodeId("r2"), IPv4Address::parse("10.2.0.20").value());
        REQUIRE_EQ(dR2.nextHopIp->toString(), "10.23.0.2");
        REQUIRE_EQ(dR2.outInterface->str(), "eth1");

        auto dR3 = decideRouterForward(snap, NodeId("r3"), IPv4Address::parse("10.2.0.20").value());
        REQUIRE_EQ(dR3.nextHopIp->toString(), "10.2.0.20");
        REQUIRE_EQ(dR3.outInterface->str(), "eth1");

        // Return toward 10.1.0.10
        auto dRet3 = decideRouterForward(snap, NodeId("r3"), IPv4Address::parse("10.1.0.10").value());
        REQUIRE_EQ(dRet3.nextHopIp->toString(), "10.23.0.1");
        REQUIRE_EQ(dRet3.outInterface->str(), "eth0");

        auto dRet2 = decideRouterForward(snap, NodeId("r2"), IPv4Address::parse("10.1.0.10").value());
        REQUIRE_EQ(dRet2.nextHopIp->toString(), "10.12.0.1");
        REQUIRE_EQ(dRet2.outInterface->str(), "eth0");

        auto dRet1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.1.0.10").value());
        REQUIRE_EQ(dRet1.nextHopIp->toString(), "10.1.0.10");
        REQUIRE_EQ(dRet1.outInterface->str(), "eth0");
    }

    // -------------------------------------------------------------------------
    // 4.5 no-route.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "no-route.json")).value()).value();

        auto dA = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("10.0.0.20").value());
        REQUIRE(dA.hostOutcome == HostSendOutcome::NoRoute);
        REQUIRE_EQ(*dA.dropReason, ErrorCode::NoRoute);

        auto dADir = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("192.168.1.30").value());
        REQUIRE(dADir.hostOutcome == HostSendOutcome::Direct);

        auto dD = decideHostSend(snap, NodeId("host-d"), IPv4Address::parse("172.16.0.10").value());
        REQUIRE(dD.hostOutcome == HostSendOutcome::ViaGateway);
        REQUIRE_EQ(dD.nextHopIp->toString(), "192.168.1.1");

        auto dR1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("172.16.0.10").value());
        REQUIRE(dR1.routerOutcome == RouterForwardOutcome::NoRoute);
        REQUIRE_EQ(*dR1.dropReason, ErrorCode::NoRoute);

        auto dR1_b = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.0.0.20").value());
        REQUIRE(dR1_b.routerOutcome == RouterForwardOutcome::Forward);
        REQUIRE_EQ(dR1_b.outInterface->str(), "eth1");

        auto dR2 = decideRouterForward(snap, NodeId("r2"), IPv4Address::parse("192.168.1.30").value());
        REQUIRE(dR2.routerOutcome == RouterForwardOutcome::Forward);
        REQUIRE_EQ(dR2.nextHopIp->toString(), "10.9.0.1");
        REQUIRE_EQ(dR2.outInterface->str(), "eth0");

        auto l3 = GraphBuilder::build(snap);
        auto dij = Dijkstra::findPath(l3, NodeId("host-d"), NodeId("host-c"));
        REQUIRE(dij.reachable);
        REQUIRE_EQ(dij.totalCost, 4.0);
    }

    // -------------------------------------------------------------------------
    // 4.6 bandwidth-vs-cost.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "bandwidth-vs-cost.json")).value()).value();
        auto l3 = GraphBuilder::build(snap);

        // Dijkstra: low-cost path host-a, r1, r2, r4, host-b (cost 4, bw 10M, routerHops 3, linkHops 4)
        auto dij = Dijkstra::findPath(l3, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(dij.reachable);
        REQUIRE_EQ(dij.totalCost, 4.0);
        REQUIRE_EQ(dij.bottleneckBandwidthBps, 10000000);
        auto expDij = l3.expandPath(dij, snap);
        REQUIRE_EQ(expDij.routerHopCount, 3);
        REQUIRE_EQ(expDij.linkHopCount, 4);

        // Widest Path: high-cost wide path host-a, r1, r3, r4, host-b (cost 12, bw 1G, routerHops 3, linkHops 4)
        auto wide = WidestPath::findPath(l3, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(wide.reachable);
        REQUIRE_EQ(wide.totalCost, 12.0);
        REQUIRE_EQ(wide.bottleneckBandwidthBps, 1000000000);
        auto expWide = l3.expandPath(wide, snap);
        REQUIRE_EQ(expWide.routerHopCount, 3);
        REQUIRE_EQ(expWide.linkHopCount, 4);

        // BFS: tie on edge count broken to lexicographic r2 < r3 -> host-a, r1, r2, r4, host-b
        auto bfsRes = BFS::findPath(l3, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(bfsRes.reachable);
        REQUIRE_EQ(bfsRes.totalCost, 4.0);
        REQUIRE_EQ(bfsRes.pathNodeIds[2].str(), "r2");

        // Policy routes for Widest path toward 10.2.0.20
        auto polWide = planPolicyRoutes(snap, wide, IPv4Address::parse("10.2.0.20").value());
        REQUIRE(polWide.success);
        REQUIRE_EQ(polWide.totalPolicyRoutesCount(), 4);
        auto r1Pol = polWide.routesForNode(NodeId("r1"));
        REQUIRE_EQ(r1Pol.size(), 1);
        REQUIRE_EQ(r1Pol[0].outInterface.str(), "eth2");
        REQUIRE_EQ(r1Pol[0].nextHop->toString(), "10.13.0.2");

        // Overridden on r1: exactly one entry: 10.2.0.0/24 via 10.12.0.2
        auto r1Over = polWide.overriddenForNode(NodeId("r1"));
        REQUIRE_EQ(r1Over.size(), 1);
        REQUIRE_EQ(r1Over[0].prefix.toString(), "10.2.0.0/24");
        REQUIRE_EQ(r1Over[0].nextHop->toString(), "10.12.0.2");
    }

    // -------------------------------------------------------------------------
    // 4.6 link-failover.json
    // -------------------------------------------------------------------------
    {
        auto draft = loadTopologyJson(loadFile(exDir + "link-failover.json")).value();
        auto snap = TopologySnapshot::freeze(draft).value();
        auto l3 = GraphBuilder::build(snap);

        // Baseline: all Up -> Dijkstra cost 4
        auto dijBase = Dijkstra::findPath(l3, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(dijBase.reachable);
        REQUIRE_EQ(dijBase.totalCost, 4.0);

        // With link l24 Down -> exclusions = [link l24: INACTIVE_COMPONENT], Dijkstra cost 12
        auto draftL24Down = draft;
        for (auto& l : draftL24Down.links) {
            if (l.id.str() == "l24") l.adminState = AdminState::Down;
        }
        auto snapL24Down = TopologySnapshot::freeze(draftL24Down).value();
        auto l3L24Down = GraphBuilder::build(snapL24Down);
        REQUIRE_EQ(l3L24Down.exclusions().size(), 1);
        REQUIRE_EQ(l3L24Down.exclusions()[0], "link:l24");

        auto dijL24Down = Dijkstra::findPath(l3L24Down, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(dijL24Down.reachable);
        REQUIRE_EQ(dijL24Down.totalCost, 12.0);
        REQUIRE_EQ(dijL24Down.pathNodeIds[2].str(), "r3");

        // Dynamic StateView with l24 Down for forwarding decision on r2:
        FaultStateView fView;
        fView.downLinks.push_back("l24");
        auto dR2 = decideRouterForward(snap, NodeId("r2"), IPv4Address::parse("10.2.0.20").value(), &fView);
        REQUIRE(dR2.routerOutcome == RouterForwardOutcome::NoRoute);
        REQUIRE_EQ(dR2.detail, "ALL_MATCHING_ROUTES_UNUSABLE");

        // With l24 and l34 Down -> unreachable
        for (auto& l : draftL24Down.links) {
            if (l.id.str() == "l34") l.adminState = AdminState::Down;
        }
        auto snapBothDown = TopologySnapshot::freeze(draftL24Down).value();
        auto l3BothDown = GraphBuilder::build(snapBothDown);
        auto dijBothDown = Dijkstra::findPath(l3BothDown, NodeId("host-a"), NodeId("host-b"));
        REQUIRE(!dijBothDown.reachable);
    }

    // -------------------------------------------------------------------------
    // 4.7 mtu-df.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "mtu-df.json")).value()).value();
        auto l3 = GraphBuilder::build(snap);
        const auto* arcR1R2 = l3.findL3Arc(NodeId("r1"), NodeId("r2"));
        REQUIRE(arcR1R2 != nullptr);
        REQUIRE_EQ(arcR1R2->mtuBytes, 576);

        const auto* arcHaR1 = l3.findL3Arc(NodeId("host-a"), NodeId("r1"));
        REQUIRE(arcHaR1 != nullptr);
        REQUIRE_EQ(arcHaR1->mtuBytes, 1500);

        const auto* arcR2Hb = l3.findL3Arc(NodeId("r2"), NodeId("host-b"));
        REQUIRE(arcR2Hb != nullptr);
        REQUIRE_EQ(arcR2Hb->mtuBytes, 1500);
    }

    // -------------------------------------------------------------------------
    // 4.7 fragmentation.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "fragmentation.json")).value()).value();
        auto l3 = GraphBuilder::build(snap);
        const auto* arcHaR1 = l3.findL3Arc(NodeId("host-a"), NodeId("r1"));
        REQUIRE(arcHaR1 != nullptr);
        REQUIRE_EQ(arcHaR1->mtuBytes, 9000);

        const auto* arcR1R2 = l3.findL3Arc(NodeId("r1"), NodeId("r2"));
        REQUIRE(arcR1R2 != nullptr);
        REQUIRE_EQ(arcR1R2->mtuBytes, 1500);
    }

    // -------------------------------------------------------------------------
    // 4.7 lossy.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "lossy.json")).value()).value();
        auto l3 = GraphBuilder::build(snap);
        const auto* arcHaR1 = l3.findL3Arc(NodeId("host-a"), NodeId("r1"));
        const auto* arcR1R2 = l3.findL3Arc(NodeId("r1"), NodeId("r2"));
        const auto* arcR2Hb = l3.findL3Arc(NodeId("r2"), NodeId("host-b"));

        REQUIRE(arcHaR1 != nullptr);
        REQUIRE(arcR1R2 != nullptr);
        REQUIRE(arcR2Hb != nullptr);

        REQUIRE(std::abs(arcHaR1->packetLossProbability - 0.05) < 1e-12);
        REQUIRE(std::abs(arcR1R2->packetLossProbability - 0.10) < 1e-12);
        REQUIRE(std::abs(arcR2Hb->packetLossProbability - 0.0) < 1e-12);

        // Path loss: 1 - (1 - 0.05)*(1 - 0.10)*(1 - 0.0) = 1 - (0.95 * 0.90 * 1.0) = 0.145
        double pathLoss = 1.0 - (1.0 - 0.05) * (1.0 - 0.10) * (1.0 - 0.0);
        REQUIRE(std::abs(pathLoss - 0.145) < 1e-12);
    }

    // -------------------------------------------------------------------------
    // 4.8 loop.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "loop.json")).value()).value();
        auto dHa = decideHostSend(snap, NodeId("host-a"), IPv4Address::parse("10.9.0.99").value());
        REQUIRE(dHa.hostOutcome == HostSendOutcome::ViaGateway);
        REQUIRE_EQ(dHa.nextHopIp->toString(), "10.1.0.1");

        auto dR1 = decideRouterForward(snap, NodeId("r1"), IPv4Address::parse("10.9.0.99").value());
        REQUIRE(dR1.routerOutcome == RouterForwardOutcome::Forward);
        REQUIRE_EQ(dR1.nextHopIp->toString(), "10.12.0.2");
        REQUIRE_EQ(dR1.outInterface->str(), "eth1");

        auto dR2 = decideRouterForward(snap, NodeId("r2"), IPv4Address::parse("10.9.0.99").value());
        REQUIRE(dR2.routerOutcome == RouterForwardOutcome::Forward);
        REQUIRE_EQ(dR2.nextHopIp->toString(), "10.12.0.1");
        REQUIRE_EQ(dR2.outInterface->str(), "eth0");
    }

    // -------------------------------------------------------------------------
    // 4.9 shortcut-cost-vs-hops.json
    // -------------------------------------------------------------------------
    {
        auto snap = TopologySnapshot::freeze(loadTopologyJson(loadFile(exDir + "shortcut-cost-vs-hops.json")).value()).value();
        auto l3 = GraphBuilder::build(snap);

        auto dij = Dijkstra::findPath(l3, NodeId("host-a"), NodeId("cloud"));
        REQUIRE(dij.reachable);
        REQUIRE_EQ(dij.totalCost, 14.0);

        auto bfs = BFS::findPath(l3, NodeId("host-a"), NodeId("cloud"));
        REQUIRE(bfs.reachable);
        REQUIRE_EQ(bfs.totalCost, 24.0);

        auto polDij = planPolicyRoutes(snap, dij, IPv4Address::parse("10.0.35.2").value());
        REQUIRE_EQ(polDij.overriddenForNode(NodeId("r1")).size(), 1);

        auto polBfs = planPolicyRoutes(snap, bfs, IPv4Address::parse("10.0.35.2").value());
        REQUIRE(polBfs.overriddenForNode(NodeId("r1")).empty());
    }
}

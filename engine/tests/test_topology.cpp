#include "test_framework.hpp"
#include "nettrace/model/topology.hpp"
#include "nettrace/model/validate.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace nettrace;

// Helper to construct baseline passing fixture G1
static TopologyDraft makeG1() {
    TopologyDraft d;
    // host-a: eth0 192.168.1.10/24, mac 02:00:00:01:00:01, gateway 192.168.1.1
    Node hostA;
    hostA.id = NodeId("host-a");
    hostA.name = "Host A";
    hostA.type = NodeType::Host;
    hostA.defaultGateway = IPv4Address::parse("192.168.1.1").value();
    Interface ifA;
    ifA.id = InterfaceId("eth0");
    ifA.name = "eth0";
    ifA.mac = MacAddress::parse("02:00:00:01:00:01").value();
    ifA.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
    hostA.interfaces.push_back(ifA);
    d.nodes.push_back(hostA);

    // sw1: ports p1, p2 (no IP, no MAC)
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

    // host-b: eth0 192.168.1.20/24, mac 02:00:00:02:00:01, no gateway
    Node hostB;
    hostB.id = NodeId("host-b");
    hostB.name = "Host B";
    hostB.type = NodeType::Host;
    Interface ifB;
    ifB.id = InterfaceId("eth0");
    ifB.name = "eth0";
    ifB.mac = MacAddress::parse("02:00:00:02:00:01").value();
    ifB.ipv4 = IPv4Subnet::parse("192.168.1.20/24").value();
    hostB.interfaces.push_back(ifB);
    d.nodes.push_back(hostB);

    // links: l1 host-a.eth0 <-> sw1.p1, l2 host-b.eth0 <-> sw1.p2
    Link l1;
    l1.id = LinkId("l1");
    l1.a = {NodeId("host-a"), InterfaceId("eth0")};
    l1.b = {NodeId("sw1"), InterfaceId("p1")};
    l1.cost = 1.0;
    l1.bandwidthBps = 100000000ULL;
    l1.propagationDelayMs = 1.0;
    l1.packetLossProbability = 0.0;
    l1.mtuBytes = 1500;
    d.links.push_back(l1);

    Link l2;
    l2.id = LinkId("l2");
    l2.a = {NodeId("host-b"), InterfaceId("eth0")};
    l2.b = {NodeId("sw1"), InterfaceId("p2")};
    l2.cost = 1.0;
    l2.bandwidthBps = 100000000ULL;
    l2.propagationDelayMs = 1.0;
    l2.packetLossProbability = 0.0;
    l2.mtuBytes = 1500;
    d.links.push_back(l2);

    return d;
}

static bool hasIssueWithCode(const ValidationReport& rep, std::string_view code) {
    for (const auto& issue : rep.issues) {
        if (issue.code == code) return true;
    }
    return false;
}

TEST_CASE("T-2-13: Validation rules 1..12 with passing and failing fixtures") {
    // 0. Baseline passing fixture G1: expect 0 errors, 0 warnings
    {
        auto g1 = makeG1();
        auto rep = validate(g1);
        REQUIRE_EQ(rep.errorCount(), 0);
        REQUIRE_EQ(rep.warningCount(), 0);
        REQUIRE(rep.ok());
    }

    // Rule 1: Invalid ID and Invalid Name
    {
        auto d = makeG1();
        d.nodes[0].id = NodeId("host a with spaces"); // contains spaces
        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "INVALID_ID"));

        auto d2 = makeG1();
        d2.nodes[0].name = std::string(65, 'x'); // > 64 chars
        auto rep2 = validate(d2);
        REQUIRE(hasIssueWithCode(rep2, "INVALID_NAME"));

        auto d3 = makeG1();
        d3.nodes[0].name = "bad\x01name"; // control char
        auto rep3 = validate(d3);
        REQUIRE(hasIssueWithCode(rep3, "INVALID_NAME"));
    }

    // Rule 2: Duplicate Node ID, Duplicate Link ID, Duplicate Interface ID inside node
    {
        auto d = makeG1();
        d.nodes.push_back(d.nodes[0]); // Duplicate host-a
        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "DUPLICATE_NODE_ID"));

        auto dLink = makeG1();
        dLink.links[1].id = LinkId("l1"); // Duplicate link l1
        auto repLink = validate(dLink);
        REQUIRE(hasIssueWithCode(repLink, "DUPLICATE_LINK_ID"));

        auto dIface = makeG1();
        Interface eth0_dup;
        eth0_dup.id = InterfaceId("eth0");
        eth0_dup.name = "eth0_copy";
        eth0_dup.mac = MacAddress::parse("02:00:00:01:00:02").value();
        eth0_dup.ipv4 = IPv4Subnet::parse("192.168.1.11/24").value();
        dIface.nodes[0].interfaces.push_back(eth0_dup);
        auto repIface = validate(dIface);
        REQUIRE(hasIssueWithCode(repIface, "DUPLICATE_INTERFACE_ID"));

        // Passing fixture: SAME interface ID on DIFFERENT nodes is legal
        auto dPass = makeG1();
        REQUIRE_EQ(dPass.nodes[0].interfaces[0].id.str(), "eth0");
        REQUIRE_EQ(dPass.nodes[2].interfaces[0].id.str(), "eth0");
        auto repPass = validate(dPass);
        REQUIRE_EQ(repPass.errorCount(), 0);
    }

    // Rule 3: Link endpoint node or interface missing -> LINK_ENDPOINT_NOT_FOUND
    {
        auto d = makeG1();
        d.links[0].a.nodeId = NodeId("ghost");
        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "LINK_ENDPOINT_NOT_FOUND"));

        auto dIface = makeG1();
        dIface.links[0].a.interfaceId = InterfaceId("eth9");
        auto repIface = validate(dIface);
        REQUIRE(hasIssueWithCode(repIface, "LINK_ENDPOINT_NOT_FOUND"));
    }

    // Rule 4: Interface already linked -> INTERFACE_ALREADY_LINKED
    {
        auto d = makeG1();
        d.links[1].b.interfaceId = InterfaceId("p1"); // l2 also uses sw1.p1
        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "INTERFACE_ALREADY_LINKED"));
    }

    // Rule 5: Link endpoints both on the same node -> SELF_LOOP_REJECTED
    {
        auto d = makeG1();
        d.links[0].b = {NodeId("host-a"), InterfaceId("eth0")};
        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "SELF_LOOP_REJECTED"));
    }

    // Rule 6: Interface IP valid host address per D-15
    {
        auto dNet = makeG1();
        dNet.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.1.0/24").value();
        auto repNet = validate(dNet);
        REQUIRE(hasIssueWithCode(repNet, "INVALID_HOST_ADDRESS"));

        auto dBcast = makeG1();
        dBcast.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.1.255/24").value();
        auto repBcast = validate(dBcast);
        REQUIRE(hasIssueWithCode(repBcast, "INVALID_HOST_ADDRESS"));

        // Passing fixtures: /31 (both valid) and /32 (valid)
        auto d31a = makeG1();
        d31a.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("10.0.0.0/31").value();
        d31a.nodes[0].defaultGateway = std::nullopt;
        auto rep31a = validate(d31a);
        REQUIRE(!hasIssueWithCode(rep31a, "INVALID_HOST_ADDRESS"));

        auto d31b = makeG1();
        d31b.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("10.0.0.1/31").value();
        d31b.nodes[0].defaultGateway = std::nullopt;
        auto rep31b = validate(d31b);
        REQUIRE(!hasIssueWithCode(rep31b, "INVALID_HOST_ADDRESS"));

        auto d32 = makeG1();
        d32.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("10.9.9.9/32").value();
        d32.nodes[0].defaultGateway = std::nullopt;
        auto rep32 = validate(d32);
        REQUIRE(!hasIssueWithCode(rep32, "INVALID_HOST_ADDRESS"));
    }

    // Rule 7: Host/Cloud defaultGateway
    {
        auto dUnreach = makeG1();
        dUnreach.nodes[0].defaultGateway = IPv4Address::parse("192.168.2.1").value();
        auto repUnreach = validate(dUnreach);
        REQUIRE(hasIssueWithCode(repUnreach, "GATEWAY_UNREACHABLE"));

        auto dOwn = makeG1();
        dOwn.nodes[0].defaultGateway = IPv4Address::parse("192.168.1.10").value();
        auto repOwn = validate(dOwn);
        REQUIRE(hasIssueWithCode(repOwn, "INVALID_GATEWAY"));

        auto dBcast = makeG1();
        dBcast.nodes[0].defaultGateway = IPv4Address::parse("192.168.1.255").value();
        auto repBcast = validate(dBcast);
        REQUIRE(hasIssueWithCode(repBcast, "INVALID_GATEWAY"));

        // Passing fixture: host 192.168.5.10/23, gateway 192.168.4.1 (valid on-link)
        auto d23 = makeG1();
        d23.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.5.10/23").value();
        d23.nodes[0].defaultGateway = IPv4Address::parse("192.168.4.1").value();
        auto rep23 = validate(d23);
        REQUIRE(!hasIssueWithCode(rep23, "GATEWAY_UNREACHABLE"));
        REQUIRE(!hasIssueWithCode(rep23, "INVALID_GATEWAY"));

        // The trap: host 192.168.1.200/25, gateway 192.168.1.100 -> GATEWAY_UNREACHABLE
        auto dTrap = makeG1();
        dTrap.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.1.200/25").value();
        dTrap.nodes[0].defaultGateway = IPv4Address::parse("192.168.1.100").value();
        auto repTrap = validate(dTrap);
        REQUIRE(hasIssueWithCode(repTrap, "GATEWAY_UNREACHABLE"));
    }

    // Rule 8: Link parameters
    {
        auto dNeg = makeG1();
        dNeg.links[0].cost = -1.0;
        auto repNeg = validate(dNeg);
        REQUIRE(hasIssueWithCode(repNeg, "NEGATIVE_WEIGHT"));

        ValidationOptions optAllowNeg;
        optAllowNeg.allowNegativeCosts = true;
        auto repNegAllowed = validate(dNeg, optAllowNeg);
        REQUIRE(!hasIssueWithCode(repNegAllowed, "NEGATIVE_WEIGHT"));

        auto dNan = makeG1();
        dNan.links[0].cost = std::numeric_limits<double>::quiet_NaN();
        auto repNan = validate(dNan);
        REQUIRE(hasIssueWithCode(repNan, "INVALID_WEIGHT"));

        auto dBw0 = makeG1();
        dBw0.links[0].bandwidthBps = 0ULL;
        auto repBw0 = validate(dBw0);
        REQUIRE(hasIssueWithCode(repBw0, "INVALID_BANDWIDTH"));

        auto dBwAvail = makeG1();
        dBwAvail.links[0].bandwidthBps = 100000000ULL;
        dBwAvail.links[0].availableBandwidthBps = 200000000ULL;
        auto repBwAvail = validate(dBwAvail);
        REQUIRE(hasIssueWithCode(repBwAvail, "INVALID_BANDWIDTH"));

        auto dDelay = makeG1();
        dDelay.links[0].propagationDelayMs = -0.1;
        auto repDelay = validate(dDelay);
        REQUIRE(hasIssueWithCode(repDelay, "INVALID_DELAY"));

        auto dLossNeg = makeG1();
        dLossNeg.links[0].packetLossProbability = -0.1;
        auto repLossNeg = validate(dLossNeg);
        REQUIRE(hasIssueWithCode(repLossNeg, "INVALID_PROBABILITY"));

        auto dLossHigh = makeG1();
        dLossHigh.links[0].packetLossProbability = 1.5;
        auto repLossHigh = validate(dLossHigh);
        REQUIRE(hasIssueWithCode(repLossHigh, "INVALID_PROBABILITY"));

        // Passing fixtures: loss 0 and loss 1
        auto dLoss0 = makeG1();
        dLoss0.links[0].packetLossProbability = 0.0;
        REQUIRE(!hasIssueWithCode(validate(dLoss0), "INVALID_PROBABILITY"));

        auto dLoss1 = makeG1();
        dLoss1.links[0].packetLossProbability = 1.0;
        REQUIRE(!hasIssueWithCode(validate(dLoss1), "INVALID_PROBABILITY"));

        // MTU 67 and 65536 rejected
        auto dMtuLow = makeG1();
        dMtuLow.links[0].mtuBytes = 67;
        REQUIRE(hasIssueWithCode(validate(dMtuLow), "INVALID_MTU"));

        auto dMtuHigh = makeG1();
        dMtuHigh.links[0].mtuBytes = 65536;
        REQUIRE(hasIssueWithCode(validate(dMtuHigh), "INVALID_MTU"));

        // MTU 68 and 65535 valid
        auto dMtu68 = makeG1();
        dMtu68.links[0].mtuBytes = 68;
        REQUIRE(!hasIssueWithCode(validate(dMtu68), "INVALID_MTU"));

        auto dMtu65535 = makeG1();
        dMtu65535.links[0].mtuBytes = 65535;
        REQUIRE(!hasIssueWithCode(validate(dMtu65535), "INVALID_MTU"));
    }

    // Rule 9: MAC rules
    {
        auto dDup = makeG1();
        dDup.nodes[2].interfaces[0].mac = dDup.nodes[0].interfaces[0].mac; // host-b MAC = host-a MAC
        auto repDup = validate(dDup);
        REQUIRE(hasIssueWithCode(repDup, "DUPLICATE_MAC"));

        auto dBcast = makeG1();
        dBcast.nodes[0].interfaces[0].mac = MacAddress::parse("ff:ff:ff:ff:ff:ff").value();
        REQUIRE(hasIssueWithCode(validate(dBcast), "INVALID_MAC"));

        auto dMcast = makeG1();
        dMcast.nodes[0].interfaces[0].mac = MacAddress::parse("01:00:5e:00:00:01").value();
        REQUIRE(hasIssueWithCode(validate(dMcast), "INVALID_MAC"));

        auto dMissing = makeG1();
        dMissing.nodes[2].interfaces[0].mac = std::nullopt;
        REQUIRE(hasIssueWithCode(validate(dMissing), "MISSING_MAC"));

        auto dSwIp = makeG1();
        dSwIp.nodes[1].interfaces[0].ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
        REQUIRE(hasIssueWithCode(validate(dSwIp), "INVALID_CONFIGURATION"));

        auto dSwMac = makeG1();
        dSwMac.nodes[1].interfaces[0].mac = MacAddress::parse("02:00:00:00:00:01").value();
        REQUIRE(hasIssueWithCode(validate(dSwMac), "INVALID_CONFIGURATION"));
    }

    // Rule 10: Services
    {
        auto dPort0 = makeG1();
        dPort0.nodes[0].services.push_back({TransportProtocol::TCP, 0, "test"});
        REQUIRE(hasIssueWithCode(validate(dPort0), "INVALID_PORT"));

        auto dPort65536 = makeG1();
        dPort65536.nodes[0].services.push_back({TransportProtocol::TCP, 0, "test"}); // via validatePort
        REQUIRE(validatePort(65536).isErr());
        REQUIRE(validatePort(0).isErr());
        REQUIRE(validatePort(-1).isErr());
        REQUIRE(validatePort(1).isOk());
        REQUIRE(validatePort(65535).isOk());

        auto dDupSvc = makeG1();
        dDupSvc.nodes[0].services.push_back({TransportProtocol::UDP, 53, "dns1"});
        dDupSvc.nodes[0].services.push_back({TransportProtocol::UDP, 53, "dns2"});
        REQUIRE(hasIssueWithCode(validate(dDupSvc), "DUPLICATE_SERVICE"));

        // (UDP, 53) and (TCP, 53) together are legal
        auto dDiffProto = makeG1();
        dDiffProto.nodes[0].services.push_back({TransportProtocol::UDP, 53, "dns-udp"});
        dDiffProto.nodes[0].services.push_back({TransportProtocol::TCP, 53, "dns-tcp"});
        auto repDiff = validate(dDiffProto);
        REQUIRE(!hasIssueWithCode(repDiff, "DUPLICATE_SERVICE"));

        // Service on sw1 or router rejected
        auto dSwSvc = makeG1();
        dSwSvc.nodes[1].services.push_back({TransportProtocol::TCP, 80, "web"});
        REQUIRE(hasIssueWithCode(validate(dSwSvc), "INVALID_CONFIGURATION"));
    }

    // Rule 11: Structure
    {
        TopologyDraft dEmpty;
        REQUIRE(hasIssueWithCode(validate(dEmpty), "EMPTY_TOPOLOGY"));

        auto dZeroIf = makeG1();
        dZeroIf.nodes[0].interfaces.clear();
        REQUIRE(hasIssueWithCode(validate(dZeroIf), "INVALID_CONFIGURATION"));

        // Router with one interface -> WARNING ROUTER_SINGLE_INTERFACE
        TopologyDraft dR;
        Node r1;
        r1.id = NodeId("r1");
        r1.type = NodeType::Router;
        Interface rIf1;
        rIf1.id = InterfaceId("eth0");
        rIf1.mac = MacAddress::parse("02:00:00:03:00:01").value();
        rIf1.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
        r1.interfaces.push_back(rIf1);
        dR.nodes.push_back(r1);
        auto repR = validate(dR);
        REQUIRE(hasIssueWithCode(repR, "ROUTER_SINGLE_INTERFACE"));
        REQUIRE_EQ(repR.errorCount(), 0); // Warning only!
        REQUIRE_EQ(repR.warningCount(), 1);

        // Router with overlapping subnets -> WARNING OVERLAPPING_SUBNETS
        Interface rIf2;
        rIf2.id = InterfaceId("eth1");
        rIf2.mac = MacAddress::parse("02:00:00:03:00:02").value();
        rIf2.ipv4 = IPv4Subnet::parse("10.0.0.129/25").value();
        r1.interfaces.push_back(rIf2);
        dR.nodes[0] = r1;
        auto repROverlap = validate(dR);
        REQUIRE(hasIssueWithCode(repROverlap, "OVERLAPPING_SUBNETS"));

        // Router with non-overlapping subnets -> NO warning
        r1.interfaces[1].ipv4 = IPv4Subnet::parse("10.0.1.1/24").value();
        dR.nodes[0] = r1;
        auto repRClean = validate(dR);
        REQUIRE(!hasIssueWithCode(repRClean, "OVERLAPPING_SUBNETS"));
        REQUIRE(!hasIssueWithCode(repRClean, "ROUTER_SINGLE_INTERFACE"));
        REQUIRE_EQ(repRClean.warningCount(), 0);
    }

    // Rule 12: Limits
    {
        auto d = makeG1(); // 3 nodes
        ValidationOptions opt;
        REQUIRE_EQ(opt.limits.maxNodes, 200);
        REQUIRE_EQ(opt.limits.maxLinks, 500);
        REQUIRE_EQ(opt.limits.maxInterfacesPerNode, 16);

        opt.limits.maxNodes = 2;
        auto rep = validate(d, opt);
        REQUIRE(hasIssueWithCode(rep, "LIMIT_EXCEEDED"));

        auto d17 = makeG1();
        for (int i = 1; i <= 16; ++i) {
            Interface extra;
            extra.id = InterfaceId("p" + std::to_string(i + 2));
            d17.nodes[1].interfaces.push_back(extra);
        }
        REQUIRE_EQ(d17.nodes[1].interfaces.size(), 18);
        auto rep17 = validate(d17);
        REQUIRE(hasIssueWithCode(rep17, "LIMIT_EXCEEDED"));
    }

    // Rule M2-13: Duplicate interface IP addresses in the same broadcast domain
    {
        // Failing fixture: host-b given same IP as host-a in the same switch domain
        auto d = makeG1();
        d.nodes[2].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "DUPLICATE_IP_IN_DOMAIN"));

        // Passing fixture: host-b has different IP 192.168.1.20 in G1
        auto dPass = makeG1();
        auto repPass = validate(dPass);
        REQUIRE(!hasIssueWithCode(repPass, "DUPLICATE_IP_IN_DOMAIN"));

        // Passing fixture: identical IP on two hosts in DIFFERENT broadcast domains (behind router)
        TopologyDraft dMultiDomain;
        Node h1; h1.id = NodeId("h1"); h1.type = NodeType::Host;
        Interface ifH1; ifH1.id = InterfaceId("eth0"); ifH1.mac = MacAddress::parse("02:00:00:01:00:01").value();
        ifH1.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
        h1.interfaces.push_back(ifH1);

        Node r; r.id = NodeId("r1"); r.type = NodeType::Router;
        Interface ifR0; ifR0.id = InterfaceId("eth0"); ifR0.mac = MacAddress::parse("02:00:00:02:00:01").value();
        ifR0.ipv4 = IPv4Subnet::parse("192.168.1.1/24").value();
        Interface ifR1; ifR1.id = InterfaceId("eth1"); ifR1.mac = MacAddress::parse("02:00:00:02:00:02").value();
        ifR1.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
        r.interfaces.push_back(ifR0);
        r.interfaces.push_back(ifR1);

        Node h2; h2.id = NodeId("h2"); h2.type = NodeType::Host;
        Interface ifH2; ifH2.id = InterfaceId("eth0"); ifH2.mac = MacAddress::parse("02:00:00:03:00:01").value();
        // Same IP 192.168.1.10, but in a completely different domain!
        ifH2.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
        h2.interfaces.push_back(ifH2);

        Link link1; link1.id = LinkId("l1"); link1.a = {NodeId("h1"), InterfaceId("eth0")}; link1.b = {NodeId("r1"), InterfaceId("eth0")};
        Link link2; link2.id = LinkId("l2"); link2.a = {NodeId("h2"), InterfaceId("eth0")}; link2.b = {NodeId("r1"), InterfaceId("eth1")};
        // Note: link2 has subnet mismatch if both have IP, so make ifR1 192.168.1.1/24 for link2
        ifR1.ipv4 = IPv4Subnet::parse("192.168.1.1/24").value();
        r.interfaces[1] = ifR1;

        dMultiDomain.nodes = {h1, r, h2};
        dMultiDomain.links = {link1, link2};
        auto repMulti = validate(dMultiDomain);
        // Duplicate IP across different domains must NOT produce DUPLICATE_IP_IN_DOMAIN
        REQUIRE(!hasIssueWithCode(repMulti, "DUPLICATE_IP_IN_DOMAIN"));
    }

    // Rule M2-16: Direct link subnet mismatch
    {
        // Failing fixture: direct link between two routed interfaces in different subnets
        TopologyDraft d;
        Node hA; hA.id = NodeId("h-a"); hA.type = NodeType::Host;
        Interface ifA; ifA.id = InterfaceId("eth0"); ifA.mac = MacAddress::parse("02:00:00:01:00:01").value();
        ifA.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
        hA.interfaces.push_back(ifA);

        Node hB; hB.id = NodeId("h-b"); hB.type = NodeType::Host;
        Interface ifB; ifB.id = InterfaceId("eth0"); ifB.mac = MacAddress::parse("02:00:00:02:00:01").value();
        ifB.ipv4 = IPv4Subnet::parse("10.0.0.20/24").value(); // Different subnet!
        hB.interfaces.push_back(ifB);

        Link l; l.id = LinkId("l1"); l.a = {NodeId("h-a"), InterfaceId("eth0")}; l.b = {NodeId("h-b"), InterfaceId("eth0")};
        d.nodes = {hA, hB};
        d.links = {l};

        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "SUBNET_MISMATCH"));

        // Passing fixture: direct link between two routed interfaces in the SAME subnet
        d.nodes[1].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.1.20/24").value();
        auto repClean = validate(d);
        REQUIRE(!hasIssueWithCode(repClean, "SUBNET_MISMATCH"));
    }

    // Rule M2-20: Inactive component reporting (warning)
    {
        // Inactive node
        auto dNodeDown = makeG1();
        dNodeDown.nodes[0].adminState = AdminState::Down;
        auto repNode = validate(dNodeDown);
        REQUIRE(hasIssueWithCode(repNode, "INACTIVE_COMPONENT"));

        // Inactive interface
        auto dIfaceDown = makeG1();
        dIfaceDown.nodes[0].interfaces[0].adminState = AdminState::Down;
        auto repIface = validate(dIfaceDown);
        REQUIRE(hasIssueWithCode(repIface, "INACTIVE_COMPONENT"));

        // Inactive link
        auto dLinkDown = makeG1();
        dLinkDown.links[0].adminState = AdminState::Down;
        auto repLink = validate(dLinkDown);
        REQUIRE(hasIssueWithCode(repLink, "INACTIVE_COMPONENT"));

        // Passing fixture: G1 baseline has 0 inactive components
        auto dUp = makeG1();
        auto repUp = validate(dUp);
        REQUIRE(!hasIssueWithCode(repUp, "INACTIVE_COMPONENT"));
    }

    // Rule M2-22: Disconnected topology check (warning)
    {
        // Warning fixture: two separate clusters
        // Cluster 1: h1 <-> sw1
        // Cluster 2: h2 <-> sw2
        TopologyDraft d;
        Node h1; h1.id = NodeId("h1"); h1.type = NodeType::Host;
        Interface if1; if1.id = InterfaceId("eth0"); if1.mac = MacAddress::parse("02:00:00:01:00:01").value();
        if1.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
        h1.interfaces.push_back(if1);

        Node sw1; sw1.id = NodeId("sw1"); sw1.type = NodeType::Switch;
        Interface p1; p1.id = InterfaceId("p1"); sw1.interfaces.push_back(p1);

        Node h2; h2.id = NodeId("h2"); h2.type = NodeType::Host;
        Interface if2; if2.id = InterfaceId("eth0"); if2.mac = MacAddress::parse("02:00:00:02:00:01").value();
        if2.ipv4 = IPv4Subnet::parse("10.0.0.10/24").value();
        h2.interfaces.push_back(if2);

        Node sw2; sw2.id = NodeId("sw2"); sw2.type = NodeType::Switch;
        Interface p2; p2.id = InterfaceId("p2"); sw2.interfaces.push_back(p2);

        Link l1; l1.id = LinkId("l1"); l1.a = {NodeId("h1"), InterfaceId("eth0")}; l1.b = {NodeId("sw1"), InterfaceId("p1")};
        Link l2; l2.id = LinkId("l2"); l2.a = {NodeId("h2"), InterfaceId("eth0")}; l2.b = {NodeId("sw2"), InterfaceId("p2")};

        d.nodes = {h1, sw1, h2, sw2};
        d.links = {l1, l2};

        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "DISCONNECTED_TOPOLOGY"));

        // Passing fixture: G1 baseline is a connected graph
        auto dG1 = makeG1();
        auto repG1 = validate(dG1);
        REQUIRE(!hasIssueWithCode(repG1, "DISCONNECTED_TOPOLOGY"));
    }

    // Rule M2-23: L2 loop detected among switches (warning)
    {
        // Warning fixture: two switches connected with two parallel links
        TopologyDraft d;
        Node sw1; sw1.id = NodeId("sw1"); sw1.type = NodeType::Switch;
        Interface p1; p1.id = InterfaceId("p1"); sw1.interfaces.push_back(p1);
        Interface p2; p2.id = InterfaceId("p2"); sw1.interfaces.push_back(p2);

        Node sw2; sw2.id = NodeId("sw2"); sw2.type = NodeType::Switch;
        Interface q1; q1.id = InterfaceId("p1"); sw2.interfaces.push_back(q1);
        Interface q2; q2.id = InterfaceId("p2"); sw2.interfaces.push_back(q2);

        Link l1; l1.id = LinkId("l1"); l1.a = {NodeId("sw1"), InterfaceId("p1")}; l1.b = {NodeId("sw2"), InterfaceId("p1")};
        Link l2; l2.id = LinkId("l2"); l2.a = {NodeId("sw1"), InterfaceId("p2")}; l2.b = {NodeId("sw2"), InterfaceId("p2")};

        d.nodes = {sw1, sw2};
        d.links = {l1, l2};

        auto rep = validate(d);
        REQUIRE(hasIssueWithCode(rep, "L2_LOOP_DETECTED"));

        // Passing fixture: G1 baseline with single switch
        auto dG1 = makeG1();
        auto repG1 = validate(dG1);
        REQUIRE(!hasIssueWithCode(repG1, "L2_LOOP_DETECTED"));
    }

    // Rule 1e: MIXED_SUBNETS_IN_DOMAIN (warning)
    {
        // Fixture 1: sw1 with host 192.168.1.10/24 and host 192.168.1.20/24 -> no warning
        auto dClean = makeG1();
        auto repClean = validate(dClean);
        REQUIRE(!hasIssueWithCode(repClean, "MIXED_SUBNETS_IN_DOMAIN"));

        // Fixture 2: sw1 with host 192.168.1.10/24 and host 192.168.2.20/24 -> exactly 1 warning, path names the domain, hint says hosts need a router
        auto dDiffNet = makeG1();
        dDiffNet.nodes[2].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.2.20/24").value();
        auto repDiffNet = validate(dDiffNet);
        REQUIRE(hasIssueWithCode(repDiffNet, "MIXED_SUBNETS_IN_DOMAIN"));
        int warnCount = 0;
        for (const auto& issue : repDiffNet.issues) {
            if (issue.code == "MIXED_SUBNETS_IN_DOMAIN") {
                warnCount++;
                REQUIRE(issue.isWarning());
                REQUIRE(issue.path.rfind("domains/", 0) == 0); // Path names the domain
                REQUIRE(issue.hint == "Hosts in different subnets require a router to communicate");
            }
        }
        REQUIRE_EQ(warnCount, 1);

        // Fixture 3: sw1 with 192.168.1.10/24 and 192.168.1.20/25 -> 1 warning
        auto dDiffPrefix = makeG1();
        dDiffPrefix.nodes[2].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.1.20/25").value();
        auto repDiffPrefix = validate(dDiffPrefix);
        REQUIRE(hasIssueWithCode(repDiffPrefix, "MIXED_SUBNETS_IN_DOMAIN"));
        int warnCountPrefix = 0;
        for (const auto& issue : repDiffPrefix.issues) {
            if (issue.code == "MIXED_SUBNETS_IN_DOMAIN") {
                warnCountPrefix++;
                REQUIRE(issue.isWarning());
            }
        }
        REQUIRE_EQ(warnCountPrefix, 1);
    }
}

TEST_CASE("T-2-17: MAC unique across the whole topology") {
    // Two nodes host-a and host-c that are NOT linked together
    TopologyDraft d;
    Node hostA;
    hostA.id = NodeId("host-a");
    hostA.type = NodeType::Host;
    Interface ifA;
    ifA.id = InterfaceId("eth0");
    ifA.mac = MacAddress::parse("02:00:00:00:00:AA").value();
    ifA.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
    hostA.interfaces.push_back(ifA);
    d.nodes.push_back(hostA);

    Node hostC;
    hostC.id = NodeId("host-c");
    hostC.type = NodeType::Host;
    Interface ifC;
    ifC.id = InterfaceId("eth0");
    ifC.mac = MacAddress::parse("02:00:00:00:00:AA").value(); // Identical MAC!
    ifC.ipv4 = IPv4Subnet::parse("10.0.0.10/24").value();
    hostC.interfaces.push_back(ifC);
    d.nodes.push_back(hostC);

    // No link between host-a and host-c: validation must still reject duplicate MAC
    auto rep = validate(d);
    REQUIRE(hasIssueWithCode(rep, "DUPLICATE_MAC"));

    // Giving host-c a unique MAC passes
    d.nodes[1].interfaces[0].mac = MacAddress::parse("02:00:00:00:00:BB").value();
    auto repClean = validate(d);
    REQUIRE(!hasIssueWithCode(repClean, "DUPLICATE_MAC"));
}

TEST_CASE("T-2-21: Port validation accepts 1..65535 and rejects 0, 65536, -1") {
    REQUIRE(validatePort(0).isErr());
    REQUIRE_EQ(validatePort(0).error().code, ErrorCode::InvalidPort);

    REQUIRE(validatePort(65536).isErr());
    REQUIRE_EQ(validatePort(65536).error().code, ErrorCode::InvalidPort);

    REQUIRE(validatePort(-1).isErr());
    REQUIRE_EQ(validatePort(-1).error().code, ErrorCode::InvalidPort);

    auto res1 = validatePort(1);
    REQUIRE(res1.isOk());
    REQUIRE_EQ(res1.value(), 1);

    auto res65535 = validatePort(65535);
    REQUIRE(res65535.isOk());
    REQUIRE_EQ(res65535.value(), 65535);
}

TEST_CASE("T-2-23: Duplicate service rejected on host") {
    TopologyDraft d = makeG1();
    d.nodes[0].services.push_back({TransportProtocol::UDP, 53, "dns"});
    d.nodes[0].services.push_back({TransportProtocol::UDP, 53, "duplicate-dns"});

    auto rep = validate(d);
    REQUIRE(hasIssueWithCode(rep, "DUPLICATE_SERVICE"));

    // Different protocols on same port are valid
    d.nodes[0].services[1].protocol = TransportProtocol::TCP;
    auto repClean = validate(d);
    REQUIRE(!hasIssueWithCode(repClean, "DUPLICATE_SERVICE"));
}

TEST_CASE("T-2-25: Topology model basics, effectiveMtu, canonical order, hash vectors and freeze") {
    // 1. effectiveMtu: link 1500, host-a iface override 1400, other end none -> 1400
    {
        Link l;
        l.id = LinkId("l1");
        l.mtuBytes = 1500;
        REQUIRE_EQ(effectiveMtu(l, 1400, std::nullopt), 1400);
        REQUIRE_EQ(effectiveMtu(l, std::nullopt, 1450), 1450);
        REQUIRE_EQ(effectiveMtu(l, 1420, 1380), 1380);
        REQUIRE_EQ(effectiveMtu(l, std::nullopt, std::nullopt), 1500);

        auto d = makeG1();
        d.nodes[0].interfaces[0].mtuBytes = 1400;
        REQUIRE_EQ(effectiveMtu(d.links[0], d), 1400);
    }

    // 2. FNV-1a 64-bit test vectors:
    // ""       -> cbf29ce484222325
    // "a"      -> af63dc4c8601ec8c
    // "foobar" -> 85944171f73967e8
    {
        REQUIRE_EQ(fnv1a64Hex(""), "cbf29ce484222325");
        REQUIRE_EQ(fnv1a64Hex("a"), "af63dc4c8601ec8c");
        REQUIRE_EQ(fnv1a64Hex("foobar"), "85944171f73967e8");
    }

    // 3. assignMissingMacs:
    // nodes host-a, host-b, r1, each one interface eth0, all missing a MAC
    // -> 02:00:00:01:00:01, 02:00:00:02:00:01, 02:00:00:03:00:01
    {
        TopologyDraft d;
        Node ha; ha.id = NodeId("host-a"); ha.type = NodeType::Host;
        Interface ha_if; ha_if.id = InterfaceId("eth0"); ha.interfaces.push_back(ha_if);
        d.nodes.push_back(ha);

        Node hb; hb.id = NodeId("host-b"); hb.type = NodeType::Host;
        Interface hb_if; hb_if.id = InterfaceId("eth0"); hb.interfaces.push_back(hb_if);
        d.nodes.push_back(hb);

        Node r1; r1.id = NodeId("r1"); r1.type = NodeType::Router;
        Interface r1_if; r1_if.id = InterfaceId("eth0"); r1.interfaces.push_back(r1_if);
        d.nodes.push_back(r1);

        REQUIRE(assignMissingMacs(d).isOk());
        REQUIRE_EQ(d.nodes[0].interfaces[0].mac->toString(), "02:00:00:01:00:01");
        REQUIRE_EQ(d.nodes[1].interfaces[0].mac->toString(), "02:00:00:02:00:01");
        REQUIRE_EQ(d.nodes[2].interfaces[0].mac->toString(), "02:00:00:03:00:01");

        // Running assignMissingMacs twice changes nothing the second time
        REQUIRE(assignMissingMacs(d).isOk());
        REQUIRE_EQ(d.nodes[0].interfaces[0].mac->toString(), "02:00:00:01:00:01");
        REQUIRE_EQ(d.nodes[1].interfaces[0].mac->toString(), "02:00:00:02:00:01");
        REQUIRE_EQ(d.nodes[2].interfaces[0].mac->toString(), "02:00:00:03:00:01");
    }

    // 4. assignMissingMacs with explicit MAC on host-b:
    // same, but host-b already has explicit MAC 02:00:00:01:00:01
    // -> host-a gets 02:00:00:02:00:01; host-b keeps its MAC; r1 gets 02:00:00:03:00:01
    {
        TopologyDraft d;
        Node ha; ha.id = NodeId("host-a"); ha.type = NodeType::Host;
        Interface ha_if; ha_if.id = InterfaceId("eth0"); ha.interfaces.push_back(ha_if);
        d.nodes.push_back(ha);

        Node hb; hb.id = NodeId("host-b"); hb.type = NodeType::Host;
        Interface hb_if; hb_if.id = InterfaceId("eth0");
        hb_if.mac = MacAddress::parse("02:00:00:01:00:01").value(); // Explicit!
        hb.interfaces.push_back(hb_if);
        d.nodes.push_back(hb);

        Node r1; r1.id = NodeId("r1"); r1.type = NodeType::Router;
        Interface r1_if; r1_if.id = InterfaceId("eth0"); r1.interfaces.push_back(r1_if);
        d.nodes.push_back(r1);

        REQUIRE(assignMissingMacs(d).isOk());
        REQUIRE_EQ(d.nodes[0].interfaces[0].mac->toString(), "02:00:00:02:00:01");
        REQUIRE_EQ(d.nodes[1].interfaces[0].mac->toString(), "02:00:00:01:00:01"); // Preserved
        REQUIRE_EQ(d.nodes[2].interfaces[0].mac->toString(), "02:00:00:03:00:01");
    }

    // 5. Freeze fails on errors and succeeds on G1
    {
        auto dErr = makeG1();
        dErr.nodes.push_back(dErr.nodes[0]); // Duplicate node host-a
        auto resErr = TopologySnapshot::freeze(dErr);
        REQUIRE(resErr.isErr());

        auto dG1 = makeG1();
        auto resG1 = TopologySnapshot::freeze(dG1);
        REQUIRE(resG1.isOk());
        const auto& snap = resG1.value();
        REQUIRE(!snap.canonicalText().empty());
        REQUIRE_EQ(snap.contentHash().size(), 16);
    }

    // 6. Snapshot hash properties:
    // - Inserting nodes/links in a different order gives the same hash
    // - Changing any single field changes it
    // - Two separate freezes give the same hash
    {
        auto dOriginal = makeG1();
        auto snap1 = TopologySnapshot::freeze(dOriginal).value();

        // Reversed nodes and links in draft
        auto dReordered = makeG1();
        std::reverse(dReordered.nodes.begin(), dReordered.nodes.end());
        std::reverse(dReordered.links.begin(), dReordered.links.end());
        auto snap2 = TopologySnapshot::freeze(dReordered).value();

        REQUIRE_EQ(snap1.contentHash(), snap2.contentHash());
        REQUIRE_EQ(snap1.canonicalText(), snap2.canonicalText());

        // Changing a single field changes the hash
        auto dModCost = makeG1();
        dModCost.links[0].cost = 2.0;
        auto snapModCost = TopologySnapshot::freeze(dModCost).value();
        REQUIRE(snap1.contentHash() != snapModCost.contentHash());

        auto dModMac = makeG1();
        dModMac.nodes[0].interfaces[0].mac = MacAddress::parse("02:00:00:01:00:99").value();
        auto snapModMac = TopologySnapshot::freeze(dModMac).value();
        REQUIRE(snap1.contentHash() != snapModMac.contentHash());

        auto dModIp = makeG1();
        dModIp.nodes[0].interfaces[0].ipv4 = IPv4Subnet::parse("192.168.1.11/24").value();
        auto snapModIp = TopologySnapshot::freeze(dModIp).value();
        REQUIRE(snap1.contentHash() != snapModIp.contentHash());
    }
}

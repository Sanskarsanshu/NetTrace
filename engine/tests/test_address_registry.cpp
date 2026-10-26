#include "test_framework.hpp"
#include "nettrace/model/address_registry.hpp"
#include "nettrace/model/topology.hpp"
#include "nettrace/model/validate.hpp"

using namespace nettrace;

TEST_CASE("T-2-18: Registry lookup returns owner inside domain and NOTHING for IP in another domain") {
    // Topology setup:
    // Domain 1: host-a1 (192.168.1.10/24, MAC 02:00:00:01:00:01) <-> r1.eth0 (192.168.1.1/24, MAC 02:00:00:02:00:01)
    // Domain 2: host-b1 (10.0.0.10/24, MAC 02:00:00:03:00:01) <-> r1.eth1 (10.0.0.1/24, MAC 02:00:00:02:00:02)
    TopologyDraft draft;

    // host-a1
    Node hostA;
    hostA.id = NodeId("host-a1");
    hostA.name = "Host A1";
    hostA.type = NodeType::Host;
    Interface ifA;
    ifA.id = InterfaceId("eth0");
    ifA.name = "eth0";
    ifA.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value();
    ifA.mac = MacAddress::parse("02:00:00:01:00:01").value();
    hostA.interfaces.push_back(ifA);
    hostA.defaultGateway = IPv4Address::parse("192.168.1.1").value();
    draft.nodes.push_back(hostA);

    // r1
    Node r1;
    r1.id = NodeId("r1");
    r1.name = "Router 1";
    r1.type = NodeType::Router;
    Interface ifR0;
    ifR0.id = InterfaceId("eth0");
    ifR0.name = "eth0";
    ifR0.ipv4 = IPv4Subnet::parse("192.168.1.1/24").value();
    ifR0.mac = MacAddress::parse("02:00:00:02:00:01").value();
    Interface ifR1;
    ifR1.id = InterfaceId("eth1");
    ifR1.name = "eth1";
    ifR1.ipv4 = IPv4Subnet::parse("10.0.0.1/24").value();
    ifR1.mac = MacAddress::parse("02:00:00:02:00:02").value();
    r1.interfaces.push_back(ifR0);
    r1.interfaces.push_back(ifR1);
    draft.nodes.push_back(r1);

    // host-b1
    Node hostB;
    hostB.id = NodeId("host-b1");
    hostB.name = "Host B1";
    hostB.type = NodeType::Host;
    Interface ifB;
    ifB.id = InterfaceId("eth0");
    ifB.name = "eth0";
    ifB.ipv4 = IPv4Subnet::parse("10.0.0.10/24").value();
    ifB.mac = MacAddress::parse("02:00:00:03:00:01").value();
    hostB.interfaces.push_back(ifB);
    hostB.defaultGateway = IPv4Address::parse("10.0.0.1").value();
    draft.nodes.push_back(hostB);

    // Link l1: host-a1.eth0 <-> r1.eth0
    Link l1;
    l1.id = LinkId("l1");
    l1.a = {NodeId("host-a1"), InterfaceId("eth0")};
    l1.b = {NodeId("r1"), InterfaceId("eth0")};
    l1.cost = 1.0;
    l1.bandwidthBps = 100000000;
    l1.propagationDelayMs = 1.0;
    l1.packetLossProbability = 0.0;
    l1.mtuBytes = 1500;
    draft.links.push_back(l1);

    // Link l2: host-b1.eth0 <-> r1.eth1
    Link l2;
    l2.id = LinkId("l2");
    l2.a = {NodeId("host-b1"), InterfaceId("eth0")};
    l2.b = {NodeId("r1"), InterfaceId("eth1")};
    l2.cost = 1.0;
    l2.bandwidthBps = 100000000;
    l2.propagationDelayMs = 1.0;
    l2.packetLossProbability = 0.0;
    l2.mtuBytes = 1500;
    draft.links.push_back(l2);

    auto freezeRes = TopologySnapshot::freeze(draft);
    REQUIRE(freezeRes.isOk());
    const auto& snapshot = freezeRes.value();

    // Build AddressRegistry
    auto regRes = AddressRegistry::build(snapshot);
    REQUIRE(regRes.isOk());
    const auto& registry = regRes.value();

    // Verify 4 registered entries (host-a1.eth0, r1.eth0, host-b1.eth0, r1.eth1)
    REQUIRE_EQ(registry.size(), static_cast<size_t>(4));

    // Get broadcast domain IDs for host-a1 and host-b1
    auto domainAOpt = registry.getDomainForInterface(NodeId("host-a1"), InterfaceId("eth0"));
    auto domainBOpt = registry.getDomainForInterface(NodeId("host-b1"), InterfaceId("eth0"));
    REQUIRE(domainAOpt.has_value());
    REQUIRE(domainBOpt.has_value());

    std::string domain1 = *domainAOpt;
    std::string domain2 = *domainBOpt;

    // The two domains must be distinct (separated by router r1)
    REQUIRE(domain1 != domain2);

    // Verify r1's interfaces belong to the respective domains
    auto domainR0Opt = registry.getDomainForInterface(NodeId("r1"), InterfaceId("eth0"));
    auto domainR1Opt = registry.getDomainForInterface(NodeId("r1"), InterfaceId("eth1"));
    REQUIRE(domainR0Opt.has_value());
    REQUIRE(domainR1Opt.has_value());
    REQUIRE_EQ(*domainR0Opt, domain1);
    REQUIRE_EQ(*domainR1Opt, domain2);

    // 1. Scoped lookup in Domain 1:
    // Looking up 192.168.1.10 (Host A1) in Domain 1 -> SUCCEEDS
    auto ipA1 = IPv4Address::parse("192.168.1.10").value();
    auto ownerA1 = registry.findOwnerInDomain(ipA1, domain1);
    REQUIRE(ownerA1.has_value());
    REQUIRE_EQ(ownerA1->nodeId.str(), "host-a1");
    REQUIRE_EQ(ownerA1->interfaceId.str(), "eth0");
    REQUIRE_EQ(ownerA1->mac.toString(), "02:00:00:01:00:01");

    // Looking up 192.168.1.1 (Router r1 eth0) in Domain 1 -> SUCCEEDS
    auto ipR1 = IPv4Address::parse("192.168.1.1").value();
    auto ownerR1 = registry.findOwnerInDomain(ipR1, domain1);
    REQUIRE(ownerR1.has_value());
    REQUIRE_EQ(ownerR1->nodeId.str(), "r1");
    REQUIRE_EQ(ownerR1->interfaceId.str(), "eth0");

    // CRITICAL REQUIREMENT: Looking up 10.0.0.10 (which belongs to Domain 2) in Domain 1 -> MUST RETURN NOTHING!
    auto ipB1 = IPv4Address::parse("10.0.0.10").value();
    auto ownerCross1 = registry.findOwnerInDomain(ipB1, domain1);
    REQUIRE_FALSE(ownerCross1.has_value()); // NOTHING!

    // Looking up 192.168.1.10 (which belongs to Domain 1) in Domain 2 -> MUST RETURN NOTHING!
    auto ownerCross2 = registry.findOwnerInDomain(ipA1, domain2);
    REQUIRE_FALSE(ownerCross2.has_value()); // NOTHING!

    // 2. Scoped lookup in Domain 2:
    // Looking up 10.0.0.10 in Domain 2 -> SUCCEEDS
    auto ownerB1 = registry.findOwnerInDomain(ipB1, domain2);
    REQUIRE(ownerB1.has_value());
    REQUIRE_EQ(ownerB1->nodeId.str(), "host-b1");
    REQUIRE_EQ(ownerB1->interfaceId.str(), "eth0");

    // 3. Duplicate IP in same domain detection
    TopologyDraft dupDraftSimple;
    Node n1; n1.id = NodeId("n1"); n1.type = NodeType::Host;
    Interface i1; i1.id = InterfaceId("eth0"); i1.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value(); i1.mac = MacAddress::parse("02:00:00:01:00:01").value();
    n1.interfaces.push_back(i1);

    Node n2; n2.id = NodeId("n2"); n2.type = NodeType::Host;
    Interface i2; i2.id = InterfaceId("eth0"); i2.ipv4 = IPv4Subnet::parse("192.168.1.10/24").value(); i2.mac = MacAddress::parse("02:00:00:02:00:01").value();
    n2.interfaces.push_back(i2);

    Link lP2p;
    lP2p.id = LinkId("lp2p");
    lP2p.a = {NodeId("n1"), InterfaceId("eth0")};
    lP2p.b = {NodeId("n2"), InterfaceId("eth0")};
    dupDraftSimple.nodes = {n1, n2};
    dupDraftSimple.links = {lP2p};

    auto dupRes = AddressRegistry::build(dupDraftSimple);
    REQUIRE(dupRes.isErr());
    REQUIRE_EQ(static_cast<int>(dupRes.error().code), static_cast<int>(ErrorCode::DuplicateIpInDomain));
}

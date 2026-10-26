#include "test_framework.hpp"
#include "nettrace/model/mac_table.hpp"

using namespace nettrace;

TEST_CASE("T-2-12: Switch learn/forward/flood/move logic; never flood to ingress; never flood to down ports") {
    MacTable table;

    // 1. Initial state
    REQUIRE(table.empty());
    REQUIRE_EQ(table.size(), static_cast<size_t>(0));

    // Lookup unknown unicast MAC -> nullopt
    auto macARes = MacAddress::parse("02:00:00:01:00:01");
    REQUIRE(macARes.isOk());
    MacAddress macA = macARes.value();
    REQUIRE_FALSE(table.lookup(macA).has_value());

    // 2. Learn MAC A on port p1
    InterfaceId port1("p1");
    MacLearnResult res1 = table.learn(macA, port1);
    REQUIRE_EQ(static_cast<int>(res1), static_cast<int>(MacLearnResult::Learned));
    REQUIRE_EQ(table.size(), static_cast<size_t>(1));
    REQUIRE_FALSE(table.empty());

    // Lookup MAC A -> port p1
    auto lookup1 = table.lookup(macA);
    REQUIRE(lookup1.has_value());
    REQUIRE_EQ(*lookup1, port1);

    // 3. Re-learn MAC A on port p1 (refresh) -> Unchanged
    MacLearnResult res2 = table.learn(macA, port1);
    REQUIRE_EQ(static_cast<int>(res2), static_cast<int>(MacLearnResult::Unchanged));
    REQUIRE_EQ(table.size(), static_cast<size_t>(1));

    // 4. Move MAC A to port p2 -> Moved
    InterfaceId port2("p2");
    MacLearnResult res3 = table.learn(macA, port2);
    REQUIRE_EQ(static_cast<int>(res3), static_cast<int>(MacLearnResult::Moved));
    REQUIRE_EQ(table.size(), static_cast<size_t>(1));

    // Lookup MAC A -> port p2
    auto lookup2 = table.lookup(macA);
    REQUIRE(lookup2.has_value());
    REQUIRE_EQ(*lookup2, port2);

    // 5. Broadcast and Multicast lookup always return nullopt (never unicast forwarded)
    auto bcastMac = MacAddress::parse("ff:ff:ff:ff:ff:ff").value();
    REQUIRE_FALSE(table.lookup(bcastMac).has_value());

    auto mcastMac = MacAddress::parse("01:00:5e:00:00:01").value();
    REQUIRE_FALSE(table.lookup(mcastMac).has_value());

    // 6. Flood set computation:
    // Switch has 4 ports: p1 (Up), p2 (Up), p3 (Down), p4 (Up)
    std::vector<Interface> allPorts;
    Interface if1; if1.id = InterfaceId("p1"); if1.name = "Port 1"; if1.adminState = AdminState::Up;
    Interface if2; if2.id = InterfaceId("p2"); if2.name = "Port 2"; if2.adminState = AdminState::Up;
    Interface if3; if3.id = InterfaceId("p3"); if3.name = "Port 3"; if3.adminState = AdminState::Down; // Down!
    Interface if4; if4.id = InterfaceId("p4"); if4.name = "Port 4"; if4.adminState = AdminState::Up;
    allPorts.push_back(if1);
    allPorts.push_back(if2);
    allPorts.push_back(if3);
    allPorts.push_back(if4);

    // Flood for frame ingressing on p1:
    // Must exclude ingress port (p1) AND down port (p3)
    auto floodFromP1 = table.getFloodPorts(port1, allPorts);
    REQUIRE_EQ(floodFromP1.size(), static_cast<size_t>(2));
    REQUIRE_EQ(floodFromP1[0].str(), "p2");
    REQUIRE_EQ(floodFromP1[1].str(), "p4");

    // Flood for frame ingressing on p2:
    // Must exclude ingress port (p2) AND down port (p3)
    auto floodFromP2 = table.getFloodPorts(port2, allPorts);
    REQUIRE_EQ(floodFromP2.size(), static_cast<size_t>(2));
    REQUIRE_EQ(floodFromP2[0].str(), "p1");
    REQUIRE_EQ(floodFromP2[1].str(), "p4");

    // 7. Remove and Clear
    REQUIRE(table.remove(macA));
    REQUIRE_EQ(table.size(), static_cast<size_t>(0));
    REQUIRE_FALSE(table.lookup(macA).has_value());

    table.learn(macA, port1);
    auto macB = MacAddress::parse("02:00:00:02:00:01").value();
    table.learn(macB, port2);
    REQUIRE_EQ(table.size(), static_cast<size_t>(2));

    table.clear();
    REQUIRE_EQ(table.size(), static_cast<size_t>(0));
    REQUIRE(table.empty());
}

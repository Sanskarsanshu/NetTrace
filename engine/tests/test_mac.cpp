#include "test_framework.hpp"
#include "nettrace/model/mac.hpp"
#include <set>

using namespace nettrace;

TEST_CASE("T-2-24: MAC parse, format, predicates, generator, and uniqueness") {
        // ACCEPT as interface MAC
        {
            auto res1 = MacAddress::parse("02:00:00:00:00:01");
            EXPECT_TRUE(res1.isOk());
            EXPECT_EQ(res1.value().toString(), "02:00:00:00:00:01");
            EXPECT_TRUE(res1.value().isValidInterfaceMac());
            EXPECT_TRUE(res1.value().isUnicast());
            EXPECT_FALSE(res1.value().isMulticast());
            EXPECT_TRUE(res1.value().isLocallyAdministered());
            EXPECT_FALSE(res1.value().isBroadcast());
            EXPECT_FALSE(res1.value().isAllZero());
        }

        {
            // AA:BB:CC:DD:EE:00 is valid hex, canonical output is lowercase
            auto res2 = MacAddress::parse("AA:BB:CC:DD:EE:00");
            EXPECT_TRUE(res2.isOk());
            EXPECT_EQ(res2.value().toString(), "aa:bb:cc:dd:ee:00");
            EXPECT_TRUE(res2.value().isUnicast()); // 0xAA has low bit 0 -> unicast
            EXPECT_FALSE(res2.value().isMulticast());
            EXPECT_TRUE(res2.value().isLocallyAdministered()); // 0xAA has second bit 1
            EXPECT_TRUE(res2.value().isValidInterfaceMac());
        }

        // REJECT as interface MAC
        {
            // Broadcast ff:ff:ff:ff:ff:ff
            auto bcast = MacAddress::parse("ff:ff:ff:ff:ff:ff").value();
            EXPECT_TRUE(bcast.isBroadcast());
            EXPECT_TRUE(bcast.isMulticast());
            EXPECT_FALSE(bcast.isUnicast());
            EXPECT_FALSE(bcast.isValidInterfaceMac());

            // Multicast 01:00:5e:00:00:01
            auto mcast = MacAddress::parse("01:00:5e:00:00:01").value();
            EXPECT_TRUE(mcast.isMulticast());
            EXPECT_FALSE(mcast.isUnicast());
            EXPECT_FALSE(mcast.isBroadcast());
            EXPECT_FALSE(mcast.isValidInterfaceMac());

            // All-zero 00:00:00:00:00:00
            auto zero = MacAddress::parse("00:00:00:00:00:00").value();
            EXPECT_TRUE(zero.isAllZero());
            EXPECT_FALSE(zero.isValidInterfaceMac());
        }

        // MAC PARSE REJECT
        const char* parseRejects[] = {
            "02-00-00-00-00-01",
            "02:00:00:00:00",
            "02:00:00:00:00:0g",
            "02:00:00:00:00:001"
        };
        for (const char* badMac : parseRejects) {
            auto res = MacAddress::parse(badMac);
            EXPECT_FALSE(res.isOk());
            EXPECT_EQ(res.error().code, ErrorCode::InvalidMac);
        }

        // Deterministic generator tests
        {
            // generateMac(1, 1) = 02:00:00:01:00:01
            auto g1 = generateMac(1, 1);
            EXPECT_TRUE(g1.isOk());
            EXPECT_EQ(g1.value().toString(), "02:00:00:01:00:01");

            // generateMac(258, 3) = 02:00:01:02:00:03 (258 = 0x0102)
            auto g2 = generateMac(258, 3);
            EXPECT_TRUE(g2.isOk());
            EXPECT_EQ(g2.value().toString(), "02:00:01:02:00:03");

            // generateMac(65535, 65535) = 02:00:ff:ff:ff:ff (valid unicast, not broadcast)
            auto g3 = generateMac(65535, 65535);
            EXPECT_TRUE(g3.isOk());
            EXPECT_EQ(g3.value().toString(), "02:00:ff:ff:ff:ff");
            EXPECT_TRUE(g3.value().isValidInterfaceMac());
            EXPECT_FALSE(g3.value().isBroadcast());

            // generateMac(65536, 0) and generateMac(0, 65536) -> error
            auto g4 = generateMac(65536, 0);
            EXPECT_FALSE(g4.isOk());

            auto g5 = generateMac(0, 65536);
            EXPECT_FALSE(g5.isOk());
        }

        // Uniqueness and validity across 100 node/interface combinations
        {
            std::set<std::string> macs;
            for (uint32_t node = 1; node <= 10; ++node) {
                for (uint32_t iface = 1; iface <= 10; ++iface) {
                    auto res = generateMac(node, iface);
                    EXPECT_TRUE(res.isOk());
                    EXPECT_TRUE(res.value().isValidInterfaceMac());

                    // Determinism check: repeated call yields same string
                    auto repeat = generateMac(node, iface);
                    EXPECT_EQ(res.value(), repeat.value());

                    macs.insert(res.value().toString());
                }
            }
            EXPECT_EQ(macs.size(), 100u);
        }
    }


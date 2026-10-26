#include "test_framework.hpp"
#include "nettrace/model/ipv4.hpp"
#include <random>
#include <set>

using namespace nettrace;

// Independent bitwise reference for property testing
static bool independentBitwiseContains(uint32_t hostAddr, uint8_t prefixLen, uint32_t testAddr) {
    if (prefixLen == 0) return true;
    uint32_t mask = 0;
    for (int i = 0; i < prefixLen; ++i) {
        mask |= (1u << (31 - i));
    }
    return (hostAddr & mask) == (testAddr & mask);
}

// T-2-01: IPv4 accept/reject table and canonical round-trip
TEST_CASE("T-2-01: IPv4 accept/reject table and canonical round-trip") {
        // ACCEPT cases
        const char* acceptCases[] = {
            "0.0.0.0",
            "255.255.255.255",
            "192.168.1.10"
        };
        for (const char* ipStr : acceptCases) {
            auto res = IPv4Address::parse(ipStr);
            EXPECT_TRUE(res.isOk());
            EXPECT_EQ(res.value().toString(), std::string(ipStr));
        }

        // REJECT cases
        const char* rejectCases[] = {
            "256.1.1.1",
            "1.2.3",
            "1.2.3.4.5",
            "",
            " 1.2.3.4",
            "1.2.3.4 ",
            "01.2.3.4",
            "1..3.4",
            "a.b.c.d",
            "-1.2.3.4",
            "1.2.3.4/24",
            "::1"
        };
        for (const char* badStr : rejectCases) {
            auto res = IPv4Address::parse(badStr);
            EXPECT_FALSE(res.isOk());
            if (std::string_view(badStr) == "::1") {
                EXPECT_EQ(res.error().code, ErrorCode::UnsupportedProtocol);
            }
        }
    }

    // T-2-02: Prefix math for /0 /8 /16 /20 /23 /24 /25 /30 /31 /32
    TEST_CASE("T-2-02: Prefix math for /0 /8 /16 /20 /23 /24 /25 /30 /31 /32") {
        // 192.168.1.77/24 -> 255.255.255.0 | 192.168.1.0 | 192.168.1.255
        {
            auto sub = IPv4Subnet::parse("192.168.1.77/24").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.255.0");
            EXPECT_EQ(sub.network().toString(), "192.168.1.0");
            EXPECT_EQ(sub.broadcast().toString(), "192.168.1.255");
        }

        // 10.0.17.5/20 -> 255.255.240.0 | 10.0.16.0 | 10.0.31.255
        {
            auto sub = IPv4Subnet::parse("10.0.17.5/20").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.240.0");
            EXPECT_EQ(sub.network().toString(), "10.0.16.0");
            EXPECT_EQ(sub.broadcast().toString(), "10.0.31.255");
        }

        // 192.168.5.10/23 -> 255.255.254.0 | 192.168.4.0 | 192.168.5.255
        {
            auto sub = IPv4Subnet::parse("192.168.5.10/23").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.254.0");
            EXPECT_EQ(sub.network().toString(), "192.168.4.0");
            EXPECT_EQ(sub.broadcast().toString(), "192.168.5.255");
        }

        // 192.168.1.200/25 -> 255.255.255.128 | 192.168.1.128 | 192.168.1.255
        {
            auto sub = IPv4Subnet::parse("192.168.1.200/25").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.255.128");
            EXPECT_EQ(sub.network().toString(), "192.168.1.128");
            EXPECT_EQ(sub.broadcast().toString(), "192.168.1.255");
        }

        // 10.0.0.5/30 -> 255.255.255.252 | 10.0.0.4 | 10.0.0.7
        // Usable hosts: 10.0.0.5 and 10.0.0.6 only
        {
            auto sub = IPv4Subnet::parse("10.0.0.5/30").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.255.252");
            EXPECT_EQ(sub.network().toString(), "10.0.0.4");
            EXPECT_EQ(sub.broadcast().toString(), "10.0.0.7");
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("10.0.0.5").value()));
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("10.0.0.6").value()));
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("10.0.0.4").value())); // network
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("10.0.0.7").value())); // broadcast
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("10.0.0.8").value())); // outside
            auto range = sub.hostRange();
            EXPECT_EQ(range.first.toString(), "10.0.0.5");
            EXPECT_EQ(range.second.toString(), "10.0.0.6");
        }

        // 10.0.0.0/31: both 10.0.0.0 and 10.0.0.1 are valid hosts (RFC 3021)
        {
            auto sub = IPv4Subnet::parse("10.0.0.0/31").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.255.254");
            EXPECT_EQ(sub.network().toString(), "10.0.0.0");
            EXPECT_EQ(sub.broadcast().toString(), "10.0.0.1");
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("10.0.0.0").value()));
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("10.0.0.1").value()));
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("10.0.0.2").value()));
            auto range = sub.hostRange();
            EXPECT_EQ(range.first.toString(), "10.0.0.0");
            EXPECT_EQ(range.second.toString(), "10.0.0.1");
        }

        // 10.9.9.9/32: mask 255.255.255.255, contains only 10.9.9.9
        {
            auto sub = IPv4Subnet::parse("10.9.9.9/32").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.255.255");
            EXPECT_EQ(sub.network().toString(), "10.9.9.9");
            EXPECT_EQ(sub.broadcast().toString(), "10.9.9.9");
            EXPECT_TRUE(sub.contains(IPv4Address::parse("10.9.9.9").value()));
            EXPECT_FALSE(sub.contains(IPv4Address::parse("10.9.9.8").value()));
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("10.9.9.9").value()));
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("10.9.9.8").value()));
            auto range = sub.hostRange();
            EXPECT_EQ(range.first.toString(), "10.9.9.9");
            EXPECT_EQ(range.second.toString(), "10.9.9.9");
        }

        // x.x.x.x/0: mask 0.0.0.0, contains every address
        {
            auto sub = IPv4Subnet::parse("192.168.1.1/0").value();
            EXPECT_EQ(sub.mask().toString(), "0.0.0.0");
            EXPECT_EQ(sub.network().toString(), "0.0.0.0");
            EXPECT_EQ(sub.broadcast().toString(), "255.255.255.255");
            EXPECT_TRUE(sub.contains(IPv4Address::parse("0.0.0.0").value()));
            EXPECT_TRUE(sub.contains(IPv4Address::parse("1.2.3.4").value()));
            EXPECT_TRUE(sub.contains(IPv4Address::parse("255.255.255.255").value()));
            // Per D-15: on /0../30 network (0.0.0.0) and broadcast (255.255.255.255) are not valid hosts
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("0.0.0.0").value()));
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("255.255.255.255").value()));
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("10.0.0.1").value()));
        }

        // Hand-calculated /8 case: 10.0.0.1/8
        // Working:
        // /8 means the highest 8 bits are 1: 11111111 00000000 00000000 00000000
        // Mask = 255.0.0.0
        // Network = 10.0.0.1 & 255.0.0.0 = 10.0.0.0
        // Broadcast = 10.0.0.0 | 0.255.255.255 = 10.255.255.255
        {
            auto sub = IPv4Subnet::parse("10.0.0.1/8").value();
            EXPECT_EQ(sub.mask().toString(), "255.0.0.0");
            EXPECT_EQ(sub.network().toString(), "10.0.0.0");
            EXPECT_EQ(sub.broadcast().toString(), "10.255.255.255");
            EXPECT_TRUE(sub.contains(IPv4Address::parse("10.250.1.2").value()));
            EXPECT_FALSE(sub.contains(IPv4Address::parse("11.0.0.1").value()));
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("10.0.0.0").value())); // network
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("10.255.255.255").value())); // broadcast
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("10.1.2.3").value()));
        }

        // Hand-calculated /16 case: 172.16.5.1/16
        // Working:
        // /16 means the highest 16 bits are 1: 11111111 11111111 00000000 00000000
        // Mask = 255.255.0.0
        // Network = 172.16.5.1 & 255.255.0.0 = 172.16.0.0
        // Broadcast = 172.16.0.0 | 0.0.255.255 = 172.16.255.255
        {
            auto sub = IPv4Subnet::parse("172.16.5.1/16").value();
            EXPECT_EQ(sub.mask().toString(), "255.255.0.0");
            EXPECT_EQ(sub.network().toString(), "172.16.0.0");
            EXPECT_EQ(sub.broadcast().toString(), "172.16.255.255");
            EXPECT_TRUE(sub.contains(IPv4Address::parse("172.16.200.1").value()));
            EXPECT_FALSE(sub.contains(IPv4Address::parse("172.17.0.1").value()));
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("172.16.0.0").value())); // network
            EXPECT_FALSE(sub.isValidHost(IPv4Address::parse("172.16.255.255").value())); // broadcast
            EXPECT_TRUE(sub.isValidHost(IPv4Address::parse("172.16.1.1").value()));
        }

        // Prefix REJECT: -1, 33, "abc"
        EXPECT_FALSE(IPv4Subnet::parse("10.0.0.1/-1").isOk());
        EXPECT_FALSE(IPv4Subnet::parse("10.0.0.1/33").isOk());
        EXPECT_FALSE(IPv4Subnet::parse("10.0.0.1/abc").isOk());
    }

    // T-2-03: The on-link table
    TEST_CASE("T-2-03: The on-link table") {
        auto h1 = IPv4Address::parse("10.0.17.5").value();
        // 10.0.17.5/20 vs 10.0.31.200 -> on-link
        EXPECT_TRUE(isOnLink(h1, 20, IPv4Address::parse("10.0.31.200").value()));
        // 10.0.17.5/20 vs 10.0.32.1 -> NOT on-link
        EXPECT_FALSE(isOnLink(h1, 20, IPv4Address::parse("10.0.32.1").value()));
        // 10.0.17.5/20 vs 10.0.15.255 -> NOT on-link
        EXPECT_FALSE(isOnLink(h1, 20, IPv4Address::parse("10.0.15.255").value()));

        auto h2 = IPv4Address::parse("192.168.5.10").value();
        // 192.168.5.10/23 vs 192.168.4.1 -> on-link
        EXPECT_TRUE(isOnLink(h2, 23, IPv4Address::parse("192.168.4.1").value()));
        // 192.168.5.10/23 vs 192.168.6.1 -> NOT on-link
        EXPECT_FALSE(isOnLink(h2, 23, IPv4Address::parse("192.168.6.1").value()));

        auto h3 = IPv4Address::parse("192.168.1.200").value();
        // 192.168.1.200/25 vs 192.168.1.130 -> on-link
        EXPECT_TRUE(isOnLink(h3, 25, IPv4Address::parse("192.168.1.130").value()));
        // 192.168.1.200/25 vs 192.168.1.100 -> NOT on-link (first three octets match: trap test)
        EXPECT_FALSE(isOnLink(h3, 25, IPv4Address::parse("192.168.1.100").value()));
    }

    // T-2-16: Seeded property tests
    TEST_CASE("T-2-16: Seeded property tests for IPv4 and subnets") {
        std::mt19937 rng(1337);
        std::uniform_int_distribution<uint32_t> ipDist(0, 0xFFFFFFFFu);
        std::uniform_int_distribution<int> prefixDist(0, 32);

        for (int i = 0; i < 500; ++i) {
            uint32_t rawIp = ipDist(rng);
            uint8_t prefix = static_cast<uint8_t>(prefixDist(rng));

            IPv4Address addr(rawIp);
            std::string formatted = addr.toString();

            // 1. Valid IPv4 round-trips
            auto parsed = IPv4Address::parse(formatted);
            EXPECT_TRUE(parsed.isOk());
            EXPECT_EQ(parsed.value(), addr);

            // 2. Subnet checks
            IPv4Subnet sub(addr, prefix);
            IPv4Address net = sub.network();
            IPv4Address bcast = sub.broadcast();

            // network(addr, prefix) contains addr
            EXPECT_TRUE(sub.contains(addr));

            // broadcast >= network
            EXPECT_GE(bcast.toUint32(), net.toUint32());

            // 3. contains() agrees with independent bitwise reference on random test addresses
            for (int k = 0; k < 5; ++k) {
                IPv4Address testTarget(ipDist(rng));
                bool expected = independentBitwiseContains(addr.toUint32(), prefix, testTarget.toUint32());
                bool actual = sub.contains(testTarget);
                EXPECT_EQ(actual, expected);
            }
        }
    }


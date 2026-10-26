#include "test_framework.hpp"
#include "nettrace/model/arp_cache.hpp"

using namespace nettrace;

TEST_CASE("T-2-19: ArpCache ops (lookup, insert, update, source tag)") {
    ArpCache cache;

    // 1. Initial state
    REQUIRE(cache.empty());
    REQUIRE_EQ(cache.size(), static_cast<size_t>(0));

    auto ip1 = IPv4Address::parse("192.168.1.10").value();
    auto ip2 = IPv4Address::parse("192.168.1.20").value();
    auto ipGateway = IPv4Address::parse("192.168.1.1").value();

    auto mac1 = MacAddress::parse("02:00:00:01:00:01").value();
    auto mac2 = MacAddress::parse("02:00:00:02:00:01").value();
    auto macGw = MacAddress::parse("02:00:00:00:00:01").value();

    // Lookup on empty cache -> nullopt
    REQUIRE_FALSE(cache.lookup(ip1).has_value());
    REQUIRE_FALSE(cache.getEntry(ip1).has_value());

    // 2. Insert learned entry for ip1
    bool inserted1 = cache.insert(ip1, mac1, ArpEntrySource::Learned);
    REQUIRE(inserted1);
    REQUIRE_EQ(cache.size(), static_cast<size_t>(1));
    REQUIRE_FALSE(cache.empty());

    // Lookup ip1
    auto lookup1 = cache.lookup(ip1);
    REQUIRE(lookup1.has_value());
    REQUIRE_EQ(*lookup1, mac1);

    auto entry1 = cache.getEntry(ip1);
    REQUIRE(entry1.has_value());
    REQUIRE_EQ(entry1->ip, ip1);
    REQUIRE_EQ(entry1->mac, mac1);
    REQUIRE_EQ(static_cast<int>(entry1->source), static_cast<int>(ArpEntrySource::Learned));

    // Attempting to insert duplicate ip1 fails
    bool dupInsert = cache.insert(ip1, mac2, ArpEntrySource::Learned);
    REQUIRE_FALSE(dupInsert);
    REQUIRE_EQ(cache.size(), static_cast<size_t>(1));

    // 3. Insert preloaded entry for gateway
    bool insertedGw = cache.insert(ipGateway, macGw, ArpEntrySource::Preloaded);
    REQUIRE(insertedGw);
    REQUIRE_EQ(cache.size(), static_cast<size_t>(2));

    auto entryGw = cache.getEntry(ipGateway);
    REQUIRE(entryGw.has_value());
    REQUIRE_EQ(entryGw->ip, ipGateway);
    REQUIRE_EQ(entryGw->mac, macGw);
    REQUIRE_EQ(static_cast<int>(entryGw->source), static_cast<int>(ArpEntrySource::Preloaded));

    // 4. Update existing entry (e.g. host ip1 changed MAC)
    auto mac1New = MacAddress::parse("02:00:00:01:00:99").value();
    bool updated1 = cache.update(ip1, mac1New, ArpEntrySource::Learned);
    REQUIRE(updated1);

    auto lookup1New = cache.lookup(ip1);
    REQUIRE(lookup1New.has_value());
    REQUIRE_EQ(*lookup1New, mac1New);

    // Update non-existent entry fails
    bool updateMissing = cache.update(ip2, mac2, ArpEntrySource::Learned);
    REQUIRE_FALSE(updateMissing);

    // 5. insertOrUpdate
    // Existing IP -> updates
    bool wasExisting = cache.insertOrUpdate(ip1, mac1, ArpEntrySource::Preloaded);
    REQUIRE(wasExisting);
    REQUIRE_EQ(*cache.lookup(ip1), mac1);
    REQUIRE_EQ(static_cast<int>(cache.getEntry(ip1)->source), static_cast<int>(ArpEntrySource::Preloaded));

    // New IP -> inserts
    bool wasExisting2 = cache.insertOrUpdate(ip2, mac2, ArpEntrySource::Learned);
    REQUIRE_FALSE(wasExisting2);
    REQUIRE_EQ(cache.size(), static_cast<size_t>(3));
    REQUIRE_EQ(*cache.lookup(ip2), mac2);

    // 6. Entries list (ordered by IP)
    auto allEntries = cache.entries();
    REQUIRE_EQ(allEntries.size(), static_cast<size_t>(3));
    // 192.168.1.1 < 192.168.1.10 < 192.168.1.20
    REQUIRE_EQ(allEntries[0].ip, ipGateway);
    REQUIRE_EQ(allEntries[1].ip, ip1);
    REQUIRE_EQ(allEntries[2].ip, ip2);

    // 7. Remove and clear
    REQUIRE(cache.remove(ip1));
    REQUIRE_EQ(cache.size(), static_cast<size_t>(2));
    REQUIRE_FALSE(cache.lookup(ip1).has_value());

    // Remove already removed IP -> false
    REQUIRE_FALSE(cache.remove(ip1));

    cache.clear();
    REQUIRE(cache.empty());
    REQUIRE_EQ(cache.size(), static_cast<size_t>(0));
}

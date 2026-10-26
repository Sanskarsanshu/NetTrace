#pragma once

#include "nettrace/model/ipv4.hpp"
#include "nettrace/model/mac.hpp"
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace nettrace {

enum class ArpEntrySource {
    Learned,
    Preloaded
};

inline std::string_view arpEntrySourceToString(ArpEntrySource source) {
    switch (source) {
        case ArpEntrySource::Learned: return "learned";
        case ArpEntrySource::Preloaded: return "preloaded";
    }
    return "learned";
}

struct ArpEntry {
    IPv4Address ip;
    MacAddress mac;
    ArpEntrySource source = ArpEntrySource::Learned;

    bool operator==(const ArpEntry& o) const {
        return ip == o.ip && mac == o.mac && source == o.source;
    }
};

// Per-node ARP cache storing IP -> MAC mappings with a source tag (D-21, M2-47).
class ArpCache {
public:
    ArpCache() = default;

    // Lookup MAC for an IP. Returns nullopt on cache miss.
    std::optional<MacAddress> lookup(const IPv4Address& ip) const;

    // Lookup full entry including source tag. Returns nullopt on cache miss.
    std::optional<ArpEntry> getEntry(const IPv4Address& ip) const;

    // Inserts or updates an entry. Returns true if an existing entry was updated, false if newly inserted.
    bool insertOrUpdate(const IPv4Address& ip, const MacAddress& mac, ArpEntrySource source);

    // Inserts an entry only if not already present. Returns true if inserted, false if already present.
    bool insert(const IPv4Address& ip, const MacAddress& mac, ArpEntrySource source);

    // Updates an existing entry. Returns true if updated, false if not present.
    bool update(const IPv4Address& ip, const MacAddress& mac, ArpEntrySource source);

    // Removes an entry by IP. Returns true if removed, false if not found.
    bool remove(const IPv4Address& ip);

    // Clears all cache entries.
    void clear();

    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    // Returns all entries sorted by IP address.
    std::vector<ArpEntry> entries() const;

private:
    std::map<IPv4Address, ArpEntry> entries_;
};

} // namespace nettrace

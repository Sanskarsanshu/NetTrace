#include "nettrace/model/arp_cache.hpp"

namespace nettrace {

std::optional<MacAddress> ArpCache::lookup(const IPv4Address& ip) const {
    auto it = entries_.find(ip);
    if (it != entries_.end()) {
        return it->second.mac;
    }
    return std::nullopt;
}

std::optional<ArpEntry> ArpCache::getEntry(const IPv4Address& ip) const {
    auto it = entries_.find(ip);
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ArpCache::insertOrUpdate(const IPv4Address& ip, const MacAddress& mac, ArpEntrySource source) {
    auto it = entries_.find(ip);
    if (it != entries_.end()) {
        it->second.mac = mac;
        it->second.source = source;
        return true; // updated
    }
    entries_[ip] = ArpEntry{ip, mac, source};
    return false; // inserted
}

bool ArpCache::insert(const IPv4Address& ip, const MacAddress& mac, ArpEntrySource source) {
    auto it = entries_.find(ip);
    if (it != entries_.end()) {
        return false;
    }
    entries_[ip] = ArpEntry{ip, mac, source};
    return true;
}

bool ArpCache::update(const IPv4Address& ip, const MacAddress& mac, ArpEntrySource source) {
    auto it = entries_.find(ip);
    if (it == entries_.end()) {
        return false;
    }
    it->second.mac = mac;
    it->second.source = source;
    return true;
}

bool ArpCache::remove(const IPv4Address& ip) {
    return entries_.erase(ip) > 0;
}

void ArpCache::clear() {
    entries_.clear();
}

std::vector<ArpEntry> ArpCache::entries() const {
    std::vector<ArpEntry> result;
    result.reserve(entries_.size());
    for (const auto& [ip, entry] : entries_) {
        result.push_back(entry);
    }
    return result;
}

} // namespace nettrace

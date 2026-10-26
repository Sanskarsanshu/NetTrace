#include "nettrace/model/mac_table.hpp"

namespace nettrace {

MacLearnResult MacTable::learn(const MacAddress& mac, const InterfaceId& ingressPort) {
    auto it = entries_.find(mac);
    if (it != entries_.end()) {
        if (it->second == ingressPort) {
            return MacLearnResult::Unchanged;
        }
        it->second = ingressPort;
        return MacLearnResult::Moved;
    }
    entries_[mac] = ingressPort;
    return MacLearnResult::Learned;
}

std::optional<InterfaceId> MacTable::lookup(const MacAddress& mac) const {
    // Broadcast and multicast MACs are never forwarded to a single learned port
    if (mac.isBroadcast() || mac.isMulticast()) {
        return std::nullopt;
    }

    auto it = entries_.find(mac);
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<InterfaceId> MacTable::getFloodPorts(const InterfaceId& ingressPort,
                                                const std::vector<Interface>& allPorts) const {
    std::vector<InterfaceId> floodPorts;
    for (const auto& port : allPorts) {
        if (port.id == ingressPort) continue;
        if (port.adminState != AdminState::Up) continue;
        floodPorts.push_back(port.id);
    }
    return floodPorts;
}

bool MacTable::remove(const MacAddress& mac) {
    return entries_.erase(mac) > 0;
}

void MacTable::clear() {
    entries_.clear();
}

std::vector<std::pair<MacAddress, InterfaceId>> MacTable::entries() const {
    std::vector<std::pair<MacAddress, InterfaceId>> result;
    result.reserve(entries_.size());
    for (const auto& [mac, port] : entries_) {
        result.emplace_back(mac, port);
    }
    return result;
}

} // namespace nettrace

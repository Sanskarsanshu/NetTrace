#pragma once

#include "nettrace/model/types.hpp"
#include "nettrace/model/mac.hpp"
#include "nettrace/model/topology.hpp"
#include <map>
#include <optional>
#include <vector>

namespace nettrace {

enum class MacLearnResult {
    Learned,   // Newly learned MAC
    Moved,     // MAC moved to a different port
    Unchanged  // MAC already known on the same ingress port
};

// Switch Layer-2 MAC forwarding table (D-09, M2-28).
class MacTable {
public:
    MacTable() = default;

    // Learns or updates the mapping for a source MAC address on an ingress port.
    MacLearnResult learn(const MacAddress& mac, const InterfaceId& ingressPort);

    // Lookups the destination port for a unicast MAC address.
    // Returns nullopt on cache miss (or if broadcast/multicast).
    std::optional<InterfaceId> lookup(const MacAddress& mac) const;

    // Computes the set of egress ports to flood an unknown unicast or broadcast frame to:
    // - Excludes the ingress port.
    // - Excludes any port whose administrative state is Down.
    std::vector<InterfaceId> getFloodPorts(const InterfaceId& ingressPort,
                                          const std::vector<Interface>& allPorts) const;

    // Remove a learned MAC entry.
    bool remove(const MacAddress& mac);

    // Clear all learned MAC entries.
    void clear();

    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    // Returns all table entries sorted by MAC address.
    std::vector<std::pair<MacAddress, InterfaceId>> entries() const;

private:
    std::map<MacAddress, InterfaceId> entries_;
};

} // namespace nettrace

#pragma once

#include "nettrace/model/broadcast_domain.hpp"
#include "nettrace/model/types.hpp"
#include "nettrace/model/ipv4.hpp"
#include "nettrace/model/mac.hpp"
#include "nettrace/model/result.hpp"
#include "nettrace/model/topology.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nettrace {

struct RegistryEntry {
    IPv4Address ip;
    MacAddress mac;
    NodeId nodeId;
    InterfaceId interfaceId;
    std::string broadcastDomainId;

    bool operator==(const RegistryEntry& o) const {
        return ip == o.ip && mac == o.mac && nodeId == o.nodeId &&
               interfaceId == o.interfaceId && broadcastDomainId == o.broadcastDomainId;
    }
};

// Global, READ-ONLY address registry built from the immutable topology snapshot (D-21, M2-46).
// Used for validation, UI ground-truth inspection, and abstracted ARP mode.
class AddressRegistry {
public:
    // Builds the read-only AddressRegistry from a TopologySnapshot.
    // Fails with ErrorCode::DuplicateIpInDomain if duplicate IPs exist in the same broadcast domain.
    static Result<AddressRegistry> build(const TopologySnapshot& snapshot);

    // Overload building from a TopologyDraft.
    static Result<AddressRegistry> build(const TopologyDraft& draft);

    // Abstracted ARP resolution: lookup the owner of an IP strictly within the specified broadcast domain.
    // Returns nullopt if the IP is unassigned or belongs to a different broadcast domain (D-21).
    std::optional<RegistryEntry> findOwnerInDomain(const IPv4Address& ip, std::string_view domainId) const;

    // Lookup entry by Node ID and Interface ID.
    std::optional<RegistryEntry> findInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const;

    // Lookup the broadcast domain ID for a given interface.
    std::optional<std::string> getDomainForInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const;

    // Get all registered entries.
    const std::vector<RegistryEntry>& entries() const { return entries_; }

    size_t size() const { return entries_.size(); }

private:
    explicit AddressRegistry(std::vector<RegistryEntry> entries)
        : entries_(std::move(entries)) {}

    std::vector<RegistryEntry> entries_;
};

} // namespace nettrace

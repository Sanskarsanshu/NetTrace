#include "nettrace/model/address_registry.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace nettrace {

Result<AddressRegistry> AddressRegistry::build(const TopologySnapshot& snapshot) {
    return build(snapshot.toDraft());
}

Result<AddressRegistry> AddressRegistry::build(const TopologyDraft& draft) {
    auto domains = computeBroadcastDomains(draft);

    // Map InterfaceRef to broadcast domain ID
    std::map<InterfaceRef, std::string> ifaceToDomain;
    for (const auto& domain : domains) {
        for (const auto& iface : domain.interfaces) {
            ifaceToDomain[iface] = domain.id;
        }
    }

    std::vector<RegistryEntry> entries;
    // Map (domainId, ip) -> InterfaceRef to detect duplicate IPs within the same domain
    std::map<std::pair<std::string, IPv4Address>, InterfaceRef> domainIpMap;

    for (const auto& node : draft.nodes) {
        if (node.adminState != AdminState::Up) continue;

        for (const auto& iface : node.interfaces) {
            if (iface.adminState != AdminState::Up) continue;
            if (!iface.ipv4.has_value() || !iface.mac.has_value()) continue;

            InterfaceRef ref{node.id, iface.id};
            std::string domainId;
            auto itDom = ifaceToDomain.find(ref);
            if (itDom != ifaceToDomain.end()) {
                domainId = itDom->second;
            } else {
                domainId = "bd-isolated-" + node.id.str() + "-" + iface.id.str();
            }

            IPv4Address ip = iface.ipv4->address();
            auto key = std::make_pair(domainId, ip);
            auto ins = domainIpMap.emplace(key, ref);
            if (!ins.second) {
                // Duplicate IP within the same broadcast domain!
                return Error(ErrorCode::DuplicateIpInDomain,
                    "Duplicate IP address '" + ip.toString() + "' in broadcast domain '" + domainId +
                    "' (shared by " + ins.first->second.nodeId.str() + "." + ins.first->second.interfaceId.str() +
                    " and " + node.id.str() + "." + iface.id.str() + ")",
                    "nodes/" + node.id.str() + "/interfaces/" + iface.id.str() + "/ipv4");
            }

            RegistryEntry entry;
            entry.ip = ip;
            entry.mac = *iface.mac;
            entry.nodeId = node.id;
            entry.interfaceId = iface.id;
            entry.broadcastDomainId = domainId;
            entries.push_back(std::move(entry));
        }
    }

    // Sort entries deterministically by (nodeId, interfaceId)
    std::sort(entries.begin(), entries.end(), [](const RegistryEntry& a, const RegistryEntry& b) {
        if (a.nodeId != b.nodeId) return a.nodeId < b.nodeId;
        return a.interfaceId < b.interfaceId;
    });

    return AddressRegistry(std::move(entries));
}

std::optional<RegistryEntry> AddressRegistry::findOwnerInDomain(const IPv4Address& ip, std::string_view domainId) const {
    for (const auto& entry : entries_) {
        if (entry.ip == ip && entry.broadcastDomainId == domainId) {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<RegistryEntry> AddressRegistry::findInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const {
    for (const auto& entry : entries_) {
        if (entry.nodeId == nodeId && entry.interfaceId == ifaceId) {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<std::string> AddressRegistry::getDomainForInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const {
    auto entry = findInterface(nodeId, ifaceId);
    if (entry.has_value()) {
        return entry->broadcastDomainId;
    }
    return std::nullopt;
}

} // namespace nettrace

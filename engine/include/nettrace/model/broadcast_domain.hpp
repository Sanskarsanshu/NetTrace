#pragma once

#include "nettrace/model/types.hpp"
#include "nettrace/model/topology.hpp"
#include <string>
#include <vector>

namespace nettrace {

struct InterfaceRef {
    NodeId nodeId;
    InterfaceId interfaceId;

    bool operator==(const InterfaceRef& other) const {
        return nodeId == other.nodeId && interfaceId == other.interfaceId;
    }
    bool operator!=(const InterfaceRef& other) const {
        return !(*this == other);
    }
    bool operator<(const InterfaceRef& other) const {
        if (nodeId != other.nodeId) return nodeId < other.nodeId;
        return interfaceId < other.interfaceId;
    }
};

struct BroadcastDomain {
    std::string id;                       // Deterministic ID (e.g. "bd-0001")
    std::vector<InterfaceRef> interfaces; // All active interfaces in this domain
    std::vector<NodeId> switchIds;        // All switch nodes participating in this domain

    bool contains(const NodeId& node, const InterfaceId& iface) const {
        for (const auto& ref : interfaces) {
            if (ref.nodeId == node && ref.interfaceId == iface) return true;
        }
        return false;
    }
};

// Computes broadcast domains over active links and active interfaces.
// Layer-2 traversal passes freely through Switches, but terminates at Hosts and Routers.
std::vector<BroadcastDomain> computeBroadcastDomains(const TopologyDraft& draft);
std::vector<BroadcastDomain> computeBroadcastDomains(const TopologySnapshot& snapshot);

// Detects Layer-2 loops among switch nodes (where STP is not modeled).
// Returns true if an active cycle exists in the switch-only subgraph.
bool detectSwitchLoops(const TopologyDraft& draft, std::vector<NodeId>* loopSwitches = nullptr);

} // namespace nettrace

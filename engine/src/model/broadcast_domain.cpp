#include "nettrace/model/broadcast_domain.hpp"

#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace nettrace {

namespace {

struct DSU {
    std::vector<int> parent;
    DSU(int n) : parent(n) {
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]);
    }
    bool unite(int i, int j) {
        int rootI = find(i);
        int rootJ = find(j);
        if (rootI != rootJ) {
            parent[rootI] = rootJ;
            return true;
        }
        return false;
    }
};

} // namespace

std::vector<BroadcastDomain> computeBroadcastDomains(const TopologyDraft& draft) {
    // Collect all active interfaces on active nodes
    std::vector<InterfaceRef> allIfaces;
    std::map<InterfaceRef, int> ifaceToIndex;
    std::unordered_map<std::string, const Node*> nodeMap;

    for (const auto& node : draft.nodes) {
        if (node.adminState != AdminState::Up) continue;
        nodeMap[node.id.str()] = &node;

        for (const auto& iface : node.interfaces) {
            if (iface.adminState != AdminState::Up) continue;
            InterfaceRef ref{node.id, iface.id};
            ifaceToIndex[ref] = static_cast<int>(allIfaces.size());
            allIfaces.push_back(ref);
        }
    }

    if (allIfaces.empty()) {
        return {};
    }

    DSU dsu(static_cast<int>(allIfaces.size()));

    // 1. Connect switch internal ports
    // On a switch, all active ports belong to the same internal Layer-2 broadcast fabric
    for (const auto& [nodeIdStr, nodePtr] : nodeMap) {
        if (nodePtr->type == NodeType::Switch) {
            int firstIdx = -1;
            for (const auto& iface : nodePtr->interfaces) {
                if (iface.adminState != AdminState::Up) continue;
                InterfaceRef ref{nodePtr->id, iface.id};
                auto it = ifaceToIndex.find(ref);
                if (it != ifaceToIndex.end()) {
                    if (firstIdx == -1) {
                        firstIdx = it->second;
                    } else {
                        dsu.unite(firstIdx, it->second);
                    }
                }
            }
        }
    }

    // 2. Connect active links
    for (const auto& link : draft.links) {
        if (link.adminState != AdminState::Up) continue;

        InterfaceRef refA{link.a.nodeId, link.a.interfaceId};
        InterfaceRef refB{link.b.nodeId, link.b.interfaceId};

        auto itA = ifaceToIndex.find(refA);
        auto itB = ifaceToIndex.find(refB);

        if (itA != ifaceToIndex.end() && itB != ifaceToIndex.end()) {
            dsu.unite(itA->second, itB->second);
        }
    }

    // Group interfaces by component root
    std::map<int, std::vector<InterfaceRef>> componentMap;
    for (size_t i = 0; i < allIfaces.size(); ++i) {
        int root = dsu.find(static_cast<int>(i));
        componentMap[root].push_back(allIfaces[i]);
    }

    // Convert to list of BroadcastDomains
    std::vector<BroadcastDomain> domains;
    for (auto& [root, ifaces] : componentMap) {
        std::sort(ifaces.begin(), ifaces.end());

        std::set<NodeId> switches;
        for (const auto& ref : ifaces) {
            auto itNode = nodeMap.find(ref.nodeId.str());
            if (itNode != nodeMap.end() && itNode->second->type == NodeType::Switch) {
                switches.insert(ref.nodeId);
            }
        }

        BroadcastDomain domain;
        domain.interfaces = std::move(ifaces);
        domain.switchIds.assign(switches.begin(), switches.end());
        domains.push_back(std::move(domain));
    }

    // Sort domains deterministically by their lowest InterfaceRef
    std::sort(domains.begin(), domains.end(), [](const BroadcastDomain& a, const BroadcastDomain& b) {
        if (a.interfaces.empty() && b.interfaces.empty()) return false;
        if (a.interfaces.empty()) return true;
        if (b.interfaces.empty()) return false;
        return a.interfaces.front() < b.interfaces.front();
    });

    // Assign IDs: bd-0001, bd-0002, ...
    for (size_t i = 0; i < domains.size(); ++i) {
        std::ostringstream ss;
        ss << "bd-" << std::setw(4) << std::setfill('0') << (i + 1);
        domains[i].id = ss.str();
    }

    return domains;
}

std::vector<BroadcastDomain> computeBroadcastDomains(const TopologySnapshot& snapshot) {
    return computeBroadcastDomains(snapshot.toDraft());
}

bool detectSwitchLoops(const TopologyDraft& draft, std::vector<NodeId>* loopSwitches) {
    std::unordered_map<std::string, const Node*> switchMap;
    std::vector<NodeId> switchNodes;
    std::unordered_map<std::string, int> switchIndex;

    for (const auto& node : draft.nodes) {
        if (node.adminState == AdminState::Up && node.type == NodeType::Switch) {
            switchIndex[node.id.str()] = static_cast<int>(switchNodes.size());
            switchNodes.push_back(node.id);
            switchMap[node.id.str()] = &node;
        }
    }

    if (switchNodes.size() < 2) {
        return false;
    }

    DSU dsu(static_cast<int>(switchNodes.size()));
    bool hasLoop = false;
    std::set<NodeId> involved;

    for (const auto& link : draft.links) {
        if (link.adminState != AdminState::Up) continue;

        auto itA = switchIndex.find(link.a.nodeId.str());
        auto itB = switchIndex.find(link.b.nodeId.str());

        // We only care about links directly between two switches
        if (itA != switchIndex.end() && itB != switchIndex.end()) {
            if (!dsu.unite(itA->second, itB->second)) {
                // Already in same component -> cycle detected!
                hasLoop = true;
                involved.insert(switchNodes[itA->second]);
                involved.insert(switchNodes[itB->second]);
            }
        }
    }

    if (hasLoop && loopSwitches) {
        loopSwitches->assign(involved.begin(), involved.end());
    }

    return hasLoop;
}

} // namespace nettrace

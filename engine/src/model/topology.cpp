#include "nettrace/model/topology.hpp"
#include "nettrace/model/validate.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <unordered_set>

namespace nettrace {

std::string_view nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::Host: return "host";
        case NodeType::Router: return "router";
        case NodeType::Switch: return "switch";
        case NodeType::CloudEndpoint: return "cloudEndpoint";
    }
    return "host";
}

std::string_view transportProtocolToString(TransportProtocol proto) {
    switch (proto) {
        case TransportProtocol::UDP: return "UDP";
        case TransportProtocol::TCP: return "TCP";
        case TransportProtocol::ICMP: return "ICMP";
    }
    return "TCP";
}

Result<uint16_t> validatePort(int port) {
    if (port < 1 || port > 65535) {
        return Result<uint16_t>::err(
            ErrorCode::InvalidPort,
            "Port must be in range 1..65535: " + std::to_string(port));
    }
    return Result<uint16_t>::ok(static_cast<uint16_t>(port));
}

const Interface* Node::findInterface(const InterfaceId& ifaceId) const {
    for (const auto& iface : interfaces) {
        if (iface.id == ifaceId) return &iface;
    }
    return nullptr;
}

Interface* Node::findInterface(const InterfaceId& ifaceId) {
    for (auto& iface : interfaces) {
        if (iface.id == ifaceId) return &iface;
    }
    return nullptr;
}

bool Node::operator==(const Node& o) const {
    return id == o.id && name == o.name && type == o.type &&
           adminState == o.adminState && interfaces == o.interfaces &&
           defaultGateway == o.defaultGateway && services == o.services &&
           forwardingEnabled == o.forwardingEnabled &&
           processingDelayMs == o.processingDelayMs &&
           simulated == o.simulated &&
           staticRoutes == o.staticRoutes;
}

bool Link::operator==(const Link& o) const {
    return id == o.id && a == o.a && b == o.b &&
           adminState == o.adminState && directionality == o.directionality &&
           cost == o.cost && bandwidthBps == o.bandwidthBps &&
           availableBandwidthBps == o.availableBandwidthBps &&
           propagationDelayMs == o.propagationDelayMs &&
           packetLossProbability == o.packetLossProbability &&
           mtuBytes == o.mtuBytes && queuePolicy == o.queuePolicy;
}

uint32_t effectiveMtu(const Link& link,
                     const std::optional<uint32_t>& overrideA,
                     const std::optional<uint32_t>& overrideB) {
    uint32_t m = link.mtuBytes;
    if (overrideA.has_value()) {
        m = std::min(m, overrideA.value());
    }
    if (overrideB.has_value()) {
        m = std::min(m, overrideB.value());
    }
    return m;
}

uint32_t effectiveMtu(const Link& link, const TopologyDraft& draft) {
    std::optional<uint32_t> ovA;
    std::optional<uint32_t> ovB;
    const auto* ifA = draft.findInterface(link.a.nodeId, link.a.interfaceId);
    if (ifA && ifA->mtuBytes.has_value()) {
        ovA = ifA->mtuBytes;
    }
    const auto* ifB = draft.findInterface(link.b.nodeId, link.b.interfaceId);
    if (ifB && ifB->mtuBytes.has_value()) {
        ovB = ifB->mtuBytes;
    }
    return effectiveMtu(link, ovA, ovB);
}

uint32_t effectiveMtu(const Link& link, const TopologySnapshot& snap) {
    std::optional<uint32_t> ovA;
    std::optional<uint32_t> ovB;
    const auto* ifA = snap.findInterface(link.a.nodeId, link.a.interfaceId);
    if (ifA && ifA->mtuBytes.has_value()) {
        ovA = ifA->mtuBytes;
    }
    const auto* ifB = snap.findInterface(link.b.nodeId, link.b.interfaceId);
    if (ifB && ifB->mtuBytes.has_value()) {
        ovB = ifB->mtuBytes;
    }
    return effectiveMtu(link, ovA, ovB);
}

uint64_t fnv1a64(std::string_view text) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char c : text) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

std::string fnv1a64Hex(std::string_view text) {
    uint64_t h = fnv1a64(text);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return std::string(buf);
}

const Node* TopologyDraft::findNode(const NodeId& id) const {
    for (const auto& node : nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

Node* TopologyDraft::findNode(const NodeId& id) {
    for (auto& node : nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const Interface* TopologyDraft::findInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const {
    const auto* node = findNode(nodeId);
    if (!node) return nullptr;
    return node->findInterface(ifaceId);
}

Interface* TopologyDraft::findInterface(const NodeId& nodeId, const InterfaceId& ifaceId) {
    auto* node = findNode(nodeId);
    if (!node) return nullptr;
    return node->findInterface(ifaceId);
}

const Link* TopologyDraft::findLink(const LinkId& id) const {
    for (const auto& link : links) {
        if (link.id == id) return &link;
    }
    return nullptr;
}

Link* TopologyDraft::findLink(const LinkId& id) {
    for (auto& link : links) {
        if (link.id == id) return &link;
    }
    return nullptr;
}

Result<void> assignMissingMacs(TopologyDraft& draft) {
    // 1. Collect all explicit MACs already assigned anywhere in the draft
    std::unordered_set<MacAddress> usedMacs;
    for (const auto& node : draft.nodes) {
        for (const auto& iface : node.interfaces) {
            if (iface.mac.has_value()) {
                usedMacs.insert(*iface.mac);
            }
        }
    }

    // 2. Sort node indices by NodeId
    std::vector<size_t> nodeIndices(draft.nodes.size());
    for (size_t i = 0; i < draft.nodes.size(); ++i) {
        nodeIndices[i] = i;
    }
    std::sort(nodeIndices.begin(), nodeIndices.end(), [&](size_t a, size_t b) {
        return draft.nodes[a].id < draft.nodes[b].id;
    });

    // 3. Visit nodes in sorted NodeId order and interfaces in sorted InterfaceId order
    uint16_t nodeRank = 1;
    for (size_t nIdx : nodeIndices) {
        auto& node = draft.nodes[nIdx];
        if (node.type == NodeType::Switch) {
            // Switch ports have NO MAC per D-23
            continue;
        }

        // Sort interface indices on this node by InterfaceId
        std::vector<size_t> ifaceIndices(node.interfaces.size());
        for (size_t j = 0; j < node.interfaces.size(); ++j) {
            ifaceIndices[j] = j;
        }
        std::sort(ifaceIndices.begin(), ifaceIndices.end(), [&](size_t a, size_t b) {
            return node.interfaces[a].id < node.interfaces[b].id;
        });

        uint16_t ifaceRank = 1;
        for (size_t ifIdx : ifaceIndices) {
            auto& iface = node.interfaces[ifIdx];
            if (!iface.mac.has_value()) {
                uint32_t candidateN = nodeRank;
                while (true) {
                    if (candidateN > 65535) {
                        return Result<void>::err(ErrorCode::LimitExceeded, "MAC generator rank exceeded 65535");
                    }
                    auto candRes = generateMac(candidateN, ifaceRank);
                    if (!candRes.isOk()) {
                        return Result<void>::err(candRes.error());
                    }
                    MacAddress candidate = candRes.value();
                    if (usedMacs.find(candidate) == usedMacs.end()) {
                        iface.mac = candidate;
                        usedMacs.insert(candidate);
                        break;
                    }
                    candidateN++;
                }
            }
            ifaceRank++;
        }

        nodeRank++;
    }

    return Result<void>::ok();
}

Result<void> normalize(TopologyDraft& draft) {
    for (auto& node : draft.nodes) {
        if (node.type == NodeType::CloudEndpoint) {
            node.simulated = true;
        }
    }
    return assignMissingMacs(draft);
}

static std::string buildCanonicalText(const std::vector<Node>& nodes, const std::vector<Link>& links) {
    std::ostringstream ss;
    // Nodes are already sorted by NodeId, interfaces by InterfaceId, services by protocol/port
    for (const auto& node : nodes) {
        ss << "NODE:" << node.id.str() << "|"
           << node.name << "|"
           << nodeTypeToString(node.type) << "|"
           << (node.adminState == AdminState::Up ? "up" : "down") << "|"
           << (node.defaultGateway.has_value() ? node.defaultGateway->toString() : "") << "|"
           << (node.type == NodeType::Router ? (node.forwardingEnabled ? "true" : "false") : "") << "|"
           << (node.processingDelayMs.has_value() ? std::to_string(*node.processingDelayMs) : "")
           << "\n";

        for (const auto& svc : node.services) {
            ss << "SERVICE:" << transportProtocolToString(svc.protocol) << "|"
               << svc.port << "|"
               << svc.label << "\n";
        }

        for (const auto& iface : node.interfaces) {
            ss << "IFACE:" << iface.id.str() << "|"
               << iface.name << "|"
               << (iface.adminState == AdminState::Up ? "up" : "down") << "|"
               << (iface.mac.has_value() ? iface.mac->toString() : "") << "|"
               << (iface.ipv4.has_value() ? iface.ipv4->toString() : "") << "|"
               << (iface.mtuBytes.has_value() ? std::to_string(*iface.mtuBytes) : "")
               << "\n";
        }

        for (const auto& sr : node.staticRoutes) {
            ss << "ROUTE:" << sr.prefix.toString() << "|"
               << (sr.nextHop.has_value() ? sr.nextHop->toString() : "") << "|"
               << (sr.outInterface.has_value() ? sr.outInterface->str() : "") << "|"
               << sr.adminPreference << "|"
               << sr.metric << "\n";
        }
    }

    for (const auto& link : links) {
        ss << "LINK:" << link.id.str() << "|"
           << (link.adminState == AdminState::Up ? "up" : "down") << "|"
           << (link.directionality == Directionality::Bidirectional ? "bidirectional" : "directed") << "|"
           << link.a.nodeId.str() << ":" << link.a.interfaceId.str() << "|"
           << link.b.nodeId.str() << ":" << link.b.interfaceId.str() << "|"
           << link.cost << "|"
           << link.bandwidthBps << "|"
           << (link.availableBandwidthBps.has_value() ? std::to_string(*link.availableBandwidthBps) : "") << "|"
           << link.propagationDelayMs << "|"
           << link.packetLossProbability << "|"
           << link.mtuBytes << "\n";
    }

    return ss.str();
}

Result<TopologySnapshot> TopologySnapshot::freeze(TopologyDraft draft, const ValidationOptions& options) {
    auto normRes = normalize(draft);
    if (!normRes.isOk()) {
        return Result<TopologySnapshot>::err(normRes.error());
    }

    ValidationReport report = validate(draft, options);
    if (report.hasErrors()) {
        return Result<TopologySnapshot>::err(ErrorCode::InvalidConfiguration, report.summary());
    }

    TopologySnapshot snap;
    snap.schemaVersion_ = draft.schemaVersion;
    snap.id_ = std::move(draft.id);
    snap.name_ = std::move(draft.name);
    snap.description_ = std::move(draft.description);
    snap.nodes_ = std::move(draft.nodes);
    snap.links_ = std::move(draft.links);

    // Sort nodes by NodeId ascending
    std::sort(snap.nodes_.begin(), snap.nodes_.end(), [](const Node& a, const Node& b) {
        return a.id < b.id;
    });

    // Sort interfaces and services within each node
    for (auto& node : snap.nodes_) {
        std::sort(node.interfaces.begin(), node.interfaces.end(), [](const Interface& a, const Interface& b) {
            return a.id < b.id;
        });
        std::sort(node.services.begin(), node.services.end(), [](const Service& a, const Service& b) {
            if (a.protocol != b.protocol) {
                return static_cast<int>(a.protocol) < static_cast<int>(b.protocol);
            }
            return a.port < b.port;
        });
        std::sort(node.staticRoutes.begin(), node.staticRoutes.end(), [](const StaticRoute& a, const StaticRoute& b) {
            if (a.prefix.prefixLength() != b.prefix.prefixLength()) {
                return a.prefix.prefixLength() > b.prefix.prefixLength();
            }
            if (a.prefix.address() != b.prefix.address()) {
                return a.prefix.address() < b.prefix.address();
            }
            if (a.nextHop != b.nextHop) {
                if (!a.nextHop.has_value()) return true;
                if (!b.nextHop.has_value()) return false;
                return a.nextHop->toUint32() < b.nextHop->toUint32();
            }
            if (a.outInterface != b.outInterface) {
                return a.outInterface < b.outInterface;
            }
            if (a.adminPreference != b.adminPreference) {
                return a.adminPreference < b.adminPreference;
            }
            return a.metric < b.metric;
        });
    }

    // Sort links by LinkId ascending
    std::sort(snap.links_.begin(), snap.links_.end(), [](const Link& a, const Link& b) {
        return a.id < b.id;
    });

    snap.canonicalText_ = buildCanonicalText(snap.nodes_, snap.links_);
    snap.contentHash_ = fnv1a64Hex(snap.canonicalText_);

    return Result<TopologySnapshot>::ok(std::move(snap));
}

Result<TopologySnapshot> TopologySnapshot::freeze(TopologyDraft draft) {
    return freeze(std::move(draft), ValidationOptions{});
}

const Node* TopologySnapshot::findNode(const NodeId& id) const {
    for (const auto& node : nodes_) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const Interface* TopologySnapshot::findInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const {
    const auto* node = findNode(nodeId);
    if (!node) return nullptr;
    return node->findInterface(ifaceId);
}

const Link* TopologySnapshot::findLink(const LinkId& id) const {
    for (const auto& link : links_) {
        if (link.id == id) return &link;
    }
    return nullptr;
}

} // namespace nettrace

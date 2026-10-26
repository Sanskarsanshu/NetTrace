#pragma once

#include "nettrace/model/error.hpp"
#include "nettrace/model/ipv4.hpp"
#include "nettrace/model/mac.hpp"
#include "nettrace/model/result.hpp"
#include "nettrace/model/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nettrace {

// Forward declarations
struct ValidationOptions;
struct ValidationReport;
class TopologySnapshot;
struct TopologyDraft;

enum class NodeType {
    Host,
    Router,
    Switch,
    CloudEndpoint
};

std::string_view nodeTypeToString(NodeType type);

enum class TransportProtocol {
    UDP,
    TCP,
    ICMP
};

std::string_view transportProtocolToString(TransportProtocol proto);

/**
 * @brief Validates transport port in range 1..65535 per D-22.
 */
Result<uint16_t> validatePort(int port);

struct Service {
    TransportProtocol protocol = TransportProtocol::TCP;
    uint16_t port = 80;
    std::string label;

    bool operator==(const Service& o) const {
        return protocol == o.protocol && port == o.port && label == o.label;
    }
};

struct TransportHeader {
    TransportProtocol protocol = TransportProtocol::TCP;
    std::optional<uint16_t> srcPort;
    std::optional<uint16_t> dstPort;
    std::optional<uint8_t> tcpFlags;
};

struct Interface {
    InterfaceId id;
    std::string name;
    std::optional<MacAddress> mac;
    std::optional<IPv4Subnet> ipv4;
    std::optional<uint32_t> mtuBytes; // Override
    AdminState adminState = AdminState::Up;

    bool operator==(const Interface& o) const {
        return id == o.id && name == o.name && mac == o.mac &&
               ipv4 == o.ipv4 && mtuBytes == o.mtuBytes && adminState == o.adminState;
    }
};

struct Node {
    NodeId id;
    std::string name;
    NodeType type = NodeType::Host;
    AdminState adminState = AdminState::Up;
    std::vector<Interface> interfaces;

    // Host & CloudEndpoint
    std::optional<IPv4Address> defaultGateway;
    std::vector<Service> services;

    // Router
    bool forwardingEnabled = true;
    std::optional<double> processingDelayMs; // nullopt = not modeled

    // CloudEndpoint
    bool simulated = false;

    const Interface* findInterface(const InterfaceId& ifaceId) const;
    Interface* findInterface(const InterfaceId& ifaceId);

    bool operator==(const Node& o) const;
};

struct LinkEndpoint {
    NodeId nodeId;
    InterfaceId interfaceId;

    bool operator==(const LinkEndpoint& o) const {
        return nodeId == o.nodeId && interfaceId == o.interfaceId;
    }
    bool operator!=(const LinkEndpoint& o) const { return !(*this == o); }
};

struct Link {
    LinkId id;
    LinkEndpoint a;
    LinkEndpoint b;
    AdminState adminState = AdminState::Up;
    Directionality directionality = Directionality::Bidirectional;
    double cost = 1.0;
    BandwidthBps bandwidthBps = 100000000ULL;
    std::optional<BandwidthBps> availableBandwidthBps;
    DelayMs propagationDelayMs = 1.0;
    Probability packetLossProbability = 0.0;
    uint32_t mtuBytes = 1500;
    std::optional<std::string> queuePolicy; // Reserved, not modeled

    bool operator==(const Link& o) const;
};

/**
 * @brief Computes effective MTU: min(link.mtuBytes, endpoint overrides that exist).
 */
uint32_t effectiveMtu(const Link& link,
                     const std::optional<uint32_t>& overrideA = std::nullopt,
                     const std::optional<uint32_t>& overrideB = std::nullopt);

uint32_t effectiveMtu(const Link& link, const TopologyDraft& draft);
uint32_t effectiveMtu(const Link& link, const TopologySnapshot& snap);

/**
 * @brief FNV-1a 64-bit non-cryptographic hash (ADR-0027).
 */
uint64_t fnv1a64(std::string_view text);
std::string fnv1a64Hex(std::string_view text);

/**
 * @brief Mutable plain structs used for building and modifying topologies.
 */
struct TopologyDraft {
    std::string schemaVersion = "1.0.0";
    std::vector<Node> nodes;
    std::vector<Link> links;

    const Node* findNode(const NodeId& id) const;
    Node* findNode(const NodeId& id);
    const Interface* findInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const;
    Interface* findInterface(const NodeId& nodeId, const InterfaceId& ifaceId);
    const Link* findLink(const LinkId& id) const;
    Link* findLink(const LinkId& id);
};

/**
 * @brief Normalizes draft: sets CloudEndpoint simulated=true, assigns missing MACs per D-23.
 */
Result<void> assignMissingMacs(TopologyDraft& draft);
Result<void> normalize(TopologyDraft& draft);

/**
 * @brief Immutable validated topology snapshot with canonical content hash (ADR-0027).
 */
class TopologySnapshot {
public:
    static Result<TopologySnapshot> freeze(TopologyDraft draft, const ValidationOptions& options);
    static Result<TopologySnapshot> freeze(TopologyDraft draft);

    const std::string& schemaVersion() const { return schemaVersion_; }
    const std::vector<Node>& nodes() const { return nodes_; }
    const std::vector<Link>& links() const { return links_; }
    const std::string& canonicalText() const { return canonicalText_; }
    const std::string& contentHash() const { return contentHash_; }

    const Node* findNode(const NodeId& id) const;
    const Interface* findInterface(const NodeId& nodeId, const InterfaceId& ifaceId) const;
    const Link* findLink(const LinkId& id) const;

private:
    TopologySnapshot() = default;

    std::string schemaVersion_ = "1.0.0";
    std::vector<Node> nodes_;
    std::vector<Link> links_;
    std::string canonicalText_;
    std::string contentHash_;
};

} // namespace nettrace

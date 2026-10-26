#include "nettrace/model/validate.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace nettrace {

std::string_view issueSeverityToString(IssueSeverity sev) {
    switch (sev) {
        case IssueSeverity::Error: return "ERROR";
        case IssueSeverity::Warning: return "WARNING";
    }
    return "ERROR";
}

bool ValidationIssue::operator<(const ValidationIssue& o) const {
    if (severity != o.severity) {
        return severity == IssueSeverity::Error; // Error before Warning
    }
    if (path != o.path) {
        return path < o.path;
    }
    return code < o.code;
}

bool ValidationReport::hasErrors() const {
    return errorCount() > 0;
}

bool ValidationReport::hasWarnings() const {
    return warningCount() > 0;
}

size_t ValidationReport::errorCount() const {
    size_t count = 0;
    for (const auto& issue : issues) {
        if (issue.isError()) count++;
    }
    return count;
}

size_t ValidationReport::warningCount() const {
    size_t count = 0;
    for (const auto& issue : issues) {
        if (issue.isWarning()) count++;
    }
    return count;
}

void ValidationReport::addIssue(ValidationIssue issue) {
    issues.push_back(std::move(issue));
}

void ValidationReport::addError(std::string code, std::string path, std::string msg, std::string hint) {
    issues.push_back(ValidationIssue{IssueSeverity::Error, std::move(code), std::move(path), std::move(msg), std::move(hint)});
}

void ValidationReport::addWarning(std::string code, std::string path, std::string msg, std::string hint) {
    issues.push_back(ValidationIssue{IssueSeverity::Warning, std::move(code), std::move(path), std::move(msg), std::move(hint)});
}

void ValidationReport::sort() {
    std::sort(issues.begin(), issues.end());
}

std::string ValidationReport::summary() const {
    std::ostringstream ss;
    ss << "ValidationReport: " << errorCount() << " error(s), " << warningCount() << " warning(s)";
    for (const auto& issue : issues) {
        ss << "\n  [" << issueSeverityToString(issue.severity) << " " << issue.code << "] "
           << issue.path << ": " << issue.message;
        if (!issue.hint.empty()) {
            ss << " (Hint: " << issue.hint << ")";
        }
    }
    return ss.str();
}

static bool isValidId(std::string_view id) {
    if (id.empty() || id.size() > 64) return false;
    for (char c : id) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) {
            return false;
        }
    }
    return true;
}

static bool isValidName(std::string_view name) {
    if (name.size() > 64) return false;
    for (unsigned char c : name) {
        if (c < 32 || c == 127) return false;
    }
    return true;
}

ValidationReport validate(const TopologyDraft& draft, const ValidationOptions& options) {
    ValidationReport report;

    // Rule 11 (Empty topology)
    if (draft.nodes.empty()) {
        report.addError("EMPTY_TOPOLOGY", "nodes", "Topology contains zero nodes");
    }

    // Rule 12 (Limits)
    if (draft.nodes.size() > options.limits.maxNodes) {
        report.addError("LIMIT_EXCEEDED", "nodes",
            "Node count " + std::to_string(draft.nodes.size()) + " exceeds limit " + std::to_string(options.limits.maxNodes));
    }
    if (draft.links.size() > options.limits.maxLinks) {
        report.addError("LIMIT_EXCEEDED", "links",
            "Link count " + std::to_string(draft.links.size()) + " exceeds limit " + std::to_string(options.limits.maxLinks));
    }

    std::set<std::string> seenNodeIds;
    std::set<std::string> seenLinkIds;
    std::set<MacAddress> seenMacs;
    std::map<std::pair<std::string, std::string>, std::string> linkedInterfaces; // (nodeId, ifaceId) -> linkId

    // Map for node lookup
    std::unordered_map<std::string, const Node*> nodeMap;

    // Iterate over nodes
    for (const auto& node : draft.nodes) {
        std::string nodePath = "nodes/" + node.id.str();

        // Rule 1: Node ID & Name
        if (!isValidId(node.id.str())) {
            report.addError("INVALID_ID", nodePath, "Node ID '" + node.id.str() + "' must be 1..64 chars from [A-Za-z0-9_.-]");
        }
        if (!isValidName(node.name)) {
            report.addError("INVALID_NAME", nodePath + "/name", "Node name exceeds 64 chars or contains control characters");
        }

        // Rule 2: Duplicate Node ID
        if (!seenNodeIds.insert(node.id.str()).second) {
            report.addError("DUPLICATE_NODE_ID", nodePath, "Duplicate node ID: '" + node.id.str() + "'");
        }
        nodeMap[node.id.str()] = &node;

        // Rule 12: Interfaces limit
        if (node.interfaces.size() > options.limits.maxInterfacesPerNode) {
            report.addError("LIMIT_EXCEEDED", nodePath + "/interfaces",
                "Node '" + node.id.str() + "' interface count " + std::to_string(node.interfaces.size()) +
                " exceeds limit " + std::to_string(options.limits.maxInterfacesPerNode));
        }

        // Rule 11: Structure (host/cloud with zero interfaces; router with one interface)
        if ((node.type == NodeType::Host || node.type == NodeType::CloudEndpoint) && node.interfaces.empty()) {
            report.addError("INVALID_CONFIGURATION", nodePath, "Host or cloud endpoint must have at least one interface");
        }
        if (node.type == NodeType::Router && node.interfaces.size() == 1) {
            report.addWarning("ROUTER_SINGLE_INTERFACE", nodePath, "Router has only one interface; packets cannot be routed across networks");
        }

        // Rule 10: Services allowed only on Host and CloudEndpoint
        if ((node.type == NodeType::Switch || node.type == NodeType::Router) && !node.services.empty()) {
            report.addError("INVALID_CONFIGURATION", nodePath + "/services",
                "Services can only be configured on Host or CloudEndpoint nodes");
        }

        // Services validation
        if (node.type == NodeType::Host || node.type == NodeType::CloudEndpoint) {
            std::set<std::pair<TransportProtocol, uint16_t>> seenServices;
            for (const auto& svc : node.services) {
                if (svc.port == 0) {
                    report.addError("INVALID_PORT", nodePath + "/services",
                        "Service port 0 is outside valid range 1..65535");
                }
                if (!seenServices.insert({svc.protocol, svc.port}).second) {
                    report.addError("DUPLICATE_SERVICE", nodePath + "/services",
                        "Duplicate service (" + std::string(transportProtocolToString(svc.protocol)) + ", " +
                        std::to_string(svc.port) + ") on host '" + node.id.str() + "'");
                }
            }
        }

        // Interfaces validation
        std::set<std::string> seenIfaceIdsInNode;
        std::vector<IPv4Subnet> nodeSubnets;

        for (const auto& iface : node.interfaces) {
            std::string ifacePath = nodePath + "/interfaces/" + iface.id.str();

            // Rule 1: Interface ID & Name
            if (!isValidId(iface.id.str())) {
                report.addError("INVALID_ID", ifacePath,
                    "Interface ID '" + iface.id.str() + "' must be 1..64 chars from [A-Za-z0-9_.-]");
            }
            if (!isValidName(iface.name)) {
                report.addError("INVALID_NAME", ifacePath + "/name",
                    "Interface name exceeds 64 chars or contains control characters");
            }

            // Rule 2: Duplicate Interface ID on same node
            if (!seenIfaceIdsInNode.insert(iface.id.str()).second) {
                report.addError("DUPLICATE_INTERFACE_ID", ifacePath,
                    "Duplicate interface ID '" + iface.id.str() + "' on node '" + node.id.str() + "'");
            }

            // Rule 8: MTU override on interface
            if (iface.mtuBytes.has_value()) {
                if (*iface.mtuBytes < 68 || *iface.mtuBytes > 65535) {
                    report.addError("INVALID_MTU", ifacePath + "/mtuBytes",
                        "Interface MTU " + std::to_string(*iface.mtuBytes) + " must be in range 68..65535");
                }
            }

            // Switch port rules (D-23: Switch ports have NO IP and NO MAC)
            if (node.type == NodeType::Switch) {
                if (iface.mac.has_value()) {
                    report.addError("INVALID_CONFIGURATION", ifacePath, "Switch port cannot have a MAC address");
                }
                if (iface.ipv4.has_value()) {
                    report.addError("INVALID_CONFIGURATION", ifacePath, "Switch port cannot have an IP address");
                }
            } else {
                // Host / Router / CloudEndpoint MAC rules
                if (!iface.mac.has_value()) {
                    report.addError("MISSING_MAC", ifacePath + "/mac",
                        "Interface '" + iface.id.str() + "' on node '" + node.id.str() + "' requires a MAC address");
                } else {
                    if (!iface.mac->isValidInterfaceMac()) {
                        report.addError("INVALID_MAC", ifacePath + "/mac",
                            "MAC address '" + iface.mac->toString() + "' is not a valid unicast interface MAC");
                    }
                    if (!seenMacs.insert(*iface.mac).second) {
                        report.addError("DUPLICATE_MAC", ifacePath + "/mac",
                            "MAC address '" + iface.mac->toString() + "' is duplicated in topology");
                    }
                }
            }

            // Rule 6: Interface IPv4 validation
            if (iface.ipv4.has_value()) {
                const auto& subnet = *iface.ipv4;
                if (!subnet.isValidHost(subnet.address())) {
                    report.addError("INVALID_HOST_ADDRESS", ifacePath + "/ipv4",
                        "IP address '" + subnet.address().toString() + "' in subnet '" + subnet.toString() +
                        "' is a network or broadcast address, not a valid host address per D-15");
                }
                nodeSubnets.push_back(subnet);
            }
        }

        // Rule 11: Overlapping subnets warning on router
        for (size_t i = 0; i < nodeSubnets.size(); ++i) {
            for (size_t j = i + 1; j < nodeSubnets.size(); ++j) {
                if (nodeSubnets[i].overlaps(nodeSubnets[j])) {
                    report.addWarning("OVERLAPPING_SUBNETS", nodePath + "/interfaces",
                        "Interfaces on node '" + node.id.str() + "' have overlapping subnets: " +
                        nodeSubnets[i].toString() + " and " + nodeSubnets[j].toString());
                }
            }
        }

        // Rule 7: Host/Cloud defaultGateway validation
        if ((node.type == NodeType::Host || node.type == NodeType::CloudEndpoint) && node.defaultGateway.has_value()) {
            const auto& gw = *node.defaultGateway;
            std::string gwPath = nodePath + "/defaultGateway";

            bool foundOnLinkSubnet = false;
            bool equalsOwnIp = false;
            bool isInvalidHostOnSubnet = false;

            for (const auto& iface : node.interfaces) {
                if (!iface.ipv4.has_value()) continue;
                const auto& sub = *iface.ipv4;

                if (sub.address() == gw) {
                    equalsOwnIp = true;
                }

                if (sub.contains(gw)) {
                    foundOnLinkSubnet = true;
                    if (!sub.isValidHost(gw)) {
                        isInvalidHostOnSubnet = true;
                    }
                }
            }

            if (equalsOwnIp) {
                report.addError("INVALID_GATEWAY", gwPath, "Default gateway '" + gw.toString() + "' equals host's own IP address");
            } else if (foundOnLinkSubnet) {
                if (isInvalidHostOnSubnet) {
                    report.addError("INVALID_GATEWAY", gwPath, "Default gateway '" + gw.toString() + "' is a network or broadcast address");
                }
            } else {
                report.addError("GATEWAY_UNREACHABLE", gwPath,
                    "Default gateway '" + gw.toString() + "' is not on-link via any interface of node '" + node.id.str() + "'");
            }
        }
    }

    // Links validation
    for (const auto& link : draft.links) {
        std::string linkPath = "links/" + link.id.str();

        // Rule 1: Link ID
        if (!isValidId(link.id.str())) {
            report.addError("INVALID_ID", linkPath, "Link ID '" + link.id.str() + "' must be 1..64 chars from [A-Za-z0-9_.-]");
        }

        // Rule 2: Duplicate Link ID
        if (!seenLinkIds.insert(link.id.str()).second) {
            report.addError("DUPLICATE_LINK_ID", linkPath, "Duplicate link ID: '" + link.id.str() + "'");
        }

        // Rule 5: Self loops
        if (link.a.nodeId == link.b.nodeId) {
            report.addError("SELF_LOOP_REJECTED", linkPath,
                "Link endpoints both attach to node '" + link.a.nodeId.str() + "' (self-loops not allowed)");
        }

        // Rule 3: Endpoints exist
        const auto itA = nodeMap.find(link.a.nodeId.str());
        if (itA == nodeMap.end()) {
            report.addError("LINK_ENDPOINT_NOT_FOUND", linkPath + "/endpointA/node",
                "Endpoint A node '" + link.a.nodeId.str() + "' does not exist");
        } else {
            if (!itA->second->findInterface(link.a.interfaceId)) {
                report.addError("LINK_ENDPOINT_NOT_FOUND", linkPath + "/endpointA/interface",
                    "Endpoint A interface '" + link.a.interfaceId.str() + "' does not exist on node '" + link.a.nodeId.str() + "'");
            }
        }

        const auto itB = nodeMap.find(link.b.nodeId.str());
        if (itB == nodeMap.end()) {
            report.addError("LINK_ENDPOINT_NOT_FOUND", linkPath + "/endpointB/node",
                "Endpoint B node '" + link.b.nodeId.str() + "' does not exist");
        } else {
            if (!itB->second->findInterface(link.b.interfaceId)) {
                report.addError("LINK_ENDPOINT_NOT_FOUND", linkPath + "/endpointB/interface",
                    "Endpoint B interface '" + link.b.interfaceId.str() + "' does not exist on node '" + link.b.nodeId.str() + "'");
            }
        }

        // Rule 4: Interface already linked
        auto keyA = std::make_pair(link.a.nodeId.str(), link.a.interfaceId.str());
        auto insA = linkedInterfaces.emplace(keyA, link.id.str());
        if (!insA.second) {
            report.addError("INTERFACE_ALREADY_LINKED", linkPath + "/endpointA",
                "Interface '" + link.a.interfaceId.str() + "' on node '" + link.a.nodeId.str() +
                "' is already connected to link '" + insA.first->second + "'");
        }

        auto keyB = std::make_pair(link.b.nodeId.str(), link.b.interfaceId.str());
        auto insB = linkedInterfaces.emplace(keyB, link.id.str());
        if (!insB.second) {
            report.addError("INTERFACE_ALREADY_LINKED", linkPath + "/endpointB",
                "Interface '" + link.b.interfaceId.str() + "' on node '" + link.b.nodeId.str() +
                "' is already connected to link '" + insB.first->second + "'");
        }

        // Rule 8: Link parameters
        if (std::isnan(link.cost) || std::isinf(link.cost)) {
            report.addError("INVALID_WEIGHT", linkPath + "/cost", "Link cost is NaN or infinite");
        } else if (link.cost < 0.0 && !options.allowNegativeCosts) {
            report.addError("NEGATIVE_WEIGHT", linkPath + "/cost", "Link cost " + std::to_string(link.cost) + " is negative");
        }

        if (link.bandwidthBps == 0) {
            report.addError("INVALID_BANDWIDTH", linkPath + "/bandwidthBps", "Link bandwidth must be strictly positive");
        }
        if (link.availableBandwidthBps.has_value() && *link.availableBandwidthBps > link.bandwidthBps) {
            report.addError("INVALID_BANDWIDTH", linkPath + "/availableBandwidthBps",
                "Available bandwidth " + std::to_string(*link.availableBandwidthBps) +
                " exceeds total bandwidth " + std::to_string(link.bandwidthBps));
        }

        if (std::isnan(link.propagationDelayMs) || std::isinf(link.propagationDelayMs) || link.propagationDelayMs < 0.0) {
            report.addError("INVALID_DELAY", linkPath + "/propagationDelayMs",
                "Link delay " + std::to_string(link.propagationDelayMs) + " cannot be negative or NaN/infinite");
        }

        if (std::isnan(link.packetLossProbability) || link.packetLossProbability < 0.0 || link.packetLossProbability > 1.0) {
            report.addError("INVALID_PROBABILITY", linkPath + "/packetLossProbability",
                "Packet loss probability " + std::to_string(link.packetLossProbability) + " must be in [0.0, 1.0]");
        }

        if (link.mtuBytes < 68 || link.mtuBytes > 65535) {
            report.addError("INVALID_MTU", linkPath + "/mtuBytes",
                "Link MTU " + std::to_string(link.mtuBytes) + " must be in range 68..65535");
        }
    }

    report.sort();
    return report;
}

} // namespace nettrace

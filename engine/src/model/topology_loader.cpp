#include "nettrace/model/topology_io.hpp"
#include "nettrace/model/mac.hpp"
#include "nettrace/model/ipv4.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "nlohmann/json.hpp"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace nettrace {

using json = nlohmann::json;

Result<TopologyDraft> loadTopologyJson(std::string_view jsonStr) {
    try {
        // 1. Size limit enforcement (2 MB per D-16)
        constexpr size_t MAX_INPUT_SIZE = 2 * 1024 * 1024;
        if (jsonStr.size() > MAX_INPUT_SIZE) {
            return Result<TopologyDraft>::err(
                ErrorCode::LimitExceeded,
                "Input payload size (" + std::to_string(jsonStr.size()) + " bytes) exceeds 2 MB limit",
                "");
        }

        // 2. Non-recursive nesting depth check (max depth 64 per D-16 / ADR-0033)
        size_t depth = 0;
        bool inString = false;
        bool escape = false;
        for (char c : jsonStr) {
            if (inString) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    inString = false;
                }
            } else {
                if (c == '"') {
                    inString = true;
                } else if (c == '{' || c == '[') {
                    depth++;
                    if (depth > 64) {
                        return Result<TopologyDraft>::err(
                            ErrorCode::LimitExceeded,
                            "JSON nesting depth exceeds 64",
                            "");
                    }
                } else if (c == '}' || c == ']') {
                    if (depth > 0) {
                        depth--;
                    }
                }
            }
        }

        // 3. Parse JSON non-throwing
        json j = json::parse(jsonStr.begin(), jsonStr.end(), nullptr, false);
        if (j.is_discarded()) {
            return Result<TopologyDraft>::err(
                ErrorCode::InvalidJson,
                "Malformed JSON syntax or truncated input",
                "");
        }

        if (!j.is_object()) {
            return Result<TopologyDraft>::err(
                ErrorCode::InvalidFieldType,
                "Root must be a JSON object",
                "");
        }

        // 4. Schema version verification
        if (!j.contains("schemaVersion")) {
            return Result<TopologyDraft>::err(
                ErrorCode::MissingField,
                "Missing required field 'schemaVersion'",
                "schemaVersion");
        }
        if (!j["schemaVersion"].is_string()) {
            return Result<TopologyDraft>::err(
                ErrorCode::InvalidFieldType,
                "Field 'schemaVersion' must be a string",
                "schemaVersion");
        }
        std::string schemaVer = j["schemaVersion"].get<std::string>();
        size_t dotPos = schemaVer.find('.');
        std::string majorStr = (dotPos == std::string::npos) ? schemaVer : schemaVer.substr(0, dotPos);
        if (majorStr != "1") {
            return Result<TopologyDraft>::err(
                ErrorCode::UnsupportedSchemaVersion,
                "Unsupported schemaVersion '" + schemaVer + "' (major version must be 1)",
                "schemaVersion");
        }

        // 5. Root required fields
        if (!j.contains("id")) {
            return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'id'", "id");
        }
        if (!j["id"].is_string()) {
            return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Field 'id' must be a string", "id");
        }

        if (!j.contains("name")) {
            return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'name'", "name");
        }
        if (!j["name"].is_string()) {
            return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Field 'name' must be a string", "name");
        }

        if (!j.contains("nodes")) {
            return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'nodes'", "nodes");
        }
        if (!j["nodes"].is_array()) {
            return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Field 'nodes' must be an array", "nodes");
        }

        if (!j.contains("links")) {
            return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'links'", "links");
        }
        if (!j["links"].is_array()) {
            return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Field 'links' must be an array", "links");
        }

        TopologyDraft draft;
        draft.schemaVersion = schemaVer;
        draft.id = j["id"].get<std::string>();
        draft.name = j["name"].get<std::string>();
        if (j.contains("description") && j["description"].is_string()) {
            draft.description = j["description"].get<std::string>();
        }

        // 6. Parse Nodes
        const auto& nodesArr = j["nodes"];
        for (size_t nIdx = 0; nIdx < nodesArr.size(); ++nIdx) {
            const auto& nVal = nodesArr[nIdx];
            std::string nPath = "nodes/" + std::to_string(nIdx);

            if (!nVal.is_object()) {
                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Node entry must be an object", nPath);
            }

            if (!nVal.contains("id")) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'id'", nPath + "/id");
            }
            if (!nVal["id"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Node 'id' must be a string", nPath + "/id");
            }

            if (!nVal.contains("name")) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'name'", nPath + "/name");
            }
            if (!nVal["name"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Node 'name' must be a string", nPath + "/name");
            }

            if (!nVal.contains("type")) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'type'", nPath + "/type");
            }
            if (!nVal["type"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Node 'type' must be a string", nPath + "/type");
            }

            std::string typeStr = nVal["type"].get<std::string>();
            NodeType nodeType = NodeType::Host;
            if (typeStr == "host") {
                nodeType = NodeType::Host;
            } else if (typeStr == "router") {
                nodeType = NodeType::Router;
            } else if (typeStr == "switch") {
                nodeType = NodeType::Switch;
            } else if (typeStr == "cloudEndpoint" || typeStr == "cloud") {
                nodeType = NodeType::CloudEndpoint;
            } else {
                return Result<TopologyDraft>::err(
                    ErrorCode::InvalidEnumValue,
                    "Unknown node type '" + typeStr + "'",
                    nPath + "/type");
            }

            Node node;
            node.id = NodeId(nVal["id"].get<std::string>());
            node.name = nVal["name"].get<std::string>();
            node.type = nodeType;

            // AdminState
            if (nVal.contains("adminState")) {
                if (!nVal["adminState"].is_string()) {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "adminState must be a string", nPath + "/adminState");
                }
                std::string st = nVal["adminState"].get<std::string>();
                if (st == "up") {
                    node.adminState = AdminState::Up;
                } else if (st == "down") {
                    node.adminState = AdminState::Down;
                } else {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidEnumValue, "Invalid adminState '" + st + "'", nPath + "/adminState");
                }
            } else if (nVal.contains("active") && nVal["active"].is_boolean()) {
                node.adminState = nVal["active"].get<bool>() ? AdminState::Up : AdminState::Down;
            }

            // ForwardingEnabled
            if (nVal.contains("forwardingEnabled") && nVal["forwardingEnabled"].is_boolean()) {
                node.forwardingEnabled = nVal["forwardingEnabled"].get<bool>();
            } else {
                node.forwardingEnabled = (nodeType == NodeType::Router);
            }

            // DefaultGateway
            if (nVal.contains("defaultGateway") && !nVal["defaultGateway"].is_null()) {
                if (!nVal["defaultGateway"].is_string()) {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "defaultGateway must be a string", nPath + "/defaultGateway");
                }
                std::string gwStr = nVal["defaultGateway"].get<std::string>();
                if (!gwStr.empty()) {
                    auto gwParsed = IPv4Address::parse(gwStr);
                    if (!gwParsed.isOk()) {
                        return Result<TopologyDraft>::err(ErrorCode::InvalidAddress, "Invalid default gateway address '" + gwStr + "'", nPath + "/defaultGateway");
                    }
                    node.defaultGateway = gwParsed.value();
                }
            }

            // Processing delay
            if (nVal.contains("processingDelayMs") && nVal["processingDelayMs"].is_number()) {
                node.processingDelayMs = nVal["processingDelayMs"].get<double>();
            }

            // Services
            if (nVal.contains("services") && nVal["services"].is_array()) {
                for (size_t sIdx = 0; sIdx < nVal["services"].size(); ++sIdx) {
                    const auto& sVal = nVal["services"][sIdx];
                    std::string sPath = nPath + "/services/" + std::to_string(sIdx);
                    if (!sVal.is_object()) continue;

                    Service svc;
                    if (sVal.contains("protocol") && sVal["protocol"].is_string()) {
                        std::string pStr = sVal["protocol"].get<std::string>();
                        if (pStr == "UDP") svc.protocol = TransportProtocol::UDP;
                        else if (pStr == "TCP") svc.protocol = TransportProtocol::TCP;
                        else svc.protocol = TransportProtocol::ICMP;
                    }
                    if (sVal.contains("port") && sVal["port"].is_number_integer()) {
                        svc.port = static_cast<uint16_t>(sVal["port"].get<int>());
                    }
                    if (sVal.contains("label") && sVal["label"].is_string()) {
                        svc.label = sVal["label"].get<std::string>();
                    }
                    node.services.push_back(svc);
                }
            }

            // StaticRoutes
            if (nVal.contains("staticRoutes") && nVal["staticRoutes"].is_array()) {
                for (size_t rIdx = 0; rIdx < nVal["staticRoutes"].size(); ++rIdx) {
                    const auto& rVal = nVal["staticRoutes"][rIdx];
                    std::string rPath = nPath + "/staticRoutes/" + std::to_string(rIdx);
                    if (!rVal.is_object()) continue;

                    if (!rVal.contains("prefix") || !rVal["prefix"].is_string()) {
                        return Result<TopologyDraft>::err(ErrorCode::MissingField, "Static route missing prefix", rPath + "/prefix");
                    }
                    auto prefRes = IPv4Subnet::parse(rVal["prefix"].get<std::string>());
                    if (!prefRes.isOk()) {
                        return Result<TopologyDraft>::err(ErrorCode::InvalidPrefix, "Invalid static route prefix", rPath + "/prefix");
                    }

                    StaticRoute sr;
                    sr.prefix = prefRes.value();
                    if (rVal.contains("nextHop") && !rVal["nextHop"].is_null() && rVal["nextHop"].is_string()) {
                        auto nhRes = IPv4Address::parse(rVal["nextHop"].get<std::string>());
                        if (!nhRes.isOk()) {
                            return Result<TopologyDraft>::err(ErrorCode::InvalidAddress, "Invalid static route nextHop", rPath + "/nextHop");
                        }
                        sr.nextHop = nhRes.value();
                    }
                    if (rVal.contains("outInterface") && !rVal["outInterface"].is_null() && rVal["outInterface"].is_string()) {
                        sr.outInterface = InterfaceId(rVal["outInterface"].get<std::string>());
                    }
                    if (rVal.contains("adminPreference") && rVal["adminPreference"].is_number_integer()) {
                        sr.adminPreference = static_cast<uint32_t>(rVal["adminPreference"].get<int>());
                    }
                    if (rVal.contains("metric") && rVal["metric"].is_number()) {
                        sr.metric = rVal["metric"].get<double>();
                    }
                    node.staticRoutes.push_back(sr);
                }
            }

            // Interfaces
            if (!nVal.contains("interfaces")) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Missing required field 'interfaces'", nPath + "/interfaces");
            }
            if (!nVal["interfaces"].is_array()) {
                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Field 'interfaces' must be an array", nPath + "/interfaces");
            }

            const auto& ifArr = nVal["interfaces"];
            for (size_t iIdx = 0; iIdx < ifArr.size(); ++iIdx) {
                const auto& iVal = ifArr[iIdx];
                std::string iPath = nPath + "/interfaces/" + std::to_string(iIdx);

                if (!iVal.is_object()) {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Interface entry must be an object", iPath);
                }

                if (!iVal.contains("id")) {
                    return Result<TopologyDraft>::err(ErrorCode::MissingField, "Interface missing required field 'id'", iPath + "/id");
                }
                if (!iVal["id"].is_string()) {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Interface 'id' must be a string", iPath + "/id");
                }

                if (!iVal.contains("name")) {
                    return Result<TopologyDraft>::err(ErrorCode::MissingField, "Interface missing required field 'name'", iPath + "/name");
                }
                if (!iVal["name"].is_string()) {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Interface 'name' must be a string", iPath + "/name");
                }

                Interface iface;
                iface.id = InterfaceId(iVal["id"].get<std::string>());
                iface.name = iVal["name"].get<std::string>();

                // AdminState
                if (iVal.contains("adminState")) {
                    if (!iVal["adminState"].is_string()) {
                        return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "adminState must be a string", iPath + "/adminState");
                    }
                    std::string st = iVal["adminState"].get<std::string>();
                    if (st == "up") {
                        iface.adminState = AdminState::Up;
                    } else if (st == "down") {
                        iface.adminState = AdminState::Down;
                    } else {
                        return Result<TopologyDraft>::err(ErrorCode::InvalidEnumValue, "Invalid interface adminState '" + st + "'", iPath + "/adminState");
                    }
                } else if (iVal.contains("active") && iVal["active"].is_boolean()) {
                    iface.adminState = iVal["active"].get<bool>() ? AdminState::Up : AdminState::Down;
                }

                // MAC Address: check "mac" or "macAddress"
                std::string macKey = iVal.contains("mac") ? "mac" : (iVal.contains("macAddress") ? "macAddress" : "");
                if (!macKey.empty() && !iVal[macKey].is_null()) {
                    if (!iVal[macKey].is_string()) {
                        return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "MAC address must be a string", iPath + "/" + macKey);
                    }
                    std::string macStr = iVal[macKey].get<std::string>();
                    if (!macStr.empty()) {
                        auto macParsed = MacAddress::parse(macStr);
                        if (!macParsed.isOk()) {
                            return Result<TopologyDraft>::err(ErrorCode::InvalidMac, "Invalid MAC address format '" + macStr + "'", iPath + "/" + macKey);
                        }
                        iface.mac = macParsed.value();
                    }
                }

                // IPv4: check "ipv4" string or pair of ("ipv4Address", "prefixLength")
                if (iVal.contains("ipv4") && !iVal["ipv4"].is_null()) {
                    if (!iVal["ipv4"].is_string()) {
                        return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "ipv4 must be a string", iPath + "/ipv4");
                    }
                    std::string cidrStr = iVal["ipv4"].get<std::string>();
                    if (!cidrStr.empty()) {
                        auto parsedSubnet = IPv4Subnet::parse(cidrStr);
                        if (!parsedSubnet.isOk()) {
                            return Result<TopologyDraft>::err(parsedSubnet.error().code, parsedSubnet.error().message, iPath + "/ipv4");
                        }
                        iface.ipv4 = parsedSubnet.value();
                    }
                } else if (iVal.contains("ipv4Address") && !iVal["ipv4Address"].is_null()) {
                    if (!iVal["ipv4Address"].is_string()) {
                        return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "ipv4Address must be a string", iPath + "/ipv4Address");
                    }
                    std::string ipStr = iVal["ipv4Address"].get<std::string>();
                    if (!ipStr.empty()) {
                        auto ipParsed = IPv4Address::parse(ipStr);
                        if (!ipParsed.isOk()) {
                            return Result<TopologyDraft>::err(ErrorCode::InvalidAddress, "Invalid IPv4 address '" + ipStr + "'", iPath + "/ipv4Address");
                        }

                        int pref = 24;
                        if (iVal.contains("prefixLength") && !iVal["prefixLength"].is_null()) {
                            if (!iVal["prefixLength"].is_number_integer()) {
                                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "prefixLength must be an integer", iPath + "/prefixLength");
                            }
                            pref = iVal["prefixLength"].get<int>();
                            if (pref < 0 || pref > 32) {
                                return Result<TopologyDraft>::err(ErrorCode::InvalidPrefix, "prefixLength must be between 0 and 32", iPath + "/prefixLength");
                            }
                        }
                        iface.ipv4 = IPv4Subnet(ipParsed.value(), static_cast<uint8_t>(pref));
                    }
                }

                // MTU
                if (iVal.contains("mtuBytes") && iVal["mtuBytes"].is_number_integer()) {
                    iface.mtuBytes = static_cast<uint32_t>(iVal["mtuBytes"].get<int>());
                }

                node.interfaces.push_back(iface);
            }

            draft.nodes.push_back(node);
        }

        // 7. Parse Links
        const auto& linksArr = j["links"];
        for (size_t lIdx = 0; lIdx < linksArr.size(); ++lIdx) {
            const auto& lVal = linksArr[lIdx];
            std::string lPath = "links/" + std::to_string(lIdx);

            if (!lVal.is_object()) {
                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Link entry must be an object", lPath);
            }

            if (!lVal.contains("id")) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Link missing required field 'id'", lPath + "/id");
            }
            if (!lVal["id"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "Link 'id' must be a string", lPath + "/id");
            }

            if (!lVal.contains("a") || !lVal["a"].is_object()) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Link missing required endpoint 'a'", lPath + "/a");
            }
            if (!lVal["a"].contains("nodeId") || !lVal["a"]["nodeId"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Link endpoint 'a' missing nodeId", lPath + "/a/nodeId");
            }
            if (!lVal["a"].contains("interfaceId") || !lVal["a"]["interfaceId"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Link endpoint 'a' missing interfaceId", lPath + "/a/interfaceId");
            }

            if (!lVal.contains("b") || !lVal["b"].is_object()) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Link missing required endpoint 'b'", lPath + "/b");
            }
            if (!lVal["b"].contains("nodeId") || !lVal["b"]["nodeId"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Link endpoint 'b' missing nodeId", lPath + "/b/nodeId");
            }
            if (!lVal["b"].contains("interfaceId") || !lVal["b"]["interfaceId"].is_string()) {
                return Result<TopologyDraft>::err(ErrorCode::MissingField, "Link endpoint 'b' missing interfaceId", lPath + "/b/interfaceId");
            }

            Link link;
            link.id = LinkId(lVal["id"].get<std::string>());
            link.a = {NodeId(lVal["a"]["nodeId"].get<std::string>()), InterfaceId(lVal["a"]["interfaceId"].get<std::string>())};
            link.b = {NodeId(lVal["b"]["nodeId"].get<std::string>()), InterfaceId(lVal["b"]["interfaceId"].get<std::string>())};

            // AdminState
            if (lVal.contains("adminState")) {
                if (!lVal["adminState"].is_string()) {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "adminState must be a string", lPath + "/adminState");
                }
                std::string st = lVal["adminState"].get<std::string>();
                if (st == "up") {
                    link.adminState = AdminState::Up;
                } else if (st == "down") {
                    link.adminState = AdminState::Down;
                } else {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidEnumValue, "Invalid link adminState '" + st + "'", lPath + "/adminState");
                }
            }

            // Directionality
            if (lVal.contains("directionality")) {
                if (!lVal["directionality"].is_string()) {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidFieldType, "directionality must be a string", lPath + "/directionality");
                }
                std::string dir = lVal["directionality"].get<std::string>();
                if (dir == "bidirectional") {
                    link.directionality = Directionality::Bidirectional;
                } else if (dir == "directed") {
                    link.directionality = Directionality::Directed;
                } else {
                    return Result<TopologyDraft>::err(ErrorCode::InvalidEnumValue, "Invalid directionality '" + dir + "'", lPath + "/directionality");
                }
            }

            // Cost
            if (lVal.contains("cost") && lVal["cost"].is_number()) {
                link.cost = lVal["cost"].get<double>();
            }

            // Bandwidth
            if (lVal.contains("bandwidthBps") && lVal["bandwidthBps"].is_number_integer()) {
                link.bandwidthBps = static_cast<uint64_t>(lVal["bandwidthBps"].get<int64_t>());
            }

            // Available Bandwidth
            if (lVal.contains("availableBandwidthBps") && !lVal["availableBandwidthBps"].is_null()) {
                if (lVal["availableBandwidthBps"].is_number_integer()) {
                    link.availableBandwidthBps = static_cast<uint64_t>(lVal["availableBandwidthBps"].get<int64_t>());
                }
            }

            // Delay
            if (lVal.contains("propagationDelayMs") && lVal["propagationDelayMs"].is_number()) {
                link.propagationDelayMs = lVal["propagationDelayMs"].get<double>();
            }

            // Loss
            if (lVal.contains("packetLossProbability") && lVal["packetLossProbability"].is_number()) {
                link.packetLossProbability = lVal["packetLossProbability"].get<double>();
            }

            // MTU
            if (lVal.contains("mtuBytes") && lVal["mtuBytes"].is_number_integer()) {
                link.mtuBytes = static_cast<uint32_t>(lVal["mtuBytes"].get<int>());
            }

            draft.links.push_back(link);
        }

        return Result<TopologyDraft>::ok(draft);

    } catch (const std::exception& e) {
        return Result<TopologyDraft>::err(
            ErrorCode::InvalidJson,
            std::string("Exception during topology parsing: ") + e.what(),
            "");
    } catch (...) {
        return Result<TopologyDraft>::err(
            ErrorCode::InvalidJson,
            "Unknown exception during topology parsing",
            "");
    }
}

} // namespace nettrace

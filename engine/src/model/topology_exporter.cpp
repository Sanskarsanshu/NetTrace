#include "nettrace/model/topology_io.hpp"
#include "nettrace/model/mac.hpp"

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
#include <iomanip>
#include <sstream>

namespace nettrace {

using ordered_json = nlohmann::ordered_json;

std::string toJson(const TopologySnapshot& snap) {
    ordered_json root;
    root["schemaVersion"] = snap.schemaVersion();
    root["id"] = snap.id();
    root["name"] = snap.name();
    if (!snap.description().empty()) {
        root["description"] = snap.description();
    }

    // Nodes (already sorted by NodeId in TopologySnapshot)
    ordered_json nodesArr = ordered_json::array();
    for (const auto& node : snap.nodes()) {
        ordered_json nObj;
        nObj["id"] = node.id.str();
        nObj["name"] = node.name;
        nObj["type"] = std::string(nodeTypeToString(node.type));
        nObj["adminState"] = (node.adminState == AdminState::Up) ? "up" : "down";

        if (node.defaultGateway.has_value()) {
            nObj["defaultGateway"] = node.defaultGateway->toString();
        } else {
            nObj["defaultGateway"] = nullptr;
        }

        if (node.type == NodeType::Router) {
            nObj["forwardingEnabled"] = node.forwardingEnabled;
        }

        if (!node.services.empty()) {
            ordered_json svcsArr = ordered_json::array();
            for (const auto& s : node.services) {
                ordered_json sObj;
                sObj["protocol"] = std::string(transportProtocolToString(s.protocol));
                sObj["port"] = s.port;
                sObj["label"] = s.label;
                svcsArr.push_back(std::move(sObj));
            }
            nObj["services"] = std::move(svcsArr);
        }

        if (!node.staticRoutes.empty()) {
            ordered_json routesArr = ordered_json::array();
            for (const auto& r : node.staticRoutes) {
                ordered_json rObj;
                rObj["prefix"] = r.prefix.toString();
                if (r.nextHop.has_value()) {
                    rObj["nextHop"] = r.nextHop->toString();
                } else {
                    rObj["nextHop"] = nullptr;
                }
                if (r.outInterface.has_value()) {
                    rObj["outInterface"] = r.outInterface->str();
                } else {
                    rObj["outInterface"] = nullptr;
                }
                rObj["adminPreference"] = r.adminPreference;
                rObj["metric"] = static_cast<int64_t>(r.metric);
                routesArr.push_back(std::move(rObj));
            }
            nObj["staticRoutes"] = std::move(routesArr);
        }

        // Interfaces (sorted by InterfaceId)
        auto ifaces = node.interfaces;
        std::sort(ifaces.begin(), ifaces.end(), [](const Interface& a, const Interface& b) {
            return a.id < b.id;
        });

        ordered_json ifacesArr = ordered_json::array();
        for (const auto& iface : ifaces) {
            ordered_json iObj;
            iObj["id"] = iface.id.str();
            iObj["name"] = iface.name;
            iObj["adminState"] = (iface.adminState == AdminState::Up) ? "up" : "down";

            if (iface.ipv4.has_value()) {
                iObj["ipv4Address"] = iface.ipv4->address().toString();
                iObj["prefixLength"] = static_cast<int>(iface.ipv4->prefixLength());
            } else {
                iObj["ipv4Address"] = nullptr;
                iObj["prefixLength"] = nullptr;
            }

            if (iface.mac.has_value()) {
                iObj["macAddress"] = iface.mac->toString();
            } else {
                iObj["macAddress"] = nullptr;
            }

            if (iface.mtuBytes.has_value()) {
                iObj["mtuBytes"] = *iface.mtuBytes;
            } else {
                iObj["mtuBytes"] = nullptr;
            }

            ifacesArr.push_back(std::move(iObj));
        }
        nObj["interfaces"] = std::move(ifacesArr);

        nodesArr.push_back(std::move(nObj));
    }
    root["nodes"] = std::move(nodesArr);

    // Links (already sorted by LinkId in TopologySnapshot)
    ordered_json linksArr = ordered_json::array();
    for (const auto& link : snap.links()) {
        ordered_json lObj;
        lObj["id"] = link.id.str();

        ordered_json epA;
        epA["nodeId"] = link.a.nodeId.str();
        epA["interfaceId"] = link.a.interfaceId.str();
        lObj["a"] = std::move(epA);

        ordered_json epB;
        epB["nodeId"] = link.b.nodeId.str();
        epB["interfaceId"] = link.b.interfaceId.str();
        lObj["b"] = std::move(epB);

        lObj["adminState"] = (link.adminState == AdminState::Up) ? "up" : "down";
        lObj["directionality"] = (link.directionality == Directionality::Bidirectional) ? "bidirectional" : "directed";
        lObj["cost"] = link.cost;
        lObj["bandwidthBps"] = link.bandwidthBps;

        if (link.availableBandwidthBps.has_value()) {
            lObj["availableBandwidthBps"] = *link.availableBandwidthBps;
        } else {
            lObj["availableBandwidthBps"] = nullptr;
        }

        lObj["propagationDelayMs"] = link.propagationDelayMs;
        lObj["packetLossProbability"] = link.packetLossProbability;
        lObj["mtuBytes"] = link.mtuBytes;

        linksArr.push_back(std::move(lObj));
    }
    root["links"] = std::move(linksArr);

    // Dump with 2-space indentation and append single trailing LF
    std::string text = root.dump(2);
    text.push_back('\n');
    return text;
}

} // namespace nettrace

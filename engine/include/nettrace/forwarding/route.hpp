#pragma once

#include "nettrace/model/types.hpp"
#include "nettrace/model/ipv4.hpp"
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace nettrace {

enum class RouteSource {
    Connected,  // Auto-installed from active interface subnet
    Static,     // User configured static route
    Default,    // Default route (0.0.0.0/0) derived from gateway
    Policy      // Policy route installed by path-planner bridge (D-04)
};

inline std::string_view routeSourceToString(RouteSource source) {
    switch (source) {
        case RouteSource::Connected: return "Connected";
        case RouteSource::Static: return "Static";
        case RouteSource::Default: return "Default";
        case RouteSource::Policy: return "Policy";
    }
    return "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, RouteSource source) {
    return os << routeSourceToString(source);
}

// Layer-3 Forwarding Route (M2-29)
struct Route {
    IPv4Subnet prefix;                       // Destination prefix CIDR
    std::optional<IPv4Address> nextHop;      // nullopt for connected (direct), else next-hop IP
    InterfaceId outInterface;                // Outgoing interface ID
    RouteSource source = RouteSource::Static;
    uint32_t adminPreference = 1;            // 0: connected, 1: static, 1: default, 1: policy (unless set)
    uint32_t metric = 0;                     // Administrative cost/metric
    std::string displayId;                   // e.g. "r1:static:10.5.0.0/16:via 10.0.0.2:eth0"
    bool valid = true;                       // Valid and usable in FIB

    bool isConnected() const { return !nextHop.has_value() || source == RouteSource::Connected; }
    bool isDefault() const { return prefix.prefixLength() == 0; }

    // Numeric tie-breaking order (ADR-0030, D-19):
    // 1. Compare nextHop as 32-bit integer ascending (direct/nullopt first)
    // 2. Compare egress InterfaceId lexicographically
    // 3. Compare source
    bool numericTieBreakLessThan(const Route& o) const {
        if (nextHop != o.nextHop) {
            if (!nextHop.has_value()) return true;
            if (!o.nextHop.has_value()) return false;
            return nextHop->toUint32() < o.nextHop->toUint32();
        }
        if (outInterface != o.outInterface) {
            return outInterface < o.outInterface;
        }
        return static_cast<int>(source) < static_cast<int>(o.source);
    }

    bool operator==(const Route& o) const {
        return prefix == o.prefix && nextHop == o.nextHop &&
               outInterface == o.outInterface && source == o.source &&
               adminPreference == o.adminPreference && metric == o.metric &&
               valid == o.valid;
    }
    bool operator!=(const Route& o) const { return !(*this == o); }
};

inline std::string makeDisplayId(std::string_view nodeName, const Route& r) {
    std::string id = std::string(nodeName) + ":" + std::string(routeSourceToString(r.source)) + ":" + r.prefix.toString();
    if (r.nextHop.has_value()) {
        id += ":via " + r.nextHop->toString();
    } else {
        id += ":direct";
    }
    id += ":" + r.outInterface.str();
    return id;
}

inline std::ostream& operator<<(std::ostream& os, const Route& r) {
    os << "Route(" << r.displayId << " " << r.prefix.toString() << " -> ";
    if (r.nextHop.has_value()) {
        os << r.nextHop->toString();
    } else {
        os << "direct";
    }
    os << " via " << r.outInterface.str() << " " << routeSourceToString(r.source)
       << " pref=" << r.adminPreference << " metric=" << r.metric << ")";
    return os;
}

} // namespace nettrace

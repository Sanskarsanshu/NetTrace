#pragma once

#include "nettrace/forwarding/route.hpp"
#include "nettrace/model/error.hpp"
#include "nettrace/model/ipv4.hpp"
#include "nettrace/model/types.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nettrace {

enum class HostSendOutcome {
    Direct,        // Destination is on-link; send directly to destination IP
    ViaGateway,    // Destination is off-link; send via gateway
    NoRoute        // No route or all matching routes unusable
};

inline std::string_view hostSendOutcomeToString(HostSendOutcome outcome) {
    switch (outcome) {
        case HostSendOutcome::Direct: return "Direct";
        case HostSendOutcome::ViaGateway: return "ViaGateway";
        case HostSendOutcome::NoRoute: return "NoRoute";
    }
    return "Unknown";
}

enum class RouterForwardOutcome {
    Forward,            // Route matched; forward to next hop
    LocalDelivery,      // Destination matches one of router's own active interfaces
    ForwardingDisabled, // Router forwarding is administratively disabled
    NoRoute             // No route or all matching routes unusable
};

inline std::string_view routerForwardOutcomeToString(RouterForwardOutcome outcome) {
    switch (outcome) {
        case RouterForwardOutcome::Forward: return "Forward";
        case RouterForwardOutcome::LocalDelivery: return "LocalDelivery";
        case RouterForwardOutcome::ForwardingDisabled: return "ForwardingDisabled";
        case RouterForwardOutcome::NoRoute: return "NoRoute";
    }
    return "Unknown";
}

// Forwarding decision containing all structured evidence (M2-36)
struct ForwardingDecision {
    NodeId nodeId;
    IPv4Address destinationIp;

    // Specific outcomes
    std::optional<HostSendOutcome> hostOutcome;
    std::optional<RouterForwardOutcome> routerOutcome;

    std::optional<InterfaceId> outInterface;
    std::optional<IPv4Address> nextHopIp;
    std::optional<Route> matchedRoute;
    std::vector<Route> candidateRoutes;
    std::vector<std::pair<Route, std::string>> rejectedRoutes;
    std::optional<ErrorCode> dropReason;
    std::string detail; // e.g. "ALL_MATCHING_ROUTES_UNUSABLE"
    bool needsArp = false;

    bool isSuccess() const {
        if (hostOutcome.has_value()) {
            return *hostOutcome != HostSendOutcome::NoRoute;
        }
        if (routerOutcome.has_value()) {
            return *routerOutcome == RouterForwardOutcome::Forward ||
                   *routerOutcome == RouterForwardOutcome::LocalDelivery;
        }
        return false;
    }

    bool isLocalDelivery() const {
        return routerOutcome.has_value() && *routerOutcome == RouterForwardOutcome::LocalDelivery;
    }

    bool isDirect() const {
        return hostOutcome.has_value() && *hostOutcome == HostSendOutcome::Direct;
    }
};

} // namespace nettrace

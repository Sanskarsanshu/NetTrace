#include "nettrace/model/error.hpp"

namespace nettrace {

std::string_view errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::None: return "NONE";
        case ErrorCode::NegativeWeight: return "NEGATIVE_WEIGHT";
        case ErrorCode::NegativeCycle: return "NEGATIVE_CYCLE";
        case ErrorCode::UnknownNode: return "UNKNOWN_NODE";
        case ErrorCode::InvalidWeight: return "INVALID_WEIGHT";
        case ErrorCode::UnsupportedAlgorithm: return "UNSUPPORTED_ALGORITHM";
        case ErrorCode::LimitExceeded: return "LIMIT_EXCEEDED";
        case ErrorCode::InvalidProbability: return "INVALID_PROBABILITY";
        case ErrorCode::NonPositiveBandwidth: return "NON_POSITIVE_BANDWIDTH";
        case ErrorCode::DanglingEndpoint: return "DANGLING_ENDPOINT";
        case ErrorCode::DuplicateId: return "DUPLICATE_ID";
        case ErrorCode::SelfLoopRejected: return "SELF_LOOP_REJECTED";
        case ErrorCode::Unreachable: return "UNREACHABLE";
        case ErrorCode::NoFeasibleRoute: return "NO_FEASIBLE_ROUTE";
        case ErrorCode::SourceEqualsDestination: return "SOURCE_EQUALS_DESTINATION";
        case ErrorCode::InvalidAddress: return "INVALID_ADDRESS";
        case ErrorCode::InvalidPrefix: return "INVALID_PREFIX";
        case ErrorCode::InvalidSubnet: return "INVALID_SUBNET";
        case ErrorCode::UnsupportedProtocol: return "UNSUPPORTED_PROTOCOL";
        case ErrorCode::InvalidMac: return "INVALID_MAC";
        case ErrorCode::DuplicateMac: return "DUPLICATE_MAC";
        case ErrorCode::DuplicateIpInDomain: return "DUPLICATE_IP_IN_DOMAIN";
        case ErrorCode::PortUnreachable: return "PORT_UNREACHABLE";
        case ErrorCode::PortClosed: return "PORT_CLOSED";
        case ErrorCode::InvalidPort: return "INVALID_PORT";
        case ErrorCode::GatewayNotOnLink: return "GATEWAY_NOT_ON_LINK";
        case ErrorCode::InvalidGateway: return "INVALID_GATEWAY";
        case ErrorCode::GatewayUnreachable: return "GATEWAY_UNREACHABLE";
        case ErrorCode::InvalidConfiguration: return "INVALID_CONFIGURATION";
        case ErrorCode::InvalidId: return "INVALID_ID";
        case ErrorCode::InvalidName: return "INVALID_NAME";
        case ErrorCode::DuplicateNodeId: return "DUPLICATE_NODE_ID";
        case ErrorCode::DuplicateInterfaceId: return "DUPLICATE_INTERFACE_ID";
        case ErrorCode::DuplicateLinkId: return "DUPLICATE_LINK_ID";
        case ErrorCode::LinkEndpointNotFound: return "LINK_ENDPOINT_NOT_FOUND";
        case ErrorCode::InterfaceAlreadyLinked: return "INTERFACE_ALREADY_LINKED";
        case ErrorCode::InvalidHostAddress: return "INVALID_HOST_ADDRESS";
        case ErrorCode::InvalidBandwidth: return "INVALID_BANDWIDTH";
        case ErrorCode::InvalidDelay: return "INVALID_DELAY";
        case ErrorCode::InvalidMtu: return "INVALID_MTU";
        case ErrorCode::MissingMac: return "MISSING_MAC";
        case ErrorCode::DuplicateService: return "DUPLICATE_SERVICE";
        case ErrorCode::EmptyTopology: return "EMPTY_TOPOLOGY";
        case ErrorCode::OverlappingSubnets: return "OVERLAPPING_SUBNETS";
        case ErrorCode::RouterSingleInterface: return "ROUTER_SINGLE_INTERFACE";
        case ErrorCode::SubnetMismatch: return "SUBNET_MISMATCH";
        case ErrorCode::InactiveComponent: return "INACTIVE_COMPONENT";
        case ErrorCode::DisconnectedTopology: return "DISCONNECTED_TOPOLOGY";
        case ErrorCode::L2LoopDetected: return "L2_LOOP_DETECTED";
        case ErrorCode::LinkDown: return "LINK_DOWN";
        case ErrorCode::TtlExpired: return "TTL_EXPIRED";
        case ErrorCode::MtuExceeded: return "MTU_EXCEEDED";
        case ErrorCode::MixedSubnetsInDomain: return "MIXED_SUBNETS_IN_DOMAIN";
        case ErrorCode::InvalidRoute: return "INVALID_ROUTE";
        case ErrorCode::RouteNextHopNotOnLink: return "ROUTE_NEXT_HOP_NOT_ON_LINK";
        case ErrorCode::RouteInterfaceNotFound: return "ROUTE_INTERFACE_NOT_FOUND";
        case ErrorCode::DuplicateRoute: return "DUPLICATE_ROUTE";
        case ErrorCode::ForwardingDisabled: return "FORWARDING_DISABLED";
        case ErrorCode::NoRoute: return "NO_ROUTE";
    }
    return "UNKNOWN_ERROR";
}

} // namespace nettrace

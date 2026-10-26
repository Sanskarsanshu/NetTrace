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
        case ErrorCode::InvalidIpAddress: return "INVALID_IP_ADDRESS";
        case ErrorCode::InvalidPrefix: return "INVALID_PREFIX";
        case ErrorCode::InvalidSubnet: return "INVALID_SUBNET";
        case ErrorCode::GatewayNotOnLink: return "GATEWAY_NOT_ON_LINK";
        case ErrorCode::LinkDown: return "LINK_DOWN";
        case ErrorCode::TtlExpired: return "TTL_EXPIRED";
        case ErrorCode::MtuExceeded: return "MTU_EXCEEDED";
    }
    return "UNKNOWN_ERROR";
}

} // namespace nettrace

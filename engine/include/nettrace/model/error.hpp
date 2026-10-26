#pragma once

#include <string>
#include <string_view>

namespace nettrace {

enum class ErrorCode {
    None = 0,
    NegativeWeight,
    NegativeCycle,
    UnknownNode,
    InvalidWeight,
    UnsupportedAlgorithm,
    LimitExceeded,
    InvalidProbability,
    NonPositiveBandwidth,
    DanglingEndpoint,
    DuplicateId,
    SelfLoopRejected,
    Unreachable,
    NoFeasibleRoute,
    SourceEqualsDestination,
    InvalidAddress,
    InvalidIpAddress = InvalidAddress,
    InvalidPrefix,
    InvalidSubnet,
    UnsupportedProtocol,
    InvalidMac,
    DuplicateMac,
    DuplicateIpInDomain,
    PortUnreachable,
    PortClosed,
    InvalidPort,
    GatewayNotOnLink,
    LinkDown,
    TtlExpired,
    MtuExceeded
};

std::string_view errorCodeToString(ErrorCode code);

inline std::ostream& operator<<(std::ostream& os, ErrorCode code) {
    return os << errorCodeToString(code);
}

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;
    std::string fieldPath;

    Error() = default;
    Error(ErrorCode c, std::string msg, std::string path = "")
        : code(c), message(std::move(msg)), fieldPath(std::move(path)) {}

    bool ok() const { return code == ErrorCode::None; }
    explicit operator bool() const { return !ok(); }
};

} // namespace nettrace

#include "nettrace/model/ipv4.hpp"
#include <cctype>
#include <iostream>
#include <sstream>
#include <vector>

namespace nettrace {

std::string IPv4Address::toString() const {
    return std::to_string(octet(0)) + "." +
           std::to_string(octet(1)) + "." +
           std::to_string(octet(2)) + "." +
           std::to_string(octet(3));
}

Result<IPv4Address> IPv4Address::parse(std::string_view str) {
    if (str.empty()) {
        return Result<IPv4Address>::err(ErrorCode::InvalidAddress, "IPv4 address string is empty");
    }

    // Check for IPv6 indicators
    if (str.find(':') != std::string_view::npos) {
        return Result<IPv4Address>::err(
            ErrorCode::UnsupportedProtocol,
            "IPv6 addresses are not supported ('" + std::string(str) + "'); expected IPv4");
    }

    // Bare address parse must reject prefix slashes
    if (str.find('/') != std::string_view::npos) {
        return Result<IPv4Address>::err(
            ErrorCode::InvalidAddress,
            "Bare IPv4 address cannot contain CIDR prefix slash: " + std::string(str));
    }

    // Prohibit leading, trailing, or embedded whitespace
    for (char c : str) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            return Result<IPv4Address>::err(
                ErrorCode::InvalidAddress,
                "IPv4 address must not contain whitespace: '" + std::string(str) + "'");
        }
    }

    // Split exactly 4 octets by '.'
    std::vector<std::string_view> octetTokens;
    size_t start = 0;
    while (start <= str.size()) {
        size_t dotPos = str.find('.', start);
        if (dotPos == std::string_view::npos) {
            octetTokens.push_back(str.substr(start));
            break;
        }
        octetTokens.push_back(str.substr(start, dotPos - start));
        start = dotPos + 1;
    }

    if (octetTokens.size() != 4) {
        return Result<IPv4Address>::err(
            ErrorCode::InvalidAddress,
            "IPv4 address must have exactly 4 octets separated by dots: '" + std::string(str) + "'");
    }

    uint8_t octets[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < 4; ++i) {
        std::string_view token = octetTokens[i];
        if (token.empty()) {
            return Result<IPv4Address>::err(
                ErrorCode::InvalidAddress,
                "IPv4 address contains an empty octet: '" + std::string(str) + "'");
        }

        // Strict rejection of leading zeros (e.g. "01.2.3.4") per ADR-0026
        if (token.size() > 1 && token[0] == '0') {
            return Result<IPv4Address>::err(
                ErrorCode::InvalidAddress,
                "Leading zeros are prohibited in IPv4 octets: '" + std::string(token) + "'");
        }

        uint32_t val = 0;
        for (char c : token) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return Result<IPv4Address>::err(
                    ErrorCode::InvalidAddress,
                    "Non-digit character in IPv4 octet: '" + std::string(token) + "'");
            }
            val = val * 10 + static_cast<uint32_t>(c - '0');
            if (val > 255) {
                return Result<IPv4Address>::err(
                    ErrorCode::InvalidAddress,
                    "IPv4 octet value exceeds 255: '" + std::string(token) + "'");
            }
        }
        octets[i] = static_cast<uint8_t>(val);
    }

    return Result<IPv4Address>::ok(IPv4Address(octets[0], octets[1], octets[2], octets[3]));
}

Result<IPv4Address> IPv4Address::netmaskFromPrefix(uint8_t prefixLen) {
    if (prefixLen > 32) {
        return Result<IPv4Address>::err(
            ErrorCode::InvalidPrefix,
            "Prefix length must be between 0 and 32, got: " + std::to_string(prefixLen));
    }
    if (prefixLen == 0) {
        return Result<IPv4Address>::ok(IPv4Address(0u));
    }
    if (prefixLen == 32) {
        return Result<IPv4Address>::ok(IPv4Address(0xFFFFFFFFu));
    }
    uint32_t maskVal = ~((1u << (32 - prefixLen)) - 1u);
    return Result<IPv4Address>::ok(IPv4Address(maskVal));
}

IPv4Address IPv4Address::network(uint8_t prefixLen) const {
    if (prefixLen > 32) prefixLen = 32;
    uint32_t m = netmaskFromPrefix(prefixLen).value().toUint32();
    return IPv4Address(value_ & m);
}

IPv4Address IPv4Address::broadcast(uint8_t prefixLen) const {
    if (prefixLen > 32) prefixLen = 32;
    uint32_t m = netmaskFromPrefix(prefixLen).value().toUint32();
    return IPv4Address((value_ & m) | (~m));
}

std::ostream& operator<<(std::ostream& os, const IPv4Address& addr) {
    return os << addr.toString();
}

IPv4Address IPv4Subnet::mask() const {
    return IPv4Address::netmaskFromPrefix(prefixLength_).value();
}

IPv4Address IPv4Subnet::network() const {
    return address_.network(prefixLength_);
}

IPv4Address IPv4Subnet::broadcast() const {
    return address_.broadcast(prefixLength_);
}

bool IPv4Subnet::contains(const IPv4Address& addr) const {
    return addr.network(prefixLength_) == network();
}

bool IPv4Subnet::isValidHost(const IPv4Address& addr) const {
    if (!contains(addr)) {
        return false;
    }
    // D-15 rules:
    if (prefixLength_ == 32) {
        // Single-address interface
        return addr == network();
    }
    if (prefixLength_ == 31) {
        // Point-to-point link (RFC 3021): both addresses usable
        return true;
    }
    // /0../30: network and broadcast addresses are NOT valid host addresses
    return addr != network() && addr != broadcast();
}

std::string IPv4Subnet::toString() const {
    return address_.toString() + "/" + std::to_string(prefixLength_);
}

Result<IPv4Subnet> IPv4Subnet::parse(std::string_view str) {
    if (str.empty()) {
        return Result<IPv4Subnet>::err(ErrorCode::InvalidPrefix, "Empty CIDR subnet string");
    }

    if (str.find(':') != std::string_view::npos) {
        return Result<IPv4Subnet>::err(ErrorCode::UnsupportedProtocol, "IPv6 subnets not supported");
    }

    size_t slashPos = str.find('/');
    if (slashPos == std::string_view::npos) {
        return Result<IPv4Subnet>::err(
            ErrorCode::InvalidPrefix,
            "CIDR subnet missing '/' separator: '" + std::string(str) + "'");
    }

    std::string_view addrStr = str.substr(0, slashPos);
    std::string_view prefixStr = str.substr(slashPos + 1);

    auto addrRes = IPv4Address::parse(addrStr);
    if (!addrRes.isOk()) {
        return Result<IPv4Subnet>::err(addrRes.error());
    }

    if (prefixStr.empty()) {
        return Result<IPv4Subnet>::err(ErrorCode::InvalidPrefix, "Prefix length string is empty");
    }

    for (char c : prefixStr) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return Result<IPv4Subnet>::err(
                ErrorCode::InvalidPrefix,
                "Prefix length must be non-negative integer digits: '" + std::string(prefixStr) + "'");
        }
    }

    // Disallow leading zeros for prefix length if length > 1
    if (prefixStr.size() > 1 && prefixStr[0] == '0') {
        return Result<IPv4Subnet>::err(
            ErrorCode::InvalidPrefix,
            "Leading zeros not allowed in prefix length: '" + std::string(prefixStr) + "'");
    }

    uint32_t prefixVal = 0;
    for (char c : prefixStr) {
        prefixVal = prefixVal * 10 + static_cast<uint32_t>(c - '0');
        if (prefixVal > 32) {
            return Result<IPv4Subnet>::err(
                ErrorCode::InvalidPrefix,
                "Prefix length exceeds 32: '" + std::string(prefixStr) + "'");
        }
    }

    return Result<IPv4Subnet>::ok(IPv4Subnet(addrRes.value(), static_cast<uint8_t>(prefixVal)));
}

std::ostream& operator<<(std::ostream& os, const IPv4Subnet& subnet) {
    return os << subnet.toString();
}

bool isOnLink(const IPv4Address& hostAddr, uint8_t prefixLen, const IPv4Address& destAddr) {
    if (prefixLen > 32) return false;
    uint32_t m = IPv4Address::netmaskFromPrefix(prefixLen).value().toUint32();
    return (hostAddr.toUint32() & m) == (destAddr.toUint32() & m);
}

} // namespace nettrace

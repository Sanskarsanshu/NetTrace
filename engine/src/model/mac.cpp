#include "nettrace/model/mac.hpp"
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace nettrace {

namespace {

int hexCharToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

std::string MacAddress::toString() const {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                  bytes_[0], bytes_[1], bytes_[2], bytes_[3], bytes_[4], bytes_[5]);
    return std::string(buf);
}

Result<MacAddress> MacAddress::parse(std::string_view str) {
    // Exact length must be 17: "xx:xx:xx:xx:xx:xx"
    if (str.size() != 17) {
        return Result<MacAddress>::err(
            ErrorCode::InvalidMac,
            "MAC address must be exactly 17 characters in length: '" + std::string(str) + "'");
    }

    // Colons must be at positions 2, 5, 8, 11, 14
    for (size_t colonPos : {2, 5, 8, 11, 14}) {
        if (str[colonPos] != ':') {
            return Result<MacAddress>::err(
                ErrorCode::InvalidMac,
                "MAC address must use colon separators: '" + std::string(str) + "'");
        }
    }

    std::array<uint8_t, 6> b;
    for (size_t i = 0; i < 6; ++i) {
        size_t idx = i * 3;
        char highChar = str[idx];
        char lowChar = str[idx + 1];

        int highVal = hexCharToInt(highChar);
        int lowVal = hexCharToInt(lowChar);

        if (highVal < 0 || lowVal < 0) {
            return Result<MacAddress>::err(
                ErrorCode::InvalidMac,
                "Non-hex character in MAC address: '" + std::string(str) + "'");
        }

        b[i] = static_cast<uint8_t>((highVal << 4) | lowVal);
    }

    return Result<MacAddress>::ok(MacAddress(b));
}

bool MacAddress::isBroadcast() const {
    return bytes_[0] == 0xFF && bytes_[1] == 0xFF && bytes_[2] == 0xFF &&
           bytes_[3] == 0xFF && bytes_[4] == 0xFF && bytes_[5] == 0xFF;
}

bool MacAddress::isMulticast() const {
    // Low bit of the first octet indicates multicast
    return (bytes_[0] & 0x01) != 0;
}

bool MacAddress::isUnicast() const {
    return !isMulticast();
}

bool MacAddress::isLocallyAdministered() const {
    // Second-lowest bit of first octet is locally administered
    return (bytes_[0] & 0x02) != 0;
}

bool MacAddress::isAllZero() const {
    return bytes_[0] == 0 && bytes_[1] == 0 && bytes_[2] == 0 &&
           bytes_[3] == 0 && bytes_[4] == 0 && bytes_[5] == 0;
}

bool MacAddress::isValidInterfaceMac() const {
    // Per D-23: unicast, not all-zero, and not broadcast
    return isUnicast() && !isAllZero() && !isBroadcast();
}

std::ostream& operator<<(std::ostream& os, const MacAddress& mac) {
    return os << mac.toString();
}

Result<MacAddress> generateMac(uint32_t nodeIndex, uint32_t interfaceIndex) {
    if (nodeIndex > 65535 || interfaceIndex > 65535) {
        return Result<MacAddress>::err(
            ErrorCode::LimitExceeded,
            "generateMac: nodeIndex and interfaceIndex must be <= 65535");
    }

    std::array<uint8_t, 6> b = {
        0x02,
        0x00,
        static_cast<uint8_t>((nodeIndex >> 8) & 0xFF),
        static_cast<uint8_t>(nodeIndex & 0xFF),
        static_cast<uint8_t>((interfaceIndex >> 8) & 0xFF),
        static_cast<uint8_t>(interfaceIndex & 0xFF)
    };

    return Result<MacAddress>::ok(MacAddress(b));
}

} // namespace nettrace

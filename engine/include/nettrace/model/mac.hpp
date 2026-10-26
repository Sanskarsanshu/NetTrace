#pragma once

#include "nettrace/model/error.hpp"
#include "nettrace/model/result.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

namespace nettrace {

/**
 * @brief Represents a 48-bit Ethernet MAC address.
 * Canonical string format is lowercase colon-separated hex (e.g. "aa:bb:cc:dd:ee:ff").
 */
class MacAddress {
public:
    constexpr MacAddress() : bytes_{0, 0, 0, 0, 0, 0} {}
    constexpr explicit MacAddress(const std::array<uint8_t, 6>& b) : bytes_(b) {}
    constexpr MacAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
        : bytes_{b0, b1, b2, b3, b4, b5} {}

    const std::array<uint8_t, 6>& bytes() const { return bytes_; }
    uint8_t operator[](size_t index) const { return bytes_[index]; }

    std::string toString() const;

    /**
     * @brief Parses a colon-separated MAC address.
     * Accepts uppercase or lowercase hex digits.
     * Rejects '-' or '.' separators, incorrect length, or non-hex characters with INVALID_MAC.
     */
    static Result<MacAddress> parse(std::string_view str);

    // Predicates
    bool isBroadcast() const;
    bool isMulticast() const;
    bool isUnicast() const;
    bool isLocallyAdministered() const;
    bool isAllZero() const;

    /**
     * @brief Validates if this MAC is suitable as an interface MAC per D-23:
     * Unicast, not all-zero, and not broadcast.
     */
    bool isValidInterfaceMac() const;

    bool operator==(const MacAddress& other) const { return bytes_ == other.bytes_; }
    bool operator!=(const MacAddress& other) const { return bytes_ != other.bytes_; }
    bool operator<(const MacAddress& other) const { return bytes_ < other.bytes_; }
    bool operator>(const MacAddress& other) const { return bytes_ > other.bytes_; }
    bool operator<=(const MacAddress& other) const { return bytes_ <= other.bytes_; }
    bool operator>=(const MacAddress& other) const { return bytes_ >= other.bytes_; }

private:
    std::array<uint8_t, 6> bytes_;
};

std::ostream& operator<<(std::ostream& os, const MacAddress& mac);

/**
 * @brief Deterministic MAC generator per D-23:
 * Form: "02:00:NN:NN:II:II" where NN:NN is nodeIndex and II:II is interfaceIndex (16-bit big-endian).
 * Indices above 65535 return an error with LimitExceeded / InvalidConfiguration.
 */
Result<MacAddress> generateMac(uint32_t nodeIndex, uint32_t interfaceIndex);

} // namespace nettrace

namespace std {
template <> struct hash<nettrace::MacAddress> {
    size_t operator()(const nettrace::MacAddress& mac) const noexcept {
        size_t h = 0;
        for (uint8_t b : mac.bytes()) {
            h = (h * 131) + b;
        }
        return h;
    }
};
} // namespace std

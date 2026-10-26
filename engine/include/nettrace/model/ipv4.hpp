#pragma once

#include "nettrace/model/error.hpp"
#include "nettrace/model/result.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <utility>

namespace nettrace {

/**
 * @brief Represents an IPv4 address stored as a 32-bit integer in host byte order.
 * Canonical representation is four decimal octets separated by dots (e.g. "192.168.1.1").
 */
class IPv4Address {
public:
    constexpr IPv4Address() : value_(0) {}
    explicit constexpr IPv4Address(uint32_t value) : value_(value) {}
    constexpr IPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
        : value_((static_cast<uint32_t>(a) << 24) |
                 (static_cast<uint32_t>(b) << 16) |
                 (static_cast<uint32_t>(c) << 8) |
                 static_cast<uint32_t>(d)) {}

    uint32_t toUint32() const { return value_; }

    uint8_t octet(size_t index) const {
        switch (index) {
            case 0: return static_cast<uint8_t>((value_ >> 24) & 0xFF);
            case 1: return static_cast<uint8_t>((value_ >> 16) & 0xFF);
            case 2: return static_cast<uint8_t>((value_ >> 8) & 0xFF);
            case 3: return static_cast<uint8_t>(value_ & 0xFF);
            default: return 0;
        }
    }

    std::array<uint8_t, 4> octets() const {
        return {octet(0), octet(1), octet(2), octet(3)};
    }

    std::string toString() const;

    /**
     * @brief Strictly parses an IPv4 string.
     * Rejects leading zeros ("01.2.3.4"), whitespace, empty octets, out-of-range octets,
     * bare prefixes ("1.2.3.4/24"), and IPv6 addresses ("::1").
     */
    static Result<IPv4Address> parse(std::string_view str);

    /**
     * @brief Generates an IPv4 subnet mask from a prefix length (0..32).
     */
    static Result<IPv4Address> netmaskFromPrefix(uint8_t prefixLen);

    IPv4Address network(uint8_t prefixLen) const;
    IPv4Address broadcast(uint8_t prefixLen) const;

    constexpr bool operator==(const IPv4Address& other) const { return value_ == other.value_; }
    constexpr bool operator!=(const IPv4Address& other) const { return value_ != other.value_; }
    constexpr bool operator<(const IPv4Address& other) const { return value_ < other.value_; }
    constexpr bool operator>(const IPv4Address& other) const { return value_ > other.value_; }
    constexpr bool operator<=(const IPv4Address& other) const { return value_ <= other.value_; }
    constexpr bool operator>=(const IPv4Address& other) const { return value_ >= other.value_; }

private:
    uint32_t value_;
};

std::ostream& operator<<(std::ostream& os, const IPv4Address& addr);

/**
 * @brief Represents an IPv4 subnet / CIDR prefix (e.g. "192.168.1.0/24").
 */
class IPv4Subnet {
public:
    IPv4Subnet() : address_(), prefixLength_(0) {}
    IPv4Subnet(IPv4Address addr, uint8_t prefixLength)
        : address_(addr), prefixLength_(prefixLength) {}

    const IPv4Address& address() const { return address_; }
    uint8_t prefixLength() const { return prefixLength_; }

    IPv4Address mask() const;
    IPv4Address network() const;
    IPv4Address broadcast() const;

    bool contains(const IPv4Address& addr) const;

    /**
     * @brief Evaluates host address validity per D-15.
     * /31: both network and broadcast are usable hosts (RFC 3021).
     * /32: single host interface.
     * /0../30: network and broadcast addresses are NOT valid host addresses.
     */
    bool isValidHost(const IPv4Address& addr) const;

    std::string toString() const;

    /**
     * @brief Parses CIDR string format "address/prefix".
     */
    static Result<IPv4Subnet> parse(std::string_view str);

    bool operator==(const IPv4Subnet& other) const {
        return address_ == other.address_ && prefixLength_ == other.prefixLength_;
    }
    bool operator!=(const IPv4Subnet& other) const { return !(*this == other); }

private:
    IPv4Address address_;
    uint8_t prefixLength_;
};

std::ostream& operator<<(std::ostream& os, const IPv4Subnet& subnet);

/**
 * @brief Tests if destAddr is directly reachable on-link given hostAddr and prefixLen.
 * Uses real subnet mask comparison; never compares only the first three octets.
 */
bool isOnLink(const IPv4Address& hostAddr, uint8_t prefixLen, const IPv4Address& destAddr);

} // namespace nettrace

namespace std {
template <> struct hash<nettrace::IPv4Address> {
    size_t operator()(const nettrace::IPv4Address& addr) const noexcept {
        return hash<uint32_t>{}(addr.toUint32());
    }
};

template <> struct hash<nettrace::IPv4Subnet> {
    size_t operator()(const nettrace::IPv4Subnet& sub) const noexcept {
        return hash<uint32_t>{}(sub.address().toUint32()) ^ (hash<uint8_t>{}(sub.prefixLength()) << 1);
    }
};
} // namespace std

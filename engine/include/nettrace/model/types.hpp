#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace nettrace {

// Strong string-backed ID types with deterministic lexicographical ordering
struct NodeId {
    std::string value;

    NodeId() = default;
    explicit NodeId(std::string v) : value(std::move(v)) {}
    explicit NodeId(const char* v) : value(v) {}

    bool operator==(const NodeId& other) const { return value == other.value; }
    bool operator!=(const NodeId& other) const { return value != other.value; }
    bool operator<(const NodeId& other) const { return value < other.value; }
    bool operator>(const NodeId& other) const { return value > other.value; }
    bool operator<=(const NodeId& other) const { return value <= other.value; }
    bool operator>=(const NodeId& other) const { return value >= other.value; }
    bool empty() const { return value.empty(); }
    const std::string& str() const { return value; }
};

struct InterfaceId {
    std::string value;

    InterfaceId() = default;
    explicit InterfaceId(std::string v) : value(std::move(v)) {}
    explicit InterfaceId(const char* v) : value(v) {}

    bool operator==(const InterfaceId& other) const { return value == other.value; }
    bool operator!=(const InterfaceId& other) const { return value != other.value; }
    bool operator<(const InterfaceId& other) const { return value < other.value; }
    bool operator>(const InterfaceId& other) const { return value > other.value; }
    bool operator<=(const InterfaceId& other) const { return value <= other.value; }
    bool operator>=(const InterfaceId& other) const { return value >= other.value; }
    bool empty() const { return value.empty(); }
    const std::string& str() const { return value; }
};

struct LinkId {
    std::string value;

    LinkId() = default;
    explicit LinkId(std::string v) : value(std::move(v)) {}
    explicit LinkId(const char* v) : value(v) {}

    bool operator==(const LinkId& other) const { return value == other.value; }
    bool operator!=(const LinkId& other) const { return value != other.value; }
    bool operator<(const LinkId& other) const { return value < other.value; }
    bool operator>(const LinkId& other) const { return value > other.value; }
    bool operator<=(const LinkId& other) const { return value <= other.value; }
    bool operator>=(const LinkId& other) const { return value >= other.value; }
    bool empty() const { return value.empty(); }
    const std::string& str() const { return value; }
};

struct PacketId {
    std::string value;

    PacketId() = default;
    explicit PacketId(std::string v) : value(std::move(v)) {}
    explicit PacketId(const char* v) : value(v) {}

    bool operator==(const PacketId& other) const { return value == other.value; }
    bool operator!=(const PacketId& other) const { return value != other.value; }
    bool operator<(const PacketId& other) const { return value < other.value; }
    bool operator>(const PacketId& other) const { return value > other.value; }
    bool operator<=(const PacketId& other) const { return value <= other.value; }
    bool operator>=(const PacketId& other) const { return value >= other.value; }
    bool empty() const { return value.empty(); }
    const std::string& str() const { return value; }
};

// Canonical unit types
using BandwidthBps = uint64_t;
using DelayMs = double;
using SimTimeMs = double;
using PacketSizeBytes = uint32_t;
using Cost = double;
using Probability = double;

inline bool isValidProbability(Probability p) {
    return !std::isnan(p) && !std::isinf(p) && p >= 0.0 && p <= 1.0;
}

inline bool isValidCost(Cost c) {
    return !std::isnan(c) && !std::isinf(c);
}

inline bool isValidDelay(DelayMs d) {
    return !std::isnan(d) && !std::isinf(d) && d >= 0.0;
}

// Directionality
enum class Directionality {
    Bidirectional,
    Directed
};

// Admin state
enum class AdminState {
    Up,
    Down
};

} // namespace nettrace

// Hash specializations
namespace std {
template <> struct hash<nettrace::NodeId> {
    size_t operator()(const nettrace::NodeId& id) const noexcept {
        return hash<string>{}(id.value);
    }
};
template <> struct hash<nettrace::InterfaceId> {
    size_t operator()(const nettrace::InterfaceId& id) const noexcept {
        return hash<string>{}(id.value);
    }
};
template <> struct hash<nettrace::LinkId> {
    size_t operator()(const nettrace::LinkId& id) const noexcept {
        return hash<string>{}(id.value);
    }
};
template <> struct hash<nettrace::PacketId> {
    size_t operator()(const nettrace::PacketId& id) const noexcept {
        return hash<string>{}(id.value);
    }
};
} // namespace std

#pragma once

#include "nettrace/model/topology.hpp"
#include "nettrace/model/result.hpp"
#include <string>
#include <string_view>

namespace nettrace {

/**
 * @brief Strict JSON loader for topology configurations (M2-G / D-16 / ADR-0033).
 * 
 * Enforces:
 * - 2 MB input size limit (ErrorCode::LimitExceeded)
 * - 64 nesting depth limit without recursion (ErrorCode::LimitExceeded)
 * - schemaVersion major version 1 (ErrorCode::UnsupportedSchemaVersion)
 * - Required fields, strict types, and enum validation
 * - Guarantees no exception escapes (catches all exceptions internally)
 */
Result<TopologyDraft> loadTopologyJson(std::string_view jsonStr);

/**
 * @brief Canonical JSON exporter for TopologySnapshot (M2-G / D-02 / ADR-0033).
 * 
 * Generates deterministic canonical JSON:
 * - Fixed key ordering
 * - Sorted nodes, interfaces, and links
 * - 2-space indentation, LF line endings, single trailing newline
 * - Persists assigned MAC addresses (D-23)
 */
std::string toJson(const TopologySnapshot& snap);

} // namespace nettrace

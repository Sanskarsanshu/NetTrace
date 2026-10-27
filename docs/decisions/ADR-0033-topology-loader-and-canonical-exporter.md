# ADR-0033: TopologyLoader Error Contract and Canonical Exporter Specification

- **Status:** Accepted (locks D-02, D-16, D-23)
- **Date:** 2026-10-27

## Context
NetTrace 3D requires strict bidirectional JSON input/output for topology configurations. The loader must parse incoming JSON into `TopologyDraft` structures safely and predictably, protecting the simulation against malformed payloads, oversized inputs, and stack overflow vulnerabilities. The exporter must generate deterministic canonical JSON such that `toJson(freeze(load(file))) == file bytes`.

## Decision
1. **Size Limit (D-16):**
   - Inputs larger than 2 MB (`2 * 1024 * 1024` bytes) are rejected immediately prior to parsing with code `LIMIT_EXCEEDED`.
2. **Nesting Depth Limit (D-16):**
   - Enforce a strict nesting depth limit of 64 using a non-recursive pre-scan. Inputs containing more than 64 nested brackets (`{` or `[`) are rejected with `LIMIT_EXCEEDED` before recursive DOM parsing, ensuring that adversarial payloads (such as 100,000 nested brackets) never cause stack overflow or crashes under ASan/UBSan.
3. **Exception Safety:**
   - No exceptions may escape `loadTopologyJson`. All parsing errors and exceptions are caught internally and returned as `Result<TopologyDraft>::failure(ErrorCode, message, path)`.
4. **Error Contract & Codes:**
   - Syntax error: `INVALID_JSON`
   - Missing required field: `MISSING_FIELD` (path: JSON pointer or field name)
   - Wrong type: `INVALID_FIELD_TYPE` (path: field location)
   - Unknown enum: `INVALID_ENUM_VALUE` (e.g. unknown node type or admin state)
   - Unsupported schema version: `UNSUPPORTED_SCHEMA_VERSION` (major version != 1)
   - Malformed address/prefix/mac: `INVALID_ADDRESS`, `INVALID_PREFIX`, `INVALID_MAC`
   - Unknown optional fields are silently ignored.
5. **Canonical Exporter Pipeline:**
   - Keys are ordered consistently: root (`schemaVersion`, `id`, `name`, `description`, `nodes`, `links`), nodes (`id`, `name`, `type`, `adminState`, `defaultGateway`, `forwardingEnabled`, `services`, `staticRoutes`, `interfaces`), links (`id`, `a`, `b`, `adminState`, `directionality`, `cost`, `bandwidthBps`, `availableBandwidthBps`, `propagationDelayMs`, `packetLossProbability`, `mtuBytes`).
   - Sorted collections: nodes sorted by `NodeId`, interfaces sorted by `InterfaceId`, links sorted by `LinkId`.
   - 2-space indentation, LF (`\n`) line endings, single trailing newline, shortest round-trip float representations.
   - Normalized MAC addresses assigned during `assignMissingMacs` are persisted in the exported file per D-23.

## Consequences
- Guaranteed deterministic round-trips: `toJson(freeze(load(file))) == file bytes`.
- Robust defense against malformed or hostile input.

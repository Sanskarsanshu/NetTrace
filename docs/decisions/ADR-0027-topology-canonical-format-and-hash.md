# ADR-0027: Topology Canonical Serialization Format and Non-Cryptographic Hash

- **Status:** Accepted
- **Date:** 2026-10-26
- **Context:** Milestone 2 (M2-B) requires an immutable `TopologySnapshot` that generates a deterministic content hash for run records, simulation caching, and change tracking.

## Context and Problem

A topology draft in NetTrace 3D can be constructed in arbitrary order (e.g., nodes added in reverse order, links added before nodes, interfaces added out of alphabetical sequence). For simulation reproducibility and artifact caching, two topologies with identical elements and configurations must produce the exact same fingerprint, while any configuration difference (e.g., a changed MTU, link cost, or MAC address) must produce a different fingerprint.

Furthermore, we need a lightweight, reproducible hashing algorithm that has zero external dependencies, is cross-platform, deterministic across architectures, and does not require a full cryptographic library (like SHA-256).

## Decision

1. **Non-Cryptographic Content Hash (FNV-1a 64-bit):**
   - We adopt the 64-bit FNV-1a (Fowler-Noll-Vo 1a) algorithm.
   - **Explicit Non-Cryptographic Notice:** This hash is **NOT cryptographic**. It is not preimage-resistant or collision-resistant against deliberate adversarial attacks, but provides excellent distribution and speed for structural identity, run recording, and test validation.
   - **Parameters:**
     - 64-bit FNV offset basis: `0xcbf29ce484222325ULL` (`14695981039346656037`)
     - 64-bit FNV prime: `0x100000001b3ULL` (`1099511628211`)
   - **Output Format:** Printed as exactly 16 lowercase hexadecimal digits (zero-padded, e.g. `%016llx`).
   - **Standard Test Vectors:**
     - `""` -> `cbf29ce484222325`
     - `"a"` -> `af63dc4c8601ec8c`
     - `"foobar"` -> `85944171f73967e8`

2. **Canonical Serialization Format:**
   - To compute the hash, the `TopologySnapshot` serializes the validated, normalized topology into a deterministic, line-delimited plain-text representation:
   - **Sorting Rules:**
     - Nodes are sorted by `NodeId` ascending.
     - Within each node, interfaces are sorted by `InterfaceId` ascending.
     - Within each host or cloud endpoint, services are sorted by protocol (UDP < TCP), then port ascending.
     - Links are sorted by `LinkId` ascending.
   - **Text Representation Structure:**
     - `NODE:<id>|<name>|<type>|<adminState>|<gateway>|<forwardingEnabled>|<processingDelayMs>`
     - For each service on the node:
       - `SERVICE:<protocol>|<port>|<label>`
     - For each interface on the node:
       - `IFACE:<id>|<name>|<adminState>|<mac>|<ip/prefix>|<mtuOverride>`
     - For each link in the topology:
       - `LINK:<id>|<adminState>|<directionality>|<a.node>:<a.iface>|<b.node>:<b.iface>|<cost>|<bandwidthBps>|<availableBandwidthBps>|<delayMs>|<lossProb>|<mtuBytes>`
   - Empty optional fields are represented as empty strings between the pipe separators.
   - All floating-point values are printed with standard fixed/lossless formatting.

3. **Lifecycle Flow:**
   - `TopologyDraft` (mutable structs) -> `normalize(draft)` (fills missing MACs per D-23) -> `validate(draft, options)` -> `freeze(draft, options) -> Result<TopologySnapshot>`.
   - `freeze()` fails with `ValidationReport` if there are any validation errors.
   - `TopologySnapshot` is immutable and caches the computed canonical hash.

## Consequences

- Deterministic hashing: any permutation of input insertion order produces identical canonical text and hash.
- Any change in node, interface, service, or link parameters produces a different hash.
- Validated by unit test `T-2-25`.

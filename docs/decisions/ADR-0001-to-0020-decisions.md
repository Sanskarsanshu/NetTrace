# NetTrace 3D — Architectural Decision Records (D-01 to D-20)

This document records the foundational architectural decisions locked before coding, resolving any potential ambiguities between `NETWORKING_RULES.md`, `ARCHITECTURE.md`, and `PROJECT.md`.

---

## D-01: Document Precedence
- **Decision:** For all networking semantics, protocol behavior, and forwarding assumptions, the order of authority is:
  1. `NETWORKING_RULES.md` (authoritative networking semantics)
  2. `ARCHITECTURE.md` (component contracts, IPC, and responsibilities)
  3. `PROJECT.md` (product vision, UI requirements, and acceptance scope)
- **Rationale:** Technical consistency requires a single definitive source of truth for networking behavior.

## D-02: Canonical JSON Naming & Unit Standards
- **Decision:** All JSON schemas use `camelCase` and declare `schemaVersion: "1.0"`.
- **Canonical Units (Engine & Backend):**
  - Bandwidth: bits per second (`bandwidthBps`, integer)
  - Time/Delay: milliseconds (`delayMs`, `propagationDelayMs`, float)
  - Packet sizes & MTU: bytes (`packetSizeBytes`, `mtuBytes`, integer)
  - Probability: float in `[0.0, 1.0]` (`lossProbability`)
- **Display Layer (Frontend):** The UI displays human-friendly units (Mbps, ms, bytes, %) and converts strictly at the API boundary.

## D-03: Unified Packet State Lifecycle
- **Decision:** Packet states form a single canonical enum:
  `CREATED`, `ROUTE_LOOKUP`, `QUEUED`, `IN_TRANSIT`, `RECEIVED`, `FORWARDED`, `DELIVERED`, `DROPPED`, `EXPIRED`, `REASSEMBLY_FAILED`, `CANCELLED`.
  - Terminal states: `DELIVERED`, `DROPPED`, `EXPIRED`, `REASSEMBLY_FAILED`, `CANCELLED`.
  - Run Status is separate: `CREATED`, `VALIDATING`, `RUNNING`, `COMPLETED`, `FAILED`, `CANCELLED`. A dropped packet produces a `COMPLETED` run with a recorded drop reason.

## D-04: Explicit Execution Modes
- **Decision:** The simulator supports two explicit modes:
  1. `PATH_PLANNING`: Static graph algorithm execution; calculates candidate route and metrics without moving packets.
  2. `PACKET_FORWARDING`: Hop-by-hop packet forwarding where each router consults its local FIB.
- **Bridge:** In `PACKET_FORWARDING` mode with a selected algorithm, the computed route is installed into router FIBs as policy-derived routes (`routeSource: "policy"`), guaranteeing that forwarding always tests actual FIB lookup semantics.

## D-05: Dual Hop Counts
- **Decision:** Every result and inspector must distinguish and report both:
  - `routerHopCount`: Count of Layer-3 router forwarding operations (FIB lookups + TTL decrements).
  - `linkHopCount`: Count of physical link traversals (including switch hops).
- **Rationale:** Avoids confusing students when packets traverse Layer-2 switches without consuming router hops.

## D-06: Deterministic Loss Sampling
- **Decision:** Packet loss is evaluated once per link traversal at the moment of transmission.
  - To prevent cross-platform differences from standard library distributions, use a custom deterministic mapping from 64-bit integer PRNG (xoshiro256** or PCG32) to `[0.0, 1.0)`.
  - Loss probability `0.0` never consumes a random draw, ensuring invariant behavior across non-lossy links.

## D-07: Link-Down Timing
- **Decision:** Link operational status is verified at both:
  1. Transmission start time (queue exit / send).
  2. Scheduled arrival time at the receiving interface.
  - If the link becomes inactive during transit, the packet is dropped with reason `LINK_DOWN`.

## D-08: Abstracted ARP Resolution (Superseded by D-21)
- **Status:** Superseded by **D-21** (ARP and Neighbor Resolution Model).
- **Original Decision:** Version 1 abstracts ARP exchanges: next-hop Layer-2 MAC address resolution is deterministic and instantaneous, emitting an explicit `NEIGHBOR_RESOLVED_ABSTRACTED` event.
- **Current Model:** D-21 introduces modeled ARP with per-node `ArpCache`, switch broadcast flooding, and unicast replies as default (`arpMode: "modeled"`), while retaining the D-08 registry lookup under `arpMode: "abstracted"`.
- **Rationale:** Preserves educational accessibility while providing students with full visibility into ARP discovery frames and cache behavior.

## D-09: Transparent Learning Bridge Model
- **Decision:** Switches operate as 802.1D transparent learning bridges:
  - Learn source MAC on ingress port.
  - Forward unicast frame to known egress port, or flood to all other ports if unknown/broadcast.
  - Never alter IP headers, never inspect IP addresses, and never decrement TTL.
  - Spanning Tree Protocol (STP) is labeled "Not modeled".

## D-10: ICMP Time Exceeded Response Policy
- **Decision:** When TTL expires at a router, an ICMP Time Exceeded (Type 11, Code 0) message is generated as an independent packet with its own unique `packetId` and payload pointing to the expired header.
  - ICMP errors are never generated in response to ICMP error packets (RFC 792 / RFC 1812).
  - ICMP responses traverse the network via their own independent FIB lookups and can themselves encounter impairments.

## D-11: Virtual Clock & Deterministic Discrete-Event Queue
- **Decision:** The simulation engine uses a virtual clock (`SimTimeMs`) driving a deterministic priority queue ordered by:
  `(scheduledTimeMs, eventPriority, insertionSequence)`.
  - No wall-clock sleeps or thread delays exist in the engine.

## D-12: Confirmed Toolchain Baseline
- **Decision:**
  - C++: C++17, GCC 16+ (MSYS2 UCRT64), CMake 4.4, MinGW Makefiles.
  - Python: Python 3.14, FastAPI 0.135+, Pydantic v2, Pytest 9.1+.
  - Frontend: React 18/19, TypeScript, Vite, React Flow, Three.js / React Three Fiber.

## D-13: Link Cost Non-Negativity
- **Decision:** By default, all link costs must be non-negative (`cost >= 0.0`).
  - Negative costs are strictly rejected by Dijkstra with `NEGATIVE_WEIGHT`.
  - Negative edge weights are accepted only in an explicit Bellman-Ford "teaching mode" for cycle detection.

## D-14: Simulated Cloud Endpoint
- **Decision:** Nodes of type `cloudEndpoint` represent simulated remote hosts. The UI and documentation prominently label them as "Simulated endpoint". No AWS credentials or cloud infrastructure are contacted.

## D-15: IPv4 Subnetting & Prefix Rules
- **Decision:** Prefixes `/0` through `/32` are fully supported:
  - `/31`: Valid point-to-point link (RFC 3021, two usable host addresses).
  - `/32`: Single host loopback or host route.
  - `/0` to `/30`: Enforces standard network and broadcast address validation for host interfaces.

## D-16: Enforcement of Configurable Safety Limits
- **Decision:** Both backend and C++ engine enforce identical hard resource caps:
  - Nodes: <= 200
  - Links: <= 500
  - Interfaces per node: <= 16
  - Packet size: 20 to 65,535 bytes
  - Initial TTL: 1 to 255
  - Max events per run: 10,000 (aborts gracefully with `LIMIT_EXCEEDED` event)
  - Engine execution timeout: 10 seconds

## D-17: Dual Graph Architecture
- **Decision:** The C++ engine maintains two graph representations:
  1. `PhysicalGraph`: Contains all nodes (hosts, routers, switches, cloud) and physical cables.
  2. `L3Graph`: Formed by collapsing Layer-2 switched segments into composite L3 edges with aggregated delay, sum of costs, min bandwidth, and combined loss `1 - Π(1-p)`.
  - Routing algorithms operate exclusively on the `L3Graph`.

## D-18: Ownership of Validation Logic
- **Decision:**
  - Python / Pydantic owns JSON structure, data types, and syntactic bounds.
  - C++ engine owns all semantic networking validation (IP collision, subnet alignment, gateway on-link verification, routing table consistency).
  - Frontend performs lightweight pre-validation for instant UX feedback without diverging from engine semantics.

## D-19: Route Tie-Breaking Rules
- **Decision:** FIB lookups resolve ties using a strict hierarchy:
  1. Longest Prefix Match (highest `prefixLength`)
  2. Lowest `adminPreference` (connected=0, static=1, policy=10)
  3. Lowest metric / cost
  4. Deterministic lexical order by `routeId`
  - Equal-Cost Multi-Path (ECMP) is labeled "Not modeled".

## D-20: Direct Next-Hop Resolution (Non-Recursive)
- **Decision:** In Version 1, every next-hop IP in a router FIB must be directly reachable on a connected subnet. Recursive next-hop route resolution is not modeled and is documented as a known simplification.

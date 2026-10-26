# TASKS.md — NetTrace 3D Implementation Checklist

**Purpose:** Ordered, testable work breakdown for building NetTrace 3D with an AI coding assistant (Google Antigravity).
**Companion docs (read first, in this order of authority for semantics):**

1. `NETWORKING_RULES.md` — authoritative networking behavior (wins any conflict about protocol semantics)
2. `ARCHITECTURE.md` — module boundaries, contracts, data flow
3. `PROJECT.md` — product scope, UI, milestones, definition of done

**Status:** Planning document. Nothing here is "done" until it is implemented, tested, and the evidence is recorded in the Completion Report for that task.

---

## 0. How to use this file

### 0.1 Rules for the coding agent

- [ ] Work on **one milestone at a time**, and within it one task group at a time. Never build everything in one response.
- [ ] Before editing, inspect the repository. Preserve working code. Do not delete or rewrite unrelated files.
- [ ] After each task group: build, run the relevant tests, fix what you broke, then tick the boxes.
- [ ] A box is ticked **only** when the code exists AND a test or a manual verification proves it. Put the evidence (command + result) in the milestone report.
- [ ] Never fabricate routes, events, metrics, or test results. Never ship mock data as a real simulation outcome.
- [ ] No dead buttons. Unimplemented features are labeled `planned` / `not modeled` or disabled with a tooltip.
- [ ] If a spec ambiguity blocks you, choose the simplest correct model, document it in `docs/decisions/`, add a test, and continue. Ask the human only for decisions that truly block work.
- [ ] Keep C++ engine, Python orchestration, and frontend visualization strictly separated.
- [ ] Update `README.md` and the relevant `docs/*.md` whenever behavior or architecture changes.

### 0.2 Status legend (use in the Feature Status Board, section 15)

| Status | Meaning |
|---|---|
| `implemented` | Code + passing tests + documented |
| `partial` | Works for a documented subset |
| `planned` | Not started, shown disabled/labeled in UI |
| `not modeled` | Deliberately out of scope; UI says "Not modeled" |

### 0.3 Task ID convention

`M<milestone>-<nn>` (example `M3-07`). Tests are `T-<milestone>-<nn>`. Decisions are `D-<nn>`.

### 0.4 Completion report template (paste at the end of every milestone)

```
## Milestone <N> report
- Files created:
- Files modified:
- Rules/requirements implemented (IDs):
- Commands run + ACTUAL results (pass/fail counts):
- Skipped/failing tests and why:
- Known limitations / simplifications:
- Features still partial/planned/not modeled:
- Spec discrepancies found:
- Next recommended task:
```

---

## 1. Decisions to lock before coding (Milestone 0 deliverable)

These resolve inconsistencies between the three documents. Record each as a short file in `docs/decisions/` (ADR style). **Defaults below are the recommended choices; change them only deliberately.**

- [ ] **D-01 Document precedence.** `NETWORKING_RULES.md` > `ARCHITECTURE.md` > `PROJECT.md` for semantics and contracts.
- [ ] **D-02 Canonical JSON naming (camelCase, schemaVersion `"1.0"`).**
  - Link: `id`, `a{nodeId,interfaceId}`, `b{nodeId,interfaceId}`, `adminState` (`up|down`), `directionality` (`bidirectional|directed`), `cost`, `bandwidthBps` (integer, internal canonical unit), `availableBandwidthBps` (nullable), `propagationDelayMs`, `packetLossProbability` (0..1), `mtuBytes`, `queue` (nullable).
  - UI displays Mbps/ms/bytes/% and converts at the boundary only.
  - Event: `eventId`, `runId`, `sequence`, `timeMs`, `type`, `layer`, `packetId`, `parentPacketId`, `nodeId`, `interfaceId`, `linkId`, `reasonCode`, `message`, `details`.
  - Route result and request fields follow `PROJECT.md` §17 names where not conflicting.
- [ ] **D-03 Packet states (single set):** `CREATED, ROUTE_LOOKUP, QUEUED, IN_TRANSIT, RECEIVED, FORWARDED, DELIVERED, DROPPED, EXPIRED, REASSEMBLY_FAILED, CANCELLED`. Terminal: `DELIVERED, DROPPED, EXPIRED, REASSEMBLY_FAILED, CANCELLED`. Run status is separate: `CREATED, VALIDATING, RUNNING, COMPLETED, FAILED, CANCELLED`. A dropped packet is still a `COMPLETED` run.
- [ ] **D-04 Two explicit modes** (shown in UI and result):
  - `PATH_PLANNING`: run an algorithm on the graph, report route and metrics, no packet moves.
  - `PACKET_FORWARDING`: per-router FIB lookup per packet. Default for "Run Simulation".
  - Bridge: the selected algorithm's path is converted into per-router **policy-derived static routes** (route source `policy`) before forwarding, so forwarding always uses a FIB. The result records which path was planned and which path was actually followed.
- [ ] **D-05 Hop counts.** Always report both `routerHopCount` (router forwarding operations) and `linkHopCount` (links traversed). Switches never count as router hops.
- [ ] **D-06 Loss convention.** Loss is drawn once per packet per link traversal at transmission start, using a seeded generator. Use a hand-written deterministic mapping from raw 64-bit output to `[0,1)` (do NOT use `std::uniform_real_distribution`, whose output differs across standard libraries). Loss `0` never consumes a random draw that changes results.
- [ ] **D-07 Link-down timing.** A link is checked at transmission start AND at scheduled reception time; if down at either, the packet fails with `LINK_DOWN`. Faults activate by simulation time or event sequence. Test this policy.
- [ ] **D-08 ARP.** v1 abstracts ARP: next-hop MAC resolution is deterministic and emits a clearly labeled `NEIGHBOR_RESOLVED_ABSTRACTED` event (project-specific type). No ARP cache timers. Labeled "ARP abstracted" in UI.
- [ ] **D-09 Switch model.** Learning bridge (learn source MAC, forward known, flood unknown/broadcast, never touch TTL/IP). L2 loop → warning at validation + runtime guard. STP `not modeled`.
- [ ] **D-10 ICMP.** `icmpErrorsEnabled` request option, default `true`. Suppress ICMP errors in response to ICMP errors. Rate limiting `not modeled` (labeled).
- [ ] **D-11 Time model.** Virtual clock + deterministic event priority queue from the start (ordering by `timeMs`, then priority, then insertion sequence). No wall-clock sleeps in engine.
- [ ] **D-12 Toolchain.** C++17, CMake, GoogleTest (or Catch2), nlohmann/json. Python 3.11+, FastAPI, Pydantic v2, pytest, httpx. Frontend: Vite + React + TypeScript, Vitest, React Testing Library, Playwright, React Flow (2D), React Three Fiber (3D, later). Confirm each against what is actually installed (M0).
- [ ] **D-13 Costs.** Link cost is non-negative by default. Negative costs are accepted only in an explicit Bellman–Ford "teaching" mode.
- [ ] **D-14 Cloud endpoint.** Node type `cloudEndpoint` behaves like a host with `simulated: true`. UI always labels it "Simulated endpoint".
- [ ] **D-15 Prefix edge cases.** Support `/0–/32`. `/31` allowed for point-to-point links; `/32` allowed only as a single-address (loopback-style) interface. Network/broadcast addresses rejected as host addresses on `/0–/30`. Document.
- [ ] **D-16 Default limits (configurable, enforced in backend AND engine):** nodes ≤ 200, links ≤ 500, interfaces/node ≤ 16, packet size 20..65535 bytes, TTL 1..255, events/run ≤ 10 000, engine timeout 10 s, request body ≤ 2 MB, response ≤ 20 MB, run history ≤ 100.
- [ ] **D-17 Two graphs.** The engine keeps (a) the **physical graph** (all nodes incl. switches) and (b) an **L3 graph** built by `GraphBuilder` where L2 segments (switch-connected) collapse into one edge/segment. Segment metrics: cost = sum, delay = sum, bandwidth = min, MTU = min, loss = `1 - Π(1-p)`. Routing algorithms run on the L3 graph.
- [ ] **D-18 Validation ownership.** Python validates JSON **structure** (Pydantic/JSON Schema). **Semantic** validation (IP/subnet/gateway/routes) lives only in the engine (`validate` command) so rules never diverge. Frontend does fast client-side checks for UX only.
- [ ] **D-19 Route tie-breaks.** Equal prefix length → lower `adminPreference` → lower metric → lower stable route ID. ECMP `not modeled`.
- [ ] **D-20 No recursive next-hop resolution in v1.** Next hops must be on-link via an interface. Documented limitation.

---

## M0 — Repository inspection and plan

**Goal:** Know exactly what exists and what toolchain is available. Produce the plan and skeleton docs. Write no feature code.

### M0-A Inspect
- [x] M0-01 List repository tree; identify existing frameworks, package managers, conventions, tests, CI.
- [x] M0-02 Record versions: `g++/clang++ --version`, `cmake --version`, `python --version`, `node --version`, `npm|pnpm --version`, `git --version`.
- [x] M0-03 Check availability of GoogleTest/Catch2, nlohmann/json, pytest, vitest. Note offline/network restrictions and choose vendoring vs download.
- [x] M0-04 If a repository already contains an implementation: build and test it **before** replacing anything; write a short "existing state" summary.

### M0-B Plan and docs
- [x] M0-05 Place `PROJECT.md`, `ARCHITECTURE.md`, `NETWORKING_RULES.md`, `TASKS.md` in the repo root.
- [x] M0-06 Create `docs/decisions/0001-…` through `00NN-…` for D-01…D-20.
- [x] M0-07 Create skeleton `README.md` with "Status: under construction; features listed as planned".
- [x] M0-08 Create the traceability table skeleton (section 14) in `docs/traceability.md`.
- [x] M0-09 Create the Feature Status Board (section 15) in `docs/status.md`, every feature `planned`.
- [x] M0-10 Create repo layout (empty dirs with `.gitkeep`): `engine/ backend/ frontend/ schemas/ examples/ scripts/ docs/`.
- [x] M0-11 Add `.gitignore`, `.editorconfig`, license placeholder, `scripts/test-all.sh` (runs engine, backend, frontend tests; fails if any fails).
- [x] M0-12 Write the concise implementation plan for M1 (files to create, order, risks).

**Exit criteria:** toolchain table recorded; decisions committed; plan written; repo skeleton exists; no feature code.

---

## M1 — C++ graph and routing algorithms

**Goal:** Correct, deterministic, tested algorithms on a generic weighted graph. No networking semantics yet beyond edge attributes.

### M1-A Engine scaffold
- [x] M1-01 `engine/CMakeLists.txt`: C++17, `-Wall -Wextra -Wpedantic`, optional `-Werror`, option for ASan/UBSan, library target `nettrace_core`, test target, CLI target placeholder.
- [x] M1-02 Test framework wired; `ctest` runs; one trivial passing test.
- [x] M1-03 `.clang-format` (and optional `clang-tidy` config).
- [x] M1-04 Build instructions in `engine/README.md`; verified on a clean build directory.

### M1-B Core types (`include/nettrace/model/`)
- [x] M1-05 Strong ID types: `NodeId`, `InterfaceId`, `LinkId`, `PacketId` (string-backed, comparable, hashable, stable ordering).
- [x] M1-06 Unit types or clearly named aliases: `BandwidthBps`, `DelayMs`, `PacketSizeBytes`, `SimTimeMs`, `Probability` (validated `[0,1]`).
- [x] M1-07 `Error` type with stable code + message + field path; `Result<T>` (or `std::variant`/expected-style) so no exceptions cross module boundaries silently.
- [x] M1-08 Error codes registry file (start with `NEGATIVE_WEIGHT`, `NEGATIVE_CYCLE`, `UNKNOWN_NODE`, `INVALID_WEIGHT`, `UNSUPPORTED_ALGORITHM`, `LIMIT_EXCEEDED`).

### M1-C Graph (`routing/graph`)
- [x] M1-09 Adjacency-list `Graph` with nodes and edges; deterministic iteration order (sorted by ID, not hash order).
- [x] M1-10 Edge attributes: `linkId`, `from`, `to`, `cost`, `bandwidthBps`, `delayMs`, `lossProbability`, `mtuBytes`, `active`.
- [x] M1-11 Directed vs bidirectional: a bidirectional link creates two arcs sharing one `linkId`; a directed link creates one.
- [x] M1-12 Parallel edges supported and distinguishable by `linkId`. Self-loops rejected or ignored with a documented rule.
- [x] M1-13 Graph validation: duplicate IDs, dangling endpoints, non-finite weights, invalid probabilities, non-positive bandwidth.
- [x] M1-14 Inactive edges excluded from search when the `excludeInactive` option is set (default on).

### M1-D Route result (`routing/route_result`)
- [x] M1-15 `RouteResult`: `reachable`, `pathNodeIds[]`, `pathLinkIds[]`, `objectiveName`, `objectiveValue`, `objectiveUnit`, `totalCost`, `linkHopCount`, `exploredNodeCount`, `relaxationCount`, `algorithmRuntimeMs`, `failureReason`, `warnings[]`.
- [x] M1-16 Do not overload one numeric field across objectives; always carry name + unit.
- [x] M1-17 Unreachable → `reachable=false` + reason, never zeros presented as success.

### M1-E Algorithms (`routing/`)
- [x] M1-18 **BFS:** fewest edges. Neighbor order stable. After reconstruction compute and report `totalCost`. Never described as minimizing cost.
- [x] M1-19 **Dijkstra:** binary-heap priority queue; reject negative/non-finite weights with `NEGATIVE_WEIGHT`. Tie-break: lower cost → fewer hops → lexicographically smaller path. Count explored nodes and relaxations.
- [x] M1-20 **Bellman–Ford:** supports negative edges; counts relaxation rounds; detects negative cycles **reachable from the source only**; reports the cycle nodes; a negative cycle in an unreachable component must not fail the run. Warn when a bidirectional negative edge creates a 2-cycle.
- [x] M1-21 **Widest path:** maximize min bandwidth along path (modified Dijkstra). Tie-break: lower total cost → fewer hops → lexicographic. Handle source == destination and unreachable.
- [x] M1-22 Source == destination: defined result for every algorithm (path of one node, zero links, documented bottleneck = not applicable).
- [x] M1-23 Source/destination not in graph → `UNKNOWN_NODE` error.
- [x] M1-24 Algorithm runtime measured with a monotonic clock and reported separately from any simulated time.
- [x] M1-25 Document tie-breaking and complexity in `docs/networking-model.md` (algorithms section).

### M1-F Tests (`engine/tests/routing/`)
- [x] T-1-01 Dijkstra picks minimum-cost path on a hand-built graph where the fewest-hop path is not the cheapest.
- [x] T-1-02 Dijkstra rejects a negative edge with `NEGATIVE_WEIGHT`.
- [x] T-1-03 BFS picks fewest edges even when costlier; reported cost is correct for the reconstructed path.
- [x] T-1-04 Bellman–Ford handles negative edges without cycles and matches the hand-computed result.
- [x] T-1-05 Bellman–Ford detects a reachable negative cycle and reports it.
- [x] T-1-06 Bellman–Ford ignores an unreachable negative cycle.
- [x] T-1-07 Widest path maximizes bottleneck; wider path may be longer/costlier.
- [x] T-1-08 Unreachable destination for all four algorithms.
- [x] T-1-09 Source equals destination for all four algorithms.
- [x] T-1-10 Parallel edges: correct edge chosen per algorithm; `pathLinkIds` identifies it.
- [x] T-1-11 Directed link usable only one way; bidirectional usable both ways.
- [x] T-1-12 Inactive link excluded; result changes accordingly.
- [x] T-1-13 Tie-breaking determinism: same graph run 100 times and with shuffled insertion order gives identical path.
- [x] T-1-14 Property test (seeded): on random non-negative graphs Dijkstra == Bellman–Ford == brute-force/Floyd–Warshall reference distance.
- [x] T-1-15 Property test: BFS length ≤ any other path's edge count.
- [x] T-1-16 Property test: widest-path bottleneck ≥ bottleneck of every other simple path (small graphs, brute force).
- [x] T-1-17 Malformed graph (dangling edge, duplicate ID, bad probability) is rejected, not crashed.
- [ ] T-1-18 Run under ASan/UBSan: clean (pending: runs on Linux CI; MinGW GCC lacks libasan).

### M1-G Optional (only after the above pass)
- [ ] M1-26 Minimal JSON I/O for a graph + request (`nlohmann/json`) and a tiny CLI `nettrace_cli route --in file.json` printing one JSON result. (Full contract is M4.)
- [ ] M1-27 `examples/graphs/` with 3 hand-verified graphs and expected outputs used by tests.
- [ ] M1-28 Micro-benchmark on 1k/10k-node graphs; record real numbers in `docs/testing.md` (do not claim performance without measurements).

**Exit criteria:** all T-1-* pass, sanitizer-clean, deterministic repeated runs, algorithms documented.

---

## M2 — Topology model, addressing, and basic forwarding decisions

**Goal:** Represent real network structure (interfaces, subnets, gateways, routes, switches) and make correct local-vs-remote and next-hop decisions with explanations. No packet time evolution yet.

### M2-A IPv4 and MAC (`model/ipv4`, `model/mac`)
- [x] M2-01 Strict IPv4 parser: exactly four decimal octets 0–255, no leading garbage, no whitespace tolerance, rejects `01.2.3.4` ambiguity per documented rule, rejects empty/oversized.
- [x] M2-02 Canonical formatter; parse→format round-trips.
- [x] M2-03 Prefix length validation `0..32`; mask from prefix; network address; broadcast address; host range; `contains(address)`.
- [x] M2-04 Host-address validity rules per D-15 (`/31`, `/32`, network/broadcast rejection).
- [x] M2-05 On-link test using masks (never "first three octets").
- [x] M2-06 IPv6 inputs rejected with a clear `UNSUPPORTED_PROTOCOL`/`INVALID_ADDRESS` message (never parsed as IPv4).
- [x] M2-07 MAC type: parse/format (canonical lowercase colon-separated), predicates (isBroadcast, isMulticast, isLocallyAdministered, isValidInterfaceMac), deterministic locally-administered generator (`02:00:NN:NN:II:II`), global topology uniqueness per D-23.

### M2-B Topology model (`model/topology`)
- [ ] M2-08 `Node` (host, router, switch, cloudEndpoint), `Interface` (id, name, mac, ipv4/prefix, mtu override, adminState), `Link` per D-02, `Topology` with schema version.
- [ ] M2-09 Host: interfaces, default gateway, static routes (optional), active flag. Router: interfaces, forwarding enabled flag, static routes, processing delay (ms, nullable = not modeled). Switch: ports, MAC table. Cloud: host + `simulated:true`.
- [ ] M2-10 Link endpoints reference real node+interface; each interface attaches to at most one link; link MTU vs interface MTU rule documented (effective MTU = min of the three).
- [ ] M2-11 Immutable topology snapshot + content hash for run records.

### M2-C Topology validation (`model/validate`)
Each check returns severity (`error` blocks run, `warning` doesn't), a stable code, field path, and a fix hint.
- [ ] M2-12 Duplicate node/interface/link IDs.
- [ ] M2-13 Duplicate interface IP addresses in the same broadcast domain / globally (choose and document).
- [ ] M2-14 Invalid IP/CIDR.
- [ ] M2-15 Link references missing node/interface; interface used by two links.
- [ ] M2-16 Interface/link subnet mismatch (two routed interfaces on one link in different subnets) → warning/error per rule.
- [ ] M2-17 Default gateway not on-link via a host interface (`GATEWAY_UNREACHABLE`).
- [ ] M2-18 Missing default gateway for a remote destination (checked at run time per request).
- [ ] M2-19 Negative cost where unsupported; invalid bandwidth/delay/loss/MTU.
- [ ] M2-20 Inactive source/destination/interface/router/link.
- [ ] M2-21 No route to destination (reported as `NO_ROUTE`, distinct from forwarding-time failure).
- [ ] M2-22 Disconnected topology (warning).
- [ ] M2-23 L2 loop detected among switches (warning: STP not modeled).
- [ ] M2-24 Empty topology; limits exceeded (`LIMIT_EXCEEDED`).
- [ ] M2-25 Unusual-but-legal configs produce **warnings**, blocking ones produce **errors**.

### M2-D Layer-2 domains and switch
- [ ] M2-26 Compute broadcast domains (connected components over active links, passing through switches only).
- [ ] M2-27 Neighbor resolution within a domain by IP→interface (abstracted ARP per D-08), emitting resolvable data (event emission in M3).
- [ ] M2-28 Switch MAC table structure: learn, move, lookup, flood set computation, exclude ingress port, exclude down ports.

### M2-E Routing/forwarding tables (`forwarding/`)
- [ ] M2-29 `Route` struct: prefix, nextHop (nullable = connected), outInterface, source (`connected|static|default|policy`), adminPreference, metric, valid flag, stable routeId.
- [ ] M2-30 Auto-install connected routes for each active addressed interface.
- [ ] M2-31 Host default gateway → default route (`0.0.0.0/0`).
- [ ] M2-32 Static route support with validation (next hop must be on-link per D-20).
- [ ] M2-33 **Longest-prefix match** with D-19 tie-breaking; usability check (interface up, next hop resolvable).
- [ ] M2-34 Host send decision: on-link → direct; off-link → gateway; none → `NO_ROUTE`; gateway invalid → `INVALID_GATEWAY`/`GATEWAY_UNREACHABLE`.
- [ ] M2-35 Router decision: destination is one of my addresses → local delivery; else LPM → outgoing interface + next hop; none → `NO_ROUTE`.
- [ ] M2-36 `Decision` struct carrying all evidence (matched prefix, candidates considered, rejected candidates + reason, chosen interface/next hop).
- [ ] M2-37 **Explanation generator** that builds text from the `Decision` struct (never hard-coded success text). Must cover the example sentences in `PROJECT.md` §11.4 and `NETWORKING_RULES.md` §23.

### M2-F Graph building and path→FIB bridge
- [ ] M2-38 `GraphBuilder`: topology → L3 graph per D-17 with correct segment metric aggregation; inactive things excluded and recorded as exclusions with reasons.
- [ ] M2-39 Path→routes converter (D-04): given a planned path, produce `policy` static routes on each router along it; never silently modify user routes; record that it happened.

### M2-G Starter topologies (`examples/`)
- [ ] M2-40 `same-subnet.json` (Host A 192.168.1.10/24, Host B 192.168.1.20/24, one switch).
- [ ] M2-41 `gateway-routing.json` (A 192.168.1.10/24 gw .1 → R1 {192.168.1.1/24, 10.0.0.1/24} → B 10.0.0.20/24).
- [ ] M2-42 `multi-router-cloud.json` (A → R1 → R2 → R3 → simulated cloud), distinct per-link metrics, with routes configured at every router.
- [ ] M2-43 Test-only topologies: `ttl-expiry`, `no-route`, `bandwidth-vs-cost`, `mtu-df`, `fragmentation`, `loop`, `lossy`, `link-failover` (two alternate paths).
- [ ] M2-44 Every example validates clean (or with documented expected warnings) and is shared by C++/backend/frontend tests.

### M2-H ARP & Transport Additions
- [ ] M2-45 Interface MAC required and globally unique (validation `DUPLICATE_MAC`); auto-generate when absent; persist on export.
- [ ] M2-46 AddressRegistry (read-only, built from topology snapshot); findOwnerInDomain(ip, domainId). Duplicate IP in one broadcast domain -> error `DUPLICATE_IP_IN_DOMAIN`.
- [ ] M2-47 ArpCache per node: lookup/insert/update, source tag, no timers.
- [ ] M2-48 NextHopResolution decision: nextHopIp, needsArp, cacheHit, explanation built from structured fields.
- [ ] M2-49 Abstracted resolver: registry lookup scoped to the sender's domain.
- [ ] M2-50 TransportHeader struct and port validation (1..65535).
- [ ] M2-51 Host services list in the topology model and validation (duplicate protocol+port on one host -> error).
- [ ] M2-52 Deterministic ephemeral-port allocator (49152..65535, wraps).

### M2-I Tests
- [x] T-2-01 IPv4 parse accepts/rejects table (leading zeros, 256, missing octet, extra octet, empty, whitespace).
- [x] T-2-02 Prefix math for `/0 /8 /16 /20 /23 /24 /25 /30 /31 /32`: network, broadcast, mask, contains.
- [x] T-2-03 On-link decision correct for `/16`, `/20`, `/23`, `/25` cases (including near-boundary addresses).
- [ ] T-2-04 Host in same subnet does not use gateway.
- [ ] T-2-05 Host in different subnet uses default gateway; destination IP unchanged in decision.
- [ ] T-2-06 No gateway + off-link → `NO_ROUTE`; gateway off-subnet → `GATEWAY_UNREACHABLE`.
- [ ] T-2-07 LPM: `0.0.0.0/0`, `10.0.0.0/8`, `10.4.0.0/16` for `10.4.7.9` picks `/16`; default doesn't beat specific by lower metric.
- [ ] T-2-08 Tie-break determinism (equal prefix/pref/metric → lowest routeId).
- [ ] T-2-09 Router chooses correct outgoing interface and next hop on all starter topologies.
- [ ] T-2-10 Router recognizes packet addressed to itself as local.
- [ ] T-2-11 Down interface/link makes a route unusable.
- [ ] T-2-12 Switch learn/forward/flood/move logic; never flood to ingress; never flood to down ports.
- [ ] T-2-13 Every validation rule M2-12…M2-24 has a failing-fixture test and a passing-fixture test.
- [ ] T-2-14 GraphBuilder aggregates segment metrics correctly (cost sum, delay sum, bandwidth min, MTU min, loss formula).
- [ ] T-2-15 Explanations contain the real evidence values for each scenario (assert on structured fields, not only strings).
- [x] T-2-16 Property tests: valid IPv4 round-trips; network(addr, prefix) contains addr; broadcast ≥ network.
- [ ] T-2-17 MAC unique across the whole topology.
- [ ] T-2-18 Registry lookup returns the owner inside the domain and NOTHING for an IP in another domain.
- [ ] T-2-19 ArpCache ops (lookup, insert, update, source tag).
- [ ] T-2-20 Next-hop IP is the gateway for a remote destination and the destination for an on-link one.
- [ ] T-2-21 Port validation (0 and 65536 rejected).
- [ ] T-2-22 Allocator determinism and wraparound.
- [ ] T-2-23 Duplicate service rejected.

**Exit criteria:** same-subnet and gateway scenarios produce correct decisions with correct explanations; validation catches all listed errors; tests pass.

---

## M3 — Packet lifecycle, TTL, ICMP, structured events

**Goal:** A deterministic discrete-event simulation that moves one packet hop by hop and emits a complete, ordered event log.

### M3-A Packet and lifecycle (`packet/`)
- [ ] M3-01 `Packet`: id, parentId, version (4), src/dst IP, protocol (`ICMP|UDP|TCP-metadata`), totalLength, ttl, identification, DF/MF/fragmentOffset fields (present, used in M7), payload label, createdAt, current node/interface/link, state.
- [ ] M3-02 Deterministic packet IDs (`pkt-0001`, ICMP `pkt-0002`, …).
- [ ] M3-03 Lifecycle state machine per D-03 with enforced legal transitions; terminal states cannot transition (assert + test).
- [ ] M3-04 Packet versioning: each transition appends history entry (node, time, ttl, state); original never silently mutated.
- [ ] M3-05 Frame struct: srcMac, dstMac, etherType, payload ref, size; rebuilt per link.
- [ ] M3-06 Only display/store IPv4 header fields actually modeled; checksum = "not modeled" (no fake values).

### M3-B Events (`events/`)
- [ ] M3-07 Event struct per D-02/`NETWORKING_RULES` §8.3 with `sequence`, `timeMs`, `layer`, `reasonCode`, structured `details` (typed where practical).
- [ ] M3-08 Event type enum covering the registry in section 13.1 (add only types that are emitted).
- [ ] M3-09 Event sink with hard limit → emits `EVENT_LIMIT_REACHED` and stops; never silently truncate.
- [ ] M3-10 Reason-code enum covering section 13.2; every terminal drop has exactly one code and a responsible node/link.
- [ ] M3-11 Event sequence strictly increasing; same-time events ordered by priority then insertion.

### M3-C Simulation core (`simulation/`)
- [ ] M3-12 Virtual clock + priority queue per D-11.
- [ ] M3-13 Simulation config: seed, limits, `icmpErrorsEnabled`, mode, max events, max steps.
- [ ] M3-14 **Host send:** local/remote decision event → neighbor resolution event → frame encapsulation event → transmit.
- [ ] M3-15 **Link transmit/receive:** time = propagation + serialization (`L*8/B`); state check per D-07; events `LINK_TRANSMIT_STARTED` / `LINK_RECEIVE`. (Loss draw hook present but disabled until M7.)
- [ ] M3-16 **Switch:** learn/forward/flood events; no TTL or IP change.
- [ ] M3-17 **Router:** receive → local-delivery check → LPM lookup (`ROUTE_LOOKUP`, `ROUTE_SELECTED`) → TTL rule (D-/rules §9.1) → `TTL_DECREMENTED` → new frame (new MACs) → transmit. Packet with TTL 1 → `TTL_EXPIRED`, drop, no forwarding.
- [ ] M3-18 Router processing delay added to simulation time when configured; else metric shows "not modeled".
- [ ] M3-19 **Destination delivery:** destination IP matches active local interface → `PACKET_DELIVERED` (`IP_DELIVERED` status); cloud endpoint delivery wording says "simulated endpoint accepted".
- [ ] M3-20 Router-addressed packets are consumed, TTL not decremented.
- [ ] M3-21 Forwarding with TTL decremented before the next hop; log both TTL-in and TTL-out at every router.
- [ ] M3-22 `NO_ROUTE` at host vs at router distinguished (node recorded).
- [ ] M3-23 Loop guard: repeated (router, interface, dst) state or step limit → `SIMULATION_LOOP_GUARD`; TTL normally ends loops first.
- [ ] M3-24 Run result: `runStatus`, `packetOutcome`, `reasonCode`, `routePlanned`, `routeFollowed`, `warnings`, `assumptions[]`, `engineVersion`, `schemaVersion`, `seed`.

### M3-D ICMP (`packet/icmp`, `simulation/`)
- [ ] M3-25 **Time Exceeded (type 11, code 0):** new packet, new ID, `parentId` = original, src = chosen router interface IP (policy documented), dst = original src, own TTL, routed by the router's own FIB, can itself fail.
- [ ] M3-26 **Destination Unreachable** for `NO_ROUTE` at a router (documented subtype).
- [ ] M3-27 Suppression: never generate an ICMP error in response to an ICMP error; labeled note about omitted rate limiting.
- [ ] M3-28 Original stays `EXPIRED/DROPPED` even if the ICMP response is delivered (test).
- [ ] M3-29 (Optional) Echo request/reply: reply is a separate packet; statuses `IP_DELIVERED`, `RESPONSE_GENERATED`, `RESPONSE_DELIVERED`. Do not label as "application success".

### M3-E Serialization for tests
- [ ] M3-30 Events and results serialize to canonical JSON (stable key order, fixed float format) for golden-file tests.
- [ ] M3-31 Golden event logs checked into `engine/tests/golden/` for scenarios A, B, C, TTL expiry, no-route.

### M3-F ARP & Transport Modeling
- [ ] M3-32 ARP frame/message model (kind ARP, EtherType 0x0806).
- [ ] M3-33 Request build and broadcast per ARP-3.
- [ ] M3-34 Switch flood and learn for ARP.
- [ ] M3-35 Owner-only reply, owner learns sender (ARP-5, ARP-6).
- [ ] M3-36 Requester caches reply and resumes the pending packet.
- [ ] M3-37 ARP timeout -> `NEXT_HOP_UNRESOLVED`.
- [ ] M3-38 Second packet in the same run uses `ARP_CACHE_HIT` (no ARP frames).
- [ ] M3-39 ARP never crosses a router; no TTL on ARP.
- [ ] M3-40 Router ARP on egress segment including final hop.
- [ ] M3-41 ARP frames obey link state and loss.
- [ ] M3-42 Transport demux at the destination and `APPLICATION_ACCEPTED`.
- [ ] M3-43 UDP closed port -> separate ICMP Port Unreachable; TCP closed -> `PORT_CLOSED` drop with the "RST not modeled" note.
- [ ] M3-44 ICMP and ARP carry no ports (assert in tests).

### M3-G Tests
- [ ] T-3-01 TTL boundary table on a 3-router path: TTL = 1, 2, 3, 4, 5, 255. Expect expiry at the correct router (TTL N+1 survives N routers).
- [ ] T-3-02 Switch never decrements TTL or rewrites IP.
- [ ] T-3-03 Router decrements exactly once per forwarding; log shows in/out TTL.
- [ ] T-3-04 IP src/dst unchanged end to end; MACs change on every routed link.
- [ ] T-3-05 Packet to router's own IP is consumed with TTL unchanged.
- [ ] T-3-06 Destination host does not decrement TTL.
- [ ] T-3-07 TTL expiry: no forwarding event after `TTL_EXPIRED`; correct router recorded.
- [ ] T-3-08 ICMP Time Exceeded is separate (different ID, parent link, own route events); can fail (e.g., no return route).
- [ ] T-3-09 Terminal-state guard: attempt to deliver after drop throws/asserts in test.
- [ ] T-3-10 Event sequence monotonic; identical input → byte-identical event JSON (determinism).
- [ ] T-3-11 Event limit reached → `EVENT_LIMIT_REACHED`, run not shown as success.
- [ ] T-3-12 Forwarding loop topology: TTL expiry (or loop guard code) ends it; never infinite.
- [ ] T-3-13 `NO_ROUTE` at host vs router distinct, and distinct from forwarding-time failure.
- [ ] T-3-14 Golden logs match for scenarios A, B, C.
- [ ] T-3-15 Cloud delivery wording never claims a real AWS resource.
- [ ] T-3-16 Golden event sequence for Scenario A with empty caches (see NETWORKING_RULES §8.4).
- [ ] T-3-17 Golden event sequence for Scenario B (see NETWORKING_RULES §8.4).
- [ ] T-3-18 ARP does not cross the router.
- [ ] T-3-19 ARP timeout when the target is absent.
- [ ] T-3-20 Second packet has no ARP events (`ARP_CACHE_HIT`).
- [ ] T-3-21 MACs rewritten per link, IPs unchanged, and the host ARPs for the GATEWAY (never the remote destination).
- [ ] T-3-22 UDP open port accepted (`APPLICATION_ACCEPTED`).
- [ ] T-3-23 UDP closed port gives a separate ICMP Port Unreachable.
- [ ] T-3-24 TCP closed port gives `PORT_CLOSED`.
- [ ] T-3-25 Same seed gives identical ARP event logs.

**Exit criteria:** deterministic hop-by-hop event logs; TTL boundary tests pass; ICMP is a separate packet; ARP modeled and cached; transport demux complete.

---

## M4 — JSON contracts, engine CLI, FastAPI integration

**Goal:** One versioned contract across C++, Python, and TypeScript; safe engine invocation; API runs all starter scenarios.

### M4-A Schemas (`schemas/`)
- [ ] M4-01 `topology.schema.json`, `simulation-request.schema.json`, `simulation-result.schema.json`, `error.schema.json` (JSON Schema 2020-12), each with `schemaVersion`.
  - Request schema additions: `arpMode` ("modeled"|"abstracted"), `preloadArpCaches` (bool), `arpTimeoutMs` (number), `transport` (`{protocol, srcPort?, dstPort?, tcpFlags?}`).
  - Topology schema additions: `host.services[]` (`[{protocol, port, label}]`).
  - Result schema additions: `arpCachesAfter` per node (`[{ip, mac, source}]`) and read-only `addressRegistry` export.
- [ ] M4-02 Enumerations for node types, algorithms, objectives, protocols, event types, reason codes, adminState.
- [ ] M4-03 Script `scripts/validate-examples.*` that validates every file in `examples/` against the schemas (runs in CI).
- [ ] M4-04 Unsupported major version → rejected with clear message; unknown optional fields ignored; unknown required enum values rejected.

### M4-B Engine CLI (`engine/src/cli`)
- [ ] M4-05 Commands: `nettrace_engine validate`, `route`, `simulate`, `version`. Reads one JSON document from stdin; writes **exactly one** JSON document to stdout; diagnostics to stderr.
- [ ] M4-06 Exit codes: `0` ok (including dropped packets), `2` invalid input, `3` limit exceeded, `4` internal error. Documented.
- [ ] M4-07 C++ parser validates required fields, types, enum values, ranges, limits; stable error codes + field paths.
- [ ] M4-08 Output includes `engineVersion`, `schemaVersion`, seed, assumptions, algorithm runtime.
- [ ] M4-09 No logging to stdout; no shell execution; no file path from input; input size capped before parsing.
- [ ] M4-10 Round-trip tests (parse → serialize) and malformed-input tests (truncated JSON, wrong types, huge numbers, deep nesting).

### M4-C Backend (`backend/app/`)
- [ ] M4-11 FastAPI app, `requirements.txt` (pinned), config via env vars (`NETTRACE_ENGINE_PATH`, limits, timeout). Engine path never from HTTP input.
- [ ] M4-12 Pydantic schemas mirroring the JSON Schemas; a test that fails if they drift from `schemas/`.
- [ ] M4-13 `engine/runner.py`: `asyncio.create_subprocess_exec` (argument list, no shell), stdin pipe, timeout, stdout size cap, exit-code mapping, JSON parse + schema check of engine output, cleanup on all paths, kill on timeout.
- [ ] M4-14 `services/topology_service.py` (structural validation + engine `validate`), `services/simulation_service.py`, `services/export_service.py`.
- [ ] M4-15 Endpoints (thin handlers, versioned prefix `/api`):
  - [ ] `GET /api/health` (API up, engine invocable, engine version, schema version)
  - [ ] `POST /api/topologies/validate`
  - [ ] `POST /api/simulations`
  - [ ] `GET /api/simulations/{run_id}`
  - [ ] `GET /api/simulations/{run_id}/export?format=json|csv`
- [ ] M4-16 Error model `{code, message, path?, details?}`; distinguishes validation error, no feasible route, packet drop (HTTP 200, valid outcome), engine failure, timeout, server error.
- [ ] M4-17 Request size limit, rate-limit-friendly behavior, CORS limited to the dev frontend origin.
- [ ] M4-18 Run store: in-memory, bounded (D-16); run ID generation; stores immutable snapshot (topology hash + request + result).
- [ ] M4-19 Structured logging to stderr with run ID; no secrets, no stack traces to clients.
- [ ] M4-20 OpenAPI available; `docs/api-contract.md` with request/response examples for all three starter scenarios and every error code.

### M4-D Tests
- [ ] T-4-01 Valid topology passes validation; each invalid example returns the expected structured errors.
- [ ] T-4-02 `POST /api/simulations` for all starter scenarios returns a schema-valid result with real events.
- [ ] T-4-03 Fake engine scripts to simulate: crash (nonzero exit), timeout, invalid JSON, oversized output, empty output → stable API errors, process stays alive.
- [ ] T-4-04 Oversized request body rejected; malformed JSON rejected; unknown enum rejected.
- [ ] T-4-05 Exported JSON validates against the documented schema; CSV has documented columns.
- [ ] T-4-06 Engine path cannot be influenced by request content (negative test).
- [ ] T-4-07 Same example run through C++ CLI directly and via API gives identical event sequence.
- [ ] T-4-08 Health endpoint reports engine missing correctly.

**Exit criteria:** API runs every starter scenario; contract validated in CI; engine failures never crash the API.

---

## M5 — React 2D interface

**Goal:** A user can create/load a topology, run it, and watch **real engine events** with inspectors.

### M5-A Scaffold
- [ ] M5-01 Vite + React + TypeScript (strict), ESLint, Prettier, Vitest, React Testing Library, Playwright.
- [ ] M5-02 Folder layout per `ARCHITECTURE.md` §3 (`app, components, features/*, api, state, types, styles`).
- [ ] M5-03 TypeScript types generated from `schemas/` (script + CI check for drift).
- [ ] M5-04 Typed API client with error mapping, timeout, abort support, and explicit "backend unreachable" state.

### M5-B State (three separate stores)
- [ ] M5-05 **Topology draft** (editable). **Run result** (immutable per run). **Presentation** (selection, panels, camera, playback cursor, speed).
- [ ] M5-06 Use React state/context/reducer first; add a store library only if needed and justified in an ADR.
- [ ] M5-07 Starting a run snapshots the topology; editing afterward never mutates a finished/running result; UI shows "topology changed since this run — rerun".

### M5-C Topology editor
- [ ] M5-08 Canvas with React Flow (or chosen alternative): zoom, pan, fit-to-view, drag nodes, selection.
- [ ] M5-09 Palette: host, router, switch, cloud endpoint (distinct icons + text labels).
- [ ] M5-10 Add/delete/edit node; interface editor (name, IPv4, prefix, MAC auto/override, MTU, adminState); host gateway field; router static routes table; router processing delay.
- [ ] M5-11 Add/delete/edit link by connecting interfaces; link form with unit-labelled fields (cost, bandwidth Mbps, delay ms, loss %, MTU bytes, state, directionality); clear direction indicator.
- [ ] M5-12 Client-side validation with inline messages; server validation panel listing errors vs warnings; click an issue to select the offending element.
- [ ] M5-13 Load starter topologies; duplicate topology; reset to starter.
- [ ] M5-14 Import/export topology JSON: size limit, schema validation, version check, safe rendering of labels (no HTML injection), friendly error on malformed file.
- [ ] M5-15 Local-storage persistence with versioned schema and migration/reject behavior.
- [ ] M5-16 Undo/redo marked `planned` (hidden or disabled with tooltip).

### M5-D Simulation controls and playback
- [ ] M5-17 Source/destination dropdowns (name + IP), scenario selector, algorithm, objective, hard constraints (inputs may be disabled until M6 and labeled), TTL (1–255), packet size, protocol, DF, seed, ICMP-errors toggle.
- [ ] M5-18 Run disabled with explanatory tooltip while inputs invalid or topology has blocking errors.
- [ ] M5-19 Run → loading state → result/errors displayed in a result area (delivered/dropped/no-route/engine-error all distinct).
- [ ] M5-20 Playback controller driven by `currentEventIndex`: Run/Play, Pause, Resume, Step, Reset, speed (animation only).
- [ ] M5-21 Pause truly stops progression; Reset returns to documented initial state; selecting a timeline event moves the cursor.
- [ ] M5-22 Handle: edit during run, reset during run, connection loss, engine error — each shows a visible message.

### M5-E Renderer, timeline, inspectors
- [ ] M5-23 **Shared view model** derived from topology + events (current node/link/packet position, highlighted route, per-element state). Renderers consume only this.
- [ ] M5-24 2D renderer: highlighted planned/followed route, current router + outgoing link highlight, packet marker moving per event, state icons for delivered/dropped/inactive/selected (icon + label, not color alone).
- [ ] M5-25 Event timeline: ordered list from engine events (time, node/link, type, message, reason code), keyboard navigable, `aria-live` status for current event; acts as the accessible alternative to animation.
- [ ] M5-26 Packet inspector: ID/status, src/dst IP, TTL (before/after), protocol, total length, fragment fields when present, current node/next hop, in/out interface, MAC src/dst, hop history, drop reason, related ICMP/parent reference. Unmodeled fields show "Not modeled".
- [ ] M5-27 Router inspector: interfaces/state, forwarding table with LPM-matched entry highlighted for the selected event, processing delay, recent events.
- [ ] M5-28 Explanation panel showing What/Where/Why/Evidence/What next (`NETWORKING_RULES` §23) from event data.
- [ ] M5-29 Result summary: outcome, reason, route (planned vs followed), router-hops and link-hops, assumptions list, simulation disclaimer.

### M5-F Shell and polish
- [ ] M5-30 Dashboard: name + explanation, new/starter topology buttons, recent local topologies, guided examples, "simulation, not real network" notice, implemented-vs-experimental indicator.
- [ ] M5-31 Workspace layout: top bar, left palette/validation, center canvas, right config/inspectors, bottom timeline/metrics; stacks on narrow screens, run controls always reachable.
- [ ] M5-32 Dark technical theme with sufficient contrast; focus rings; labels on all controls; tooltips for networking terms (TTL, MTU, DF, LPM, next hop…).
- [ ] M5-33 Empty, loading, and error states for every panel.

### M5-G Tests
- [ ] T-5-01 Source/destination selection and request payload construction.
- [ ] T-5-02 Run disabled/reports errors for invalid TTL, size, missing endpoints, blocking validation errors.
- [ ] T-5-03 Timeline order equals engine event order; no reordering by animation.
- [ ] T-5-04 Selecting a router shows its inspector; selecting an event shows correct packet fields.
- [ ] T-5-05 Pause/resume/step/reset behavior with fake timers.
- [ ] T-5-06 Importing malformed/oversized/wrong-version files shows an error and does not crash.
- [ ] T-5-07 Backend-unreachable and engine-error states render.
- [ ] T-5-08 Accessibility: axe checks on main views; keyboard-only flow for run + step + inspect.
- [ ] T-5-09 Playwright e2e: load gateway starter → run → step through events → see TTL change and delivery from the real backend.
- [ ] T-5-10 No place in the code computes routes/TTL/delivery on the client (grep/lint check + review).

**Exit criteria:** a user can load or build a topology, run against the real backend, and observe engine-backed events with working inspectors.

---

## M6 — Six routing criteria, constraints, metrics, comparison, export

**Goal:** Each criterion has documented, tested semantics; users can compare runs fairly.

### M6-A Criteria semantics (`routing/objectives`)
- [ ] M6-01 **Cost:** additive; Dijkstra on `cost`.
- [ ] M6-02 **Packet loss:** minimize `1 − Π(1−pᵢ)` by running Dijkstra on weights `−ln(1−pᵢ)` (additive and exact under independence); handle `p = 1` (infinite weight = edge unusable) and `p = 0`. Report resulting probability, not the transformed weight.
- [ ] M6-03 **Available bandwidth:** widest path on `availableBandwidthBps` (falls back to capacity only if documented and labeled).
- [ ] M6-04 **Congestion/utilization:** utilization = load/capacity over a named interval; objective = minimize **maximum** utilization along the path (minimax variant); also report the aggregate used. Offered load above 100% labeled "offered load". If no load model is present → `NOT_MODELED`, not 0%.
- [ ] M6-05 **MTU compatibility:** feasibility constraint only (packet length vs each egress MTU, DF, fragmentation allowed). Never an additive cost.
- [ ] M6-06 **Router processing load:** per-router modeled delay (ms) converted to additive node weight (split onto outgoing edges); label "configured processing load", never "CPU %".
- [ ] M6-07 UI/API expose **exactly these six** primary criteria.
- [ ] M6-08 No arbitrary weighted sum. (Optional later: normalized weighted score, weights exposed, flagged "heuristic".)

### M6-B Constraints
- [ ] M6-09 Hard constraints: exclude inactive links/nodes, min bandwidth, max loss, MTU/fragmentation feasibility, optional max delay, max hops.
- [ ] M6-10 Pipeline: filter by constraints → optimize objective → secondary tie-break → stable ID tie-break. Tie-break shown in result.
- [ ] M6-11 Per-edge exclusion reasons recorded (`INACTIVE`, `BELOW_MIN_BANDWIDTH`, `LOSS_TOO_HIGH`, `MTU_INFEASIBLE`…).
- [ ] M6-12 If nothing feasible → `NO_FEASIBLE_ROUTE` with list of constraints that rejected candidates (distinct from `NO_ROUTE`).
- [ ] M6-13 Invalid algorithm/objective pairs rejected clearly (e.g., BFS with a "minimize loss" objective).
- [ ] M6-14 Candidate explanation: bounded K alternative paths (Yen, K ≤ 5) with accept/reject reasons. Mark `partial` if limited.

### M6-C Metrics (`metrics/`)
- [ ] M6-15 Path metrics per `NETWORKING_RULES` §7.7 with units: total cost, router hops + link hops, propagation delay, serialization delay estimate, processing delay, estimated one-way delay (components listed), bottleneck bandwidth, estimated loss (independent), max utilization, MTU status, explored nodes/relaxations, algorithm runtime.
- [ ] M6-16 Observed metrics (from packet events) kept separate: delivered/dropped, observed loss over named sample, actual simulated delay.
- [ ] M6-17 Missing/unmodeled metrics are `null` + `"not modeled"` marker in output and UI.
- [ ] M6-18 Document every formula and unit in `docs/networking-model.md`.

### M6-D Comparison and export
- [ ] M6-19 Save runs (client-side list, and backend history) with full inputs: topology ID/version/hash, algorithm, objective, constraints, packet params, seed, engine/schema version.
- [ ] M6-20 Compare ≥ 2 runs: side-by-side table; **inputs diff shown first**; metrics highlighted for better/worse with direction explained; comparing different topologies requires an explicit warning banner.
- [ ] M6-21 Algorithm comparison on the same topology/request (multi-algorithm run): selected path, cost, bottleneck, loss estimate, explored nodes, runtime, and an explanation of why paths differ.
- [ ] M6-22 Export JSON (full) and CSV (tabular metrics); PDF = `planned`.
- [ ] M6-23 UI: policy panel (objective radio of six, constraint inputs with units), metric tooltips defining each metric, "estimated vs observed" labels.

### M6-E Tests
- [ ] T-6-01 Cost vs bandwidth objective choose different paths on `bandwidth-vs-cost` topology (acceptance scenario 10).
- [ ] T-6-02 Loss objective result equals brute-force minimum of `1−Π(1−p)` over all simple paths (small random graphs, seeded).
- [ ] T-6-03 Loss formula numeric tests (`p=0`, `p=1`, mixed values).
- [ ] T-6-04 Widest path with min-bandwidth constraint excludes thin links.
- [ ] T-6-05 MTU constraint excludes small-MTU links when DF set; allows when fragmentation permitted.
- [ ] T-6-06 Utilization objective picks lower max-utilization path; no load model → `NOT_MODELED`.
- [ ] T-6-07 Processing-delay objective works and is unit-labeled.
- [ ] T-6-08 `NO_FEASIBLE_ROUTE` lists the rejecting constraints; differs from `NO_ROUTE`.
- [ ] T-6-09 Missing metric never rendered as zero (API + UI test).
- [ ] T-6-10 Hop counts: routerHopCount excludes switches, linkHopCount includes all links.
- [ ] T-6-11 Comparison view shows input differences before metrics; cross-topology warning appears.
- [ ] T-6-12 Export round-trip validates against schema; includes all required metadata.

**Exit criteria:** all six criteria tested with documented semantics; constraint rejection explained; export works.

---

## M7 — Failure injection, loss, delay, congestion, MTU and fragmentation

**Goal:** Reproducible faults and richer packet behavior with accurate event logs.

### M7-A Fault model
- [ ] M7-01 `Fault` = target ID, type, start (time or event sequence), optional duration, applied change, source (`injected` vs permanent edit). Faults never mutate the saved topology.
- [ ] M7-02 Fault types: link down/up, router down/up, interface down/up, delay increase, loss increase, bandwidth reduction, MTU reduction, gateway removed/wrong, mismatched subnet, route removed, low TTL, queue pressure (if queues exist).
- [ ] M7-03 Fault events in the log (`FAULT_ACTIVATED`, `FAULT_CLEARED`) with changed values before/after.
- [ ] M7-04 D-07 timing policy implemented and tested.
- [ ] M7-05 Staleness: a path-planning result is marked **stale** after topology/fault changes; forwarding mode reads current state at each lookup.
- [ ] M7-06 Optional reroute: if enabled, recompute per selected policy and **record that recomputation occurred**; otherwise fail with a precise reason. Never silently alter settings to make a scenario succeed.
- [ ] M7-07 Distinguish "no route found" from "route found but forwarding failed" in output and UI.

### M7-B Loss, delay, congestion
- [ ] M7-08 Seeded loss per D-06; `PACKET_LOSS` event with link and draw metadata (probability, seed stream index — not the raw RNG secret); 0% never drops.
- [ ] M7-09 Seed recorded in result; unseeded request → engine picks and **returns** a seed (so the run is reproducible), UI notes it.
- [ ] M7-10 Delay components kept separate: propagation, serialization, queueing (if modeled), processing.
- [ ] M7-11 Bandwidth reduction affects serialization delay and widest-path metrics.
- [ ] M7-12 Congestion: **minimal honest model** — either (a) configured utilization used for penalty/queueing-delay estimate labeled "simplified congestion model", or (b) finite FIFO queue with `QUEUE_OVERFLOW`. Pick one, document, and label. No fake queue events if not implemented.
- [ ] M7-13 Processing queue/capacity (optional, `planned` if skipped); label configured load, not CPU.

### M7-C MTU, DF, fragmentation, reassembly
- [ ] M7-14 MTU check at each router egress against **IP total length**; `size == MTU` fits, `MTU+1` needs action. Outcomes enum from `NETWORKING_RULES` §10.5.
- [ ] M7-15 DF set + too big → drop `MTU_EXCEEDED_DF_SET`; separate ICMP Dest-Unreachable/Frag-Needed (type 3 code 4) if enabled; Path-MTU-Discovery flow documented as educational only.
- [ ] M7-16 DF clear → fragment: max payload `floor((MTU − 20)/8)*8`; offsets in 8-byte units; MF on all but last; same Identification; per-fragment total length ≤ MTU; payload coverage exact, no gaps/overlap. Only fragment offset 0 carries the transport header (D-22).
- [ ] M7-17 Fragments are independent packets (own IDs, `parentId`), each experiences TTL/loss/delay; `FRAGMENT_CREATED` events.
- [ ] M7-18 Re-fragmentation at a later smaller-MTU link preserves correct MF and offsets.
- [ ] M7-19 Reassembly **at destination only**: byte-range tracking, consistency checks, timeout (documented ms); events `REASSEMBLY_STARTED/COMPLETED`, `REASSEMBLY_TIMEOUT`; missing fragments listed.
- [ ] M7-20 Fragment loss → original datagram `REASSEMBLY_FAILED`; siblings never auto-marked delivered.
- [ ] M7-21 Inspector shows payload length vs IP total length vs frame length distinctly; shows transport header present only on offset 0.

### M7-D UI
- [ ] M7-22 Fault panel: add/remove faults, enable/disable, timing; shows "temporary fault vs permanent edit".
- [ ] M7-23 Before/after comparison view for a fault (reuses M6 comparison).
- [ ] M7-24 Timeline/inspector handle fragments and fault events; fragment tree view under the original packet.
- [ ] M7-25 Troubleshooting helper: shows the first failing decision and its explanation.

### M7-E Tests (regression suite)
- [ ] T-7-01 Fragmentation example 4000/1500/DF0 → payloads 1480/1480/1020, totals 1500/1500/1040, offsets 0/185/370, MF 1/1/0, shared ID; fragment offset 0 contains the transport (UDP 8B) header, subsequent fragments do not (test assert).
- [ ] T-7-02 Boundary: size == MTU (no fragmentation), MTU+1 (action), MTU 68 minimum edge cases, payload not multiple of 8 on last fragment only.
- [ ] T-7-03 DF=1 + small MTU → `MTU_EXCEEDED_DF_SET`, original not delivered, ICMP separate.
- [ ] T-7-04 Re-fragmentation across 1500 → 1000 → 576 gives valid non-overlapping coverage.
- [ ] T-7-05 Reassembly success; reassembly failure when one fragment lost; timeout path.
- [ ] T-7-06 Fixed seed → identical drops/events across runs and across platforms (CI matrix if available); different seed → different when loss > 0.
- [ ] T-7-07 Loss 0% → no drops over 10 000 simulated traversals.
- [ ] T-7-08 Link disabled before run → no route/`LINK_DOWN`; disabled mid-run per D-07; never traversed.
- [ ] T-7-09 Router disabled → forwarding fails with precise reason; reroute option records recomputation.
- [ ] T-7-10 Alternate-path topology (`link-failover`): reroute on vs off produce documented different results.
- [ ] T-7-11 Delay increase changes `timeMs` and estimated delay but not route/TTL.
- [ ] T-7-12 Misconfigured gateway / mismatched subnet produce the specific error codes.
- [ ] T-7-13 Observed vs estimated loss reported separately.
- [ ] T-7-14 Fault never mutates the stored topology (hash unchanged).

**Exit criteria:** failure scenarios produce accurate, reproducible logs and reports; regression tests pass.

---

## M8 — 3D visualization

**Goal:** Optional 3D view on the **same** view model and event cursor.

- [ ] M8-01 Add React Three Fiber/three.js as a **lazy-loaded** chunk; 2D build must work without it.
- [ ] M8-02 WebGL capability detection → graceful fallback to 2D with a message.
- [ ] M8-03 Render nodes/links/routes from the shared view model; z-position optional in topology (`position.z`), auto-layout when absent.
- [ ] M8-04 Subnet clusters shown as visually distinct regions with labels.
- [ ] M8-05 Camera: rotate, pan, zoom, reset-to-default button, fit-to-view.
- [ ] M8-06 Highlight selected route and current packet; follow playback cursor; drop/deliver states with icon + label.
- [ ] M8-07 Click node/link/packet opens the **same** inspectors as 2D.
- [ ] M8-08 2D/3D toggle preserves selection, cursor, and results; toggling never triggers a new run.
- [ ] M8-09 Performance guard: node/link count threshold → suggest 2D; low-power mode (reduced effects).
- [ ] M8-10 Keyboard-accessible controls and the timeline remain the accessible alternative.
- [ ] M8-11 No simulation, routing, or TTL logic inside 3D code (lint/review check).
- [ ] T-8-01 View-model equality test: same topology+events produce identical view-model snapshots consumed by 2D and 3D.
- [ ] T-8-02 Toggle test: result object identity unchanged, cursor preserved.
- [ ] T-8-03 Fallback test with WebGL disabled.
- [ ] T-8-04 Manual performance check on a ~50-node topology; record measured frame rate honestly in docs.

**Exit criteria:** switching views never changes simulation results; 2D remains fully functional.

---

## M9 — Validation, documentation, demo, release

### M9-A Documentation
- [ ] M9-01 `README.md`: what it is, features (accurate status), architecture diagram, prerequisites, install, run engine/backend/frontend, starter scenarios, tests, screenshots/GIFs (only real ones), limitations, future work, disclaimer that it is a simulator.
- [ ] M9-02 `docs/architecture.md`: components, data flow React → FastAPI → C++ → back, event schema, build/runtime.
- [ ] M9-03 `docs/networking-model.md`: supported behaviors, TTL convention (N+1 rule), routing vs forwarding, two modes, loss/delay model, MTU/fragmentation assumptions, all simplifications and `not modeled` items, formulas with units.
- [ ] M9-04 `docs/api-contract.md`: endpoints, examples, error codes, schema versioning.
- [ ] M9-05 `docs/testing.md`: all test commands, expected results for each starter scenario, benchmark results (measured), optional real-world validation steps.
- [ ] M9-06 Finalize `docs/traceability.md` (section 14) and `docs/status.md` (section 15) — every row has code + test + status.

### M9-B Demo and benchmarks
- [ ] M9-07 One-command setup + run scripts (`scripts/dev.*`), documented; verified from a clean clone.
- [ ] M9-08 Demo script: scenarios A → B → C → TTL expiry → link failure + reroute → DF/MTU → fragmentation → cost-vs-bandwidth comparison.
- [ ] M9-09 Repeatable demo fixtures with fixed seeds and expected outputs.
- [ ] M9-10 Benchmarks: algorithm runtime vs graph size; end-to-end simulation latency; frontend render with N nodes. Real numbers only.

### M9-C Quality gates
- [ ] M9-11 CI workflow: build engine (+sanitizers), run engine tests, backend tests, frontend unit tests, schema validation of examples, type-check, lint, Playwright e2e on the starter scenarios.
- [ ] M9-12 Full end-to-end acceptance run of the 10 scenarios (section 12) — all green or failures disclosed.
- [ ] M9-13 Security pass (section 16).
- [ ] M9-14 Accessibility pass: keyboard, screen-reader labels, contrast, non-color indicators.
- [ ] M9-15 Known-limitations list reviewed against actual behavior (no overclaiming).

### M9-D Optional real-world validation (opt-in, isolated, never default)
- [ ] M9-16 `scripts/validate-linux-netns.sh`: creates namespaces/veth pairs, requires root, clearly warns, cleans up on exit/trap, never runs in CI by default.
- [ ] M9-17 Compare selected behaviors (TTL expiry hop, fragmentation sizes with `ping -s`/`tracepath`, DF behavior) against simulator output; document matches **and mismatches**.
- [ ] M9-18 Optional TShark/pcap comparison notes. pcap export remains `planned`.

**Exit criteria:** a new developer can install, run, test, and understand the project from the docs with no undocumented steps.

---

## 12. Acceptance scenario matrix

Each row needs: example file, C++ integration test, API test, e2e/UI test, golden event log where useful.

### 12.1 `NETWORKING_RULES.md` scenarios

| ID | Scenario | Must prove | Example file | Engine test | API test | UI e2e |
|---|---|---|---|---|---|---|
| A | Same-subnet | no router, no gateway, TTL unchanged, ARP abstracted label | same-subnet | [ ] | [ ] | [ ] |
| B | Via gateway | dst IP unchanged, MAC rewritten, TTL −1, LPM on router | gateway-routing | [ ] | [ ] | [ ] |
| C | Multi-router → cloud | one decrement per router, switches none, "simulated" wording | multi-router-cloud | [ ] | [ ] | [ ] |
| D | No gateway / no route | `NO_ROUTE`, no success path drawn, failure location shown | no-route | [ ] | [ ] | [ ] |
| E | TTL expiry | router with TTL 1 drops, separate ICMP | ttl-expiry | [ ] | [ ] | [ ] |
| F | Fragmentation | 1480/1480/1020, 0/185/370, MF 1/1/0, reassembly | fragmentation | [ ] | [ ] | [ ] |
| G | DF + small MTU | `MTU_EXCEEDED_DF_SET`, separate ICMP | mtu-df | [ ] | [ ] | [ ] |
| H | Link failure after planning | stale path, no traversal of down link, recompute recorded | link-failover | [ ] | [ ] | [ ] |
| I | Routing loop | ends by TTL (or `SIMULATION_LOOP_GUARD`), cycle ≠ loop | loop | [ ] | [ ] | [ ] |
| J | Loss + estimate | estimated vs observed separate, seeded | lossy | [ ] | [ ] | [ ] |
| K | IPv6 MTU | only if IPv6 built; otherwise IPv6 rejected clearly (`not modeled`) | — | [ ] | [ ] | [ ] |

### 12.2 `PROJECT.md` §20.5 end-to-end list

- [ ] 1 Same-subnet delivery
- [ ] 2 Two-subnet gateway delivery
- [ ] 3 Multi-router delivery with sufficient TTL
- [ ] 4 Multi-router TTL expiry
- [ ] 5 No route
- [ ] 6 Disabled link
- [ ] 7 Packet loss with fixed seed
- [ ] 8 DF + too-small MTU
- [ ] 9 IPv4 fragmentation + successful reassembly
- [ ] 10 Different route for cost vs bandwidth objective

---

## 13. Registries (implement as enums; every item emitted must have a test)

### 13.1 Event types

- [ ] `RUN_STARTED` `PACKET_CREATED` `HOST_ROUTE_LOOKUP` `NEIGHBOR_RESOLVED_ABSTRACTED` `FRAME_ENCAPSULATED`
- [ ] `ARP_CACHE_HIT` `ARP_CACHE_MISS` `ARP_REQUEST_SENT` `ARP_REQUEST_RECEIVED` `ARP_REPLY_SENT` `ARP_REPLY_RECEIVED` `ARP_CACHE_UPDATED` `ARP_RESOLUTION_FAILED`
- [ ] `SWITCH_MAC_LEARNED` `SWITCH_FORWARDED` `SWITCH_FLOODED`
- [ ] `ROUTE_LOOKUP` `ROUTE_SELECTED` `NO_ROUTE` `NO_FEASIBLE_ROUTE`
- [ ] `PACKET_QUEUED` `LINK_TRANSMIT_STARTED` `LINK_RECEIVE` `PACKET_FORWARDED`
- [ ] `TTL_DECREMENTED` `TTL_EXPIRED`
- [ ] `ICMP_GENERATED`
- [ ] `TRANSPORT_DEMUX`
- [ ] `FRAGMENT_CREATED` `FRAGMENT_DROPPED` `REASSEMBLY_STARTED` `REASSEMBLY_COMPLETED`
- [ ] `FAULT_ACTIVATED` `FAULT_CLEARED`
- [ ] `PACKET_DELIVERED` `PACKET_DROPPED`
- [ ] `RUN_COMPLETED`

### 13.2 Reason codes (stable, uppercase snake case)

- [ ] `INVALID_ADDRESS` `INVALID_PREFIX` `INVALID_GATEWAY` `GATEWAY_UNREACHABLE`
- [ ] `NO_ROUTE` `NO_FEASIBLE_ROUTE` `NEXT_HOP_UNRESOLVED`
- [ ] `INTERFACE_DOWN` `LINK_DOWN` `DESTINATION_UNAVAILABLE`
- [ ] `TTL_EXPIRED` `HOP_LIMIT_EXPIRED` (IPv6 only)
- [ ] `MTU_EXCEEDED_DF_SET` `IPV6_PACKET_TOO_BIG` (IPv6 only)
- [ ] `PACKET_LOSS` `QUEUE_OVERFLOW` `PROCESSING_QUEUE_OVERFLOW`
- [ ] `REASSEMBLY_TIMEOUT`
- [ ] `PORT_UNREACHABLE` `PORT_CLOSED`
- [ ] `DUPLICATE_MAC` `DUPLICATE_IP_IN_DOMAIN` `INVALID_MAC` `INVALID_PORT`
- [ ] `SIMULATION_LOOP_GUARD` `EVENT_LIMIT_REACHED` `SIMULATION_CANCELLED`
- [ ] `UNSUPPORTED_PROTOCOL` `INVALID_CONFIGURATION` `LIMIT_EXCEEDED`
- [ ] `NEGATIVE_WEIGHT` `NEGATIVE_CYCLE` `UNKNOWN_NODE`

### 13.3 Fragmentation/MTU outcomes
- [ ] `FITS_MTU` `FRAGMENTED` `FRAGMENTATION_NEEDED_DF_SET` `FRAGMENT_DROPPED` `REASSEMBLED` `REASSEMBLY_TIMEOUT` `MTU_NOT_MODELED` (`IPV6_PACKET_TOO_BIG` only with IPv6)

### 13.4 Delivery statuses
- [ ] `IP_DELIVERED` `APPLICATION_ACCEPTED` (only if app modeled) `RESPONSE_GENERATED` `RESPONSE_DELIVERED`

### 13.5 Packet kinds
- [ ] `IP` `ARP`

---

## 14. Traceability table (fill in as you go; keep in `docs/traceability.md`)

| Rule (source) | Module | Test ID(s) | Status |
|---|---|---|---|
| Same-subnet avoids gateway (NR §4.2, §21A) | `forwarding/host_decision` | T-2-04, T-3-xx | planned |
| Mask-based on-link, any prefix (NR §4.2) | `model/ipv4` | T-2-02, T-2-03 | planned |
| Gateway must be on-link (NR §4.3) | `model/validate` | T-2-06 | planned |
| LPM + deterministic tie-break (NR §6.2) | `forwarding/fib` | T-2-07, T-2-08 | planned |
| Switch never touches TTL/IP (NR §5.2) | `simulation/switch` | T-3-02, T-2-12 | planned |
| MAC rewritten per segment (NR §5.3) | `simulation/router` | T-3-04 | planned |
| TTL: receive 1 → drop; else decrement (NR §9.1) | `simulation/router` | T-3-01, T-3-03, T-3-07 | planned |
| Router-addressed packet not decremented (NR §9.1.8, §16) | `simulation/router` | T-3-05 | planned |
| ICMP is separate packet (NR §9.4) | `packet/icmp` | T-3-08 | planned |
| Dijkstra rejects negative (NR §7.2) | `routing/dijkstra` | T-1-02 | planned |
| Bellman-Ford reachable neg cycle only (NR §7.3) | `routing/bellman_ford` | T-1-05, T-1-06 | planned |
| Widest path bottleneck (NR §7.4) | `routing/widest` | T-1-07 | planned |
| No unit-mixing weighted sum (NR §7.5, PROJECT §13.2) | `routing/objectives` | T-6-xx | planned |
| Loss formula `1−Π(1−p)` (NR §12.3) | `metrics/loss` | T-6-03 | planned |
| Estimated vs observed loss separate (NR §17.4, §25) | `metrics/` | T-7-13 | planned |
| MTU vs IP total length (NR §10.1) | `simulation/mtu` | T-7-02 | planned |
| Fragment offsets 8-byte units, MF, ID (NR §10.2) | `packet/fragment` | T-7-01 | planned |
| DF drop + ICMP frag-needed (NR §10.2, §21G) | `simulation/mtu` | T-7-03 | planned |
| Reassembly only at destination (NR §10.2) | `simulation/reassembly` | T-7-05 | planned |
| Seeded deterministic loss (NR §1.3, §12.3) | `simulation/rng` | T-7-06, T-7-07 | planned |
| Missing metric = null, not 0 (NR §1.3, §22) | `metrics/` | T-6-09 | planned |
| Event limit never silent (NR §18) | `events/sink` | T-3-11 | planned |
| Link down never traversed (NR §14.1) | `simulation/link` | T-7-08 | planned |
| Animation = engine events only (NR §1.1) | `frontend/features/simulation` | T-5-03, T-5-10 | planned |
| Cloud endpoint simulated wording (NR §16) | engine + UI | T-3-15 | planned |
| Engine invoked without shell (ARCH §5.3) | `backend/engine/runner.py` | T-4-03, T-4-06 | planned |
| Engine output single JSON (ARCH §6.6) | `engine/cli` | T-4-07 | planned |

Add a row for every remaining rule in `NETWORKING_RULES.md` (sections 3–23) before M3 ends.

---

## 15. Feature status board (keep in `docs/status.md`; update every milestone)

| Feature | Status | Notes |
|---|---|---|
| Dijkstra / BFS / Bellman-Ford / widest path | planned | M1 |
| Topology validation | planned | M2 |
| IPv4 subnetting, any prefix | planned | M2 |
| LPM forwarding, default gateway | planned | M2 |
| Learning switch | planned | M2 (STP not modeled) |
| ARP exchange | not modeled | abstracted resolution only |
| TTL + ICMP Time Exceeded | planned | M3 |
| Event timeline from engine | planned | M3/M5 |
| FastAPI integration | planned | M4 |
| React 2D editor + playback | planned | M5 |
| Six routing criteria | planned | M6 |
| Run comparison + JSON/CSV export | planned | M6 |
| PDF export | planned | later |
| Fault injection | planned | M7 |
| Packet loss (seeded) | planned | M7 |
| Congestion | planned | M7 (simplified model, labeled) |
| MTU/DF/fragmentation/reassembly | planned | M7 |
| 3D view | planned | M8 |
| UDP/TCP metadata | planned | optional; full TCP not modeled |
| IPv6 | not modeled | rejected with clear error |
| NAT / ACL / firewall / VLAN / MPLS | not modeled | later |
| OSPF-like / RIP-like protocols | not modeled | Bellman-Ford is NOT a RIP implementation |
| ECMP | not modeled | |
| Undo/redo | planned | |
| User accounts / cloud save | not modeled | |
| Real AWS integration | not modeled | cloud endpoint always simulated |
| pcap export, hex dump | planned | later |
| Linux netns validation | planned | opt-in only |

---

## 16. Cross-cutting checklists

### 16.1 Security and safety
- [ ] No shell strings built from input anywhere; subprocess uses argument lists.
- [ ] Engine path from server config only.
- [ ] Input limits enforced in frontend, backend **and** engine (D-16).
- [ ] Engine timeout + output-size cap; process killed on timeout.
- [ ] Imported files size-limited and schema-validated; labels escaped on render (no `dangerouslySetInnerHTML` with user text).
- [ ] No secrets in repo/topologies; `.env.example` only.
- [ ] No real packets sent; no root/privileged operations by default; no cloud resources/credentials.
- [ ] Stack traces and file paths never shown to end users.
- [ ] Dependencies pinned; `pip-audit`/`npm audit` reviewed; no unnecessary libraries installed.
- [ ] Fuzz/malformed-input tests for the C++ parser (at least a corpus-based test).

### 16.2 Reliability
- [ ] Engine crash, timeout, bad JSON, oversize output → clean API errors, API keeps running.
- [ ] Frontend survives backend loss mid-run and malformed responses.
- [ ] User edits/reset during a run do not corrupt the run record.

### 16.3 Accessibility and honesty of UI
- [ ] State never conveyed by color alone (icon + text).
- [ ] Timeline is a full alternative to the animation; `aria-live` for current event.
- [ ] Every disabled control explains why.
- [ ] Persistent "Simulation — not real network traffic" notice.
- [ ] "Not modeled" displayed (never `0`) for missing metrics.
- [ ] No success animation before the engine confirms delivery.
- [ ] Every simplification (ARP abstracted, STP not modeled, ICMP rate limiting omitted, simplified congestion) visible in UI help.

### 16.4 Code quality
- [ ] No giant `main.cpp` / `main.py` / mega-component; modules per `ARCHITECTURE.md`.
- [ ] No duplicated routing/forwarding logic in Python or TypeScript.
- [ ] No magic numbers (named constants with units).
- [ ] No global mutable state in the engine.
- [ ] Deterministic ordering everywhere user-visible (no hash-order dependence).
- [ ] Types explicit at module boundaries.

---

## 17. Prompt templates for Antigravity

Paste the matching prompt at the start of each session. Replace `<…>`.

### 17.1 Kickoff (M0)
```
Read PROJECT.md, ARCHITECTURE.md, NETWORKING_RULES.md and TASKS.md fully.
Do Milestone M0 only. Inspect the repository and toolchain, record versions,
write the decision records D-01..D-20 in docs/decisions/, create the repo
skeleton, and produce a short plan for M1. Do not write feature code.
Finish with the completion report template from TASKS.md section 0.4.
```

### 17.2 Implement a milestone group
```
Work on TASKS.md tasks <M?-A> only (IDs <M?-01..M?-nn>).
Before coding: restate the plan in a few lines and list the files you will
create or change. Implement in small steps. Build and run the relevant tests
after each step. Tick only the boxes you have verified, and show the exact
commands you ran and their real output counts. Do not start the next group.
End with the completion report.
```

### 17.3 Fix failing tests
```
Run <command>. For each failure: explain the root cause, fix the code (not the
test, unless the test is wrong per NETWORKING_RULES.md — cite the rule),
rerun, and report actual results. Do not delete or skip tests to get green.
```

### 17.4 Review/audit
```
Audit milestone <N> against TASKS.md and NETWORKING_RULES.md. For each task ID
mark: verified / partial / missing, with file and test references. List any
claims in README/docs/UI not supported by code or tests. Do not change code.
```

### 17.5 Spec conflict
```
You found a conflict between <doc A §x> and <doc B §y>. Do not silently pick.
Apply D-01 precedence, write a short ADR in docs/decisions/, add a test that
locks the chosen behavior, and mention it in the milestone report.
```

---

## 18. Final Definition of Done

All of the following must be true (copy of `PROJECT.md` §25 plus architecture/rules gates):

- [ ] Clean clone installs and runs using only documented commands.
- [ ] C++ algorithms implemented and tested (T-1-*), sanitizer-clean.
- [ ] Topology validation catches the listed configuration errors (T-2-13).
- [ ] Scenarios A–J (and K or a clear IPv6 rejection) pass in engine, API, and UI tests.
- [ ] Routing and forwarding are separate; the two modes are labeled; planned vs followed path both shown.
- [ ] TTL convention documented and boundary-tested; ICMP errors are separate packets.
- [ ] The event timeline, animation, and both renderers derive from real engine output.
- [ ] Inspectors show real state; unmodeled fields say "Not modeled".
- [ ] Six criteria have documented, tested semantics; constraints and rejection reasons are shown.
- [ ] Several failure scenarios are reproducible with seeds.
- [ ] Metrics are defined, unit-labeled, and never fabricated; estimated vs observed are separate.
- [ ] 2D works smoothly; 3D (if shipped) uses the same state and is optional.
- [ ] All automated tests pass, or known failures are disclosed in the README.
- [ ] README, architecture, networking-model, api-contract, and testing docs match the actual implementation and list real limitations.
- [ ] A new developer can reproduce the demo without undocumented steps.

**Implementation priority (reminder):** correctness → tests → API integration → useful 2D interface → advanced protocol/failure behavior → 3D polish.

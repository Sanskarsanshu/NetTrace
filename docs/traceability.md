# NetTrace 3D — Requirements & Test Traceability Matrix

This document tracks all networking rules, architectural constraints, and functional requirements from `NETWORKING_RULES.md`, `ARCHITECTURE.md`, and `PROJECT.md` against their implementation modules and test cases.

---

## Milestone 1 Test Execution Mapping (T-1-01 .. T-1-18)

| Test ID | Test Name | File | Status |
|---|---|---|---|
| T-1-01 | Dijkstra min-cost path & tie-breaking | `engine/tests/test_dijkstra.cpp` | passed |
| T-1-02 | Dijkstra negative weight rejection | `engine/tests/test_dijkstra.cpp` | passed |
| T-1-03 | BFS shortest hop & zero-cost links | `engine/tests/test_bfs.cpp` | passed |
| T-1-04 | Bellman-Ford negative edge handling | `engine/tests/test_bellman_ford.cpp` | passed |
| T-1-05 | Bellman-Ford reachable negative cycle detection | `engine/tests/test_bellman_ford.cpp` | passed |
| T-1-06 | Bellman-Ford unreachable negative cycle tolerance | `engine/tests/test_bellman_ford.cpp` | passed |
| T-1-07 | Widest-path bottleneck capacity maximization | `engine/tests/test_widest_path.cpp` | passed |
| T-1-08 | Unreachable destination handling | `engine/tests/test_graph.cpp` | passed |
| T-1-09 | Source == destination edge cases | `engine/tests/test_graph.cpp` | passed |
| T-1-10 | Parallel edges & link ID tracking | `engine/tests/test_graph.cpp` | passed |
| T-1-11 | Directed vs bidirectional link traversal | `engine/tests/test_graph.cpp` | passed |
| T-1-12 | Inactive node and link filtering | `engine/tests/test_graph.cpp` | passed |
| T-1-13 | Tie-breaking determinism across 100 permutations | `engine/tests/test_graph.cpp` | passed |
| T-1-14 | Cross-algorithm consistency property | `engine/tests/test_graph.cpp` | passed |
| T-1-15 | BFS hop optimality property | `engine/tests/test_graph.cpp` | passed |
| T-1-16 | Widest-path bottleneck optimality property | `engine/tests/test_graph.cpp` | passed |
| T-1-17 | Graph validation rejects malformed graphs | `engine/tests/test_graph.cpp` | passed |
| T-1-18 | Memory safety under ASan and UBSan | `engine/` | passed (Linux CI) |

---

## Requirements & Networking Rules Traceability

| Rule / Requirement | Source | Target Module | Test IDs | Status |
|---|---|---|---|---|
| Dijkstra picks min-cost path over fewer hops | NR §7.2, PROJECT §6.2 | `engine/routing/dijkstra` | T-1-01 | implemented |
| Dijkstra negative weight rejection | NR §7.2 | `engine/routing/dijkstra` | T-1-02 | implemented |
| BFS fewest link hops & post-reconstruction cost | NR §7.1, PROJECT §6.2 | `engine/routing/bfs` | T-1-03 | implemented |
| Bellman-Ford negative edge handling | NR §7.3 | `engine/routing/bellman_ford` | T-1-04 | implemented |
| Bellman-Ford reachable negative cycle detection | NR §7.3 | `engine/routing/bellman_ford` | T-1-05 | implemented |
| Bellman-Ford ignores unreachable negative cycle | NR §7.3 | `engine/routing/bellman_ford` | T-1-06 | implemented |
| Widest-path bottleneck bandwidth maximization | NR §7.4, PROJECT §6.2 | `engine/routing/widest` | T-1-07 | implemented |
| Unreachable destination for all algorithms | TASKS M1-17 | `engine/routing/*` | T-1-08 | implemented |
| Source == destination edge case handling | TASKS M1-22 | `engine/routing/*` | T-1-09 | implemented |
| Parallel edges & link ID tracking | TASKS M1-10 | `engine/routing/graph` | T-1-10 | implemented |
| Directed vs bidirectional link traversal | TASKS M1-11 | `engine/routing/graph` | T-1-11 | implemented |
| Inactive link and node exclusion | TASKS M1-12 | `engine/routing/graph` | T-1-12 | implemented |
| Tie-breaking determinism (100 shuffled runs) | TASKS M1-13 | `engine/routing/*` | T-1-13 | implemented |
| Property: Dijkstra == Bellman-Ford == Floyd-Warshall | TASKS M1-14 | `engine/routing/*` | T-1-14 | implemented |
| Property: BFS hop count optimality | TASKS M1-15 | `engine/routing/bfs` | T-1-15 | implemented |
| Property: Widest-path bottleneck optimality | TASKS M1-16 | `engine/routing/widest` | T-1-16 | implemented |
| Graph validation rejects malformed graphs | TASKS M1-17 | `engine/routing/graph` | T-1-17 | implemented |
| Memory safety under ASan/UBSan | TASKS M1-18 | `engine/` | T-1-18 | implemented (Linux CI) |
| IPv4 strict parsing, prefix math & on-link decision | NR §4.2, D-15 | `engine/model/ipv4` | T-2-01, T-2-02, T-2-03, T-2-16 | implemented |
| MAC address format, predicates & deterministic generator | NR §5.3, D-23 | `engine/model/mac` | T-2-24 | implemented |
| Same-subnet delivery avoids default gateway | NR §4.2, PROJECT §4.1 | `engine/forwarding/host_decision` | T-2-04 | planned |
| Gateway must be on directly connected subnet | NR §4.3 | `engine/model/validate` | T-2-06 | planned |
| Longest Prefix Match (LPM) FIB lookup + tie-breaks | NR §6.2 | `engine/forwarding/fib` | T-2-07, T-2-08 | planned |
| Learning switch MAC table (never touches TTL/IP) | NR §5.2, M2-28 | `engine/model/mac_table` | T-2-12 | implemented |
| Router rewrites L2 MAC frame per outgoing link | NR §5.3, PROJECT §7.6 | `engine/simulation/router` | T-2-09 | planned |
| Topology validation (rules 1..12 element-level) | PROJECT §5.4, TASKS M2-12..24 | `engine/model/validate` | T-2-13 | implemented |
| Topology model, canonical order & FNV-1a hash | TASKS M2-08..11, ADR-0027 | `engine/model/topology` | T-2-25 | implemented |
| IPv4 TTL decrement order (receive 1 -> drop; else dec) | NR §9.1 | `engine/simulation/router` | T-3-01, T-3-02 | planned |
| Router-addressed packet not decremented | NR §9.1.8 | `engine/simulation/router` | T-3-03 | planned |
| ICMP Time Exceeded is separate packet & route | NR §9.4, PROJECT §7.4 | `engine/packet/icmp` | T-3-04, T-3-05 | planned |
| Structured immutable event sequence & schema | ARCH §6, PROJECT §11 | `engine/events/event_sink` | T-3-06 | planned |
| Event limit enforcement without crashing | ARCH §6.5 | `engine/events/event_sink` | T-3-07 | planned |
| Engine invoked with arg list (no shell injection) | ARCH §5.3 | `backend/app/engine/runner.py` | T-4-01 | planned |
| Engine output is valid single JSON schema | ARCH §6.6 | `engine/src/main.cpp` | T-4-02 | planned |
| FastAPI health, validate, and simulation endpoints | PROJECT §16 | `backend/app/api/` | T-5-01, T-5-02 | planned |
| React 2D canvas renders authoritative events only | NR §1.1, DESIGN §1.3 | `frontend/src/features/simulation` | T-6-01 | planned |
| Six routing criteria implemented and tested | PROJECT §13, NR §7.5 | `engine/routing/objectives` | T-7-01..06 | planned |
| No heterogeneous unit-mixing weighted sum | PROJECT §13.2 | `engine/routing/objectives` | T-7-07 | planned |
| End-to-end loss formula 1 - product(1 - p_i) | NR §12.3 | `engine/metrics/loss` | T-7-08 | planned |
| Deterministic seeded PRNG for packet loss | NR §1.3, §12.3 | `engine/simulation/rng` | T-8-01 | planned |
| Disabled link / router prevents forwarding | NR §14.1 | `engine/simulation/` | T-8-02 | planned |
| MTU check, DF drop, and IPv4 fragmentation | NR §10, PROJECT §8 | `engine/simulation/mtu` | T-8-03, T-8-04 | planned |
| Reassembly at destination only | NR §10.2 | `engine/simulation/reassembly` | T-8-05 | planned |
| 3D view renders same state as 2D | PROJECT §10.2 | `frontend/src/features/3d` | T-9-01 | planned |
| ARP-1: Next-hop IP resolved, never remote dest | NR §5.4, D-21 | `engine/simulation/arp` | T-3-21 | planned |
| ARP-2: ARP cache hit skips ARP frames | NR §5.4, D-21 | `engine/simulation/arp` | T-3-20 | planned |
| ARP-3: ARP cache miss broadcasts request | NR §5.4, D-21 | `engine/simulation/arp` | T-3-16 | planned |
| ARP-4: Switch floods broadcast & learns source MAC | NR §5.4, D-21 | `engine/simulation/switch` | T-3-16 | planned |
| ARP-5: Only owner replies to ARP request | NR §5.4, D-21 | `engine/simulation/arp` | T-3-16, T-3-19 | planned |
| ARP-6: Target updates cache & sends unicast reply | NR §5.4, D-21 | `engine/simulation/arp` | T-3-16 | planned |
| ARP-7: Requester caches reply & transmits data frame | NR §5.4, D-21 | `engine/simulation/arp` | T-3-16 | planned |
| ARP-8: ARP timeout drops with NEXT_HOP_UNRESOLVED | NR §5.4, D-21 | `engine/simulation/arp` | T-3-19 | planned |
| ARP-9: ARP frames distinct, parentId, obey link/loss | NR §5.4, D-21 | `engine/simulation/arp` | T-3-16, T-3-25 | planned |
| ARP-10: Routers ARP on egress segment to destination | NR §5.4, D-21 | `engine/simulation/router` | T-3-17 | planned |
| ARP-11: Unmodeled ARP features labeled in UI | NR §5.4, D-21 | `engine/simulation/arp` | M3-44 / UI | planned |
| Transport: Port range 1..65535 validation | NR §8.4, D-22 | `engine/model/topology` | T-2-21 | implemented |
| Transport: Host services list & validation | NR §8.4, D-22 | `engine/model/validate` | T-2-23 | implemented |
| Transport: Deterministic ephemeral allocator | NR §8.4, D-22 | `engine/simulation/transport` | T-2-22 | planned |
| Transport: Demux listener -> APPLICATION_ACCEPTED | NR §8.4, D-22 | `engine/simulation/host` | T-3-22 | planned |
| Transport: UDP closed port -> ICMP Port Unreachable | NR §8.4, D-22 | `engine/simulation/host` | T-3-23 | planned |
| Transport: TCP closed port -> PORT_CLOSED drop | NR §8.4, D-22 | `engine/simulation/host` | T-3-24 | planned |
| Transport: ICMP and ARP carry no ports | NR §8.4, D-22 | `engine/packet/` | M3-44 | planned |
| MAC: Globally unique mock MAC across topology | NR §5.1, D-23 | `engine/model/validate` | T-2-17 | implemented |
| MAC: Generator 02:00:NN:NN:II:II determinism | NR §5.1, D-23 | `engine/model/mac` | T-2-24 | implemented |
| Registry: Scoped domain lookup in AddressRegistry | NR §5.4, D-21 | `engine/model/address_registry` | T-2-18 | implemented |
| Cache: Per-node ArpCache operations | NR §5.4, D-21 | `engine/model/arp_cache` | T-2-19 | implemented |



# NetTrace 3D — Feature Status Board

**Last Updated:** Milestone 2 Progress (47 ctest unit tests passing; T-1-18 verified via Linux ASan/UBSan CI run 40385595609)  
**Status Legend:**
- `implemented`: Code complete, tested, documented, and verified.
- `partial`: Works for a documented subset.
- `planned`: Not yet implemented, visibly disabled or labeled as planned in UI.
- `not modeled`: Deliberately out of scope for NetTrace 3D; UI explicitly displays "Not modeled".

---

| Feature | Status | Target / Notes |
|---|---|---|
| Graph Data Structure & Validation | implemented | M1 (Deterministic ordering, parallel edges, inactive filtering; T-1-08..T-1-17) |
| Dijkstra Minimum-Cost Algorithm | implemented | M1 (Min-heap, non-negative weight checks, tie-breaking; T-1-01, T-1-02) |
| BFS Minimum-Hop Algorithm | implemented | M1 (Fewest hops, stable neighbor order, post-reconstructed cost; T-1-03) |
| Bellman-Ford Negative-Weight Algorithm | implemented | M1 (Negative weights, reachable negative cycle detection, 2-cycle warning; T-1-04..T-1-06) |
| Widest-Path Bottleneck Bandwidth Algorithm | implemented | M1 (Max-heap bottleneck bandwidth, tie-breaking; T-1-07) |
| Memory Safety Sanitizers (ASan/UBSan) | implemented | M1 (Verified via Linux CI job; T-1-18) |
| Topology Data Model & Element Validation | implemented | M2-B (TopologyDraft, TopologySnapshot, FNV-1a hash, element validation rules 1..12; T-2-13, T-2-17, T-2-21, T-2-23, T-2-25) |
| Domain & Network Validation | implemented | M2-C (DUPLICATE_IP_IN_DOMAIN, SUBNET_MISMATCH, INACTIVE_COMPONENT, DISCONNECTED_TOPOLOGY, L2_LOOP_DETECTED, MIXED_SUBNETS_IN_DOMAIN; T-2-13) |
| Broadcast Domains & Address Registry | implemented | M2-D / M2-H (computeBroadcastDomains, AddressRegistry findOwnerInDomain scoped lookup; T-2-18) |
| Simulated Transparent Learning Bridge | implemented | M2-D (MacTable: learn, move, lookup, flood set excluding ingress/down; STP not modeled; T-2-12) |
| Per-Node ARP Cache | implemented | M2-H (ArpCache lookup, insert, update, source tag learned/preloaded; T-2-19) |
| IPv4 Subnetting & CIDR Calculation | implemented | M2-A (Strict parsing, canonical formatting, prefix math, usableHostRange, D-15 host validity; T-2-01..T-2-03, T-2-16) |
| MAC Addressing & Generator | implemented | M2-A (Colon-separated, predicates, deterministic generator D-23; T-2-24) |
| Local vs. Gateway Subnet Decision | implemented | M2-E (Host decision; on-link direct send vs gateway send; T-2-04..T-2-06) |
| Longest Prefix Match (LPM) FIB Lookup | implemented | M2-E (FIB LPM with D-19 numeric tie-breaking, usability filter, explanations; T-2-07..T-2-11, T-2-15, T-2-20, T-2-22, T-2-26) |
| Layer-3 Graph Aggregation & Path Bridge | implemented | M2-F (GraphBuilder D-17 segment metric aggregation, BFS/Dijkstra/WidestPath on L3Graph, D-04 policy route injection; T-2-14, T-2-27..T-2-30) |

| ARP Neighbor Resolution | planned | M3 (Modeled ARP simulation with packet queues; abstracted mode supported) |
| Transport Layer & UDP Demux | planned | M3 (Ports 1..65535, host services, ICMP Port Unreachable on closed port) |
| Packet Lifecycle & State Machine | planned | M3 (Event priority queue, deterministic scheduling) |
| Router IPv4 TTL Decrement & Boundary Check | planned | M3 (Order: check 1 -> drop; else dec) |
| ICMP Time Exceeded Response Generation | planned | M3 (Separate simulated packet & route) |
| Immutable Structured Event Log | planned | M3 (Event sink, event history logging) |
| C++ Engine CLI & JSON IPC | planned | M4 (nettrace-cli stdin/stdout JSON) |
| FastAPI Backend Service & Endpoints | planned | M4 (FastAPI orchestrator, runner, health/validate/simulate) |
| Pydantic v2 Schema Validation | planned | M4 (Strict schema validation) |
| React 2D Canvas & Topology Editor | planned | M5 (React Flow 2D canvas, component nodes/links) |
| Simulation Playback Cursor (Step/Pause/Reset) | planned | M5 (Playback cursor, step forward/back) |
| TTL Ribbon & Header Inspectors | planned | M5 (Visual inspectors, timeline filter chips) |
| Six Routing Criteria Engine & UI | planned | M6 (cost, packet-loss rate, available bandwidth, congestion/utilization, MTU compatibility, router processing load) |
| Run Comparison & Metric Differences | planned | M6 (Multi-run diff viewer) |
| JSON / CSV Export | planned | M6 (Machine-readable export) |
| Link / Router Fault Injection | planned | M7 (Down faults, failover) |
| Deterministic Seeded Packet Loss | planned | M7 (PRNG deterministic loss draws) |
| Link Congestion & Queue Penalty | planned | M7 (Congestion model) |
| MTU Check, DF Drop & IPv4 Fragmentation | planned | M7 (Constraint MTU, fragmentation) |
| Destination-Only Fragment Reassembly | planned | M7 (Reassembly queue) |
| 3D Topology Visualization (Three.js / R3F) | planned | M8 (3D spatial graph) |
| Simulated Cloud / EC2 Endpoint | planned | Labeled "Simulated endpoint", no real AWS credentials |
| Linux netns / Wireshark Validation | planned | Opt-in educational lab guide |
| IPv4 Header Checksum | not modeled | Checksum recalculation not modeled |
| TCP Handshake & State Machine | not modeled | Transport metadata only; "TCP handshake not modeled"; TCP closed port drops with PORT_CLOSED |
| IPv6 Routing & Headers | not modeled | Rejected with explicit error code |
| NAT / Firewall / ACL / VLAN / MPLS | not modeled | Future extension |
| Dynamic Routing Protocols (OSPF / BGP / RIP) | not modeled | Bellman-Ford algorithm is NOT a RIP implementation |
| ECMP (Equal-Cost Multi-Path) | not modeled | Deterministic tie-breaking rules applied |
| Cloud Multi-User Accounts & Cloud Save | not modeled | Local JSON export / import |

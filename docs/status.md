# NetTrace 3D — Feature Status Board

**Last Updated:** Milestone 1 Completion  
**Status Legend:**
- `implemented`: Code complete, tested, documented, and verified.
- `partial`: Works for a documented subset.
- `planned`: Not yet implemented, visibly disabled or labeled as planned in UI.
- `not modeled`: Deliberately out of scope for NetTrace 3D; UI explicitly displays "Not modeled".

---

| Feature | Status | Target / Notes |
|---|---|---|
| Graph Data Structure & Validation | implemented | M1 (Deterministic ordering, parallel edges, inactive filtering) |
| Dijkstra Minimum-Cost Algorithm | implemented | M1 (Min-heap, non-negative weight checks, tie-breaking) |
| BFS Minimum-Hop Algorithm | implemented | M1 (Fewest hops, stable neighbor order, post-reconstructed cost) |
| Bellman-Ford Negative-Weight Algorithm | implemented | M1 (Negative weights, reachable negative cycle detection, 2-cycle warning) |
| Widest-Path Bottleneck Bandwidth Algorithm | implemented | M1 (Max-heap bottleneck bandwidth, tie-breaking) |
| Topology Data Model & Element Validation | implemented | M2-B (TopologyDraft, TopologySnapshot, FNV-1a hash, element validation rules 1..12; T-2-13, T-2-17, T-2-21, T-2-23, T-2-25) |
| IPv4 Subnetting & CIDR Calculation | implemented | M2-A (Strict parsing, canonical formatting, prefix math, D-15 host validity) |
| MAC Addressing & Generator | implemented | M2-A (Colon-separated, predicates, deterministic generator D-23; T-2-24) |
| Local vs. Gateway Subnet Decision | planned | M2 |
| Longest Prefix Match (LPM) FIB Lookup | planned | M2 |
| Simulated Transparent Learning Bridge | planned | M2 (STP not modeled) |
| ARP Neighbor Resolution | planned | M3 (Modeled ARP with per-node caches; aging, retries, gratuitous/proxy ARP = not modeled; abstracted mode supported) |
| Transport Layer & UDP Demux | planned | M3 (Ports 1..65535, host services, ICMP Port Unreachable on closed port) |
| Packet Lifecycle & State Machine | planned | M3 |
| Router IPv4 TTL Decrement & Boundary Check | planned | M3 |
| ICMP Time Exceeded Response Generation | planned | M3 (Separate simulated packet) |
| Immutable Structured Event Log | planned | M3 |
| C++ Engine CLI & JSON IPC | planned | M4 |
| FastAPI Backend Service & Endpoints | planned | M5 |
| Pydantic v2 Schema Validation | planned | M5 |
| React 2D Canvas & Topology Editor | planned | M6 |
| Simulation Playback Cursor (Step/Pause/Reset) | planned | M6 |
| TTL Ribbon & Header Inspectors | planned | M6 |
| Six Routing Criteria Engine & UI | planned | M7 |
| Run Comparison & Metric Differences | planned | M7 |
| JSON / CSV Export | planned | M7 (PDF export planned for later) |
| Link / Router Fault Injection | planned | M8 |
| Deterministic Seeded Packet Loss | planned | M8 |
| Link Congestion & Queue Penalty | planned | M8 (Simplified model) |
| MTU Check, DF Drop & IPv4 Fragmentation | planned | M8 |
| Destination-Only Fragment Reassembly | planned | M8 |
| 3D Topology Visualization (Three.js / R3F) | planned | M9 |
| Simulated Cloud / EC2 Endpoint | planned | Labeled "Simulated endpoint", no real AWS credentials |
| Linux netns / Wireshark Validation | planned | Opt-in educational guide |
| TCP Handshake & State Machine | not modeled | Transport metadata only; "TCP handshake not modeled"; TCP closed port drops with PORT_CLOSED |
| IPv6 Routing & Headers | not modeled | Rejected with explicit error code |
| NAT / Firewall / ACL / VLAN / MPLS | not modeled | Future extension |
| Dynamic Routing Protocols (OSPF / BGP / RIP) | not modeled | Bellman-Ford algorithm is NOT a RIP implementation |
| ECMP (Equal-Cost Multi-Path) | not modeled | Deterministic tie-breaking rules applied |
| Cloud Multi-User Accounts & Cloud Save | not modeled | Local JSON export / import |

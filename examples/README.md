# NetTrace 3D — Starter and Test Topologies

This directory contains the authoritative reference topologies for NetTrace 3D.
Per **D-02** and **D-16**, all topologies follow `schemaVersion: "1.0"`, carry valid MAC addresses (persisted per **D-23**), and are formatted in canonical JSON.
Expected simulation outcomes and test vectors are maintained separately in `TEST_VECTORS.md` and automated tests.

---

| Topology File | What It Demonstrates | Milestones Used |
|---|---|---|
| `same-subnet.json` | Same-subnet direct delivery across an L2 Ethernet switch without routing (Scenario A). | M2, M3, M5 |
| `gateway-routing.json` | Inter-subnet delivery via a default gateway router (Scenario B). | M2, M3, M5 |
| `multi-router-cloud.json` | Multi-hop routing across three routers to a simulated cloud endpoint (Scenario C). | M2, M3, M5, M6 |
| `ttl-expiry.json` | Hop-by-hop TTL decrement, TTL expiration at intermediate routers, and ICMP Time Exceeded generation. | M2, M3 |
| `no-route.json` | Missing default gateway at host level and missing route at router level (`NO_ROUTE`). | M2, M3, M4 |
| `bandwidth-vs-cost.json` | Diamond topology contrasting low-cost narrow paths (Dijkstra) with high-cost wide paths (Widest Path). | M2, M6 |
| `link-failover.json` | Dual redundant diamond paths for link fault injection and dynamic path failover. | M2, M7 |
| `mtu-df.json` | Bottleneck link MTU (576 bytes) testing Don't Fragment (DF=1) packet drop semantics. | M2, M7 |
| `fragmentation.json` | Jumbo frame ingress (9000 bytes) testing router-level IPv4 packet fragmentation (DF=0). | M2, M7 |
| `lossy.json` | Links with configured loss rates testing end-to-end cumulative loss probability formulas. | M2, M7 |
| `loop.json` | Mutual static routes forming an IP forwarding loop to demonstrate TTL loop guard termination. | M2, M3 |
| `shortcut-cost-vs-hops.json` | High-cost direct link contrasting BFS fewest hops vs Dijkstra lowest cost. | M2, M6 |

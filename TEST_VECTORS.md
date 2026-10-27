# TEST_VECTORS.md — Independent expected values for NetTrace 3D

**Purpose:** Hand-calculated expected results so tests check the code against the spec, not against the code's own output.

## How to use this file

1. These values are **authoritative**. Copy them into tests as literals (with a comment citing this file and section).
2. **Never** generate an expected value by running your own code and pasting the result.
3. If your code disagrees with a value here, the code is presumed wrong. If you believe the value is wrong, do not change it silently: write an ADR in `docs/decisions/` explaining why, then update this file in a separate commit.
4. Floating-point comparisons use an absolute tolerance of `1e-12` unless stated otherwise.
5. Where a fixture says "give every interface a valid MAC", use `generateMac` or any unique valid unicast MACs; MACs are not part of these expectations unless stated.

Sections: **1** M2-E (forwarding), **2** M2-F (graph building, policy routes), **3** small earlier additions, **4** M2-G (starter and test topologies).

---

## 1. M2-E — Forwarding tables, LPM, decisions

### 1.1 Fixture S (Scenario B: two subnets, one router)

| Node | Interface | IPv4 | Notes |
|---|---|---|---|
| host-a | eth0 | 192.168.1.10/24 | defaultGateway 192.168.1.1 |
| r1 | eth0 | 192.168.1.1/24 | |
| r1 | eth1 | 10.0.0.1/24 | |
| host-b | eth0 | 10.0.0.20/24 | defaultGateway 10.0.0.1 |

Links (all Up, bidirectional): `host-a.eth0 <-> r1.eth0`, `host-b.eth0 <-> r1.eth1`.

**Forwarding tables**

| Node | Expected routes (exactly) |
|---|---|
| host-a | (1) Connected 192.168.1.0/24 via eth0, no next hop, preference 0. (2) Default 0.0.0.0/0, next hop 192.168.1.1 via eth0, preference 1 |
| r1 | (1) Connected 192.168.1.0/24 via eth0. (2) Connected 10.0.0.0/24 via eth1 |

**Decisions**

| Node | Destination | Outcome | Next hop | Egress | Matched prefix |
|---|---|---|---|---|---|
| host-a | 192.168.1.1 | Direct | 192.168.1.1 | eth0 | 192.168.1.0/24 |
| host-a | 10.0.0.20 | ViaGateway | 192.168.1.1 | eth0 | 0.0.0.0/0 |
| host-a | 192.168.1.77 | Direct | 192.168.1.77 | eth0 | 192.168.1.0/24 |
| r1 | 10.0.0.20 | Forward | 10.0.0.20 | eth1 | 10.0.0.0/24 |
| r1 | 192.168.1.10 | Forward | 192.168.1.10 | eth0 | 192.168.1.0/24 |
| r1 | 10.0.0.1 | LocalDelivery | n/a | n/a | n/a |
| r1 | 192.168.1.1 | LocalDelivery | n/a | n/a | n/a |
| r1 | 172.16.0.5 | NoRoute (`NO_ROUTE`) | n/a | n/a | n/a |

Additional cases:

- host-a → 10.0.0.20: the destination IP in the decision stays `10.0.0.20`; the next hop is never the remote destination (ARP-1).
- Remove host-b's default gateway: host-b → 192.168.1.10 is `NoRoute`; host-b → 10.0.0.1 is `Direct`.
- Set host-a.eth0 adminState Down: host-a → 10.0.0.20 is `NoRoute` with `detail = ALL_MATCHING_ROUTES_UNUSABLE`.
- Set r1 `forwardingEnabled = false`: r1 → 10.0.0.20 is `ForwardingDisabled` with reason code `FORWARDING_DISABLED`.

### 1.2 Fixture L (longest-prefix match)

Router `r`: eth0 `10.0.0.1/24`, eth1 `10.1.0.1/24`. Static routes (all preference 1):

| Prefix | Next hop | Metric |
|---|---|---|
| 0.0.0.0/0 | 10.0.0.254 | 0 |
| 10.0.0.0/8 | 10.0.0.2 | 5 |
| 10.4.0.0/16 | 10.0.0.3 | 5 |
| 10.4.7.0/24 | 10.0.0.4 | 5 |

| Destination | Expected chosen route | Why |
|---|---|---|
| 10.4.7.9 | 10.4.7.0/24 via 10.0.0.4 | longest prefix (/24) |
| 10.4.8.9 | 10.4.0.0/16 via 10.0.0.3 | /16 beats /8 and default |
| 10.200.1.1 | 10.0.0.0/8 via 10.0.0.2 | the default route has a lower metric (0) but must NOT win over a more specific prefix |
| 11.1.1.1 | 0.0.0.0/0 via 10.0.0.254 | only match |
| 10.0.0.55 | Connected 10.0.0.0/24 via eth0 | /24 beats the /8 and the default |

### 1.3 Tie-breaks (add these static routes to `r`)

| Scenario | Routes | Expected winner |
|---|---|---|
| Metric | `10.9.0.0/16 via 10.0.0.2 metric 10` vs `10.9.0.0/16 via 10.0.0.3 metric 5` | via 10.0.0.3 (lower metric) |
| Preference before metric | add `10.9.0.0/16 via 10.0.0.5, adminPreference 0, metric 100` | via 10.0.0.5 (preference is checked before metric) |
| **Numeric order trap** | `10.8.0.0/16 via 10.0.0.9` vs `10.8.0.0/16 via 10.0.0.10`, same preference and metric | via **10.0.0.9** (numeric next-hop order; string order would wrongly pick 10.0.0.10) |

### 1.4 Usability fallback

On `r`, add `10.4.0.0/16 via 10.0.0.3 metric 5` (egress eth0) and `10.4.0.0/16 via 10.1.0.2 metric 50` (egress eth1), removing the earlier `10.4.0.0/16` route.

| State | Destination 10.4.1.1 → expected |
|---|---|
| all Up | via 10.0.0.3 (eth0), the eth1 route rejected as `HIGHER_METRIC` |
| eth0 Down | via 10.1.0.2 (eth1), the eth0 route rejected as `UNUSABLE_INTERFACE_DOWN` |
| eth0 and eth1 Down | a shorter usable prefix if any matches (here `10.0.0.0/8` via eth0 is also Down, default via eth0 is Down), so `NoRoute` with `detail = ALL_MATCHING_ROUTES_UNUSABLE` |
| link on eth0 Down, interface Up | the eth0 route rejected as `UNUSABLE_LINK_DOWN` |

### 1.5 Static route validation

Router with `eth0 10.0.0.1/24`:

| Route | Expected |
|---|---|
| 10.5.0.0/16 via 10.0.0.2 | valid |
| 10.5.0.0/16 via 172.16.0.1 | `ROUTE_NEXT_HOP_NOT_ON_LINK` |
| 10.5.0.0/16 via 10.0.0.1 (own IP) | `INVALID_ROUTE` |
| 10.5.0.1/16 via 10.0.0.2 (host bits set) | `INVALID_ROUTE` |
| 10.5.0.0/16 with outInterface `eth9` | `ROUTE_INTERFACE_NOT_FOUND` |
| 10.5.0.0/16 with neither next hop nor interface | `INVALID_ROUTE` |
| 10.5.0.0/16 outInterface `eth0`, no next hop | valid (directly attached; next hop is the destination) |
| 0.0.0.0/0 via 10.0.0.254 | valid |
| the same route listed twice | warning `DUPLICATE_ROUTE` |

### 1.6 Next-hop resolution

| Case | Expected |
|---|---|
| host-a → 10.0.0.20, empty ArpCache | nextHopIp 192.168.1.1, needsArp = true |
| same, ArpCache holds 192.168.1.1 → (r1.eth0's MAC from the fixture) | needsArp = false, cachedMac equals that MAC |
| host-a → 192.168.1.77 | nextHopIp 192.168.1.77 |

### 1.7 Explanation checks

For host-a → 10.0.0.20 in Fixture S the structured evidence must be:

| Key | Value |
|---|---|
| destinationIp | 10.0.0.20 |
| matchedPrefix | 0.0.0.0/0 |
| routeSource | Default |
| nextHopIp | 192.168.1.1 |
| egressInterface | eth0 |
| onLink | false |

**Substitution guard:** build the explanation for Fixture S, then for a copy with every address changed (192.168.1.x → 172.20.1.x and 10.0.0.x → 10.50.0.x). The second text must contain the new addresses and contain **none** of the old ones. This proves text is built from fields.

### 1.8 Ephemeral ports

- First three values: 49152, 49153, 49154.
- The 16384th value is 65535.
- The 16385th value is 49152 (wraps).
- Two allocators (two hosts) are independent.

---

## 2. M2-F — GraphBuilder and path-to-routes bridge

### 2.1 Fixture A (two links through a switch)

| Node | Interface | IPv4 |
|---|---|---|
| host-a | eth0 | 192.168.1.10/24 |
| host-b | eth0 | 192.168.1.20/24 |
| sw1 | p1, p2 | none |

| Link | Endpoints | cost | bandwidthBps | availableBandwidthBps | delayMs | loss | mtu |
|---|---|---|---|---|---|---|---|
| l1 | host-a.eth0 – sw1.p1 | 1 | 100000000 | 80000000 | 1.0 | 0.1 | 1500 |
| l2 | sw1.p2 – host-b.eth0 | 2 | 50000000 | 30000000 | 2.5 | 0.2 | 1400 |

L3 nodes: host-a, host-b (the switch is not an L3 node). Expected arcs: exactly 2 (host-a→host-b, host-b→host-a). Each arc:

| Metric | Value |
|---|---|
| cost | 3 |
| bandwidthBps (min) | 50000000 |
| availableBandwidthBps (min) | 30000000 |
| propagationDelayMs (sum) | 3.5 |
| packetLossProbability | 1 − (0.9 × 0.8) = **0.28** |
| mtuBytes (min of effective MTUs) | 1400 |
| underlyingLinkIds | host-a→host-b: [l1, l2]; host-b→host-a: [l2, l1] |
| switchesTraversed | [sw1] |
| physical link hops | 2 |

Variations:

- Remove `availableBandwidthBps` from l2 only → segment `availableBandwidthBps` is **not modeled** (nullopt), because one link lacks it.
- Set host-b.eth0 interface MTU override to 1280 → segment MTU 1280 (effective MTU = min of link and endpoint overrides).
- Make l2 **directed** from sw1 to host-b → arc host-a→host-b exists, arc host-b→host-a does **not**.
- Set sw1 adminState Down → 0 arcs; exclusions = exactly `[node sw1: INACTIVE_COMPONENT]`.
- Set link l2 adminState Down → 0 arcs; exclusions = exactly `[link l2: INACTIVE_COMPONENT]`.
- Set interface host-b.eth0 adminState Down → 0 arcs; exclusions = exactly `[interface host-b/eth0: INACTIVE_COMPONENT]`.

Exclusions list only elements whose **own** admin state is Down.

### 2.2 Three hosts on one switch

host-a 192.168.1.10/24, host-b 192.168.1.20/24, host-c 192.168.1.30/24, each linked to sw1 (ports p1, p2, p3). Expected: 3 L3 nodes, **6 arcs** (3 pairs × 2 directions). Every pair has 2 physical link hops and switchesTraversed `[sw1]`.

### 2.3 Fixture M (multi-router with a switch and a shortcut)

| Node | Interface | IPv4 |
|---|---|---|
| host-a | eth0 | 192.168.1.10/24, gateway 192.168.1.1 |
| sw1 | p1, p2 | none |
| r1 | eth0 | 192.168.1.1/24 |
| r1 | eth1 | 10.0.12.1/30 |
| r1 | eth2 | 10.0.13.1/30 |
| r2 | eth0 | 10.0.12.2/30 |
| r2 | eth1 | 10.0.23.1/30 |
| r3 | eth0 | 10.0.23.2/30 |
| r3 | eth1 | 10.0.35.1/30 |
| r3 | eth2 | 10.0.13.2/30 |
| cloud (CloudEndpoint) | eth0 | 10.0.35.2/30, gateway 10.0.35.1 |

| Link | Endpoints | cost |
|---|---|---|
| lA | host-a.eth0 – sw1.p1 | 1 |
| lB | sw1.p2 – r1.eth0 | 1 |
| l12 | r1.eth1 – r2.eth0 | 5 |
| l23 | r2.eth1 – r3.eth0 | 5 |
| l13 | r1.eth2 – r3.eth2 | 20 |
| l3c | r3.eth1 – cloud.eth0 | 2 |

All links: bidirectional, Up, bandwidthBps 100000000, delay 1.0, loss 0, MTU 1500.

**L3 graph:** nodes {host-a, r1, r2, r3, cloud} (5). Undirected pairs: host-a–r1 (via sw1, cost 2, 2 physical links), r1–r2 (5), r2–r3 (5), r1–r3 (20), r3–cloud (2) → **5 pairs, 10 arcs**.

**Paths from host-a to cloud**

| Algorithm | L3 path | totalCost | routerHopCount | linkHopCount (physical) | switches |
|---|---|---|---|---|---|
| Dijkstra | host-a, r1, r2, r3, cloud | 14 | 3 | 5 | [sw1] |
| BFS | host-a, r1, r3, cloud | 24 | 2 | 4 | [sw1] |
| Widest path (all bandwidths equal, tie → lower cost) | host-a, r1, r2, r3, cloud | 14 | 3 | 5 | [sw1] |

BFS minimizes the number of **L3 edges** (3 vs 4), not physical links. Router hops count routers strictly between source and destination.

### 2.4 Policy routes (path → forwarding entries)

Destination: cloud, IP 10.0.35.2. Policy routes are `/32` host routes with source `Policy`, installed on every node on the path except the destination.

**Dijkstra path** — exactly 4 routes:

| Node | Route | Egress |
|---|---|---|
| host-a | 10.0.35.2/32 via 192.168.1.1 | eth0 |
| r1 | 10.0.35.2/32 via 10.0.12.2 | eth1 |
| r2 | 10.0.35.2/32 via 10.0.23.2 | eth1 |
| r3 | 10.0.35.2/32 directly attached (no next hop) | eth1 |

**BFS path** — exactly 3 routes:

| Node | Route | Egress |
|---|---|---|
| host-a | 10.0.35.2/32 via 192.168.1.1 | eth0 |
| r1 | 10.0.35.2/32 via 10.0.13.2 | eth2 |
| r3 | 10.0.35.2/32 directly attached | eth1 |

**User route override check.** Add a user static route on r1: `10.0.35.0/30 via 10.0.13.2` (egress eth2).

| Case | r1 lookup for 10.0.35.2 | overriddenUserRoutes |
|---|---|---|
| no policy routes | static /30 via 10.0.13.2, egress eth2 | n/a |
| Dijkstra-path policy routes | policy /32 via 10.0.12.2, egress eth1 | exactly one entry: r1 `10.0.35.0/30 via 10.0.13.2` |
| BFS-path policy routes | policy /32 via 10.0.13.2, egress eth2 | **empty** (same next hop, nothing actually overridden) |

After building policy routes the topology snapshot **hash is unchanged**, and r1's user routes list is unchanged.

**Error cases:** an unreachable destination → no routes and `NO_ROUTE`; a destination IP that does not belong to the last node of the path → `INVALID_CONFIGURATION`.

### 2.5 Determinism

- Building the L3 graph twice from the same snapshot gives identical canonical text.
- Building it from drafts whose nodes and links were inserted in a different (shuffled) order gives the same text.
- The aggregated L2 path inside a domain uses fewest physical links, ties broken by lexicographic node ID (STP is not modeled).

---

## 3. Smaller earlier additions

### 3.1 usableHostRange

| Prefix | first .. last |
|---|---|
| 192.168.1.0/24 | 192.168.1.1 .. 192.168.1.254 |
| 10.0.0.4/30 | 10.0.0.5 .. 10.0.0.6 |
| 10.0.0.0/31 | 10.0.0.0 .. 10.0.0.1 |
| 10.9.9.9/32 | 10.9.9.9 .. 10.9.9.9 |
| 0.0.0.0/0 | 0.0.0.1 .. 255.255.255.254 |
| 10.0.16.0/20 | 10.0.16.1 .. 10.0.31.254 |
| 192.168.4.0/23 | 192.168.4.1 .. 192.168.5.254 |

### 3.2 MIXED_SUBNETS_IN_DOMAIN (warning)

| Fixture | Expected |
|---|---|
| sw1 with host 192.168.1.10/24 and host 192.168.1.20/24 | no warning |
| sw1 with host 192.168.1.10/24 and host 192.168.2.20/24 | exactly 1 warning (hint: hosts need a router) |
| sw1 with host 192.168.1.10/24 and host 192.168.1.20/25 | exactly 1 warning |

---

## 4. M2-G — Starter and test topologies (`examples/`)

### 4.0 Conventions for every example file

- `schemaVersion` `"1.0"`, plus `id`, `name`, `description`. No expectations are stored inside the JSON; they live here and in tests.
- Every link is bidirectional, adminState Up, cost 1, bandwidthBps 100000000, propagationDelayMs 1.0, packetLossProbability 0.0, mtuBytes 1500, unless a table below says otherwise.
- Every host/router/cloud interface has an explicit, unique, valid MAC (assigned by `normalize` and **persisted**, D-23). Switch ports have no IP and no MAC.
- Each file must: load OK, validate with **0 errors and 0 warnings** (unless stated), freeze OK, give the same hash on two loads, and satisfy `toJson(freeze(load(file))) == file bytes` (the committed file is already in canonical form).
- "Dijkstra/BFS/Widest" below mean the M1 algorithms run on the L3 graph from M2-F, with costs as given. "Hops" are `routerHopCount` (routers strictly between source and destination) and `linkHopCount` (physical links).

### 4.1 `same-subnet.json` (Scenario A)

host-a `192.168.1.10/24`, host-b `192.168.1.20/24` (service UDP 5000 "echo"), switch sw1 (ports p1, p2). Links: l1 `host-a.eth0 – sw1.p1`, l2 `host-b.eth0 – sw1.p2`.

- host-a → 192.168.1.20: `Direct`, next hop 192.168.1.20, matched 192.168.1.0/24.
- L3 graph: 2 nodes, 2 arcs; each arc cost 2, delay 2.0, bandwidth 100000000, mtu 1500, switches [sw1], 2 physical links.

### 4.2 `gateway-routing.json` (Scenario B)

Exactly Fixture S from section 1.1, plus host-b service UDP 5000 "echo". No switch.

- FIBs and decisions: the tables in 1.1.
- L3 graph: 3 nodes, 4 arcs. Dijkstra host-a → host-b: `[host-a, r1, host-b]`, cost 2, routerHops 1, linkHops 2.

### 4.3 `multi-router-cloud.json` (Scenario C)

| Node | Interface | IPv4 |
|---|---|---|
| host-a | eth0 | 192.168.1.10/24, gateway 192.168.1.1 |
| sw1 | p1, p2 | none |
| r1 | eth0 / eth1 | 192.168.1.1/24 / 10.0.12.1/30 |
| r2 | eth0 / eth1 | 10.0.12.2/30 / 10.0.23.1/30 |
| r3 | eth0 / eth1 | 10.0.23.2/30 / 10.0.35.1/30 |
| cloud (CloudEndpoint) | eth0 | 10.0.35.2/30, gateway 10.0.35.1, service UDP 5000 "echo" |

| Link | Endpoints | cost | bandwidthBps | delayMs | loss |
|---|---|---|---|---|---|
| lA | host-a.eth0 – sw1.p1 | 1 | 1000000000 | 0.5 | 0 |
| lB | sw1.p2 – r1.eth0 | 1 | 1000000000 | 0.5 | 0 |
| l12 | r1.eth1 – r2.eth0 | 5 | 100000000 | 5.0 | 0 |
| l23 | r2.eth1 – r3.eth0 | 8 | 50000000 | 12.0 | 0.01 |
| l3c | r3.eth1 – cloud.eth0 | 2 | 100000000 | 2.0 | 0 |

Static routes: r1 `10.0.35.0/30 via 10.0.12.2`, `10.0.23.0/30 via 10.0.12.2`; r2 `192.168.1.0/24 via 10.0.12.1`, `10.0.35.0/30 via 10.0.23.2`; r3 `192.168.1.0/24 via 10.0.23.1`, `10.0.12.0/30 via 10.0.23.1`.

Expected:

- Dijkstra host-a → cloud: `[host-a, r1, r2, r3, cloud]`, totalCost **17**, routerHops 3, linkHops 5, switches [sw1], summed delay 20.0, bottleneck bandwidth 50000000, estimated loss 0.01.
- Forwarding toward 10.0.35.2: r1 next hop 10.0.12.2 egress eth1; r2 next hop 10.0.23.2 egress eth1; r3 `Forward` next hop 10.0.35.2 egress eth1 (connected).
- Return toward 192.168.1.10: r3 next hop 10.0.23.1 egress eth0; r2 next hop 10.0.12.1 egress eth0; r1 `Forward` next hop 192.168.1.10 egress eth0.
- For M3 later: 3 routers, so initial TTL 3 expires at r3 and TTL 4 reaches the cloud endpoint.

### 4.4 `ttl-expiry.json`

| Node | Interface | IPv4 |
|---|---|---|
| host-a | eth0 | 10.1.0.10/24, gateway 10.1.0.1 |
| r1 | eth0 / eth1 | 10.1.0.1/24 / 10.12.0.1/30 |
| r2 | eth0 / eth1 | 10.12.0.2/30 / 10.23.0.1/30 |
| r3 | eth0 / eth1 | 10.23.0.2/30 / 10.2.0.1/24 |
| host-b | eth0 | 10.2.0.20/24, gateway 10.2.0.1 |

Links (default metrics): `host-a.eth0–r1.eth0`, `r1.eth1–r2.eth0`, `r2.eth1–r3.eth0`, `r3.eth1–host-b.eth0`. Static routes: r1 `10.2.0.0/24 via 10.12.0.2`, `10.23.0.0/30 via 10.12.0.2`; r2 `10.1.0.0/24 via 10.12.0.1`, `10.2.0.0/24 via 10.23.0.2`; r3 `10.1.0.0/24 via 10.23.0.1`, `10.12.0.0/30 via 10.23.0.1`.

Expected:

- Dijkstra host-a → host-b: 5 nodes, cost 4, routerHops 3, linkHops 4.
- Forwarding toward 10.2.0.20: r1 next hop 10.12.0.2 eth1; r2 next hop 10.23.0.2 eth1; r3 `Forward` next hop 10.2.0.20 eth1. Return toward 10.1.0.10: r3 next hop 10.23.0.1 eth0; r2 next hop 10.12.0.1 eth0; r1 `Forward` next hop 10.1.0.10 eth0.
- For M3 later (TTL on arrival at each router): initial TTL 1 expires at r1; 2 at r2; 3 at r3; 4 is delivered to host-b; 255 is delivered.

### 4.5 `no-route.json`

| Node | Interface | IPv4 |
|---|---|---|
| sw1 | p1, p2, p3 | none |
| host-a | eth0 | 192.168.1.10/24, **no gateway** |
| host-d | eth0 | 192.168.1.30/24, gateway 192.168.1.1 |
| r1 | eth0 / eth1 / eth2 | 192.168.1.1/24 / 10.0.0.1/24 / 10.9.0.1/24 |
| host-b | eth0 | 10.0.0.20/24, gateway 10.0.0.1 |
| r2 | eth0 / eth1 | 10.9.0.2/24 / 172.16.0.1/24 |
| host-c | eth0 | 172.16.0.10/24, gateway 172.16.0.1 |

Links (default metrics): `host-a.eth0–sw1.p1`, `host-d.eth0–sw1.p2`, `r1.eth0–sw1.p3`, `r1.eth1–host-b.eth0`, `r1.eth2–r2.eth0`, `r2.eth1–host-c.eth0`. Static routes: r2 `192.168.1.0/24 via 10.9.0.1`, `10.0.0.0/24 via 10.9.0.1`. **r1 deliberately has no route to 172.16.0.0/24.**

Expected (0 errors, 0 warnings):

- host-a → 10.0.0.20: `NoRoute` at host-a, reason `NO_ROUTE` (a host-level failure).
- host-a → 192.168.1.30: `Direct`.
- host-d → 172.16.0.10: `ViaGateway`, next hop 192.168.1.1.
- r1 → 172.16.0.10: `NoRoute`, reason `NO_ROUTE` (a router-level failure, different node from the case above).
- r1 → 10.0.0.20: `Forward`, egress eth1. r2 → 192.168.1.30: `Forward`, next hop 10.9.0.1, egress eth0.
- Planning vs forwarding: the L3 graph **is** connected: Dijkstra host-d → host-c gives `[host-d, r1, r2, host-c]`, cost 4, even though forwarding fails at r1.

### 4.6 Plan D (diamond) shared by `bandwidth-vs-cost.json` and `link-failover.json`

| Node | Interface | IPv4 |
|---|---|---|
| host-a | eth0 | 10.1.0.10/24, gateway 10.1.0.1 |
| r1 | eth0 / eth1 / eth2 | 10.1.0.1/24 / 10.12.0.1/30 / 10.13.0.1/30 |
| r2 | eth0 / eth1 | 10.12.0.2/30 / 10.24.0.1/30 |
| r3 | eth0 / eth1 | 10.13.0.2/30 / 10.34.0.1/30 |
| r4 | eth0 / eth1 / eth2 | 10.24.0.2/30 / 10.34.0.2/30 / 10.2.0.1/24 |
| host-b | eth0 | 10.2.0.20/24, gateway 10.2.0.1 |

Link endpoints: `ha host-a.eth0–r1.eth0`, `l12 r1.eth1–r2.eth0`, `l24 r2.eth1–r4.eth0`, `l13 r1.eth2–r3.eth0`, `l34 r3.eth1–r4.eth1`, `lb r4.eth2–host-b.eth0`.

Static routes (both files): r1 `10.2.0.0/24 via 10.12.0.2`; r2 `10.2.0.0/24 via 10.24.0.2`, `10.1.0.0/24 via 10.12.0.1`; r3 `10.2.0.0/24 via 10.34.0.2`, `10.1.0.0/24 via 10.13.0.1`; r4 `10.1.0.0/24 via 10.24.0.1`.

**`bandwidth-vs-cost.json`** metrics:

| Link | cost | bandwidthBps |
|---|---|---|
| ha, lb | 1 | 1000000000 |
| l12, l24 | 1 | 10000000 |
| l13, l34 | 5 | 1000000000 |

| Algorithm | Path host-a → host-b | totalCost | bottleneck bandwidth | routerHops | linkHops |
|---|---|---|---|---|---|
| Dijkstra | host-a, r1, r2, r4, host-b | 4 | 10000000 | 3 | 4 |
| Widest | host-a, r1, r3, r4, host-b | 12 | 1000000000 | 3 | 4 |
| BFS (tie on edge count, lexicographic r2 < r3) | host-a, r1, r2, r4, host-b | 4 | 10000000 | 3 | 4 |

Policy routes for the **Widest** path toward 10.2.0.20 (4 routes): host-a `/32 via 10.1.0.1` eth0; r1 `via 10.13.0.2` eth2; r3 `via 10.34.0.2` eth1; r4 directly attached eth2. `overriddenUserRoutes` = exactly one entry: r1 `10.2.0.0/24 via 10.12.0.2`. (host-a's default route and r3's static route have the same next hop as the policy route, so they are not overridden.)

**`link-failover.json`** metrics: every link bandwidthBps 100000000; costs ha 1, l12 1, l24 1, l13 5, l34 5, lb 1.

| State | Result |
|---|---|
| all Up | Dijkstra `[host-a, r1, r2, r4, host-b]`, cost 4 |
| l24 Down | exclusions exactly `[link l24: INACTIVE_COMPONENT]`; Dijkstra `[host-a, r1, r3, r4, host-b]`, cost 12 |
| l24 and l34 Down | `reachable = false` |
| l24 Down, forwarding decisions | r1 → 10.2.0.20 still `Forward` via 10.12.0.2; **r2** → 10.2.0.20 `NoRoute` with `detail = ALL_MATCHING_ROUTES_UNUSABLE` (route found, forwarding fails) |

### 4.7 Plan Q (line) shared by `mtu-df.json`, `fragmentation.json`, `lossy.json`

| Node | Interface | IPv4 |
|---|---|---|
| host-a | eth0 | 10.1.0.10/24, gateway 10.1.0.1 |
| r1 | eth0 / eth1 | 10.1.0.1/24 / 10.12.0.1/30 |
| r2 | eth0 / eth1 | 10.12.0.2/30 / 10.2.0.1/24 |
| host-b | eth0 | 10.2.0.20/24, gateway 10.2.0.1, service UDP 5000 "echo" |

Links: `la host-a.eth0–r1.eth0`, `lm r1.eth1–r2.eth0`, `lb r2.eth1–host-b.eth0`. Static routes: r1 `10.2.0.0/24 via 10.12.0.2`; r2 `10.1.0.0/24 via 10.12.0.1`. Baseline: Dijkstra host-a → host-b `[host-a, r1, r2, host-b]`, cost 3, routerHops 2, linkHops 3.

| File | Link overrides | Expected |
|---|---|---|
| `mtu-df.json` | lm mtuBytes **576** | arc r1–r2 mtu 576; arcs host-a–r1 and r2–host-b mtu 1500; path MTU (min of arcs) 576. For M7: DF=1 with IP size 576 passes, 577 and 1000 are dropped at r1 with `MTU_EXCEEDED_DF_SET` |
| `fragmentation.json` | la mtuBytes **9000**; lm and lb 1500 | arc MTUs 9000 / 1500 / 1500; path MTU 1500. For M7 (DF=0, fragmented at r1): size 4000 → totals 1500/1500/1040, payloads 1480/1480/1020, offsets 0/185/370, MF 1/1/0; size 3000 → totals 1500/1500/40, payloads 1480/1480/20, offsets 0/185/370, MF 1/1/0; size 1501 → totals 1500/21, payloads 1480/1, offsets 0/185, MF 1/0; size 1500 → not fragmented |
| `lossy.json` | la loss **0.05**, lm loss **0.10**, lb loss 0.0 | per-arc loss exactly 0.05, 0.10, 0.0; estimated path loss `1 − (0.95 × 0.90 × 1.0)` = **0.145** (tolerance 1e-12); the loss-0 arc never contributes randomness |

### 4.8 `loop.json`

| Node | Interface | IPv4 |
|---|---|---|
| host-a | eth0 | 10.1.0.10/24, gateway 10.1.0.1 |
| r1 | eth0 / eth1 | 10.1.0.1/24 / 10.12.0.1/30 |
| r2 | eth0 / eth1 | 10.12.0.2/30 / 10.2.0.1/24 |
| host-c | eth0 | 10.2.0.10/24, gateway 10.2.0.1 |

Links (default metrics): `host-a.eth0–r1.eth0`, `r1.eth1–r2.eth0`, `r2.eth1–host-c.eth0`. Static routes: r1 `10.9.0.0/24 via 10.12.0.2`; r2 `10.9.0.0/24 via 10.12.0.1` (a deliberate loop for the unowned destination 10.9.0.99).

Expected (0 errors; 0 warnings, since loop detection is not implemented):

- host-a → 10.9.0.99: `ViaGateway`, next hop 10.1.0.1.
- r1 → 10.9.0.99: `Forward`, next hop 10.12.0.2, egress eth1. r2 → 10.9.0.99: `Forward`, next hop 10.12.0.1, egress eth0.
- For M3 later: initial TTL 5 expires at **r1** after exactly 4 forwarding operations (r1, r2, r1, r2).

### 4.9 `shortcut-cost-vs-hops.json`

Exactly Fixture M from section 2.3, plus: services cloud UDP 5000 "echo"; static routes r1 `10.0.35.0/30 via 10.0.13.2` and `10.0.23.0/30 via 10.0.12.2`; r2 `192.168.1.0/24 via 10.0.12.1`, `10.0.35.0/30 via 10.0.23.2`; r3 `192.168.1.0/24 via 10.0.13.1`, `10.0.12.0/30 via 10.0.23.1`.

Expected: the path table in 2.3 (Dijkstra cost 14 vs BFS cost 24) and the policy-route table in 2.4, including the one overridden user route on r1 for the Dijkstra path and none for the BFS path.

### 4.10 Loader error fixtures (for the JSON loader)

| Input | Expected code |
|---|---|
| `{` (truncated) | `INVALID_JSON` |
| valid JSON without `nodes` | `MISSING_FIELD`, path `nodes` |
| `nodes` is a string | `INVALID_FIELD_TYPE`, path `nodes` |
| node type `"firewall"` | `INVALID_ENUM_VALUE`, path to that node's type |
| interface address `1.2.3` | `INVALID_ADDRESS` |
| prefix `33` | `INVALID_PREFIX` |
| MAC `02-00-00-00-00-01` | `INVALID_MAC` |
| `schemaVersion` `"2.0"` | `UNSUPPORTED_SCHEMA_VERSION` |
| input larger than 2 MB | `LIMIT_EXCEEDED` |
| nesting deeper than 64 | `LIMIT_EXCEEDED` |
| an unknown **optional** field | ignored, loads OK |

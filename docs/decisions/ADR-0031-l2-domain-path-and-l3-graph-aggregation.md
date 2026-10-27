# ADR-0031: Layer-2 Domain Path Selection and L3 Graph Aggregation (M2-F)

## Context
Milestone 2 Group M2-F aggregates physical Layer-2 broadcast domains into Layer-3 composite arcs (`GraphBuilder`) and translates planned end-to-end paths into hop-by-hop forwarding policies (`PathBridge`) per D-04, D-05, and D-17.

Within a broadcast domain containing Layer-2 switches and multiple Layer-3 endpoints (hosts, routers, cloud endpoints), multiple physical links may exist between any two Layer-3 nodes. A deterministic rule is required to determine the physical Layer-2 path traversed inside each domain and compute the aggregated segment metrics.

## Decisions

### 1. Layer-2 Path Selection Inside a Broadcast Domain
- Between any ordered pair of Layer-3 nodes `(u, v)` within the same broadcast domain, the Layer-2 path is computed using Breadth-First Search (BFS) over the domain's active physical topology (including switches).
- **Tie-Breaking Rule:** When multiple paths between `u` and `v` have an identical minimum number of physical links, ties are broken deterministically by lexicographically comparing the sequence of traversed node IDs (`NodeId::str()`).
- **Directionality:** A directed physical link (`Directionality::Directed`) can only be traversed from endpoint `a` to endpoint `b`. A composite L3 arc exists from `u` to `v` if and only if every physical link on the L2 path is traversable in that direction.
- Spanning Tree Protocol (STP) is labeled "Not modeled" per D-09.

### 2. Segment Metric Aggregation Formulas
For the unique deterministic physical path `P = [l_1, l_2, ..., l_k]` between `u` and `v`:
- `cost = sum(l_i.cost)`
- `propagationDelayMs = sum(l_i.propagationDelayMs)`
- `bandwidthBps = min(l_i.bandwidthBps)`
- `mtuBytes = min(effectiveMtu(l_i))` across all links in `P`, where `effectiveMtu` accounts for link and attached interface MTU overrides.
- `packetLossProbability = 1.0 - product(1.0 - l_i.packetLossProbability)`
- `availableBandwidthBps`: `min(l_i.availableBandwidthBps)` if ALL links in `P` specify available bandwidth; `nullopt` if ANY link on the segment lacks it.

### 3. Exclusions Recording
- Elements whose OWN administrative state is `AdminState::Down` (node, interface, link) are recorded in the graph builder report as `INACTIVE_COMPONENT`.
- Elements that are administratively `Up` but connected to down elements are not listed as inactive components.

### 4. Policy Route Installation & Non-Destructive Invariance
- `planPolicyRoutes(snapshot, routeResult, destIp)` installs `/32` host routes with `source = RouteSource::Policy` on every node along the path except the destination.
- For each hop from `node_i` to `node_{i+1}`:
  - If `node_{i+1}` is the final destination, `nextHop = nullopt` (direct delivery).
  - If `node_{i+1}` is an intermediate router, `nextHop = interface IP of node_{i+1} in the shared domain`.
  - `egressInterface = interface ID of node_i in the shared domain`.
- The `TopologySnapshot` and user routes are NEVER modified; the snapshot content hash remains invariant.
- `overriddenUserRoutes` records any user static/default route matching `destIp` that would have sent traffic to a different next hop or egress interface.

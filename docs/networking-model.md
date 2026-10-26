# NetTrace 3D — Networking & Routing Algorithms Model

**Document:** `docs/networking-model.md`  
**Purpose:** Technical documentation for graph routing algorithms, tie-breaking logic, and complexity targets.

---

## 1. Graph Representation

The core network graph is modeled as an adjacency-list `Graph` in `engine/include/nettrace/routing/graph.hpp`.
- **Deterministic Iteration:** Nodes and outgoing edges are maintained in sorted order (`NodeId::operator<`, `Edge::operator<`).
- **Parallel Edges:** Supported and distinguished by unique `linkId`.
- **Directionality:** Bidirectional links generate two symmetric directed arcs sharing one `linkId`.
- **Self-Loops:** Strictly rejected with error code `SELF_LOOP_REJECTED`.
- **Inactive Elements:** Edges and nodes with `active == false` are excluded from search when `excludeInactive` is set (default true).

---

## 2. Routing Algorithms

### 2.1 Dijkstra (Minimum Additive Cost)
- **Objective:** Find the path from source $s$ to destination $d$ minimizing total edge cost $\sum c_e$.
- **Data Structure:** Binary min-heap (`std::priority_queue`).
- **Edge Weight Constraint:** Strictly non-negative ($c_e \ge 0$). Any negative edge weight halts execution with error code `NEGATIVE_WEIGHT`.
- **Time Complexity:** $\mathcal{O}((V + E) \log V)$ using binary heap.
- **Tie-Breaking Hierarchy:**
  1. Lower cumulative distance/cost.
  2. Fewer link hops.
  3. Lexicographically smaller predecessor node ID.

### 2.2 BFS (Fewest Link Hops)
- **Objective:** Find the path from $s$ to $d$ with the minimum number of traversed links.
- **Data Structure:** FIFO Queue (`std::queue`).
- **Post-Hoc Cost Calculation:** After reconstructing the shortest-hop path, BFS computes and reports `totalCost`, `bottleneckBandwidthBps`, `estimatedDelayMs`, and `estimatedLossProbability`.
- **Time Complexity:** $\mathcal{O}(V + E)$.

### 2.3 Bellman-Ford (Negative Edge Weights & Cycle Detection)
- **Objective:** Find shortest paths in graphs with negative edge weights; detect reachable negative-weight cycles.
- **Relaxation Rounds:** Up to $|V| - 1$ rounds of edge relaxation from source $s$.
- **Reachable Negative Cycle Detection:** On the $|V|$-th round, checks for any relaxation originating from a reachable node ($\text{dist}[u] < \infty$). Unreachable negative cycles in disconnected components do not cause failure.
- **Cycle Extraction:** Traces predecessor pointers $|V|$ steps backwards to enter the cycle, then records the exact loop in `cycleNodeIds`.
- **2-Cycle Warning:** Generates explicit warnings when a bidirectional link with negative weight creates a 2-node cycle.
- **Time Complexity:** $\mathcal{O}(V \cdot E)$.

### 2.4 Widest Path (Bottleneck Bandwidth Maximization)
- **Objective:** Maximize the minimum bandwidth along the path: $\max \min_{e \in P} B_e$.
- **Data Structure:** Modified Dijkstra with max-heap (`std::priority_queue`).
- **Time Complexity:** $\mathcal{O}((V + E) \log V)$.
- **Tie-Breaking Hierarchy:**
  1. Greater bottleneck bandwidth.
  2. Lower total path cost.
  3. Fewer link hops.
  4. Lexicographically smaller predecessor node ID.

---

## 3. Boundary Cases & Invariants

- **Source Equals Destination:**
  - `reachable = true`
  - `pathNodeIds = {source}`
  - `pathLinkIds = {}`
  - `totalCost = 0.0`, `linkHopCount = 0`, `routerHopCount = 0`
  - Telemetry records 1 node explored and 0 relaxations.
- **Unreachable Destination:**
  - `reachable = false`
  - `failureCode = UNREACHABLE`
  - Numeric fields report `0` or null; never represented as a successful zero-cost path.
- **Source or Destination Missing:**
  - `reachable = false`
  - `failureCode = UNKNOWN_NODE`

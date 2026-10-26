# ADR-0030: Forwarding Information Base (FIB) Numeric Tie-Breaking Order and StateView Usability

## Context
Milestone 2 Group M2-E implements Forwarding Tables (FIB), Longest-Prefix Match (LPM) lookups with evidence, and forwarding decisions for hosts and routers. Rule D-19 requires a deterministic tie-break ordering when multiple routes match with equal prefix length, administrative preference, and metric. Furthermore, routes must be evaluated for operational usability (whether egress interface, link, or node is administratively Up), with the ability to inject dynamic fault state (M7).

## Decisions

### 1. Numeric Route Tie-Breaking Hierarchy (D-19 Refinement)
When resolving a destination IPv4 address against matching routes in the FIB, candidates are strictly evaluated in order:
1. **Longest Prefix Match**: Highest `prefixLength()` (e.g. `/24` > `/16` > `/8` > `/0`). A lower-cost default route never overrides a more specific subnet route.
2. **Lowest Administrative Preference**: Ascending order (Connected: `0` < Static: `1` < Default: `1` < Policy: `1`).
3. **Lowest Metric**: Ascending numeric value (`5` < `10`).
4. **Numeric Route Order (Trap Avoidance)**:
   - Next-hop comparison: Directly connected routes (`nextHop == nullopt`) precede non-connected routes (`nextHop.has_value()`).
   - If both have a `nextHop`, compare the IPv4 addresses as **32-bit unsigned integers** in host byte order:
     `a.nextHop->toUint32() < b.nextHop->toUint32()`.
     *Note:* Lexicographical string comparison is explicitly rejected because `"10.0.0.10" < "10.0.0.9"` lexicographically, whereas numerically `10.0.0.9 < 10.0.0.10`.
   - If next hops are identical: Compare egress `InterfaceId` lexicographically (`a.outInterface < b.outInterface`).
   - If egress interfaces are identical: Compare `RouteSource` enum value.

### 2. Usability StateView Abstraction
A route is operationally usable if and only if:
- The hosting node is `AdminState::Up`.
- The egress interface is `AdminState::Up`.
- The link attached to the egress interface (if any) is `AdminState::Up`.

To decouple usability from static snapshot state and allow runtime fault injection (M7), the engine defines an abstract `StateView`:
```cpp
class StateView {
public:
    virtual ~StateView() = default;
    virtual bool isNodeUp(const NodeId& id) const = 0;
    virtual bool isInterfaceUp(const NodeId& nodeId, const InterfaceId& ifaceId) const = 0;
    virtual bool isLinkUp(const LinkId& linkId) const = 0;
};
```
A default `SnapshotStateView` reads the administrative states directly from the `TopologySnapshot`.

### 3. Usability Filter Timing in LPM Lookup
Unusable routes are filtered out **before** candidate selection. Unusable matching routes are recorded in the decision evidence with their specific rejection reason (`UNUSABLE_INTERFACE_DOWN`, `UNUSABLE_LINK_DOWN`, `UNUSABLE_NODE_DOWN`). As a result, a shorter prefix that is usable can correctly win over a longer prefix whose egress link/interface is down.

### 4. Canonical Format Version 1.1
To include static routes in `TopologySnapshot::freeze()`, `Node` now serializes `ROUTE:` lines sorted by prefix length, address, next-hop, and interface. The FNV-1a hash algorithm and its standard test vectors remain identical.

# ADR-0029: Forwarding Information Base (FIB), Longest-Prefix Match (LPM), and Decision Engine

## Context
Milestone 2 Group M2-E requires modeling forwarding tables (`Route`, `Fib`), Longest-Prefix Match (LPM) lookups with deterministic tie-breaking (D-19), host and router forwarding decision logic (M2-34, M2-35), structured decision evidence (M2-36), dynamic educational explanation generation (M2-37), next-hop resolution (M2-48), and deterministic ephemeral port allocation (M2-52).

## Decisions

### 1. Route Representation (`Route`)
- `prefix`: `IPv4Subnet` representing the destination CIDR block (e.g. `10.4.0.0/16`, `0.0.0.0/0`).
- `nextHop`: `std::optional<IPv4Address>`:
  - `std::nullopt` indicates a directly connected route (on-link destination).
  - A concrete address indicates an on-link next-hop router.
- `outInterface`: `InterfaceId` identifying the egress NIC.
- `source`: Enum `RouteSource` (`Connected`, `Static`, `Default`, `Policy`).
- `adminPreference`: `uint32_t` determining administrative priority:
  - Connected: `0`
  - Static: `1`
  - Policy: `10`
  - Default: `20`
- `metric`: `uint32_t` representing administrative link/path cost.
- `valid`: `bool` indicating if the route is installed and usable.
- `routeId`: `std::string` unique and deterministic identifier (e.g., `rt-c-eth0`, `rt-default`, `rt-s-0001`).

### 2. FIB Population and Interface Operational State
- When constructing a node's FIB:
  - Active addressed interfaces automatically install a `Connected` route for their subnet with preference `0`, `metric = 0`, `nextHop = nullopt`.
  - Host nodes with a configured `defaultGateway` install a `Default` route (`0.0.0.0/0`) via that gateway on the interface hosting the gateway's subnet.
  - Explicit static routes configured on routers/hosts are validated: per D-20, their `nextHop` must be on-link via an active interface.
- Usability check: If an interface or link is `AdminState::Down`, routes pointing out that interface are considered unusable during lookup.

### 3. Longest-Prefix Match (LPM) Tie-Breaking Hierarchy (D-19)
When resolving a destination IPv4 address against the FIB, matching candidates are filtered and evaluated by a strict order:
1. **Longest Prefix Match**: Highest `prefixLength()` (e.g., `/16` beats `/8` and `/0`). A lower-preference default route never beats a specific prefix simply due to cost.
2. **Lowest Administrative Preference**: E.g., `Connected` (`0`) < `Static` (`1`) < `Policy` (`10`) < `Default` (`20`).
3. **Lowest Metric / Cost**: E.g., metric `1` < metric `10`.
4. **Lexicographical Route ID**: Deterministic tie-breaker (`routeId < other.routeId`).

### 4. Host Forwarding Decision (M2-34)
- **Local / On-Link Delivery**: If destination IP belongs to one of the host's connected interface subnets:
  - Decision type: `DirectSend`
  - Outgoing interface: The interface containing the subnet
  - `nextHopIp`: Destination IP (on-link neighbor)
  - `needsArp`: `true`
- **Remote / Gateway Delivery**: If destination is not on-link, consult FIB/default gateway:
  - Decision type: `GatewaySend`
  - Outgoing interface: Gateway interface
  - `nextHopIp`: Configured default gateway IP
  - `destinationIp` in IP header remains the final remote destination (D-08, D-21).
  - `needsArp`: `true`
- **Unroutable / No Gateway**: If off-link and no default gateway exists, fails with `NO_ROUTE`. If gateway is off-subnet, fails with `GATEWAY_UNREACHABLE`.

### 5. Router Forwarding Decision (M2-35)
- **Local Ingress Delivery**: If destination IP matches any of the router's own active interface IPs:
  - Decision type: `LocalDelivery`
  - Packet is locally consumed; TTL is not decremented (D-09).
- **Forwarding via FIB**: Otherwise, perform LPM lookup in the router FIB:
  - If match found: Decision type `Forward`, with chosen interface, next-hop IP, and matched route.
  - If no route matches: Decision type `Drop` with reason code `NO_ROUTE`.

### 6. Structured Evidence & Explanation Generation (M2-36, M2-37)
- The `ForwardingDecision` struct records:
  - `destinationIp`, `decisionType`, `chosenInterface`, `nextHopIp`, `matchedRoute`
  - `candidateRoutes`: list of all routes whose prefix contains `destinationIp`
  - `rejectedRoutes`: list of matching candidate routes that lost the tie-break, with exact reasons (e.g. `"shorter prefix: /8 vs /16"`, `"higher metric: 10 vs 1"`, `"interface down"`).
- `generateExplanation(decision)` dynamically generates human-readable sentences matching `PROJECT.md` §11.4 and `NETWORKING_RULES.md` §23 from these structured fields.

### 7. Ephemeral Port Allocator (M2-52)
- Manages ephemeral source ports for transport packets.
- Range: `49152..65535`.
- Per-node allocator starts at `49152`, increments monotonically, and wraps back to `49152` upon reaching `65535`.

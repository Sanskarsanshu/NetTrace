# ADR-0028: Broadcast Domains, AddressRegistry, and Switch MAC Table

## Context
Milestone 2 requires modeling Layer-2 broadcast domains (M2-26), duplicate IP validation within broadcast domains (M2-13, M2-46), read-only `AddressRegistry` (D-21, M2-46, M2-49), per-node `ArpCache` (D-21, M2-47), and switch MAC forwarding tables (D-09, M2-28).

## Decisions

### 1. Broadcast Domain Definition and Computation (M2-26)
- **Boundary:** Routers and Hosts terminate Layer-2 broadcast domains. Traversal never crosses between two interfaces of a router or host.
- **Pass-through:** Transparent Switches forward and flood Layer-2 broadcast across all active switch ports.
- **Active Only:** Only administratively active (`Up`) links and active interfaces/nodes participate in broadcast domain connectivity.
- **Algorithm:**
  - Build an undirected graph where vertices are `InterfaceRef` (identifying `(nodeId, interfaceId)`).
  - An active link between two host/router interfaces connects their two interface endpoints directly (point-to-point broadcast domain).
  - An active link between a host/router interface and a switch port connects the interface to that switch port.
  - An active link between two switch ports connects those switch ports.
  - Inside any switch node, all active ports are connected together at Layer 2 (internal bridge crossbar).
  - Find connected components of this interface graph.
  - Each connected component forms a `BroadcastDomain` with deterministic ID `bd-0001`, `bd-0002`, ... ordered by the lowest `(nodeId, interfaceId)` in the component.

### 2. Duplicate IP Validation Rule (M2-13 & M2-46)
- Per D-23 and RFC standards, IP addresses must be unique within their Layer-2 broadcast domain.
- If two host/router interfaces in the same broadcast domain have the identical IPv4 address, validation emits Error `DUPLICATE_IP_IN_DOMAIN`.
- Having the same IP on interfaces in different, disjoint broadcast domains (e.g., private `192.168.1.1` on two independent LANs behind different router ports) is legal and does not produce an error.

### 3. Direct Link Subnet Mismatch (M2-16)
- When an active link connects two routed interfaces (neither endpoint is a switch port), both interfaces must reside on the same IP subnet (i.e., `subA.contains(subB.address()) && subB.contains(subA.address())` and `subA.prefixLength() == subB.prefixLength()`).
- If subnets differ, validation emits Error `SUBNET_MISMATCH`.

### 4. Inactive Component Warning (M2-20)
- Any node, interface, or link configured with `adminState == AdminState::Down` generates Warning `INACTIVE_COMPONENT`.

### 5. Disconnected Topology Warning (M2-22)
- If the undirected graph of active nodes and active links contains more than one connected component, validation generates Warning `DISCONNECTED_TOPOLOGY`.

### 6. L2 Switch Loop Warning (M2-23)
- In the subgraph consisting exclusively of switch nodes and active links between them, cycles represent Layer-2 loops.
- Because Spanning Tree Protocol (STP) is explicitly not modeled (D-09), any cycle among switches generates Warning `L2_LOOP_DETECTED`.

### 7. AddressRegistry Structure (D-21, M2-46)
- Global, read-only lookup table constructed from `TopologySnapshot`.
- Each entry contains `{ip, mac, nodeId, interfaceId, broadcastDomainId}`.
- `findOwnerInDomain(ip, domainId)` returns the matching entry if found within the given `domainId`; if the IP is in another domain or unassigned, it returns `std::nullopt`.

### 8. Switch MAC Forwarding Table (D-09, M2-28)
- Maintains dynamic mappings `MacAddress -> InterfaceId` (port).
- `learn(mac, ingressPort)`: records or updates port mapping. Returns enum `Learned`, `Updated` (MAC movement), or `Unchanged`.
- `lookup(mac)`: returns mapped port if present, else `std::nullopt`.
- `getFloodPorts(ingressPort, allPorts)`: returns all ports except `ingressPort` whose `adminState` is `Up`.

### 9. Per-Node ARP Cache (D-21, M2-47)
- Stores entries `{ip, mac, source}` where `source` is `Learned` or `Preloaded`.
- Provides `lookup`, `insertOrUpdate`, `clear`, and `entries`.

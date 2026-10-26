# NETWORKING_RULES.md
## NetTrace 3D — Authoritative Networking Semantics and Simulation Rules

**Document status:** Normative implementation specification  
**Audience:** Antigravity coding agent, frontend/backend developers, simulation-engine developers, testers, and technical reviewers  
**Purpose:** Define the networking behavior NetTrace 3D must simulate so that routes, packet movement, forwarding decisions, failures, metrics, and teaching explanations are technically consistent.

> **Implementation priority:** These rules define the authoritative behavior of the simulator. If a UI idea, animation, shortcut, or example conflicts with this document, follow this document or explicitly document a deliberate educational simplification. Never present a simplified model as full real-world behavior.

---

# 1. Core principles

## 1.1 The simulator must model behavior, not just draw paths

A visible packet animation is not proof that a packet was successfully forwarded. Every animation must be driven by events emitted by the simulation engine. The engine must decide:

- Which interface receives a frame or packet.
- Whether the destination is local to the current host or must be sent to a gateway.
- Which route and next hop a router selects.
- Whether the selected interface and link are operational.
- Whether the packet is allowed by the forwarding rules.
- Whether TTL expires.
- Whether MTU and fragmentation rules permit forwarding.
- Whether congestion, delay, loss, or resource constraints affect delivery.
- Whether a response packet, such as ICMP Time Exceeded, is generated.
- Whether the packet is delivered, dropped, or remains in flight when the simulation ends.

The frontend must render these decisions and their events. It must not independently invent a successful path or silently override the engine.

## 1.2 Separate topology, control plane, forwarding plane, and packet state

Use distinct concepts:

1. **Topology:** Nodes, interfaces, links, link properties, and operational state.
2. **Configuration:** IP addresses, masks/prefixes, default gateways, routes, routing policies, MTUs, and protocol settings.
3. **Control plane:** Route computation or routing-protocol-like updates that determine which routes are available.
4. **Forwarding plane:** Per-packet lookup and forwarding decisions using the current forwarding table.
5. **Packet state:** Source/destination addresses, TTL, size, protocol, fragmentation fields, current location, and lifecycle.
6. **Events:** Immutable records explaining what happened and why.
7. **Metrics:** Values derived from topology, selected path, traffic, and actual simulation outcomes.

Do not conflate the route-computation result with successful packet delivery. A route can exist while delivery fails due to a down link, TTL expiry, MTU/DF behavior, packet loss, congestion, or destination unavailability.

## 1.3 Determinism and reproducibility

- A simulation run must use a recorded seed for all randomized loss, tie-breaking, or traffic generation.
- The same topology, configuration, algorithm, traffic, and seed should produce the same result unless a feature explicitly introduces nondeterministic external input.
- Record the engine version, schema version, selected algorithm, seed, and simulation settings in each run result.
- Never fabricate metrics, packet events, or route outcomes to make a demonstration look successful.
- If a metric is unknown or not modeled, report `null` / “not modeled” rather than inventing a number.

## 1.4 Explicitly label simplifications

The simulator is educational, not a full operating-system network stack. For every simplification that affects results, document it in the UI or help documentation. Examples include:

- No real Ethernet bit-level encoding or physical-layer waveform simulation.
- No real ARP/NDP exchange unless explicitly implemented.
- No full TCP congestion-control or retransmission implementation unless explicitly implemented.
- No full dynamic routing protocol unless explicitly implemented.
- No real AWS infrastructure unless a separate real-cloud integration is deliberately built.
- Simplified queueing or delay calculations, if used, must be named as estimates.

---

# 2. Terminology

Use these terms consistently in code, API payloads, UI labels, and documentation.

| Term | Meaning |
|---|---|
| Node | A topology entity such as a host, switch, router, or cloud endpoint. |
| Interface / NIC | A node's attachment point to a link, with its own state and configuration. |
| Link | A connection between two interfaces, with delay, bandwidth, MTU, loss, and operational state. |
| Subnet / prefix | An IP address range described by an address and prefix length, such as `192.168.10.0/24`. |
| Directly connected network | A network reachable through an interface because an address/prefix is configured on that interface. |
| Default gateway | A next-hop router a host uses for destinations not considered on-link. |
| Route | A rule mapping a destination prefix to an outgoing interface and optionally a next hop. |
| Forwarding table / FIB | The active set of routes used to forward packets. |
| Next hop | The immediate Layer-3 neighbor to which a router sends a packet. It is not necessarily the final destination. |
| Hop | For this simulator's route metrics, a Layer-3 forwarding step, normally one router forwarding decision. Define any alternative metric explicitly. |
| Path cost | An algorithm-specific additive edge weight. It is not automatically delay, hop count, or bandwidth. |
| MTU | Maximum size of an IP packet that can be carried on a link without fragmentation at that link. |
| TTL | IPv4 Time To Live field, decremented by each router forwarding operation. |
| Drop | A terminal discard event for a packet or fragment, with a reason code. |
| Delivery | The packet reaches its intended destination and passes destination-side validation. |
| Control plane | The process that computes, learns, or installs routes. |
| Data plane | The per-packet forwarding behavior that applies the installed forwarding information. |

---

# 3. Supported model and boundaries

## 3.1 Node types

At minimum support:

- **Host:** Endpoint that originates and receives packets. Has one or more interfaces, IP configuration, routing table/default gateway, and protocol behavior.
- **Switch:** Layer-2 forwarding device. For the initial model, use a transparent Ethernet bridge with a MAC forwarding table, unless a clearly labeled simplified switch model is chosen.
- **Router:** Layer-3 forwarding device. Has multiple interfaces, IP addresses, connected routes, optional static routes, a forwarding table, and per-interface MTU/state.
- **Cloud endpoint:** A simulated remote network/service endpoint. It is not an actual cloud resource. Give it an explicit IP address and simulated reachability.
- **Optional generic network/service node:** Only add if its behavior is specified and tested.

A node must not silently act as both a switch and router. If a device supports both roles, represent those capabilities explicitly.

## 3.2 Link model

Each link should define, at minimum:

- Unique ID.
- Endpoint node IDs and interface IDs.
- Operational state (`up` / `down`).
- Bandwidth capacity, with explicit units (for example, bits per second).
- Propagation/one-way delay, with explicit units (for example, milliseconds).
- MTU in bytes.
- Base packet-loss probability or rate, with a documented interpretation.
- Optional queue capacity and congestion model.
- Optional administrative cost used by routing algorithms.
- Optional duplex model; if full-duplex, each direction has its own capacity/queue or an explicitly shared abstraction.

Validate values at configuration time. Bandwidth must be positive; delay must be nonnegative; MTU must be a valid positive integer and should be at least large enough for the modeled headers; loss probability must be within `[0,1]`; costs must satisfy the chosen algorithm's constraints.

## 3.3 Layering model

The simulator may expose OSI and TCP/IP teaching views, but must not imply every packet event corresponds to a complete implementation of every layer.

- **Application:** Application message or service request (optional).
- **Transport:** TCP or UDP metadata and behavior if modeled.
- **Internet / Network:** IPv4/IPv6 addressing, routing, TTL/Hop Limit, fragmentation behavior.
- **Link:** Ethernet-like frames, MAC addresses, switch forwarding, per-link MTU.
- **Physical:** Abstract transmission over a link with configured delay, bandwidth, and loss; no bit-level signal simulation unless implemented.

Each event should identify the layer or layers relevant to that event.

---

# 4. Addressing, subnetting, and local delivery

## 4.1 IPv4 address and prefix validation

- Parse IPv4 addresses strictly as four decimal octets, each from `0` through `255`.
- Prefix length must be from `/0` through `/32`.
- Reject malformed addresses, invalid octets, invalid prefixes, and ambiguous input.
- Normalize addresses to a canonical form for storage and comparison.
- A network address is calculated by applying the prefix mask to the address.
- A subnet broadcast address, where applicable, is the network address with host bits set to `1`.
- Do not allow the network or broadcast address as a normal host address on conventional IPv4 subnets unless the simulator deliberately supports special-case addressing and labels it.

For example, `192.168.10.34/24` belongs to `192.168.10.0/24`.

## 4.2 IPv4 on-link decision

For a host with source address/prefix `A/P` and destination `D`:

1. Calculate the network prefix of `A/P`.
2. Calculate the network prefix of `D` using the same prefix length `P`.
3. If they match, the destination is considered on-link under this simplified single-interface model.
4. Otherwise, the host must select a route, normally its default gateway.
5. If there is no suitable route or default gateway, fail with `NO_ROUTE`.

For multi-interface hosts, use the routing table and interface-specific connected prefixes rather than assuming a single interface. Longest-prefix match should determine the most specific applicable route.

Do not decide “same subnet” by comparing only the first three octets. `/16`, `/20`, `/23`, `/25`, and other prefixes must work correctly.

## 4.3 Default gateway

- A configured default gateway must be reachable on-link through the selected host interface under the supported model.
- A gateway is a next hop, not the final destination.
- The host retains the original destination IP when sending to the gateway.
- The Layer-2 destination MAC is the gateway interface's MAC (if MAC behavior is modeled).
- If the gateway is absent, invalid, down, or not reachable through an appropriate interface, produce a specific configuration or forwarding error.
- Do not silently route to a gateway that is on a different subnet unless an explicit special route or “on-link gateway” configuration is supported and explained.

## 4.4 IPv6 boundary

If IPv6 is supported, model it separately rather than applying IPv4 assumptions to it:

- IPv6 uses a 128-bit address and prefix length from `/0` to `/128`.
- IPv6 routers decrement Hop Limit, not IPv4 TTL.
- IPv6 routers do not fragment packets in transit. The source may fragment using the IPv6 Fragment extension header when supported.
- IPv6 has no broadcast address; multicast and Neighbor Discovery have different semantics.
- IPv6 Path MTU Discovery and ICMPv6 behavior differ from IPv4.

If IPv6 is not implemented, reject IPv6 configuration clearly. Do not parse an IPv6 address and then simulate it with IPv4 rules.

---

# 5. Layer-2 switching and MAC behavior

## 5.1 Ethernet-like frame model

When Layer-2 behavior is enabled, an IP packet is encapsulated in a link-layer frame for each link hop. Model at least:

- Source MAC address.
- Destination MAC address.
- EtherType or an equivalent payload-type label.
- Payload reference/size.
- Optional VLAN ID only if VLAN behavior is implemented.

Use locally administered, deterministic simulated MAC addresses unless the user explicitly configures addresses. Prevent collisions in a Layer-2 broadcast domain.

## 5.2 Switch learning and forwarding

For a learning Ethernet switch:

1. On frame ingress, learn the source MAC on the ingress port.
2. If the source MAC was already learned on another port, update the entry to the new port (MAC movement).
3. If the destination MAC is known and its port is operational, forward only to that port, unless it is the ingress port (then filtering behavior applies).
4. If the destination MAC is unknown, flood to eligible ports other than the ingress port.
5. Broadcast frames are flooded to eligible ports other than ingress.
6. A switch must not decrement IP TTL/Hop Limit.
7. A switch must not change source/destination IP addresses.
8. A switch must not perform Layer-3 route lookup unless explicitly configured as a Layer-3 switch/router.

MAC learning entries should have an optional aging timeout. If aging is not modeled, document that entries persist for the simulation run. Prevent infinite Layer-2 loops through a simplified loop guard or clearly warn that spanning-tree behavior is not modeled. If a topology contains loops and STP is not implemented, do not claim to simulate a production Ethernet network correctly.

## 5.3 MAC addresses are hop-by-hop

For a routed packet:

- The source host's IP packet retains its source and destination IP addresses under ordinary routing.
- The Ethernet frame is rebuilt for each Layer-2 segment.
- At each router egress, source MAC becomes the router's outgoing-interface MAC and destination MAC becomes the next-hop neighbor's MAC.
- MAC addresses are not end-to-end identifiers across routed segments.
- IP source/destination addresses change only if a modeled function such as NAT explicitly changes them.

If MAC-level detail is disabled, still show the conceptual next-hop/egress-interface decision and label the view as an abstraction.

---

# 6. Routing table and route selection

## 6.1 Route representation

Each route should include:

- Destination prefix.
- Next-hop IP or directly connected indication.
- Outgoing interface ID.
- Route source/type (connected, static, learned, default, or policy-derived).
- Administrative preference/priority if implemented.
- Metric/cost if used.
- Route validity and installation state.
- Optional expiration/age for dynamic routes if implemented.

A route is usable only if its next hop is resolvable, the outgoing interface is operational, and the route's destination/prefix is valid. The exact resolution rules must be deterministic.

## 6.2 Longest-prefix match (LPM)

When multiple forwarding-table entries match a destination, select the route with the longest matching prefix first.

Example:
- `0.0.0.0/0` → default route.
- `10.0.0.0/8` → route A.
- `10.4.0.0/16` → route B.
- Destination `10.4.7.9` matches all three, but `/16` wins.

After LPM, use configured route preference and metric to resolve eligible routes according to the documented policy. Do not let a lower-priority default route beat a more-specific prefix simply because it has a lower numerical cost.

If multiple routes have equal preference and metric, use a deterministic tie-break (for example, stable route ID) unless equal-cost multipath (ECMP) is explicitly implemented.

## 6.3 Connected and default routes

- Installing an interface address/prefix should create or make available the corresponding connected route if that behavior is enabled.
- A default route matches destinations not matched by a more-specific route.
- A route to the next hop must itself be resolvable; avoid recursive next-hop resolution loops.
- If recursive resolution is supported, bound the recursion depth and detect cycles.
- If recursive resolution is not supported, require next hops to be directly reachable and state this limitation.

## 6.4 Route computation versus forwarding

Routing algorithms such as Dijkstra or Bellman–Ford may compute candidate paths. The resulting path must be converted into valid per-router forwarding entries, or the engine must use a clearly defined source-to-destination path simulation mode.

Support two distinct modes if practical:

1. **Path-planning mode:** Compute a path in a graph and compare candidate routes. This is useful for algorithm education.
2. **Packet-forwarding mode:** At each router, perform a forwarding-table lookup based on the current destination and current network state.

Do not mix these modes silently. A globally selected path may not be the path followed by independently configured router forwarding tables. The UI must identify which mode is active.

## 6.5 Routing loops

- Detect repeated forwarding states, such as the same router/interface/destination/prefix state, or enforce a finite hop/step limit.
- TTL/Hop Limit remains the primary packet-level loop limiter in the modeled IP forwarding behavior.
- If the loop is detected by an engine safety guard before TTL expires, label it `SIMULATION_LOOP_GUARD`, not as if a real router necessarily detected the loop.
- Emit each forwarding decision until termination, subject to configured event limits.
- A route-computation algorithm must also detect graph cycles where relevant; not every graph cycle is a routing loop.
- Bellman–Ford negative-cycle detection applies to the mathematical weighted graph. It is not the same as an IP forwarding loop.

---

# 7. Routing algorithms and metric semantics

The engine must state what each algorithm optimizes and validate whether its assumptions hold.

## 7.1 Breadth-First Search (BFS)

- Objective: minimum number of edges/hops in an unweighted graph.
- All edges are treated equally for path selection.
- Do not describe BFS as minimum latency, minimum cost, or maximum bandwidth unless the graph has been transformed into a corresponding unweighted representation.
- Complexity: `O(V + E)` for a standard adjacency-list implementation.

## 7.2 Dijkstra

- Objective: minimum sum of nonnegative edge weights.
- All edge weights used by Dijkstra must be finite and nonnegative.
- Reject or clearly disable Dijkstra when any selected edge has a negative weight.
- The selected weight must be named (e.g., administrative cost or estimated delay); do not silently change what “cost” means.
- Standard complexity with a binary heap and adjacency list is `O((V + E) log V)`, depending on implementation.
- A path's summed cost is distinct from its hop count and from its bottleneck bandwidth.

## 7.3 Bellman–Ford

- Objective: shortest path under additive edge weights, including negative edges if the model allows them.
- Detect reachable negative-weight cycles that make shortest-path values undefined.
- A negative cycle elsewhere in a disconnected component must not be reported as affecting the selected source unless it is reachable from the source under the algorithm's semantics.
- Negative routing costs are mathematical teaching inputs; they do not represent normal production IP link costs.
- Complexity: `O(VE)`.

## 7.4 Widest path

- Objective: maximize the minimum bandwidth along the path (the bottleneck capacity).
- The path score is `min(edge bandwidth)` along that path, not the sum of bandwidths.
- If ties occur, apply a documented secondary criterion such as lower total administrative cost, then stable path ID.
- A wider path is not necessarily lower latency or lower loss.

## 7.5 Custom policy

A custom policy must declare:
- Hard constraints (paths that are disallowed).
- Optimization objective (what is minimized/maximized).
- Tie-breaking rules.
- Required input fields.
- Behavior for missing or invalid metrics.

Do not create a single weighted sum that directly adds milliseconds, Mbps, packet-loss percentages, bytes, and CPU load. These units are not comparable without normalization and explicit user-approved weights.

## 7.6 Required six criteria

The main route-comparison interface must support these six criteria, with semantics defined below:

1. **Cost:** A dimensionless or explicitly unit-defined administrative/path cost. If it is a sum of link costs, state that. Do not assume it means money or latency.
2. **Packet-loss rate:** Probability/rate of loss on a link or estimated end-to-end loss on a path. Distinguish measured simulation outcomes from a probability estimate.
3. **Available bandwidth:** Effective usable bandwidth or path bottleneck. For a simple static path, bottleneck bandwidth is the minimum capacity of its links. If traffic competes for capacity, available bandwidth must account for modeled utilization/queues.
4. **Congestion/link utilization:** Utilization should be defined as traffic load divided by link capacity over a named interval. A route's congestion score could be maximum link utilization or a documented aggregate; do not mix the two.
5. **MTU compatibility:** Whether the packet fits each link's MTU, or whether fragmentation is possible/allowed. This is a feasibility constraint, not automatically a scalar route cost.
6. **Router processing load:** A modeled estimate of processing demand or queueing delay at routers. State the units and model. Do not present arbitrary CPU percentages as measured real hardware load.

Use a clear multi-criteria method:
- First filter paths that violate hard constraints, if the selected policy says they are forbidden.
- Then optimize the selected primary objective.
- Use documented secondary criteria to break ties.
- Report every criterion separately.
- If the user asks for a combined score, normalize values to defined ranges, expose the weights, explain whether lower or higher is better, and display the score as a heuristic—not a physical law.

## 7.7 Path metrics

For a candidate path:
- Total additive cost = sum of edge costs, if additive cost is the selected definition.
- Hop count = count of defined Layer-3 forwarding steps; state whether source/destination and switches are excluded.
- End-to-end propagation delay = sum of one-way link delays on the selected direction.
- Transmission delay may be estimated as packet bits divided by link bit rate for each link; state whether link-layer overhead is included.
- Bottleneck bandwidth = minimum link capacity along the path.
- Estimated independent loss probability for link loss probabilities `p_i` = `1 - product(1 - p_i)`. This estimate assumes independent per-link losses and excludes queue loss unless included.
- Observed loss rate = dropped packets divided by transmitted packets over a named simulation interval or traffic set; do not confuse it with the probability estimate.
- Maximum utilization = maximum link utilization across the path over the chosen interval.
- Router processing delay/load = sum or maximum of the modeled router values, with exact aggregation stated.
- MTU outcome = fit/no-fit/fragmented/fragmentation-required-but-forbidden/unknown, depending on packet and protocol settings.

If there is no valid path, report no path. Do not report a zero-cost, zero-delay, or infinite-bandwidth successful route as a substitute.

---

# 8. Packet structure and lifecycle

## 8.1 Minimum packet fields

A simulated packet should include:

- Unique packet ID.
- Parent/original packet ID for generated ICMP replies or fragments.
- Source and destination IP.
- IP version.
- Upper-layer protocol (e.g., ICMP, TCP, UDP, or a simulator-defined payload).
- Total IP packet length in bytes.
- IPv4 TTL or IPv6 Hop Limit.
- IPv4 identification, DF, MF, and fragment offset when IPv4 fragmentation is modeled.
- Payload metadata or payload reference.
- Creation time and current simulated time.
- Current node/interface/link and direction.
- Current status: created, queued, in transit, received, delivered, dropped, or expired.
- Fragment/reassembly metadata.
- Optional transport ports, flags, and sequence information only if transport behavior is implemented.

Packet data should be immutable where practical. Each forwarding step can create a new state/version and append an event rather than overwriting the history.

## 8.2 Packet lifecycle states

Recommended states:

`CREATED → ROUTE_LOOKUP → QUEUED → IN_TRANSIT → RECEIVED → FORWARDED → ... → DELIVERED`

Possible terminal outcomes:
- `DELIVERED`
- `DROPPED`
- `EXPIRED`
- `FRAGMENTED` is a transition/event, not necessarily a terminal outcome for the original datagram if fragments continue.
- `REASSEMBLY_FAILED`
- `CANCELLED` when a user explicitly stops the run.

Use precise state transitions. A packet cannot be delivered after a terminal drop/expiry. If fragments are created, track the original datagram and every fragment's outcome.

## 8.3 Event model

Every meaningful state change must produce an event containing:
- Event ID and run ID.
- Simulated timestamp or monotonic event sequence.
- Packet ID and optional parent packet ID.
- Node, interface, link, and direction when applicable.
- Event type and relevant OSI/TCP-IP layer.
- Before/after state or the key changed fields.
- Human-readable explanation.
- Stable machine-readable reason code for failures and decisions.
- Optional structured details for the inspector.

Recommended event types include:
- `PACKET_CREATED`
- `HOST_ROUTE_LOOKUP`
- `ARP_REQUIRED` (only if ARP is modeled; otherwise no ARP exchange is implied)
- `FRAME_ENCAPSULATED`
- `SWITCH_MAC_LEARNED`
- `SWITCH_FORWARDED`
- `SWITCH_FLOODED`
- `ROUTE_LOOKUP`
- `ROUTE_SELECTED`
- `NO_ROUTE`
- `PACKET_QUEUED`
- `LINK_TRANSMIT_STARTED`
- `LINK_RECEIVE`
- `TTL_DECREMENTED`
- `TTL_EXPIRED`
- `ICMP_GENERATED`
- `FRAGMENT_CREATED`
- `FRAGMENT_DROPPED`
- `REASSEMBLY_STARTED`
- `REASSEMBLY_COMPLETED`
- `PACKET_DELIVERED`
- `PACKET_DROPPED`
- `RUN_COMPLETED`

The frontend must render the engine's event stream in order. If event batching or animation interpolation is used, preserve the underlying order and do not change the event semantics.

---

# 9. TTL, Hop Limit, and routing loops

## 9.1 IPv4 TTL semantics

TTL is an 8-bit IPv4 header field. In real IP forwarding, a router must reduce TTL by at least one; it is commonly modeled as a decrement of one per forwarding operation. For this educational simulator:

1. A source creates a packet with configured initial TTL in the range `1..255`.
2. A host sends the packet onto its first link without decrementing TTL merely for sending.
3. A Layer-2 switch does not decrement TTL.
4. A router that is going to forward an IPv4 packet checks whether forwarding would exhaust TTL.
5. If the packet arrives with TTL `1`, the router must not forward it. It discards the packet and may generate an ICMP Time Exceeded response if the response is permitted and rate limits are not modeled.
6. If TTL is greater than `1`, the router decrements TTL by one before forwarding.
7. The forwarded packet carries the decremented TTL.
8. A router does not decrement TTL for a packet it locally receives and consumes, because it is not forwarding that packet.
9. The destination host does not decrement TTL just to receive the packet.

For clarity, use this convention in the simulator: if a router receives TTL `1`, emit `TTL_EXPIRED` and drop; otherwise decrement by one and forward. This means a packet with initial TTL `N+1` can traverse `N` router-forwarding operations and still be forwarded by the Nth router with TTL `1`, but it will not survive a further router-forwarding operation.

**Important:** TTL is not a path cost. Never decrement TTL by link cost, distance, delay, or number of switches.

## 9.2 IPv6 Hop Limit

If IPv6 is supported, apply analogous forwarding behavior to Hop Limit:
- A router does not forward a packet with Hop Limit `0` or with forwarding decrement that would exhaust it.
- Decrement by one per IPv6 router forwarding operation.
- Switches do not decrement it.
- IPv6 uses ICMPv6 Time Exceeded, not IPv4 ICMP.

## 9.3 TTL example

Initial IPv4 TTL = `3`:

| Step | Device/event | TTL result |
|---|---|---:|
| Source host creates packet | Packet created | 3 |
| Switch forwards frame | Layer-2 forwarding | 3 |
| Router R1 forwards | Decrement | 2 |
| Switch forwards frame | Layer-2 forwarding | 2 |
| Router R2 forwards | Decrement | 1 |
| Router R3 attempts to forward | TTL expired; drop | No forwarding |

The exact number of routers traversed depends on the route. Make event logs show both the received TTL and the outgoing TTL at every router.

## 9.4 ICMP Time Exceeded

When TTL expires at a router:
- Drop the original packet.
- Emit a `TTL_EXPIRED` / `PACKET_DROPPED` event with reason `TTL_EXPIRED`.
- If ICMP is modeled and allowed, create a **new** ICMP response packet with a new packet ID and a parent/reference to the original packet.
- The response's source IP is the selected router interface address for the return path, according to the simulator's documented policy.
- The response's destination IP is the original packet's source IP.
- The response is routed back using the router's own forwarding table. Do not teleport it directly to the source.
- The response has its own TTL and can itself be lost, dropped, or fail due to no route.
- The original packet remains dropped even if the ICMP response reaches the source.
- Suppress ICMP errors in cases where real implementations normally avoid generating them, such as in response to another ICMP error, and for non-initial fragments where the required transport header is unavailable, if these packet types are modeled.
- ICMP generation can be simplified for the first version, but the UI must label any omitted suppression/rate-limiting rules.

Use a similar separate-response model for supported ICMP Destination Unreachable outcomes. Do not represent ICMP as a status change of the original packet.

---

# 10. MTU and IPv4 fragmentation

## 10.1 MTU checks

- MTU is measured in bytes and applies to the IP packet size for this model, unless a link explicitly documents a different Layer-2-inclusive convention.
- Compare the IP total length (header + payload) against the outgoing link/interface MTU.
- A packet whose total IP length is less than or equal to MTU fits.
- A packet larger than MTU requires fragmentation, packetization changes, or dropping/error behavior according to IP version, DF, and implementation support.
- Do not compare only payload size against MTU.
- Do not assume all links have the same MTU.
- The next-hop link's MTU matters at each router egress.

## 10.2 IPv4 fragmentation

If IPv4 fragmentation is implemented, use these rules:

- The IPv4 base header is 20 bytes when there are no options. If IP options are modeled, account for the actual header length.
- Each fragment has its own IPv4 header.
- Fragment payload size, except the final fragment, must be a multiple of 8 bytes because fragment offset is measured in 8-byte units.
- Maximum fragment payload for a simple header model is `floor((MTU - headerLength) / 8) * 8`.
- Each fragment's total length is its header length plus fragment payload length and must not exceed the outgoing MTU.
- Fragment offset is the byte offset of that fragment's payload within the original datagram divided by 8.
- MF (More Fragments) is `1` on every fragment except the final fragment of the original datagram, which has MF `0`. If a previously fragmented packet is fragmented further, preserve the correct end-of-datagram meaning of MF.
- All fragments of one original IPv4 datagram share the same Identification value.
- Fragmentation can occur again at a later router if a subsequent link has a smaller MTU and DF is `0`.
- A router must not fragment an IPv4 packet if DF is set. It drops the packet and may send ICMP Destination Unreachable, Fragmentation Needed (Type 3, Code 4).
- Fragment loss means the original datagram cannot be reassembled unless all required fragments arrive before the reassembly timeout.
- Fragment reassembly occurs at the destination host, not at intermediate routers.
- Do not treat each fragment as an independent application-level message.
- Fragment offset and MF are IPv4 fields; do not apply them as if they were IPv6 base-header fields.

### Worked fragmentation example

Assumptions:
- IPv4 header = 20 bytes, no options.
- Original IP total length = 4000 bytes.
- Original payload length = `4000 - 20 = 3980` bytes.
- Outgoing MTU = 1500 bytes.
- DF = `0`.

Maximum non-final fragment payload:

`floor((1500 - 20) / 8) * 8 = 1480 bytes`

Fragments:

| Fragment | Payload bytes | Total IP length | Offset units | Byte offset | MF |
|---|---:|---:|---:|---:|---:|
| 1 | 1480 | 1500 | 0 | 0 | 1 |
| 2 | 1480 | 1500 | 185 | 1480 | 1 |
| 3 | 1020 | 1040 | 370 | 2960 | 0 |

The third fragment's total length is 1040 bytes, not 1020, because it includes the 20-byte IP header. The fragment payloads sum to 3980 bytes.

Do not use byte offset as the IPv4 Fragment Offset field. The field stores `byte_offset / 8`.

## 10.3 IPv4 fragmentation and TTL

- Every fragment being forwarded by a router experiences the router's TTL forwarding rule.
- If fragmentation occurs at a router, fragments emitted by that router carry the decremented TTL for that forwarding operation.
- A fragment that later expires or is dropped does not cause the engine to silently mark sibling fragments as delivered.
- Reassembly succeeds only when all required byte ranges are present, metadata is consistent, and the reassembly timeout has not expired.
- If any fragment is missing, the original datagram eventually fails reassembly; report which fragment(s) were lost or absent.

## 10.4 IPv6 MTU behavior

For IPv6:
- Routers do not fragment packets in transit.
- If a packet is too large for the outgoing link, drop it and, when ICMPv6 is modeled, send Packet Too Big (ICMPv6 Type 2) toward the source.
- Source-side IPv6 fragmentation may be implemented separately.
- Do not reuse IPv4 DF/fragment-offset/MF logic for IPv6.

## 10.5 Fragmentation outcomes and UI

Show one of:
- `FITS_MTU`
- `FRAGMENTED`
- `FRAGMENTATION_NEEDED_DF_SET`
- `IPV6_PACKET_TOO_BIG`
- `FRAGMENT_DROPPED`
- `REASSEMBLED`
- `REASSEMBLY_TIMEOUT`
- `MTU_NOT_MODELED`

Show packet size, interface/link MTU, header size, payload size, and reason. The packet inspector must make it clear whether a displayed length is payload length, IP total length, or Layer-2 frame length.

---

# 11. ICMP and delivery errors

## 11.1 ICMP model

ICMP is a control/error protocol, not a magical delivery guarantee. It is carried in a separate IP packet. A reply may take a different route from the original packet and may fail independently.

Support at least these educational cases if ICMP is included:
- Echo Request and Echo Reply for ping-like demonstrations.
- Time Exceeded on TTL expiry.
- Destination Unreachable / No route.
- Fragmentation Needed when IPv4 DF prevents forwarding over a smaller-MTU link.
- Packet Too Big for IPv6, if IPv6 is implemented.

For each generated ICMP packet, record its own route lookup, TTL/Hop Limit, queue/transit events, and final status.

## 11.2 Failure reasons

Use stable reason codes rather than relying only on display text. Recommended codes:

- `INVALID_ADDRESS`
- `INVALID_PREFIX`
- `INVALID_GATEWAY`
- `GATEWAY_UNREACHABLE`
- `NO_ROUTE`
- `NEXT_HOP_UNRESOLVED`
- `INTERFACE_DOWN`
- `LINK_DOWN`
- `DESTINATION_UNAVAILABLE`
- `TTL_EXPIRED`
- `HOP_LIMIT_EXPIRED`
- `MTU_EXCEEDED_DF_SET`
- `IPV6_PACKET_TOO_BIG`
- `PACKET_LOSS`
- `QUEUE_OVERFLOW`
- `PROCESSING_QUEUE_OVERFLOW`
- `REASSEMBLY_TIMEOUT`
- `SIMULATION_LOOP_GUARD`
- `EVENT_LIMIT_REACHED`
- `SIMULATION_CANCELLED`
- `UNSUPPORTED_PROTOCOL`
- `INVALID_CONFIGURATION`

Every drop must identify the node or link responsible when known and provide a user-readable explanation. Never report “network error” alone if a more precise cause is available.

---

# 12. Link transmission, delay, loss, and congestion

## 12.1 Delay components

Keep delay components separate when modeled:

- Propagation delay: configured time for a signal/frame to travel across the link.
- Transmission/serialization delay: packet/frame bits divided by link bit rate.
- Queueing delay: time waiting behind earlier traffic.
- Processing delay: time spent by a host/router/switch processing an event.

End-to-end one-way delay can be reported as the sum of modeled components along the actual path. State which components are included. Do not claim a realistic measured latency if only configured propagation delay is included.

For a packet of `L` bytes on a link with capacity `B` bits/s, a simple serialization estimate is:

`serialization_delay_seconds = (L * 8) / B`

Use the modeled transmission length consistently (IP packet length or Layer-2 frame length) and state whether headers are included.

## 12.2 Link capacity and direction

- Bandwidth is a rate, not an amount of data.
- If the link is full-duplex, each direction may transmit independently at the configured rate unless the implementation explicitly shares a resource.
- If half-duplex or shared-capacity behavior is supported, define collision/contention behavior; otherwise do not pretend to simulate it.
- Units must be explicit and internally consistent. Convert Mbps/Gbps to bits per second correctly.
- A route's bottleneck capacity is the minimum applicable capacity across its links in the selected direction.

## 12.3 Packet loss

If per-link loss is modeled probabilistically:
- Define whether the probability is applied on transmission start, at link reception, or per packet attempt; use one consistent convention.
- Use the run seed and stable random-number generation so results are reproducible.
- Do not draw a packet as delivered after a loss event.
- Loss of one packet does not imply all later packets are lost unless the link/device has entered a modeled failure state.
- If no randomized loss is enabled, a configured `0%` link loss must not produce random drops.
- Report both configured loss probability and observed simulation loss separately.

For independent link loss probabilities `p1 ... pn`, estimated path loss probability is:

`1 - product(1 - p_i)`

This is a probability estimate under independence assumptions, not necessarily the observed result in a finite run.

## 12.4 Congestion and queues

If queues are implemented:
- Define queue capacity (packets or bytes), scheduling discipline, and drop policy.
- Queue capacity must be finite or explicitly unlimited.
- A queue overflow must generate `QUEUE_OVERFLOW` with the affected queue and packet.
- Link utilization over a time window is transmitted bits divided by capacity bits over the same window, with the exact measurement interval shown.
- Utilization can exceed 100% only as an offered-load ratio, not as actual successful transmitted capacity. If displaying offered load above capacity, label it “offered load,” not “achieved utilization.”
- Do not calculate utilization from one packet and present it as a stable long-term network utilization metric without explaining the interval.
- If congestion is merely represented by a user-defined delay or loss factor, label it as a simplified congestion model.

If queues are not implemented in the first version, allow configured fixed delay/loss but label congestion metrics as `NOT_MODELED` or a clearly identified heuristic. Do not create fake queue events.

---

# 13. Router processing load

Router processing load is a simulation parameter, not real hardware telemetry.

If modeled, define:
- A configurable processing capacity, such as abstract operations per second or service time per packet.
- A per-packet processing demand or per-router processing delay.
- Queue capacity and service discipline if contention is included.
- The measurement interval and units for any utilization estimate.

Possible simplified model:
- Each router has a configurable base processing delay per packet.
- Optional processing queue serializes packets or applies a modeled service rate.
- Processing load is computed from offered packet work relative to configured capacity over a time interval.

Do not label a manually assigned value such as `70` as real CPU usage unless it comes from a real measured source. Prefer “simulated processing utilization” or “configured processing load.”

If the feature is not implemented, display “Not modeled” rather than a default 0%, which could be mistaken for a measured zero load.

---

# 14. Route failure, topology changes, and recovery

## 14.1 Link/router failure

When a link is disabled:
- New transmissions over it must fail or be blocked according to the chosen link-down timing model.
- Packets already in transit must follow a documented policy: either complete if the model treats transmission as already committed, or fail if the link goes down before reception. Choose one deterministic policy and test it.
- Do not allow a packet to traverse a disabled link just because a route was computed before the failure.
- If alternate routing is implemented, recompute/update route state or let per-router FIB lookups select a valid alternate route.
- A path-planning result may remain stale; mark it stale after topology/configuration changes.

When a router is disabled:
- Its interfaces cannot forward packets.
- Neighboring routers may retain stale routes only if the simulator explicitly models convergence/stale routing.
- Otherwise, mark affected routes invalid and produce a deterministic failure or recomputation.

## 14.2 Failure injection

Support testable fault types:
- Link down/up.
- Router down/up.
- Added propagation delay.
- Packet-loss probability change.
- Bandwidth reduction.
- MTU reduction.
- Gateway removed or misconfigured.
- Route removed or made invalid.
- Low TTL.
- Queue saturation/congestion, if queues exist.
- Processing-capacity reduction, if processing queues exist.

Every fault should have:
- Target ID.
- Start time/event sequence.
- Optional duration/end time.
- Applied configuration change.
- Resulting events and measured effects.
- Clear distinction between permanent topology edit and temporary fault injection.

## 14.3 Recovery and recomputation

- Recompute path-planning output when its input topology or metric configuration changes.
- In forwarding mode, use the current forwarding table and link/interface state at the time of each lookup.
- If dynamic convergence is not modeled, do not claim to simulate OSPF/BGP convergence. State that route tables update instantly or only when the user explicitly recomputes.
- If dynamic convergence is implemented, model convergence delay and temporary stale routes explicitly.

---

# 15. TCP and UDP boundaries

## 15.1 UDP

If UDP is modeled:
- Include source/destination ports and UDP length.
- UDP is connectionless; do not invent a handshake.
- Delivery of one UDP datagram does not guarantee delivery of other datagrams or ordering.
- A UDP packet can be lost or fragmented according to IP rules.
- An application response is separate traffic and must be explicitly generated by a modeled service.

## 15.2 TCP

If TCP is modeled:
- Distinguish an IP packet carrying a TCP segment from the higher-level TCP connection.
- A TCP connection normally requires handshake state (SYN, SYN-ACK, ACK) before established data transfer.
- TCP sequence numbers, acknowledgments, retransmissions, receive windows, and congestion control require explicit state and algorithms.
- Do not claim TCP reliability if the simulator only animates one packet.
- If TCP is not fully implemented, use labels such as “TCP metadata demonstration” rather than “full TCP simulation.”
- A packet drop in a TCP-like demo should not automatically create a retransmission unless TCP retransmission logic is enabled.

## 15.3 Application delivery

A packet reaching a host's IP stack does not necessarily mean the application accepted the data. If application delivery is modeled, validate destination service/port and application state separately. Otherwise, report “IP delivered to destination host,” not “application request succeeded.”

---

# 16. Destination handling and delivery definition

A packet is `DELIVERED` only when it reaches the intended destination under the currently selected simulation mode.

In packet-forwarding mode:
1. Destination node is reachable.
2. Packet arrives at the destination's active interface.
3. Destination IP matches a local address (or another explicitly modeled local-delivery rule).
4. Packet passes any modeled protocol/size validation.
5. No earlier terminal failure occurred.

A packet addressed to the router itself is locally consumed, not forwarded through the router. TTL is not decremented merely because a router receives a packet for one of its own IP addresses.

For a cloud endpoint, `DELIVERED` means the simulated endpoint accepted the packet. It does not prove a real service, DNS name, internet connection, or AWS resource exists.

Use separate statuses where appropriate:
- `IP_DELIVERED`
- `APPLICATION_ACCEPTED`
- `RESPONSE_GENERATED`
- `RESPONSE_DELIVERED`

If only IP-level delivery is implemented, do not show “request successful” when the simulator has not modeled the application.

---

# 17. Six-criteria route comparison: operational rules

The route comparison feature must not treat every metric as interchangeable.

## 17.1 Hard constraints versus objectives

Examples of hard constraints:
- No disabled link or disabled router.
- Packet must fit MTU, or fragmentation must be permitted.
- Required destination must be reachable.
- Maximum allowed loss estimate.
- Maximum allowed delay.
- Maximum hop count.
- Required interface/protocol capability.

Examples of objectives:
- Minimize administrative cost.
- Minimize estimated delay.
- Maximize bottleneck bandwidth.
- Minimize maximum link utilization.
- Minimize modeled processing delay.
- Minimize estimated packet-loss probability.

The UI should let the user specify which constraints are mandatory and which metrics are optimization preferences.

## 17.2 Criteria calculation and missing values

- Cost: sum only if the configured cost model is additive.
- Loss: report configured per-link probability and estimated path probability separately.
- Bandwidth: use path bottleneck capacity for a simple path; if competing traffic is modeled, calculate or estimate available capacity using the defined queue/traffic model.
- Congestion: report maximum link utilization or the selected aggregate, and show the interval.
- MTU: inspect every egress link along the actual route; report whether fragmentation is required and whether it can occur.
- Processing load: report configured/modelled per-router demand and the chosen aggregation.
- Missing metric: `null` / “not modeled,” not `0`.
- A route with invalid metrics must not be ranked as if missing values were favorable.

## 17.3 Tie-breaking

Tie-breaking must be stable and visible. Suggested order:
1. Satisfy all hard constraints.
2. Optimize the primary objective.
3. Apply explicitly selected secondary objective(s).
4. Prefer fewer hops if requested.
5. Use stable path ID or lexical node-ID order as final deterministic tie-break.

Do not claim two paths are “equally optimal” if the hidden tie-break selected one without explaining it.

## 17.4 Comparing route and packet outcomes

Route comparison predicts or calculates path properties. Packet simulation measures what happened to a specific packet or traffic set. Keep separate:
- Candidate path metrics.
- Actual selected path.
- Actual packet events.
- Observed delivery/drop outcome.
- Estimated metrics versus measured simulation metrics.

A path can have a good estimated score but fail for an individual packet due to randomized loss or a fault that occurs during the run.

---

# 18. Simulation time and event ordering

Use a virtual simulation clock, not wall-clock sleeps, as the source of truth for networking behavior.

- Every event has a simulation timestamp or monotonic sequence.
- Events are processed in deterministic order.
- For equal timestamps, use a stable tie-break, such as event priority then insertion sequence.
- Animation speed changes presentation only; it must not alter packet TTL, route choice, loss results, or simulation timestamps.
- Pausing the UI must pause playback, not rewrite the already-computed event history.
- Step-forward should advance to the next event or next clearly defined packet transition.
- If the engine uses discrete-event simulation, store pending transmission completion, queue service completion, fault activation, and timeout events in a priority queue.
- If the first version uses a simpler sequential simulation, state its timing assumptions and do not claim realistic concurrent packet contention.
- Event limits must stop safely with `EVENT_LIMIT_REACHED`; never silently truncate and show a successful run.

For multiple packets, define event order deterministically. Do not rely on unstable iteration order of hash maps or asynchronous UI callbacks.

---

# 19. Security, validation, and safe simulation

- Treat all topology JSON, imported files, node labels, and user-provided addresses as untrusted input.
- Validate on both frontend and backend/engine boundaries.
- Enforce limits for node count, link count, packet count, event count, payload size, and simulation duration.
- Prevent malformed topology from causing infinite loops, unbounded memory, or excessive CPU use.
- Use stable IDs and validate that referenced nodes/interfaces/links exist.
- Reject duplicate IDs and invalid endpoint references.
- Avoid invoking shell commands with user-controlled strings. If the backend starts the C++ executable, use safe process APIs and pass validated JSON through stdin rather than constructing a shell command.
- Do not accept arbitrary C++ code, shell scripts, or executable uploads as topology inputs.
- The default cloud endpoint must remain simulated. Never request AWS credentials or create actual AWS resources as part of normal simulation.
- Imported topology files should be size-limited and schema-validated.
- User text shown in the UI must be rendered safely to avoid script injection.
- Use a schema version and explicit migration/rejection behavior for incompatible imports.

---

# 20. Data contracts and reason-code conventions

Use a versioned JSON contract shared by frontend, backend, and C++ engine. Exact field names can be refined in `ARCHITECTURE.md`, but semantics must remain stable.

## 20.1 Example topology fragment

```json
{
  "schemaVersion": "1.0",
  "nodes": [
    {
      "id": "host-a",
      "type": "host",
      "interfaces": [
        {
          "id": "host-a-eth0",
          "mac": "02:00:00:00:00:01",
          "ipv4": {
            "address": "192.168.10.10",
            "prefixLength": 24
          },
          "mtu": 1500,
          "adminState": "up"
        }
      ],
      "defaultGateway": "192.168.10.1"
    },
    {
      "id": "router-a",
      "type": "router",
      "interfaces": [
        {
          "id": "router-a-lan",
          "mac": "02:00:00:00:00:02",
          "ipv4": {
            "address": "192.168.10.1",
            "prefixLength": 24
          },
          "mtu": 1500,
          "adminState": "up"
        }
      ]
    }
  ],
  "links": [
    {
      "id": "link-host-router",
      "a": {
        "nodeId": "host-a",
        "interfaceId": "host-a-eth0"
      },
      "b": {
        "nodeId": "router-a",
        "interfaceId": "router-a-lan"
      },
      "adminState": "up",
      "bandwidthBps": 1000000000,
      "propagationDelayMs": 1,
      "mtuBytes": 1500,
      "packetLossProbability": 0
    }
  ]
}
```

This is an illustrative contract, not a complete topology. The implementation must validate that the link endpoints exist, are compatible, and do not violate interface/link constraints.

## 20.2 Example event

```json
{
  "eventId": "event-0007",
  "runId": "run-001",
  "sequence": 7,
  "timeMs": 4.2,
  "type": "TTL_DECREMENTED",
  "layer": "network",
  "packetId": "packet-001",
  "nodeId": "router-a",
  "interfaceId": "router-a-wan",
  "reasonCode": null,
  "message": "Router decremented IPv4 TTL from 5 to 4 before forwarding.",
  "details": {
    "ttlBefore": 5,
    "ttlAfter": 4
  }
}
```

The example event's numeric values are illustrative. The engine must calculate real values for each run.

## 20.3 Reason-code rules

- Codes are stable API values in uppercase snake case.
- Display text can be localized or revised without changing the code.
- Every terminal drop/expiry has a reason code.
- Validation errors should identify the invalid field and the violated rule.
- Avoid using one generic error code for unrelated failures.
- If a failure is a simulator safety limit rather than a protocol-defined network outcome, use a simulator-specific code.

---

# 21. Required scenarios and expected behavior

These scenarios should become automated tests and interactive tutorials. Expected results assume no unmentioned faults, all relevant interfaces up, and supported protocol behavior.

## Scenario A — Same-subnet delivery

Topology:
- Host A: `192.168.1.10/24`
- Host B: `192.168.1.20/24`
- Both connected to the same Layer-2 switch.
- No router is needed for this destination.

Expected:
- Host A determines that Host B is on-link.
- If ARP is implemented, Host A resolves Host B's MAC before unicast delivery; if ARP is not implemented, the simulator must explicitly skip or abstract this step.
- The frame is forwarded by the switch to Host B's port.
- No router forwarding occurs.
- IPv4 TTL is unchanged by the switch.
- Host B accepts the packet if the destination IP matches its local interface and no modeled protocol error occurs.
- Do not route through the default gateway for an ordinary on-link destination.

## Scenario B — Different-subnet delivery via default gateway

Topology:
- Host A: `192.168.1.10/24`, gateway `192.168.1.1`
- Router LAN interface: `192.168.1.1/24`
- Router WAN interface: `10.0.0.1/24`
- Destination Host B: `10.0.0.20/24` on the router's WAN-side network.

Expected:
- Host A identifies the destination as off-link and selects its default gateway.
- Host A's IP destination remains `10.0.0.20`.
- The first frame's destination MAC is the gateway's LAN MAC if Layer-2 MAC behavior is enabled.
- Router receives the frame and checks whether the destination IP is local to itself. It is not.
- Router performs route lookup and selects its directly connected `10.0.0.0/24` route.
- Router decrements TTL before forwarding and builds a new outgoing frame with its WAN MAC as source and Host B's MAC as destination.
- Host B receives and delivers the packet.
- The router does not rewrite the IP source/destination unless NAT is explicitly modeled.

## Scenario C — Multi-router route to simulated cloud endpoint

Topology:
- Host A → Router R1 → Router R2 → simulated cloud endpoint.
- Correct routes/default gateways exist at each Layer-3 hop.
- All links and interfaces are up.

Expected:
- Host A sends off-link traffic to its gateway.
- Each router performs a lookup using the destination IP.
- Each forwarding router decrements IPv4 TTL once before forwarding.
- Switches, if present, do not decrement TTL.
- The cloud endpoint is a simulated destination, not a real AWS resource.
- Delivery is reported only when the simulated endpoint receives a packet addressed to its configured IP.

## Scenario D — No default gateway / no route

Expected:
- An off-link host destination with no usable route fails with `NO_ROUTE`.
- The engine does not draw a successful packet path.
- If ICMP is implemented and permitted, a separately generated error may be returned according to the model.
- The UI explains whether the failure happened at the source host or a router.

## Scenario E — TTL expiry

Expected:
- Initial TTL is set to a small positive value.
- Each router forwarding step decrements TTL exactly once.
- A router receiving TTL `1` does not forward the packet.
- The original packet terminates with `TTL_EXPIRED`.
- If enabled, a separate ICMP Time Exceeded packet is created and routed back independently.

## Scenario F — IPv4 fragmentation

Input:
- IPv4 total length 4000 bytes.
- Header length 20 bytes.
- Outgoing MTU 1500 bytes.
- DF = 0.

Expected:
- Fragment payloads: 1480, 1480, 1020 bytes.
- Fragment total lengths: 1500, 1500, 1040 bytes.
- Fragment offsets in 8-byte units: 0, 185, 370.
- MF flags: 1, 1, 0.
- All fragments share the same IPv4 Identification.
- Destination reassembles only if all required fragments arrive in time.

## Scenario G — DF set and MTU too small

Expected:
- If an IPv4 packet exceeds the outgoing MTU and DF=1, router does not fragment it.
- The original packet is dropped with `MTU_EXCEEDED_DF_SET`.
- If ICMP is enabled and permitted, the router generates a separate ICMP Destination Unreachable, Fragmentation Needed response.
- Do not mark the original packet delivered.

## Scenario H — Link failure after route computation

Expected:
- A path computed before a link failure is marked stale or revalidated.
- A packet must not successfully traverse a link that is down at the relevant transmission/reception time.
- If alternate routes are supported, the engine may choose one according to current forwarding state; otherwise the packet fails with a specific reason.
- The result records whether route recomputation occurred.

## Scenario I — Routing loop

Expected:
- A forwarding loop cannot run forever.
- TTL eventually expires under ordinary IPv4 semantics if the packet continues to be forwarded.
- If an earlier engine guard stops the run, the result uses `SIMULATION_LOOP_GUARD` and explicitly identifies the safety guard.
- A graph cycle alone is not enough to declare a packet forwarding loop.

## Scenario J — Packet loss and path-loss estimate

Expected:
- Configured link-loss probabilities are displayed separately from observed drops.
- The estimated end-to-end loss probability uses the independent-loss formula when that assumption is selected.
- Actual loss uses seeded simulation outcomes.
- The UI does not claim a packet was delivered after the engine records it as lost.

## Scenario K — IPv6 MTU (only if IPv6 implemented)

Expected:
- A router does not fragment an oversized IPv6 packet.
- The packet is dropped at the oversized egress.
- If ICMPv6 is modeled and permitted, a separate Packet Too Big response is routed toward the source.
- IPv4 fragmentation fields are not used for IPv6.

---

# 22. Testing requirements

Every networking rule that changes an observable outcome must have automated tests.

## 22.1 Unit tests

At minimum:
- IPv4 parse, normalization, prefix validation, subnet/network/broadcast calculations.
- On-link versus off-link decision across `/0`, `/8`, `/16`, `/20`, `/23`, `/24`, `/25`, `/31`, and `/32` where supported.
- Longest-prefix match.
- Route preference and deterministic tie-breaking.
- Gateway validation and next-hop resolution.
- Dijkstra rejects negative weights.
- Bellman–Ford detects reachable negative cycles.
- BFS minimizes edge count in unweighted graphs.
- Widest path maximizes bottleneck capacity.
- TTL decrement and expiry boundary.
- MTU equality boundary: packet size equal to MTU fits; one byte over requires action.
- IPv4 fragmentation offsets, MF flags, total lengths, and header accounting.
- DF prevents IPv4 fragmentation.
- IPv6 router does not fragment, if IPv6 exists.
- MAC learning and unknown-unicast flooding if switching is modeled.
- Loss reproducibility for a fixed seed.
- Link/router failure behavior.
- Reassembly success and timeout.
- Terminal states cannot transition to delivery.
- Metrics are `null` when not modeled, not falsely zero.

## 22.2 Integration tests

Test the required scenarios A–K. Verify:
- Selected path and next hops.
- Ordered event sequence.
- TTL values at each router.
- Frame MAC addresses where enabled.
- Actual delivery/drop result.
- Failure reason codes.
- ICMP response lifecycle, if implemented.
- Fragment/reassembly state, if implemented.
- Metric calculations and their units.
- Reproducibility under identical inputs and seed.

## 22.3 Property-based tests

Where practical, assert:
- A valid IPv4 address round-trips through parse/serialize normalization.
- Fragment payloads cover the original payload exactly once, with no gaps or overlaps, when fragmentation succeeds.
- Every fragment total length is at most the outgoing MTU.
- Every non-final fragment payload length is divisible by 8.
- Fragment offsets are expressed in 8-byte units.
- No route returned by a path algorithm uses a disabled link when disabled links are hard constraints.
- Dijkstra's result matches a trusted reference implementation on random nonnegative graphs.
- A fixed seed produces the same packet-loss decisions and outcomes.
- Every terminal packet outcome has a stable reason/status and no subsequent forwarding event.

## 22.4 Test result reporting

The implementation agent must report:
- Test command(s) executed.
- Number of tests passed/failed.
- Any skipped tests and why.
- Known unsupported networking behavior.
- Any discrepancy between this specification and implemented behavior.

Do not claim tests passed unless they were actually run.

---

# 23. UI and teaching requirements

The interface must help a learner understand the cause of each event.

For every significant decision, provide:
- **What happened?** A concise event summary.
- **Where?** Node, interface, or link.
- **Why?** Human-readable reason.
- **Evidence:** Relevant values (destination prefix, selected next hop, TTL before/after, packet length, MTU, link state, loss decision).
- **What next?** Forwarded, queued, fragmented, delivered, or dropped.

Examples:
- “Destination `192.168.1.20` is on-link for `192.168.1.10/24`; the host sends directly through the switch.”
- “Destination `10.0.0.20` is outside `192.168.1.0/24`; the host selects default gateway `192.168.1.1`.”
- “Router R1 selected `10.0.0.0/24` using longest-prefix match.”
- “IPv4 TTL changed from 4 to 3 before forwarding.”
- “Packet size 4000 bytes exceeds outgoing MTU 1500 bytes. DF is clear, so the simulator creates three fragments.”
- “Packet dropped: IPv4 TTL was 1 on router ingress; no forwarding occurred.”

Never display a generic green success animation before the engine confirms delivery. Distinguish estimated route metrics from observed packet events. When a feature is not implemented, say “Not modeled” rather than hiding it or inventing behavior.

---

# 24. Implementation priority

Implement and test in this order:

## Phase 1 — Core correctness
1. Strict topology/configuration validation.
2. IPv4 addressing and subnet logic.
3. Host on-link/off-link selection and default gateway.
4. Router forwarding table and longest-prefix match.
5. BFS, Dijkstra, Bellman–Ford, widest path with input validation.
6. Deterministic path results and event schema.
7. TTL decrement/expiry.
8. Link/interface up/down and `NO_ROUTE` failures.
9. Basic packet lifecycle and delivery/drop statuses.

## Phase 2 — Link and packet detail
1. Per-hop MAC/frame behavior.
2. Switch learning/flooding if in scope.
3. MTU checks.
4. IPv4 fragmentation and reassembly.
5. ICMP Time Exceeded and Destination Unreachable if in scope.
6. Packet loss with deterministic seeded RNG.
7. Delay and bandwidth metrics.

## Phase 3 — Traffic and richer behavior
1. Virtual simulation clock and deterministic event queue.
2. Queues and congestion model.
3. Router processing model.
4. Multiple simultaneous packets and competing traffic.
5. Fault injection and recovery.
6. Optional IPv6, TCP state, and richer OSI/TCP-IP views.

A feature should not be shown as fully implemented until its behavior and tests are complete. Keep unsupported features behind a disabled control or label them as planned/not modeled.

---

# 25. Final acceptance checklist

Before declaring NetTrace 3D networking behavior complete, verify:

- [ ] Same-subnet traffic does not unnecessarily use a default gateway.
- [ ] Off-subnet traffic requires a valid route/gateway.
- [ ] Subnet calculations support arbitrary valid prefixes, not only `/24`.
- [ ] Routers use longest-prefix match and deterministic tie-breaking.
- [ ] Switches do not decrement TTL or rewrite IP addresses.
- [ ] Router forwarding decrements TTL exactly once per forwarding operation.
- [ ] TTL expiry prevents forwarding and uses a precise reason code.
- [ ] ICMP errors, when modeled, are separate packets routed independently.
- [ ] MAC addresses change per routed Layer-2 segment; IP addresses remain stable unless NAT is explicitly modeled.
- [ ] Dijkstra rejects negative weights; Bellman–Ford detects reachable negative cycles.
- [ ] BFS and widest path optimize their stated objectives.
- [ ] MTU is compared against the full IP packet length.
- [ ] IPv4 fragmentation uses correct 8-byte offsets, MF flags, header lengths, and DF behavior.
- [ ] IPv6 routers do not fragment packets in transit, if IPv6 is supported.
- [ ] Loss, delay, utilization, bandwidth, and processing metrics have explicit units and assumptions.
- [ ] Estimated loss and observed loss are reported separately.
- [ ] Missing metrics are shown as not modeled, not zero.
- [ ] Faults affect packets according to a deterministic, documented timing policy.
- [ ] Every terminal drop/expiry has a stable reason code.
- [ ] Event order is deterministic for identical inputs and seed.
- [ ] UI animations are driven by actual engine events.
- [ ] Required scenarios have automated tests.
- [ ] Documentation states all simplifications and unsupported behaviors.
- [ ] The simulated cloud endpoint never implies a real cloud resource was created.

---

# 26. Implementation instruction for Antigravity

Read `PROJECT.md`, `ARCHITECTURE.md`, and this file before implementing networking behavior. Treat `NETWORKING_RULES.md` as the source of truth for protocol and routing semantics.

Before coding:
1. Inspect the existing repository and preserve working code.
2. Identify the actual C++ compiler, Python environment, frontend package manager, and available test frameworks.
3. Create a traceability checklist mapping each rule and acceptance item to a source module and test.
4. Mark each feature as `implemented`, `partial`, `planned`, or `not modeled`.
5. Resolve contradictions between docs explicitly; do not silently invent a rule.

During implementation:
- Keep routing/path-planning separate from per-packet forwarding.
- Keep protocol facts separate from educational simplifications.
- Use versioned JSON contracts and stable reason codes.
- Drive visual packet movement from engine events.
- Add tests with every networking feature.
- Never use mock data as if it were a real simulation result.
- Never claim a feature or test is complete without evidence.

At the end of each milestone, report:
- Files created/changed.
- Rules implemented.
- Tests actually run and their results.
- Known limitations and simplifications.
- Features still marked partial/planned/not modeled.
- Next recommended milestone.

**Definition of done:** A learner can inspect any packet's journey, understand each forwarding decision and each failure, and trust that the displayed route, TTL, fragmentation, metrics, and delivery status are derived from the same deterministic simulation engine.

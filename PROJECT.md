# NetTrace 3D --- Complete Project Specification

**Purpose:** This document is the primary source of truth for
implementing NetTrace 3D with an AI coding assistant such as Google
Antigravity. Read it before creating or modifying code. It defines the
product vision, functional requirements, networking semantics,
architecture, interface, data model, milestones, testing rules, and
definition of done.

**Status:** Specification for a planned project. Do not describe any
feature as completed until it is implemented and verified.

------------------------------------------------------------------------

## 1. Product Summary

### 1.1 Name

**NetTrace 3D --- Network Routing, Protocol Simulation and Visualization
Platform**

### 1.2 One-line description

NetTrace 3D is an interactive web-based network laboratory where users
create simulated networks, send packets between hosts, observe routing
decisions at every hop, inspect packet and router state, reproduce
network failures, and compare routing algorithms and network metrics in
2D and 3D.

### 1.3 Problem being solved

Computer-network behavior is often invisible to students. A user may
know the definitions of IP addressing, subnetting, routing tables, TTL,
ICMP, MTU, packet loss, and congestion without being able to see how
they interact. Experiments on real networks can be difficult to set up,
and uncontrolled failures can be disruptive.

NetTrace 3D makes this behavior observable in a safe, reproducible
simulation. A user should be able to create a topology, choose a source
and destination, configure packet parameters, run a simulation, pause at
each hop, inspect decisions, introduce faults, and compare the results.

### 1.4 Questions the application should answer

-   Is the destination in the source host's local subnet?
-   Does the source send directly to the destination or to its default
    gateway?
-   Which route and next hop are selected?
-   Why did one algorithm select a different path from another?
-   How does a router process a packet?
-   What happens when TTL expires?
-   Why can a valid route exist while packet delivery fails?
-   What happens when a router, interface, or link fails?
-   How do bandwidth, packet loss, congestion, router processing delay,
    and MTU affect a route?
-   How do packet headers and link-layer addresses change during
    forwarding?
-   How can two algorithms be compared fairly on the same topology?
-   Which simulated results have been validated against real
    operating-system behavior?

### 1.5 Target users

-   Students learning computer networks, C++, algorithms, and systems
    software.
-   Instructors demonstrating network behavior.
-   Developers preparing for systems/software engineering interviews.
-   Users who want a visual environment for controlled networking
    experiments.

### 1.6 Non-negotiable principles

1.  **Correctness before animation.** The simulation engine is
    authoritative. The UI must animate engine events rather than
    inventing its own behavior.
2.  **Simulation must be clearly labeled.** Do not claim that a
    simulated packet is a real packet or that estimated metrics are
    measured real-world results.
3.  **Separate routing and forwarding.** Route calculation, per-router
    forwarding, packet lifetime, link transmission, and UI animation are
    separate operations.
4.  **Make decisions explainable.** The user should be able to inspect
    why a route was selected and why a packet was forwarded or dropped.
5.  **No silent failures.** Invalid input, unreachable destinations,
    route calculation errors, and simulation errors must be reported
    clearly.
6.  **Preserve working code.** Before editing an existing repository,
    inspect it and understand its framework, conventions, and current
    state. Do not overwrite unrelated files.
7.  **Implement incrementally.** Build and test the core simulator
    before adding advanced protocols or 3D effects.
8.  **Do not fake completeness.** A button must perform its stated
    action. If a feature is not implemented, label it as planned or
    disabled rather than showing fake results.

------------------------------------------------------------------------

## 2. Product Scope

### 2.1 Core product

The first complete version must include:

-   Editable network topology with hosts, routers, switches, and links.
-   IP addresses, subnet masks/CIDR, interfaces, default gateways, and
    routing tables.
-   Three guided scenarios: same subnet, different subnets through a
    gateway, and multi-router/cloud destination.
-   C++ routing engine with Dijkstra, BFS, Bellman--Ford, and
    widest-path algorithms.
-   A clear separation between routing algorithms and packet forwarding.
-   Packet configuration, including TTL and packet size.
-   Hop-by-hop simulation, event timeline, packet inspector, and router
    inspector.
-   TTL expiration and simulated ICMP Time Exceeded events.
-   Configurable link metrics and six routing criteria.
-   Link/router failures, packet loss, delay, congestion, and MTU
    experiments.
-   Algorithm and route comparison reports.
-   Responsive 2D topology visualization.
-   A 3D topology view after the 2D/core simulator is stable.
-   Automated tests, documented limitations, and a reproducible demo
    topology.

### 2.2 Optional/later features

Do not let these delay the first working version:

-   Full TCP connection state machine and retransmission behavior.
-   UDP application examples.
-   Detailed ARP cache timers and Ethernet switching tables.
-   NAT, firewall rules, ACLs, VLANs, MPLS, OSPF-like and RIP-like
    protocol modes.
-   Packet hex dump and pcap export.
-   Linux network namespace validation and optional TShark/Wireshark
    integration.
-   Real AWS infrastructure integration.
-   User accounts, cloud-saved topologies, collaboration, and classroom
    management.
-   Advanced traffic generation and queueing models.

The cloud/EC2 endpoint in the core version is a **simulated endpoint**.
Do not require real AWS credentials or create cloud resources by
default.

------------------------------------------------------------------------

## 3. Primary User Workflow

The intended workflow is:

1.  Open the dashboard.
2.  Choose a starter topology or create a blank topology.
3.  Add hosts, switches, routers, and links.
4.  Configure IP addresses, subnet masks, interfaces, gateways, and link
    properties.
5.  Validate the topology and display any configuration errors.
6.  Select a source host and destination host.
7.  Select a scenario, routing algorithm, optimization objective, TTL,
    packet size, and relevant constraints.
8.  Press **Run Simulation**.
9.  Watch the packet travel through the network, or use **Step**,
    **Pause**, **Resume**, and **Reset**.
10. Click a router or packet to inspect its current state.
11. Review the event timeline and final outcome.
12. Change one parameter or inject a failure and run again.
13. Compare the resulting route and metrics with earlier runs.
14. Export a report or save the topology locally.

The UI must make this workflow discoverable without requiring the user
to understand every advanced option first.

------------------------------------------------------------------------

## 4. Network Scenarios

### 4.1 Scenario A --- Same subnet / local delivery

Example:

-   Host A: `192.168.1.10/24`
-   Host B: `192.168.1.20/24`
-   Both hosts connect to the same Layer 2 switch.

Expected behavior:

1.  Host A applies its subnet mask to determine whether the destination
    is local.
2.  If local, it resolves the destination MAC address using simulated
    ARP if the address is not already known.
3.  Host A sends an Ethernet frame to Host B through the switch.
4.  The switch forwards the frame based on its simulated MAC forwarding
    table, if switching behavior is implemented.
5.  Host B receives the frame and the packet is marked delivered.

In the basic scenario, a router is not needed. The simulator must not
route through a gateway unnecessarily. If the topology does not include
a switch, a direct Layer 2 link may be supported as a simpler case.

Display: - Source and destination IPs and MACs. - ARP request/reply
events when needed. - Switch forwarding decision. - Link traversal and
delivery status. - A clear note that IP routing was not required.

### 4.2 Scenario B --- Different subnets through a default gateway

Example:

-   Host A: `192.168.1.10/24`, gateway `192.168.1.1`
-   Router interface R1-A: `192.168.1.1/24`
-   Router interface R1-B: `10.0.0.1/24`
-   Host B: `10.0.0.20/24`, gateway `10.0.0.1`

Expected behavior:

1.  Host A determines that Host B is outside its local subnet.
2.  Host A resolves the MAC address of its default gateway, not the
    remote host.
3.  Host A transmits a frame addressed to the gateway's MAC address.
4.  The router receives the frame, removes the incoming Layer 2
    encapsulation, checks the IP packet, decrements TTL according to
    IPv4 forwarding semantics, and performs a route/forwarding lookup.
5.  The router selects the outgoing interface and next-hop address.
6.  It resolves the next-hop MAC if needed and creates a new outgoing
    Layer 2 frame.
7.  Host B receives the packet.

The source and destination IP addresses normally remain unchanged across
ordinary routing, unless an explicitly modeled feature such as NAT
changes them. Ethernet source/destination addresses are link-specific
and are rewritten for each outgoing link.

Display the gateway decision and the router's selected outgoing
interface.

### 4.3 Scenario C --- Multi-router path to a remote/cloud endpoint

Example:

`Host A → R1 → R2 → R3 → Cloud Host`

Each router and link may have different costs, bandwidth, delay, loss
probability, MTU, utilization, and processing load.

Expected behavior:

1.  Validate that the source and destination are configured and active.
2.  Determine the source's local/remote destination decision.
3.  Calculate a candidate route according to the chosen algorithm and
    objective, where applicable.
4.  Simulate packet transmission and per-router forwarding.
5.  Check link/router availability and configured impairment.
6.  Apply TTL, MTU, and other implemented protocol rules.
7.  Deliver the packet or record the precise reason for failure.
8.  Optionally create a separate simulated ICMP response when
    appropriate.
9.  Show a final summary and metrics.

The destination may be labeled **Cloud / simulated EC2 endpoint**. It
must not be represented as a real EC2 instance unless actual AWS
integration is later implemented.

### 4.4 Scenario handling rules

-   These are different topology/use cases, not three different IP
    protocols.
-   A local subnet delivery can avoid IP routing.
-   A gateway scenario may require only one router.
-   A multi-hop scenario may require multiple router forwarding
    decisions.
-   If the topology cannot support the selected scenario, explain what
    is missing and provide a way to correct it.

------------------------------------------------------------------------

## 5. Topology Editor

### 5.1 Node types

At minimum:

**Host** - Unique node ID and display name. - IPv4 address and
CIDR/subnet mask per interface. - MAC address in simulation. - Default
gateway. - Active/inactive state. - Optional source/destination role.

**Router** - Unique ID and name. - Multiple network interfaces. - IP
address, subnet/CIDR, and MAC address per interface. - Forwarding
enabled/disabled state. - Routing/forwarding table. - Simulated
processing delay or load. - Optional packet filtering/NAT settings in
later versions.

**Switch** - Unique ID and name. - Layer 2 ports. - Simulated MAC
forwarding table if switching behavior is included. - Active/inactive
state.

**Cloud endpoint** - Simulated remote host with configurable IP and
interface properties. - Visually distinct cloud icon/region. - Explicit
label that it is simulated.

### 5.2 Link properties

Each link should support:

-   Unique ID.
-   Endpoint node and interface at each end.
-   Directed or bidirectional configuration; the UI should make
    direction clear.
-   Link cost (non-negative in the standard Dijkstra profile).
-   Nominal capacity/bandwidth.
-   Available bandwidth or current load.
-   One-way propagation delay.
-   Packet-loss probability.
-   MTU.
-   Active/inactive state.
-   Optional queue capacity and queueing model.
-   Optional label and visual grouping.

All units must be displayed consistently. For example, bandwidth in
Mbps, delay in milliseconds, MTU in bytes, and packet loss as a
percentage.

### 5.3 Editor actions

-   Add node, delete node, edit node.
-   Add link, delete link, edit link.
-   Move nodes and rearrange layout.
-   Zoom, pan, fit-to-view.
-   Select one or multiple nodes/links if supported.
-   Undo/redo as a later enhancement.
-   Duplicate a topology.
-   Load a starter topology.
-   Save/load topology as a versioned JSON file.
-   Validate configuration.
-   Reset to the starter topology.

### 5.4 Validation

Detect and explain issues such as:

-   Duplicate node IDs or interface addresses.
-   Invalid IP address or CIDR.
-   Missing default gateway for a remote destination.
-   Gateway not reachable on a directly connected subnet.
-   Interface and link subnet mismatch.
-   Link references a missing node/interface.
-   Negative link cost where unsupported.
-   Invalid bandwidth, delay, loss probability, or MTU.
-   Inactive source, destination, interface, router, or link.
-   A destination that has no route.
-   A disconnected topology.

Warnings should distinguish a configuration that is unusual from one
that prevents the simulation from running.

------------------------------------------------------------------------

## 6. Routing Engine

### 6.1 Graph representation

Represent the network graph in the C++ engine. A graph edge represents a
connection with relevant properties. The topology model should
distinguish: - Physical/logical connectivity. - IP subnets and
interfaces. - Router forwarding decisions. - Routing algorithm weights.

Do not treat every host-to-host graph edge as an IP routing hop.
Router-hop counting must reflect actual router forwarding operations.

### 6.2 Algorithms

**Dijkstra** - Finds a minimum-total-weight path with non-negative edge
weights. - Use a priority queue. - Reject unsupported negative weights
with a clear error. - Complexity target for an adjacency-list
implementation: `O((V + E) log V)` under the usual binary-heap
assumptions.

**BFS** - Finds a path with the fewest edges when edges are
unweighted. - Do not claim BFS minimizes weighted cost. - If the UI
displays cost for the BFS-selected route, calculate that cost after
route reconstruction.

**Bellman--Ford** - Supports negative edge weights and can detect
reachable negative-weight cycles. - Standard networking link metrics
should normally be non-negative. - If a bidirectional graph has negative
edge weights, it may create a negative cycle; validate and explain
this. - This algorithm can demonstrate relaxation rounds, but must not
be falsely described as a complete implementation of RIP or any other
routing protocol.

**Widest Path** - Selects a path that maximizes the minimum available
bandwidth across its links. - It is not the same as minimum-latency
routing. - Correctly handle source equals destination and unreachable
destinations.

**Custom Policy** - Apply hard constraints first, then optimize a
selected objective. - The UI should explain the chosen objective,
constraints, and how each is evaluated. - Do not silently add
heterogeneous values such as milliseconds, Mbps, percentages, and CPU
utilization into one arbitrary sum.

### 6.3 Routing objectives

Support these six criteria:

1.  **Cost:** minimize the sum of link costs.
2.  **Packet-loss rate:** minimize the route's end-to-end loss
    probability.
3.  **Available bandwidth:** maximize bottleneck bandwidth or enforce a
    minimum bandwidth requirement.
4.  **Congestion / link utilization:** avoid overloaded links using a
    documented utilization/queueing penalty.
5.  **MTU / packet-size compatibility:** treat as a feasibility
    constraint or model fragmentation/drop behavior; do not treat MTU as
    an ordinary additive cost.
6.  **Router processing load:** model as an estimated
    processing/queueing delay or documented penalty. CPU percentage
    alone is not a precise measure of forwarding speed.

For independent link loss probabilities `p_i`, the end-to-end loss
probability is:

`P(loss) = 1 - product(1 - p_i)`

The end-to-end success probability under this simplified independence
assumption is:

`P(success) = product(1 - p_i)`

These estimates must be labeled as model-based. If independence is not
assumed, explain the alternate model.

### 6.4 Hard constraints versus optimization

The interface should separate: - **Hard constraints:** conditions a
route must satisfy, such as minimum bandwidth, maximum acceptable loss,
required MTU compatibility, and excluding inactive links. -
**Optimization objective:** the one criterion being minimized/maximized
among feasible candidates.

If no route satisfies the constraints, show **No feasible route** and
explain which constraints rejected the candidates.

### 6.5 Route comparison

For a given topology and source/destination, allow users to compare
routes or algorithm outcomes. Report: - Selected path and ordered
hops. - Total link cost. - Number of routers traversed. - Estimated
end-to-end delay. - Bottleneck bandwidth. - Estimated end-to-end loss
probability. - Congestion/utilization information where modeled. - MTU
feasibility and fragmentation status. - Number of explored nodes or
relaxations where meaningful. - Algorithm execution time measured
separately from simulated packet travel time. - Final packet delivery
outcome.

Do not compare results from different topologies or configurations
without clearly identifying the differences.

### 6.6 Routing versus forwarding

This distinction is fundamental:

-   **Routing algorithm:** calculates a route or populates routing
    information.
-   **Forwarding logic:** at each router, consults the forwarding table
    and selects the outgoing interface/next hop for the current packet.
-   **Packet simulation:** executes transmission, delay, loss, TTL, and
    other modeled behavior.
-   **Visualization:** displays events emitted by the simulator.

These must be distinct modules or clearly separated responsibilities.

------------------------------------------------------------------------

## 7. Packet and IPv4 Simulation

### 7.1 Packet configuration

Before running, allow the user to configure: - Source host. -
Destination host. - Initial IPv4 TTL. - Total packet size in bytes. -
Protocol selection from implemented options, such as ICMP, UDP, or a
simplified TCP example. - IPv4 Don't Fragment (DF) setting where
fragmentation is modeled. - Optional payload label or test message. -
Optional flow/packet ID. - Optional simulation seed for reproducible
randomized loss.

Validate input ranges and show useful errors.

### 7.2 Packet lifecycle

A packet should have a well-defined state such as: - Created. - Waiting
for transmission. - In transit. - Arrived at router. - Forwarding
lookup. - Queued, if queueing is modeled. - Forwarded. - Delivered. -
Dropped. - Timed out or aborted by user.

Use structured events to record state transitions. The UI should not
independently decide packet state.

### 7.3 IPv4 TTL

-   IPv4 TTL is an 8-bit field used to limit packet lifetime,
    operationally decremented by routers as they forward packets.
-   TTL is not link cost, latency, bandwidth, or a user-selected route
    weight.
-   Each forwarding router decrements TTL according to the simulator's
    documented order of operations.
-   If the TTL reaches the expiration condition, the packet must not be
    forwarded further.
-   The simulator must be consistent about when it decrements TTL and
    when it checks for expiration.
-   Display initial TTL, current TTL, router where expiration occurred,
    and event history.
-   In the common simplified model, a path requiring `N` router
    forwarding operations needs an initial TTL greater than `N` to
    complete forwarding. Explain the exact convention used by the
    engine.
-   TTL behavior must be unit tested at boundary values.

### 7.4 ICMP Time Exceeded

When IPv4 TTL expires, the simulator may create a **separate** simulated
ICMP Time Exceeded message (Type 11, Code 0) toward the original source,
subject to the modeled policy.

-   Do not pretend the original packet is simply reversed.
-   Track the original packet ID and the ICMP response's own ID.
-   Show the router that generated the response.
-   The response follows its own simulated return route and can itself
    fail.
-   Include a configuration/policy note that real devices may suppress
    ICMP responses in certain circumstances.
-   Do not use the same TTL-expired packet as the response.

### 7.5 IPv4 header fields

The packet inspector should display fields supported by the
simulation: - Version. - Header length. - Total length. -
Identification. - Flags: DF and MF. - Fragment offset. - TTL. -
Protocol. - Header checksum status if checksum behavior is
implemented. - Source IP. - Destination IP. - Optional payload length
and payload summary.

Do not display fabricated checksum values if checksums are not actually
calculated.

### 7.6 Link-layer encapsulation

At each link, show: - Incoming frame source/destination MAC. - Outgoing
frame source/destination MAC. - EtherType if modeled. - Incoming and
outgoing interface. - Explain that normal routing replaces the Layer 2
frame for the next link while retaining the IP source/destination unless
NAT or another explicit feature changes them.

### 7.7 Packet loss and delay

-   Use an explicit, documented random model for probabilistic packet
    loss.
-   Support a fixed seed so that a user can reproduce an experiment.
-   Record where a packet was lost.
-   Link delay and router processing delay must use consistent units.
-   If queueing/congestion is modeled, document the queueing
    approximation.
-   Distinguish simulated travel/processing time from the actual runtime
    of the C++ algorithm.

------------------------------------------------------------------------

## 8. MTU, Fragmentation and Reassembly

### 8.1 Key distinction

TCP segmentation and IP fragmentation are different concepts. The
simulator must not describe them as interchangeable.

### 8.2 IPv4 fragmentation

When IPv4 fragmentation is enabled and allowed: - Account for the IPv4
header size. - Respect the outgoing link MTU. - Display fragment ID,
total length, MF flag, and fragment offset. - Fragment offsets are
expressed in units of 8 bytes. - Track fragments independently through
the modeled network. - Reassemble at the destination only when the
required fragments arrive and the reassembly model says they can be
assembled. - Show a clear failure if required fragments are lost or
reassembly fails. - Explain that fragment payload sizes (except the
final fragment) must align with the 8-byte offset unit.

Example to test: an IPv4 packet of 4000 bytes total, with a 20-byte
header, over MTU 1500. The original payload is 3980 bytes. Assuming no
options and standard IPv4 fragmentation: - Fragment 1: payload 1480
bytes, total length 1500, offset 0, MF=1. - Fragment 2: payload 1480
bytes, total length 1500, offset 185, MF=1. - Fragment 3: payload 1020
bytes, total length 1040, offset 370, MF=0.

The fragment offsets refer to the original IP payload, not the complete
packet.

### 8.3 DF and MTU failure

If DF is set and the packet cannot fit the outgoing MTU: - Do not
fragment the packet. - Record a drop/failure event. - The simulator may
show an appropriate ICMP fragmentation-needed / Packet Too Big style
diagnostic as supported by the model. - Explain Path MTU Discovery as an
optional educational flow rather than claiming a complete implementation
unless built.

### 8.4 IPv6 note

IPv6 routers do not fragment packets in transit. If IPv6 is later
supported, implement its different fragmentation and ICMPv6 rules rather
than reusing IPv4 rules unchanged.

------------------------------------------------------------------------

## 9. OSI and TCP/IP Learning Mode

This is an educational view, not a claim that every real protocol stack
executes seven independent OSI modules.

### 9.1 OSI layers

1.  **Physical:** link availability and bit-transmission concept.
2.  **Data Link:** Ethernet frames, MAC addresses, switching,
    ARP-related local delivery.
3.  **Network:** IPv4 addressing, routing, TTL, fragmentation, ICMP.
4.  **Transport:** TCP/UDP ports and transport-level behavior if
    implemented.
5.  **Session:** conceptual session management.
6.  **Presentation:** conceptual data representation/encoding.
7.  **Application:** application message or service.

### 9.2 TCP/IP mapping

Display the usual conceptual mapping: - Application layer: OSI
Application, Presentation, and Session. - Transport layer: OSI
Transport. - Internet layer: OSI Network. - Link/network-access layer:
OSI Data Link and Physical.

### 9.3 Encapsulation demonstration

For a send/receive flow, show the conceptual wrapping and unwrapping of
application data, transport header, IP header, and link-layer frame.
Only display fields actually represented by the simulation. Keep a clear
distinction between conceptual teaching diagrams and full protocol
implementations.

### 9.4 TCP and UDP extensions

If TCP is implemented, clearly scope which behaviors are real modeled
state and which are simplified: - TCP ports. - SYN/SYN-ACK/ACK
connection setup. - Sequence and acknowledgment numbers. - FIN/RST
events where implemented. - Retransmission on modeled loss. - Receive
window and simplified flow-control concepts. - Congestion-control model
only if implemented and documented.

For UDP: - Source and destination ports. - Datagram length/checksum
fields only if modeled. - No TCP-like handshake. - Delivery is not
guaranteed by UDP itself.

Do not claim a complete TCP/IP stack if only a simplified educational
state machine is implemented.

------------------------------------------------------------------------

## 10. Topology Visualization: 2D and 3D

### 10.1 2D mode

The 2D topology view is the primary operational interface and must be
completed first.

Requirements: - Display hosts, switches, routers, and simulated cloud
endpoints with distinct icons. - Show node names and optional IP
labels. - Draw links with configurable properties. - Highlight the
selected route. - Animate packet movement along links based on the
simulation event timeline. - Highlight the current router and outgoing
link. - Use clear colors/icons for delivered, dropped, inactive, and
selected states. - Support zoom, pan, fit-to-view, selection, and a
readable layout. - Avoid using color as the only indicator; include
labels/icons. - Provide an accessible alternative in the event
timeline. - Support small and moderately large topologies without making
labels unreadable.

### 10.2 3D mode

Build only after the simulator and 2D mode are reliable.

Requirements: - Represent the same nodes, links, routes, and events as
2D. - Allow rotate, pan, zoom, and reset camera. - Group networks into
visually distinct regions or subnet clusters. - Highlight the selected
route and current packet. - Clicking a node opens the same inspector
used by 2D mode. - Provide a button to return to a useful default
camera. - Ensure 3D mode is optional and does not block the main
workflow on devices where performance is limited. - Do not duplicate
simulation logic in the 3D renderer.

### 10.3 Animation rules

-   Animation is a presentation of recorded events, not the source of
    truth.
-   Pause, resume, single-step, and reset must be reliable.
-   If the user pauses, event progression must stop.
-   Reset must return to the documented initial state.
-   Fast-forward/slow motion may change presentation speed but must not
    alter network outcomes.
-   A failed packet must stop on the appropriate event and show the
    failure reason.

------------------------------------------------------------------------

## 11. Packet Timeline and Inspector

### 11.1 Timeline events

The event timeline should support structured entries such as: - Packet
created. - Local/remote subnet decision. - ARP request/reply if
modeled. - Frame transmitted. - Packet arrived at router. - Forwarding
table lookup. - Selected next hop. - TTL decremented. - Packet queued. -
Link loss. - Router/interface unavailable. - MTU/DF failure. - Fragment
created. - Fragment received. - Packet forwarded. - Packet delivered. -
TTL expired. - ICMP response created. - ICMP response delivered or
dropped. - User paused/reset the simulation.

Each event should include a timestamp or simulation-time value,
node/link reference, packet ID, event type, and human-readable
explanation.

### 11.2 Packet inspector

When a packet or event is selected, show: - Packet ID and status. -
Source/destination IP. - Current TTL. - Protocol. - Total length and
fragmentation fields if modeled. - Current node and next hop. -
Incoming/outgoing interface. - Link-layer source/destination addresses
where applicable. - Route and hop history. - Drop reason, if
applicable. - Related ICMP packet reference, if applicable.

### 11.3 Router inspector

Show: - Router name/ID. - Active/inactive state. - Interfaces,
addresses, subnet/CIDR, and link status. - Forwarding table entries. -
Destination prefix, next hop, outgoing interface, metric, and route
source where modeled. - Current packet-processing event. - Configured
processing delay/load. - Recent forwarding/drop history. - Neighbor or
ARP information if implemented.

### 11.4 Explainability

For every routing/forwarding decision, provide a plain-language
explanation such as: - "Destination is in a directly connected subnet;
use interface eth1." - "Destination is remote; send to default gateway
192.168.1.1." - "Selected route has the lowest total configured cost
among feasible routes." - "This link was excluded because it is
inactive." - "The route failed the minimum bandwidth constraint." -
"Packet dropped because the next-hop MTU is too small and DF is set." -
"Packet TTL expired at router R2 before forwarding."

Explanations must be generated from actual engine decisions and state,
not hard-coded to always show success.

------------------------------------------------------------------------

## 12. Failure Injection and Troubleshooting Lab

Users should be able to change the network deliberately and observe the
result.

### 12.1 Faults to support

-   Disable/enable a link.
-   Disable/enable a router.
-   Disable/enable an interface.
-   Increase packet-loss probability on a link.
-   Increase link delay.
-   Reduce bandwidth.
-   Increase link utilization or introduce queue pressure.
-   Reduce MTU.
-   Configure an incorrect gateway.
-   Configure a mismatched subnet.
-   Remove a route or introduce a missing route.
-   Configure an unreachable destination.
-   Set TTL too low.
-   Set DF and a packet size that exceeds an outgoing MTU.
-   Optionally simulate a forwarding loop or stale forwarding
    information.

### 12.2 Troubleshooting behavior

For each injected fault: - Record the changed setting. - Show its effect
on route calculation and/or packet forwarding. - Distinguish "no route
found" from "route found but packet failed during forwarding." - Explain
whether rerouting is available. - If automatic rerouting is implemented,
recalculate according to the selected policy and document when it
happens. - Provide before/after comparisons.

Do not silently change topology settings to make a scenario succeed.

------------------------------------------------------------------------

## 13. The Six Routing Criteria

The project must expose exactly these six primary criteria in its
routing-policy UI:

1.  Cost.
2.  Packet-loss rate.
3.  Available bandwidth.
4.  Congestion / link utilization.
5.  MTU / packet-size compatibility.
6.  Router processing load.

### 13.1 Metric semantics

**Cost:** additive link metric; minimize total cost.

**Packet loss:** use a documented end-to-end loss model. With
independent link loss probabilities, use `1 - product(1 - p_i)`.

**Available bandwidth:** route bottleneck is the minimum available
bandwidth on any link in the route. Widest path maximizes this
bottleneck.

**Congestion/utilization:** utilization is modeled load divided by
capacity. Explain whether the simulator uses a threshold, penalty, or
queue model.

**MTU compatibility:** feasibility/fragmentation constraint based on
packet size, headers, DF, and the outgoing path. MTU is not an ordinary
additive metric.

**Router processing load:** model as estimated per-router processing
delay or queueing penalty. Explain how configured load maps to delay; do
not imply CPU percentage directly equals packet-forwarding speed.

### 13.2 UI policy controls

Allow the user to: - Select one primary objective. - Configure hard
constraints such as minimum bandwidth or maximum acceptable loss. -
Exclude inactive links/nodes. - View why a candidate route was accepted
or rejected. - Compare other metrics without confusing them with the
selected objective.

Do not silently use a single arbitrary weighted sum of values with
different units. If a weighted policy is introduced later, normalize
inputs, expose weights, explain the formula, and test it.

------------------------------------------------------------------------

## 14. Metrics and Experiment Reports

Every completed run should provide a summary appropriate to the
scenario.

### 14.1 Route metrics

-   Route sequence.
-   Number of router-forwarding hops.
-   Total configured cost.
-   Estimated one-way delay.
-   Bottleneck available bandwidth.
-   Estimated end-to-end packet-loss probability.
-   Link utilization or congestion information.
-   MTU/fragmentation result.
-   Number of explored nodes or relaxations, where meaningful.
-   Algorithm execution duration.
-   Packet delivery/drop result and reason.

### 14.2 Metric definitions

-   **Algorithm execution time:** actual measured wall-clock runtime of
    the algorithm code, separate from simulated packet travel time.
-   **Simulated end-to-end delay:** modeled link
    propagation/transmission/queueing plus router processing delays
    according to the documented model.
-   **Bottleneck bandwidth:** minimum available bandwidth among the
    links on the route.
-   **End-to-end loss:** model-based estimate under the documented
    assumptions, not a guarantee of real-world loss.
-   **Hop count:** define whether the displayed number means router
    forwarding operations or links traversed. Prefer showing both when
    useful.
-   **Delivery status:** based on packet simulation outcome, not simply
    whether a route exists.

### 14.3 Comparison and export

-   Save a run's configuration and results.
-   Compare at least two runs with clear labels.
-   Export a human-readable report as JSON or CSV; PDF can be a later
    enhancement.
-   Include topology ID/version, algorithm, objective, seed,
    constraints, packet parameters, and simulation version in exported
    results.
-   Do not compare unlike experiments without showing their different
    inputs.

------------------------------------------------------------------------

## 15. Suggested Technical Architecture

### 15.1 Frontend

**React + TypeScript + Vite**

Responsibilities: - Application shell and navigation. - Topology
editor. - Source/destination and simulation controls. - 2D topology
renderer. - Optional 3D renderer. - Packet timeline. - Packet/router
inspectors. - Metrics and comparison views. - Client-side form
validation and clear error messages.

Use reusable components and typed API contracts. Keep UI state separate
from the simulator's authoritative state. Avoid putting all logic in one
large component.

Possible frontend libraries (choose only after checking project
constraints): - React Flow or a comparable graph/canvas library for the
2D editor. - Three.js or React Three Fiber for 3D. - A charting library
for metrics. - A lightweight state-management library only if React
state/context is insufficient.

Do not install every possible library. Choose the smallest set that
meets requirements.

### 15.2 Backend / orchestration

**Python + FastAPI**

Responsibilities: - Validate simulation requests. - Manage topology and
experiment schemas. - Invoke the C++ engine or a well-defined native
binding. - Stream or return structured events. - Store optional saved
topologies and run histories. - Produce reports. - Keep API contracts
typed and documented.

The backend should not reimplement C++ route selection in a way that
causes results to disagree. C++ owns the routing/simulation behavior;
Python validates, orchestrates, and translates data where necessary.

### 15.3 C++ engine

**C++17 or later, with a documented build system**

Responsibilities: - Topology graph representation. - Routing
algorithms. - Candidate-route evaluation. - Forwarding-table lookup and
per-router decisions. - Packet state and lifecycle. - TTL and supported
ICMP behavior. - Link/node status and packet-loss model. -
MTU/fragmentation model. - Event generation. - Metrics and deterministic
tests.

Keep modules separated. Suggested modules: - `topology` - `routing` -
`forwarding` - `packet` - `simulation` - `metrics` - `serialization` -
`tests`

Use explicit types for IP addresses, node IDs, link IDs, packet IDs, and
metric values where practical. Avoid ambiguous magic numbers and global
mutable state.

### 15.4 How frontend, backend, and engine communicate

Recommended first implementation: 1. React sends a validated JSON
simulation request to FastAPI. 2. FastAPI validates schema and topology.
3. FastAPI invokes the C++ executable with a documented JSON
input/output contract, or uses a tested native binding. 4. The C++
engine returns structured events and metrics as JSON. 5. FastAPI returns
the result or streams events. 6. React renders the same event sequence
in its timeline and topology.

Do not attempt to execute C++ directly inside the browser in the initial
version. WebAssembly can be considered later.

### 15.5 Persistence

For the first local version: - Support JSON import/export of
topologies. - Support JSON export of run results. - Local browser
storage may be used for convenience, with a clear versioned schema.

A database is optional for the first version. If persistent multi-user
storage is added later, choose a database deliberately and define
migrations, access controls, and backups.

### 15.6 Suggested repository layout

Adapt to the actual repository if it already exists. Do not force this
structure if the project has established conventions.

``` text
nettrace-3d/
├── PROJECT.md
├── README.md
├── docs/
│   ├── architecture.md
│   ├── networking-model.md
│   ├── api-contract.md
│   └── testing.md
├── frontend/
│   ├── src/
│   │   ├── app/
│   │   ├── components/
│   │   ├── features/
│   │   │   ├── topology/
│   │   │   ├── simulation/
│   │   │   ├── packet-inspector/
│   │   │   ├── router-inspector/
│   │   │   └── metrics/
│   │   ├── api/
│   │   ├── types/
│   │   └── styles/
│   └── tests/
├── backend/
│   ├── app/
│   │   ├── api/
│   │   ├── schemas/
│   │   ├── services/
│   │   └── tests/
│   └── requirements.txt
├── engine/
│   ├── include/
│   ├── src/
│   ├── tests/
│   └── CMakeLists.txt
├── examples/
│   ├── same-subnet.json
│   ├── gateway-routing.json
│   └── multi-router-cloud.json
└── scripts/
```

------------------------------------------------------------------------

## 16. API Contract (Initial Proposal)

Use versioned, validated JSON. Adjust endpoints after inspecting the
repository, but document the final contract.

### 16.1 Health check

`GET /api/health`

Returns service status and available engine/version information.

### 16.2 Topology validation

`POST /api/topologies/validate`

Input: a topology JSON document.

Output: - `valid`: boolean. - `errors`: array of structured errors. -
`warnings`: array of structured warnings. - Optional normalized
topology.

### 16.3 Run simulation

`POST /api/simulations`

Input should include: - Topology. - Source node ID. - Destination node
ID. - Selected algorithm. - Optimization objective. - Hard
constraints. - Initial TTL. - Packet size. - Protocol/DF options
supported by the engine. - Random seed. - Simulation options.

Output should include: - Run ID. - Simulation status. - Selected route
and metrics. - Ordered structured events. - Packet state/history. -
Final delivery/drop reason. - Warnings and model assumptions.

For long runs, consider a job ID plus a WebSocket or Server-Sent Events
stream. Do not add streaming complexity before the basic
request/response workflow works.

### 16.4 Get a run

`GET /api/simulations/{run_id}`

Returns saved run details if persistence is enabled.

### 16.5 Export a run

`GET /api/simulations/{run_id}/export?format=json`

CSV may be added for tabular metrics. Validate requested formats and
return clear errors.

### 16.6 API rules

-   Validate every request.
-   Use stable error codes and human-readable messages.
-   Do not expose arbitrary local file paths or execute user-supplied
    shell commands.
-   Apply request-size limits.
-   Keep API schemas synchronized with TypeScript and C++ serialization.
-   Include API versioning when breaking schema changes are introduced.

------------------------------------------------------------------------

## 17. Data Model (Conceptual)

The final schema may vary, but it should represent these entities
clearly.

### 17.1 Topology

-   `id`
-   `name`
-   `version`
-   `nodes[]`
-   `links[]`
-   `createdAt` or local export metadata, if relevant

### 17.2 Node

-   `id`
-   `name`
-   `type`: host/router/switch/cloudEndpoint
-   `position`: x/y and optional z
-   `interfaces[]`
-   `active`
-   Type-specific configuration

### 17.3 Interface

-   `id`
-   `name`
-   `ipv4Address`
-   `prefixLength`
-   `macAddress`
-   `active`
-   Optional MTU override
-   Connected link reference

### 17.4 Link

-   `id`
-   `sourceNodeId`
-   `sourceInterfaceId`
-   `targetNodeId`
-   `targetInterfaceId`
-   `directionality`
-   `cost`
-   `capacityMbps`
-   `availableBandwidthMbps` or documented load model
-   `delayMs`
-   `lossProbability`
-   `mtuBytes`
-   `active`

### 17.5 Simulation request

-   `topologyVersion`
-   `sourceNodeId`
-   `destinationNodeId`
-   `algorithm`
-   `objective`
-   `constraints`
-   `initialTtl`
-   `packetSizeBytes`
-   `df`
-   `protocol`
-   `randomSeed`
-   Optional maximum event count and simulation limits

### 17.6 Simulation event

-   `eventId`
-   `simulationTimeMs` or documented time unit
-   `packetId`
-   `eventType`
-   `nodeId` and/or `linkId`
-   `message`
-   `details` object containing structured, type-specific information

### 17.7 Route result

-   `reachable`
-   `pathNodeIds[]`
-   `pathLinkIds[]`
-   `totalCost`
-   `routerHopCount`
-   `linkHopCount`
-   `estimatedDelayMs`
-   `bottleneckBandwidthMbps`
-   `estimatedLossProbability`
-   `mtuStatus`
-   `algorithmRuntimeMs`
-   `exploredNodeCount` or relevant operation counter
-   `deliveryStatus`
-   `failureReason`

Use null or an explicit "not applicable" state for unavailable metrics;
do not fill missing values with fabricated zeroes.

------------------------------------------------------------------------

## 18. UI Pages and Components

### 18.1 Dashboard / landing page

Display: - Project name and short explanation. - Buttons to open a new
topology or starter scenario. - Recent local topologies/runs if
available. - Shortcuts to guided examples. - Simulation disclaimer. -
Clear indication of what is implemented versus experimental.

### 18.2 Main simulation workspace

Suggested layout: - **Top bar:** topology name, save/import/export, run
status, reset. - **Left panel:** node/link palette and topology
validation. - **Center:** 2D/3D topology canvas. - **Right panel:**
configuration, packet inspector, router inspector. - **Bottom panel:**
event timeline and metrics summary.

Use resizable panels only if they improve the workflow. On narrow
screens, stack panels and preserve access to run controls.

### 18.3 Simulation controls

Include: - Source dropdown with host name and IP. - Destination dropdown
with host name and IP. - Scenario selector. - Routing algorithm
dropdown. - Objective dropdown. - Hard-constraint controls. - Initial
TTL input. - Packet size input. - DF checkbox where applicable. -
Optional random seed. - Run, Pause, Resume, Step, Reset. - 2D/3D
toggle. - Simulation speed control that affects animation only. - A
clear validation/result area.

Disable invalid actions with a useful explanation. Do not allow Run if
required fields are invalid.

### 18.4 Visual style

Use a professional technical-lab aesthetic: - Dark navy/charcoal
background with blue/cyan accents is an acceptable default. - Clear
contrast and legible text. - Restrained glow and gradients; avoid
excessive visual effects. - Distinct icons for hosts, routers, switches,
and cloud endpoints. - Consistent state indicators for active, inactive,
selected, delivered, and dropped. - Tooltips for networking
terminology. - Empty states and loading states. - Accessible keyboard
focus, labels, and status text. - Responsive behavior for desktop and
tablet.

Visual polish is important, but must not take priority over accurate
behavior and usability.

------------------------------------------------------------------------

## 19. Error Handling and Edge Cases

Handle all of the following explicitly: - Source or destination
missing. - Source equals destination. - Source/destination inactive. -
Invalid IP or subnet. - Destination is local but no Layer 2 path
exists. - No default gateway for a remote destination. - Gateway is not
reachable on a connected subnet. - No route found. - A route exists but
a link/router fails during forwarding. - TTL expires at a particular
router. - DF is set and outgoing MTU is too small. - Fragment loss
prevents reassembly. - Random loss is enabled but the seed is not
fixed. - Invalid or unsupported algorithm/objective combination. -
Negative edge weights in Dijkstra mode. - Reachable negative cycle in
Bellman--Ford mode. - Source equals destination edge case in widest
path. - Parallel edges between the same nodes. - Empty topology. - Very
large topology or event count exceeding configured limits. - Engine
process fails or returns invalid JSON. - Frontend loses connection while
a simulation is running. - User resets or edits topology during a run.

Every error should be visible, understandable, and testable.

------------------------------------------------------------------------

## 20. Testing Strategy

The project is not complete simply because the UI renders. Core
networking and algorithm behavior must have automated tests.

### 20.1 C++ algorithm unit tests

-   Dijkstra chooses the minimum non-negative total-cost route.
-   Dijkstra rejects unsupported negative weights.
-   BFS chooses the fewest-edge route on an unweighted graph.
-   BFS reports the correct calculated cost for its reconstructed path.
-   Bellman--Ford handles negative edges and detects reachable negative
    cycles.
-   Widest Path maximizes the bottleneck bandwidth.
-   Unreachable destination is handled.
-   Source equals destination is handled.
-   Parallel edges are handled correctly.
-   Directed and bidirectional links are represented correctly.

### 20.2 Forwarding and network tests

-   Same-subnet delivery avoids unnecessary routing.
-   Remote-subnet traffic uses the configured default gateway.
-   A router selects the correct outgoing interface and next hop.
-   Layer 2 addresses are updated per outgoing link in the simulated
    model.
-   IP addresses remain unchanged under ordinary routing when NAT is not
    enabled.
-   TTL decreases consistently at each forwarding router.
-   TTL expiration drops the packet and records the correct router.
-   ICMP Time Exceeded is a separate response packet when enabled.
-   The response has its own return-path events and may fail.
-   Inactive links and routers prevent forwarding.
-   Packet loss is reproducible with a fixed seed.
-   MTU/DF behavior is correct for supported cases.
-   IPv4 fragment offsets, MF flags, and reassembly are correct.
-   No-route failure is distinct from forwarding-time failure.

### 20.3 API tests

-   Valid topology passes validation.
-   Invalid schema returns useful errors.
-   Valid simulation request returns a structured result.
-   Engine errors do not crash the API process.
-   Exported JSON matches the documented schema.
-   Request limits and malformed inputs are handled safely.

### 20.4 Frontend tests

-   Source/destination selection works.
-   Run is disabled or reports errors for invalid inputs.
-   Events appear in the timeline in order.
-   Selecting a router displays its inspector.
-   Selecting a packet/event displays the correct details.
-   Pause/resume/step/reset work.
-   2D and 3D show the same simulation state.
-   Results and errors are accessible without relying only on color.

### 20.5 End-to-end acceptance scenarios

Create deterministic example topologies and expected results for: 1.
Same-subnet delivery. 2. Two-subnet gateway delivery. 3. Multi-router
delivery with sufficient TTL. 4. Multi-router TTL expiry. 5. No route.
6. Disabled link. 7. Packet loss with a fixed seed. 8. DF + too-small
MTU. 9. IPv4 fragmentation and successful reassembly. 10. Different
route selection for cost versus bandwidth objective.

Document all intentional simplifications.

------------------------------------------------------------------------

## 21. Development Milestones

Build in this order. Each milestone must run and be tested before
starting the next.

### Milestone 0 --- Repository inspection and plan

-   Inspect existing files, package manager, build tools, and current
    framework.
-   Do not overwrite unrelated code.
-   Produce a short implementation plan.
-   Confirm C++, Python, and frontend toolchain availability.
-   Add or update this project specification and README.

### Milestone 1 --- C++ graph and routing algorithms

-   Implement graph types and validation.
-   Implement Dijkstra, BFS, Bellman--Ford, and widest path.
-   Add unit tests and example graph.
-   Add JSON input/output only after algorithms are testable.

**Exit criteria:** tests pass and route results are deterministic.

### Milestone 2 --- Topology model and basic forwarding

-   Add hosts, routers, interfaces, subnets, links, and gateways.
-   Implement local-versus-remote subnet decision.
-   Implement next-hop/forwarding decisions.
-   Add the three starter topologies.

**Exit criteria:** same-subnet and gateway scenarios work with correct
explanations.

### Milestone 3 --- Packet lifecycle, TTL, events

-   Implement packet state.
-   Implement router-hop TTL semantics.
-   Implement drop reasons and optional ICMP Time Exceeded events.
-   Create structured event output.

**Exit criteria:** deterministic hop-by-hop event logs and TTL boundary
tests pass.

### Milestone 4 --- FastAPI integration

-   Implement health, validation, and simulation endpoints.
-   Invoke the C++ engine through a safe documented interface.
-   Validate request/response schemas.

**Exit criteria:** API can run all starter scenarios and returns valid
JSON.

### Milestone 5 --- React 2D interface

-   Build dashboard and simulation workspace.
-   Add topology editor and source/destination controls.
-   Connect to the API.
-   Render actual engine events.
-   Add packet and router inspectors.

**Exit criteria:** a user can create or load a topology and observe a
real engine-backed simulation.

### Milestone 6 --- Routing criteria and metrics

-   Implement six criteria with documented semantics.
-   Add hard constraints and candidate rejection explanations.
-   Add comparison reports and exports.

**Exit criteria:** metric calculations have tests and UI definitions.

### Milestone 7 --- Failure injection and MTU

-   Add link/router failure, delay, loss, congestion, MTU/DF,
    fragmentation, and supported reassembly behavior.
-   Add regression tests.

**Exit criteria:** failure scenarios produce accurate event logs and
reports.

### Milestone 8 --- 3D visualization

-   Add 3D view backed by the same topology and event model.
-   Implement camera controls and route highlighting.
-   Keep 2D as a fully functional fallback.

**Exit criteria:** switching views does not change simulation results.

### Milestone 9 --- Validation, documentation and demo

-   Add comprehensive README, architecture notes, test instructions, and
    known limitations.
-   Run all automated tests.
-   Create repeatable demo scenarios and benchmark results.
-   If possible, validate selected behaviors with Linux network
    namespaces and packet-capture tools.

**Exit criteria:** another developer can install, run, test, and
understand the project using the documentation.

------------------------------------------------------------------------

## 22. Instructions for Antigravity / AI Coding Assistant

Follow these instructions whenever working in this repository.

1.  Read `PROJECT.md` fully before coding.
2.  Inspect the current repository before changing files. Identify
    existing stack, package manager, naming conventions, and tests.
3.  Do not rewrite the entire project just because a different
    architecture is suggested. Adapt the specification to the actual
    repository while preserving its goals.
4.  Start with a concise plan for the current milestone. Work in small,
    reviewable steps.
5.  Implement only a manageable milestone or subtask at a time.
6.  After each meaningful change, run relevant tests, type checks,
    builds, and linters that are available.
7.  Fix errors caused by your changes before moving on. Do not hide
    failures or claim tests passed when they were not run.
8.  Keep C++ routing logic, packet simulation, Python orchestration, and
    frontend visualization separated.
9.  Use typed data structures and documented JSON contracts. Avoid
    unexplained magic numbers.
10. Never fabricate metrics, routes, packet events, or test results.
11. Do not add dead buttons, placeholder charts presented as real data,
    or mock simulation outcomes to the production workflow.
12. If a feature is too large, create a minimal working version and
    document what remains.
13. Preserve working code and unrelated functionality. Avoid deleting
    files without a clear reason.
14. Keep networking semantics consistent with this document. If a
    simplification is necessary, document it in code and UI.
15. When a technical ambiguity exists, choose the simplest correct
    model, document the choice, and add tests.
16. Use deterministic seeds in tests that involve randomized packet
    loss.
17. Do not introduce real cloud resources, credentials, external packet
    transmission, or privileged host-network changes by default.
18. Ask for clarification only when a decision materially blocks
    implementation. Otherwise make a reasonable, documented choice.
19. At the end of each task, report:
    -   Files created/modified.
    -   What works now.
    -   Commands/tests run and their actual results.
    -   Known limitations.
    -   The next recommended task.
20. Update the README and project documentation when the architecture or
    behavior changes.

### First task for Antigravity

Do not attempt to build all features in one response. First inspect the
repository and toolchain, summarize the existing state, identify any
missing dependencies, propose a milestone plan, and then implement
**Milestone 1: the C++ graph and routing algorithms with unit tests**.
If the repository already has an implementation, review and test it
before replacing anything.

------------------------------------------------------------------------

## 23. Security, Reliability and Safety

-   The simulator must run locally without requiring privileged
    networking access.
-   Do not execute user-provided strings as shell commands.
-   If invoking a C++ executable, pass validated arguments and use safe
    process APIs.
-   Apply input-size and event-count limits.
-   Validate topology IDs and references before execution.
-   Keep real credentials and secrets out of the repository.
-   Do not make real network changes or transmit packets as part of the
    default simulation.
-   If a later validation mode uses Linux namespaces, raw sockets,
    packet capture, or privileged commands, make it explicitly opt-in,
    document permissions, and isolate it from the default simulation.
-   Handle engine crashes and malformed engine output gracefully.
-   Do not allow malformed topology files to crash the frontend or
    backend.

------------------------------------------------------------------------

## 24. Documentation Requirements

The repository should include:

### README.md

-   What NetTrace 3D is.
-   Main features.
-   Architecture diagram.
-   Prerequisites and installation.
-   How to run frontend, backend, and C++ engine.
-   Starter scenarios.
-   How to run tests.
-   Screenshots or demo GIFs after they exist.
-   Known limitations and future work.

### docs/architecture.md

-   Component responsibilities.
-   Data flow from React to FastAPI to C++ and back.
-   Event schema.
-   Build and runtime details.

### docs/networking-model.md

-   Supported networking behaviors.
-   TTL convention.
-   Routing and forwarding distinction.
-   Loss/delay model.
-   MTU/fragmentation assumptions.
-   Simplifications and unsupported protocol behavior.

### docs/api-contract.md

-   Endpoints.
-   Request/response examples.
-   Error codes.
-   Schema versioning.

### docs/testing.md

-   Unit, API, frontend, and end-to-end test commands.
-   Expected results for starter scenarios.
-   Optional real-world validation steps.

------------------------------------------------------------------------

## 25. Definition of Done

The first strong portfolio-ready version is complete only when:

-   [ ] The repository installs and runs using documented commands.
-   [ ] The C++ routing algorithms are implemented and covered by tests.
-   [ ] Topology validation catches common configuration errors.
-   [ ] Same-subnet, gateway, and multi-router scenarios work.
-   [ ] Routing and forwarding are separate and understandable.
-   [ ] TTL expiration is handled consistently and tested.
-   [ ] ICMP Time Exceeded is modeled as a separate event/packet where
    enabled.
-   [ ] The event timeline comes from actual engine output.
-   [ ] Packet and router inspectors display real simulation state.
-   [ ] The six routing criteria have clear, tested semantics.
-   [ ] At least several failure scenarios work reproducibly.
-   [ ] Metrics are defined, labeled, and not fabricated.
-   [ ] 2D topology visualization works smoothly.
-   [ ] 3D mode, if included, uses the same simulation state.
-   [ ] Automated tests pass, or any known failing tests are clearly
    disclosed.
-   [ ] README and architecture/networking documentation are accurate.
-   [ ] A new developer can reproduce the demo without undocumented
    steps.

------------------------------------------------------------------------

## 26. Final Product Vision

NetTrace 3D should feel like an interactive systems laboratory, not
merely a network diagram with moving dots.

A user should be able to create a topology, send a packet, observe
routing and forwarding, inspect the exact reason for a decision or
failure, change a network condition, rerun the experiment, and compare
results. The codebase should be modular enough that routing algorithms,
protocol models, metrics, and renderers can evolve independently.

The project's strongest engineering value is the combination of: -
Correct, tested C++ algorithms. - Explicit packet-forwarding and TTL
semantics. - Explainable, event-driven simulation. - Reproducible fault
injection and metric comparisons. - A useful React interface with 2D and
optional 3D views. - Clear documentation of what is simulated and what
has been validated against real systems.

**Implementation priority:** correctness → tests → API integration →
useful 2D interface → advanced protocol/failure behavior → 3D polish.

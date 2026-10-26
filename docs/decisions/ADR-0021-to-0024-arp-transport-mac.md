# NetTrace 3D — Architectural Decision Records (D-21 to D-24)

This document records architectural decisions extending NetTrace 3D to support modeled ARP with per-node caches, mandatory unique MACs, and transport layer ports.

---

## D-21: ARP and Neighbor Resolution Model (Supersedes D-08)
- **Decision:** Version 1 models Address Resolution Protocol (ARP) while preserving an optional abstracted resolution mode. Each host and router interface has a mock MAC address. The system defines two key structures:
  1. **AddressRegistry:** Global, **READ-ONLY**, constructed from the immutable topology snapshot. Contains entries with `{ip, mac, nodeId, interfaceId, broadcastDomainId}`.
     - Used exclusively for topology validation, a read-only UI ground-truth panel, and the `abstracted` ARP mode.
     - Nodes running in `modeled` mode **NEVER** read the AddressRegistry to resolve addresses.
  2. **ArpCache:** One per node. Maps `IP -> MAC`, with a provenance source tag (`learned` | `preloaded`).
     - No aging timers in v1 (entries persist for the simulation run).
     - Cleared at the start of each run unless `request.preloadArpCaches` is set to `true`.
- **Request Options:**
  - `arpMode`: `"modeled"` (default) | `"abstracted"`.
  - In `"abstracted"` mode, address resolution is an immediate registry lookup **SCOPED TO THE SENDER'S BROADCAST DOMAIN** and emits the labeled event `NEIGHBOR_RESOLVED_ABSTRACTED`. An address in another broadcast domain is never resolvable by lookup.
- **Rationale:** Models realistic Layer-2 broadcast discovery, switch flooding/learning, and cache populating while retaining an abstracted mode for simple routing exercises.

---

## D-22: Transport Layer and Ports
- **Decision:** Packets carry transport metadata: `{protocol: UDP | TCP | ICMP, srcPort, dstPort, tcpFlags?}`.
  - Ports are integers in the range `1..65535` (port `0` is invalid).
  - Only `UDP` and `TCP` have ports. `ICMP` and `ARP` have no ports.
  - **TCP is metadata only:** No handshake, no sequence/acknowledgement numbers, and no state machine. The UI prominently displays the note: `"TCP handshake not modeled"`.
  - Routers and switches never inspect or modify transport ports.
  - Hosts define listening services: `services: [{protocol, port, label}]`.
  - **Source Port Allocation:** Taken from the simulation request if specified; otherwise allocated deterministically per host per run from ephemeral range `49152` upward (wrapping from `65535` back to `49152`).
  - **Fragmentation Rule:** Only fragment offset `0` carries the transport header; subsequent fragments carry data payloads only.
- **Rationale:** Enables port-level demultiplexing and realistic ICMP Port Unreachable error generation without complicating the engine with full TCP state machines.

---

## D-23: MAC Addressing and Rules
- **Decision:**
  - Every host, router, and cloud-endpoint interface has **ONE** mock MAC address that is **globally unique across the entire topology** (not merely unique per broadcast domain).
  - Locally administered bit set (bit 1 of octet 0 = 1), multicast bit clear (bit 0 of octet 0 = 0).
  - Broadcast (`ff:ff:ff:ff:ff:ff`), multicast (low bit of first octet set, e.g. `01:00:5e:00:00:01`), and all-zero (`00:00:00:00:00:00`) MACs are **never valid interface MACs**.
  - Auto-generated MACs follow the deterministic pattern `02:00:NN:NN:II:II` (where `NN:NN` is node index and `II:II` is interface index as 16-bit big-endian). Generated MACs are persisted into exported topology JSON so they stay stable when nodes are added.
  - Switches maintain a dynamic Layer-2 MAC forwarding table; switches do not have per-port interface MACs.
- **Rationale:** Ensures clean, collision-free Layer-2 simulation, repeatable traces, and strict separation between host/router interfaces and transparent switches.

---

## D-24: Size Accounting & MTU
- **Decision:**
  - IPv4 `totalLength` = 20 (base IP header) + transport header (`UDP`: 8 bytes, `TCP`: 20 bytes, `ICMP`: 8 bytes) + application payload bytes.
  - MTU checks compare the link MTU against the IPv4 `totalLength`.
  - The standard fragmentation example stays valid in terms of IP total length 4000; fragment offset 0 carries the UDP header.
- **Rationale:** Accurately reflects standard IP framing where the transport header is encapsulated within the IP payload.

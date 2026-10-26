# NetTrace 3D --- Architecture and Internal Software Design

**Document:** `ARCHITECTURE.md`\
**Project:** NetTrace 3D --- Network Routing, Protocol Simulation and
Visualization Platform\
**Purpose:** Define component boundaries, data flow, module
responsibilities, interfaces, and implementation rules for the
codebase.\
**Status:** Planned architecture. Update this document when
implementation decisions change.

> **Relationship to `PROJECT.md`:** `PROJECT.md` defines the complete
> product requirements and networking behavior. This file explains how
> the software should be structured to implement those requirements. If
> the existing repository already has a working architecture, inspect it
> first and adapt these recommendations instead of rewriting the project
> blindly.

------------------------------------------------------------------------

## 1. Architecture Goals

NetTrace 3D must be a modular, testable simulation application---not
merely a network diagram with animated icons.

The architecture must support:

1.  Correct, deterministic routing-algorithm results.
2.  A packet simulation engine that is separate from route calculation.
3.  Structured events that drive both the timeline and network
    animation.
4.  A frontend that can inspect topology, packets, routers, and
    experiment results.
5.  A backend API that validates requests and safely invokes the C++
    engine.
6.  Reproducible experiments using documented settings and random seeds.
7.  Automated tests at algorithm, simulation, API, and UI levels.
8.  Future additions such as 3D visualization, protocol extensions, and
    optional real-system validation without replacing the core engine.

## 2. High-Level System

### 2.1 Main components

-   **Frontend:** React + TypeScript + Vite.
-   **API/orchestration backend:** Python + FastAPI.
-   **Simulation engine:** C++17 or later.
-   **Topology and run files:** Versioned JSON for the first local
    version.
-   **Optional persistence:** A database may be added later if users
    need server-side saved topologies, accounts, or shared experiments.

### 2.2 Component diagram

``` mermaid
flowchart TD
    U[User] --> UI[React + TypeScript UI]
    UI --> STATE[Frontend State and View Model]
    STATE --> API[FastAPI REST API]
    API --> VALID[Request and Topology Validation]
    VALID --> ORCH[Simulation Orchestrator]
    ORCH --> CPP[C++ Routing and Simulation Engine]
    CPP --> ROUTE[Routing Algorithms]
    CPP --> FWD[Forwarding and Packet Lifecycle]
    CPP --> EVENTS[Structured Events and Metrics]
    ROUTE --> EVENTS
    FWD --> EVENTS
    EVENTS --> ORCH
    ORCH --> API
    API --> STATE
    STATE --> CANVAS[2D Topology Renderer]
    STATE --> THREE[Optional 3D Renderer]
    STATE --> TIMELINE[Event Timeline and Inspectors]
```

### 2.3 Source of truth

The **C++ engine is authoritative** for: - Route selection. - Forwarding
decisions. - Packet state transitions. - TTL and supported protocol
behavior. - Simulated link/router failures. - Generated events. - Route
and simulation metrics.

FastAPI validates, orchestrates, and transports data. It must not
independently reimplement routing or invent packet outcomes. The
frontend presents the result; it must not calculate a different route or
infer success from animation.

The topology document is the source of configuration. A simulation run
should use an immutable snapshot of the topology and run parameters so
editing the canvas does not silently change an already-running
experiment.

------------------------------------------------------------------------

## 3. Repository Structure

Use this as a recommended structure, not an instruction to restructure
an existing working repository without review.

``` text
nettrace-3d/
├── PROJECT.md
├── ARCHITECTURE.md
├── README.md
├── docs/
│   ├── networking-model.md
│   ├── api-contract.md
│   ├── testing.md
│   └── decisions/
│       ├── 0001-engine-boundary.md
│       └── 0002-event-schema.md
├── frontend/
│   ├── src/
│   │   ├── app/
│   │   ├── components/
│   │   ├── features/
│   │   │   ├── topology/
│   │   │   ├── simulation/
│   │   │   ├── packet-inspector/
│   │   │   ├── router-inspector/
│   │   │   ├── metrics/
│   │   │   └── settings/
│   │   ├── api/
│   │   ├── state/
│   │   ├── types/
│   │   └── styles/
│   └── tests/
├── backend/
│   ├── app/
│   │   ├── main.py
│   │   ├── api/
│   │   ├── schemas/
│   │   ├── services/
│   │   ├── engine/
│   │   └── tests/
│   └── requirements.txt
├── engine/
│   ├── include/nettrace/
│   │   ├── model/
│   │   ├── routing/
│   │   ├── forwarding/
│   │   ├── simulation/
│   │   ├── events/
│   │   ├── metrics/
│   │   └── serialization/
│   ├── src/
│   ├── tests/
│   └── CMakeLists.txt
├── schemas/
│   ├── topology.schema.json
│   ├── simulation-request.schema.json
│   └── simulation-result.schema.json
├── examples/
│   ├── same-subnet.json
│   ├── gateway-routing.json
│   └── multi-router-cloud.json
└── scripts/
```

### 3.1 Structure rules

-   Keep routing algorithms independent of React, FastAPI, and UI
    libraries.
-   Keep packet-simulation behavior independent of the topology
    renderer.
-   Keep request schemas and serialization at module boundaries.
-   Do not place all logic in a single `main.cpp`, `main.py`, or React
    component.
-   Avoid circular imports and duplicated business rules.
-   If a different structure is already in place, preserve its
    conventions unless there is a concrete problem.

------------------------------------------------------------------------

## 4. Frontend Architecture

### 4.1 Responsibilities

The frontend is responsible for: - Editing and displaying the
topology. - Collecting simulation configuration. - Calling backend
APIs. - Showing validation errors and warnings. - Presenting engine
events as a timeline and animation. - Displaying packet/router
inspectors and metrics. - Providing pause, resume, step, reset, and
animation-speed controls. - Exporting/downloading files where
appropriate.

The frontend is **not** responsible for deciding whether a packet was
delivered, which router should forward it, or whether TTL expired.

### 4.2 Feature modules

**`features/topology/`** - Node/link palette. - Topology canvas. - Node
and link property forms. - Interface and IP configuration. - Topology
validation display. - Import/export and starter scenarios.

**`features/simulation/`** - Source/destination selectors. - Algorithm
and objective selectors. - TTL, packet size, DF, and seed controls. -
Run lifecycle controls. - Simulation request creation. - Event playback
controller.

**`features/packet-inspector/`** - Packet header fields supported by the
engine. - Current packet status and history. - Related fragment or ICMP
packet references.

**`features/router-inspector/`** - Interfaces and state. - Forwarding
table. - Current forwarding decision. - Recent event history.

**`features/metrics/`** - Route metrics. - Algorithm comparison. -
Before/after fault comparison. - Exportable experiment summaries.

**`components/`** - Reusable buttons, dialogs, dropdowns, status badges,
tooltips, tabs, and layout components.

### 4.3 State management

Keep three categories of state distinct:

1.  **Topology draft state:** current editable nodes, links, and
    configuration.
2.  **Simulation result state:** immutable run request, events, final
    packet state, and metrics returned by the backend.
3.  **Presentation state:** selected node/event, open panel, camera
    position, current playback index, and animation speed.

Editing a topology after a run must not mutate that run's recorded
result. A new simulation should create a new run ID or client-side run
identity.

### 4.4 Event playback

The frontend should render a recorded event sequence using a playback
cursor.

-   `currentEventIndex` indicates the event being displayed.
-   Pause/resume changes playback only.
-   Step advances one event.
-   Animation speed changes how quickly the cursor advances, not the
    simulated metrics.
-   Reset returns the cursor to the documented initial state.
-   Selecting an event should update the current packet/router
    inspector.
-   The same event cursor drives both 2D and 3D renderers.

Do not implement a second, independent simulation inside the animation
component.

### 4.5 Visualization boundary

Define a shared renderer-facing view model, derived from the topology
and engine events. Both 2D and 3D renderers consume this view model.

The 2D renderer is the primary interface and must work without 3D
dependencies. The 3D renderer is optional and must not own routing,
packet state, or metrics.

------------------------------------------------------------------------

## 5. Backend Architecture

### 5.1 Responsibilities

FastAPI is responsible for: - HTTP endpoints and API versioning. -
Request/response schema validation. - Topology validation before engine
execution. - Safe invocation of the C++ engine. - Enforcing timeouts and
event/output limits. - Converting engine failures into stable API
errors. - Optional run persistence and report export. - Health checks
and engine version reporting.

FastAPI should not contain a second implementation of Dijkstra, packet
forwarding, TTL decrement, or fragmentation logic.

### 5.2 Suggested modules

-   `api/`: endpoint definitions and dependency wiring.
-   `schemas/`: Pydantic request and response models.
-   `services/topology_service.py`: topology validation/orchestration.
-   `services/simulation_service.py`: run creation and execution.
-   `services/export_service.py`: JSON/CSV report generation.
-   `engine/runner.py`: safe process invocation and output parsing.
-   `main.py`: app initialization, middleware, routers, health endpoint.

Keep endpoint functions thin. Business orchestration belongs in
services, and process execution belongs in the engine adapter.

### 5.3 Engine invocation

For the first version, prefer a **C++ executable with JSON
input/output**. It is easier to build, debug, and test independently
than a native binding. A pybind11 or other native binding can be
considered later if performance or deployment needs justify it.

Recommended flow:

1.  FastAPI validates the request against its schema.
2.  The backend writes or pipes a validated JSON request to the engine.
3.  The backend invokes a fixed, configured executable using a safe
    process API; do not build a shell command from user input.
4.  The engine parses the request, validates engine-specific invariants,
    runs the simulation, and emits one versioned JSON result.
5.  The backend checks exit status, output size, timeout, and JSON
    schema.
6.  The backend returns a typed response or a stable error.

Prefer stdin/stdout pipes over temporary files when practical. If
temporary files are necessary, use securely generated paths, bounded
file sizes, and cleanup in all exit paths.

Do not accept an executable path from the HTTP request. Configure the
engine path through server configuration or environment variables.

### 5.4 Long-running runs

Start with synchronous request/response for small deterministic
examples. Introduce a background job and WebSocket or Server-Sent Events
only when runs genuinely need streaming or can exceed normal request
timeouts.

If streaming is added: - Define event framing and ordering. - Include
run IDs and sequence numbers. - Handle client disconnects. - Support
cancellation. - Do not let partial streamed events contradict the final
result. - Keep a bounded event buffer.

### 5.5 API error model

Return consistent errors with: - Stable machine-readable `code`. -
Human-readable `message`. - Optional field/path information. - Optional
structured details safe to expose.

Distinguish validation failure, no feasible route, simulation drop,
engine failure, timeout, and server error. A packet drop is a valid
simulation outcome, not necessarily an HTTP error.

------------------------------------------------------------------------

## 6. C++ Engine Architecture

### 6.1 Layered modules

**Model layer** - IDs, nodes, interfaces, links, IP/subnet values,
packet configuration, and topology. - Input validation and normalized
topology representation.

**Routing layer** - Dijkstra. - BFS. - Bellman--Ford. - Widest path. -
Objective-specific route evaluation and constraints. - Route result and
algorithm counters.

**Forwarding layer** - Destination-prefix lookup or the documented
simplified forwarding-table model. - Next-hop and outgoing-interface
decision. - Local subnet/default gateway decision. - Per-router
forwarding records.

**Packet layer** - Packet fields and lifecycle. - IPv4 TTL. - Supported
header and fragmentation fields. - Related packet references for
simulated ICMP.

**Simulation layer** - Discrete sequence of transmissions and router
processing events. - Link status, delay, packet loss, and configured
load model. - Simulation limits and deterministic random generator. -
Delivery/drop outcome.

**Event layer** - Typed event model. - Event sequence numbers. -
Simulation time. - Node/link/packet references. - Human-readable
explanation plus structured details.

**Metrics layer** - Route cost, hop counts, estimated delay, bottleneck
bandwidth, loss estimate, MTU status, and algorithm runtime. - Clear
separation between measured code runtime and simulated network time.

**Serialization layer** - Versioned JSON parser/serializer. - Validation
of required fields and enum values. - Stable error messages.

### 6.2 Dependency direction

Recommended dependency direction:

`Model → Routing / Forwarding → Packet / Simulation → Events / Metrics → Serialization`

The exact direction can vary where needed, but avoid cyclic
dependencies. For example, the routing module should not import the UI
event renderer or depend on FastAPI.

### 6.3 Core type boundaries

Prefer explicit types over loosely structured strings: - `NodeId` -
`InterfaceId` - `LinkId` - `PacketId` - `IPv4Address` - `PrefixLength` -
`BandwidthMbps` - `DelayMilliseconds` - `PacketSizeBytes` -
`SimulationTimeMilliseconds`

A lightweight wrapper type or strong naming convention is sufficient; do
not over-engineer the type system before the core model is stable.

### 6.4 Routing algorithm contracts

Every routing algorithm should accept a validated graph, source,
destination, and relevant objective parameters, and return a structured
result.

A route result should distinguish: - `reachable` - ordered node IDs -
ordered link IDs - objective value - supporting metrics -
explored-node/relaxation counters where meaningful - algorithm runtime -
reason for failure, if unreachable or infeasible

Do not overload one numeric field to mean cost for one algorithm and
bandwidth for another without identifying the objective and unit.

### 6.5 Determinism

-   Use stable node/link ordering when tie-breaking is needed.
-   Document tie-breaking rules.
-   Use a seeded pseudorandom generator for probabilistic loss.
-   Avoid dependence on unordered-map iteration order for user-visible
    route choices.
-   Record the seed and engine version in each run result.

### 6.6 C++ process contract

The executable should: - Read a versioned request from stdin or a
documented input path. - Write exactly one versioned JSON result to
stdout. - Write diagnostics to stderr. - Use nonzero exit codes for
malformed input or fatal engine failures. - Avoid mixing human debug
logs into stdout. - Enforce limits for topology size, packet count, and
event count. - Never execute commands contained in the input.

------------------------------------------------------------------------

## 7. Shared Contracts and Serialization

### 7.1 Schema ownership

Define a versioned JSON schema shared across frontend, backend, and
engine. Keep these synchronized: - TypeScript types. - Pydantic
models. - C++ parser/serializer expectations. - Example topology and
simulation files. - API documentation.

Where possible, maintain canonical JSON Schema files in `schemas/` and
validate examples against them.

### 7.2 Versioning

Every topology and simulation request/result should carry a schema
version, for example:

``` json
{
  "schemaVersion": "1.0"
}
```

The exact versioning format can be chosen by the implementation, but it
must be consistent. Reject unsupported major versions with a clear
message. Add optional fields compatibly where possible.

### 7.3 Simulation request example

This is illustrative; align exact fields with the implemented schema.

``` json
{
  "schemaVersion": "1.0",
  "topology": {
    "id": "example-multihop",
    "nodes": [],
    "links": []
  },
  "sourceNodeId": "host-a",
  "destinationNodeId": "cloud-host",
  "algorithm": "DIJKSTRA",
  "objective": "COST",
  "constraints": {
    "minBandwidthMbps": null,
    "maxLossProbability": null,
    "requiredMtuBytes": null
  },
  "packet": {
    "initialTtl": 8,
    "sizeBytes": 1200,
    "dontFragment": false,
    "protocol": "ICMP"
  },
  "randomSeed": 12345
}
```

Do not assume that this example is the final schema. Once implemented,
document and test the exact request format.

### 7.4 Event envelope

Each event should have a stable common envelope, such as:

``` json
{
  "eventId": "evt-0007",
  "sequence": 7,
  "simulationTimeMs": 12.5,
  "type": "ROUTER_FORWARD",
  "packetId": "pkt-001",
  "nodeId": "router-2",
  "linkId": "link-r2-r3",
  "message": "Router selected the outgoing interface and forwarded the packet.",
  "details": {
    "ttlBefore": 6,
    "ttlAfter": 5,
    "nextHopNodeId": "router-3"
  }
}
```

Use event-specific typed details where practical. Avoid placing all
fields into an unstructured message string.

### 7.5 Event ordering and time

-   `sequence` determines authoritative event order.
-   `simulationTimeMs` represents modeled simulation time, not the time
    required to render the UI.
-   Events at the same simulation time are still ordered by sequence.
-   Wall-clock engine runtime is reported separately.
-   The frontend should not reorder events based on animation timing.

------------------------------------------------------------------------

## 8. Data Ownership and State Transitions

### 8.1 Topology ownership

The frontend owns the editable draft while the user is editing. When a
run starts, the frontend submits a snapshot. The backend validates and
passes that snapshot to the engine. The run result stores or returns the
topology version/hash and parameters used.

### 8.2 Run lifecycle

Suggested run states: - `CREATED` - `VALIDATING` - `RUNNING` -
`COMPLETED` - `FAILED` - `CANCELLED`

A simulation that completes with a dropped packet is still `COMPLETED`;
the packet's delivery state is `DROPPED`. Do not conflate application
execution status with packet outcome.

### 8.3 Packet lifecycle

Suggested packet states: - `CREATED` - `QUEUED` - `IN_TRANSIT` -
`AT_NODE` - `FORWARDED` - `DELIVERED` - `DROPPED`

Add fragment-specific states only where fragmentation is implemented.
Every state transition must be represented consistently in the engine's
internal model and event output.

### 8.4 Reset and rerun

Resetting playback in the frontend should reset the view cursor.
Starting a fresh simulation should create a fresh run based on the
current topology snapshot and parameters. If the user changes topology
during a run, do not mutate the existing run; ask them to rerun to see
the new configuration.

------------------------------------------------------------------------

## 9. Performance and Resource Limits

Initial limits should be configurable and documented. Begin with
conservative values, then benchmark before raising them.

Consider limits for: - Maximum nodes and links in a topology. - Maximum
interfaces per node. - Maximum packet size and fragment count. - Maximum
events per run. - Maximum engine execution time. - Maximum request and
response body sizes. - Maximum retained run history in local/server
storage.

The UI should warn before a run that exceeds limits. The backend and
engine must enforce limits independently; frontend validation alone is
not sufficient.

Do not claim real-time performance guarantees without measurements.
Record representative benchmark results in documentation when available.

------------------------------------------------------------------------

## 10. Security and Operational Boundaries

-   The default application is a simulation and must not transmit real
    packets.
-   Do not require administrator/root privileges for normal operation.
-   Do not create real AWS resources as part of a demo.
-   Never store secrets in topology files or committed configuration.
-   Do not expose arbitrary filesystem paths or shell command execution
    through the API.
-   Use safe process APIs to invoke the engine.
-   Validate every input at the API boundary and again in the engine
    where necessary.
-   Apply request and output limits to protect against excessive memory
    use.
-   If optional real-system validation is later added, keep it isolated,
    opt-in, documented, and separate from the default simulator.

------------------------------------------------------------------------

## 11. Testing Boundaries

Test each layer independently before relying on end-to-end tests.

### C++ tests

-   Algorithm correctness and edge cases.
-   Forwarding and next-hop decisions.
-   TTL and packet lifecycle.
-   Loss determinism and link failures.
-   MTU and fragmentation behavior.
-   Serialization round trips and malformed input.

### Backend tests

-   Request schema validation.
-   Topology validation.
-   Safe engine invocation.
-   Timeout and nonzero-exit handling.
-   Invalid JSON output handling.
-   Stable error mapping.
-   Endpoint integration.

### Frontend tests

-   Form validation and request payload construction.
-   Loading/error/success states.
-   Event ordering and playback controls.
-   Inspectors reflect selected events.
-   Renderer consumes engine-backed state.
-   Switching 2D/3D does not alter the result.

### Integration tests

-   Use the same example files across C++, backend, and frontend.
-   Verify a complete request-to-result path.
-   Check that the frontend displays the same route and delivery outcome
    as the engine.
-   Include a negative test for a route that exists but fails due to TTL
    or a disabled link.

------------------------------------------------------------------------

## 12. Logging, Diagnostics and Observability

-   Send structured application logs to stderr or the backend logging
    system, not into the engine's JSON stdout.
-   Include a run ID in logs when available.
-   Avoid logging secrets or unnecessarily large packet payloads.
-   Record engine exit code, duration, and sanitized error details.
-   Show concise user-facing errors and keep technical details available
    for debugging.
-   Provide a health endpoint that reports API availability and whether
    the engine can be invoked.
-   Do not expose local file paths, environment secrets, or internal
    stack traces to normal users.

------------------------------------------------------------------------

## 13. Build and Development Workflow

### 13.1 Development sequence

1.  Inspect repository and available toolchains.
2.  Build and test the standalone C++ algorithms.
3.  Implement topology and forwarding.
4.  Implement packet lifecycle and event generation.
5.  Define and test JSON contracts.
6.  Integrate FastAPI with the engine.
7.  Build the React 2D interface around actual API results.
8.  Add the six routing criteria and reports.
9.  Add failure injection and MTU/fragmentation behavior.
10. Add the 3D renderer using the shared view model.
11. Add documentation and repeatable examples.
12. Run the complete test suite and record actual outcomes.

### 13.2 Required completion report

At the end of each milestone, the coding assistant must state: - What
was implemented. - Files created or modified. - Commands and tests
actually run. - Test/build results, including failures. - Known
limitations. - Next recommended task.

Never claim a test passed if it was not run. Never claim an end-to-end
feature works if it is only represented by mock data.

------------------------------------------------------------------------

## 14. Architecture Decision Rules

When choosing between implementation options:

1.  Prefer the simplest design that preserves correct networking
    semantics.
2.  Keep modules small and testable.
3.  Avoid introducing databases, queues, WebSockets, native bindings, or
    cloud infrastructure before they are needed.
4.  Use a C++ executable with JSON as the initial engine boundary unless
    the existing repository gives a strong reason otherwise.
5.  Use synchronous API calls for small simulations; add streaming only
    when needed.
6.  Build the 2D experience before 3D.
7.  Make random behavior reproducible in tests.
8.  Document any deviation from this architecture.
9.  Preserve a working repository instead of performing a wholesale
    rewrite.
10. Let measured requirements---not speculative scale---drive
    optimization.

------------------------------------------------------------------------

## 15. Definition of Architecture Done

The architecture is considered implemented---not merely
documented---when:

-   [ ] The frontend has a typed API client and separate
    topology/simulation state.
-   [ ] The backend validates inputs and invokes the engine safely.
-   [ ] The C++ engine can run independently from the web UI.
-   [ ] The engine emits versioned, structured JSON.
-   [ ] Route selection, forwarding, packet simulation, and rendering
    are separated.
-   [ ] Events drive the timeline and both renderers.
-   [ ] Tests cover the boundaries between the modules.
-   [ ] Engine failures and invalid inputs are handled cleanly.
-   [ ] Build/run commands are documented and reproducible.
-   [ ] The actual implementation and its limitations are reflected in
    the README and this document.

**Core architectural rule:** the engine decides what happens; the
backend validates and coordinates; the frontend makes those results
understandable.

# DESIGN.md — NetTrace 3D Visual and Interaction Design

**Purpose:** Define how NetTrace 3D looks, reads, and behaves, so an AI coding assistant (Google Antigravity) can build a consistent, accessible, honest interface.
**Companion docs:** `PROJECT.md` (§10, §11, §18 UI scope), `ARCHITECTURE.md` (§4 frontend), `NETWORKING_RULES.md` (§23 teaching requirements), `TASKS.md` (M5, M6, M7, M8).
**Precedence:** If this file conflicts with `NETWORKING_RULES.md` about what the interface may *claim*, the rules file wins. This file owns look, layout, motion, copy, and interaction details.
**Status:** Design specification. Nothing here is built until it passes the checks in section 17.

---

## 1. Design concept

### 1.1 Subject, audience, job

- **Subject:** an interactive network laboratory. Packets, routers, TTL, MTU, forwarding decisions, failures.
- **Audience:** students, instructors, and developers who know the vocabulary but cannot see it working.
- **Primary job of the interface:** let a person ask "why did that happen?" about any hop and get an answer taken from engine data.

### 1.2 Visual idea: the instrument bench

NetTrace should feel like a measurement instrument on a lab bench, not a game and not a generic dashboard. The vernacular comes from traceroute output, packet captures, rack diagrams, and oscilloscope traces: precise, quiet, information-first.

**The one memorable element is the TTL ribbon** (section 9). Every packet carries a visible TTL gauge, and a ribbon beneath the canvas shows TTL going *in* and *out* at every router along the path. TTL is the least intuitive concept for learners and the one the spec stresses most, so it earns the boldness. Everything else stays restrained.

### 1.3 Principles

1. **Engine truth, never theatre.** The interface only shows what an engine event says happened. No success animation before `PACKET_DELIVERED`. No invented route.
2. **Explain the decision, not just the outcome.** Every selection answers What, Where, Why, Evidence, What next.
3. **Shape plus text plus color.** State is never carried by color alone.
4. **Honest about limits.** Unmodeled things say "Not modeled". Estimates say "Estimated". Observations say "Observed". Simulation is labeled as simulation.
5. **Quiet by default.** Boldness is spent on the TTL ribbon and the packet marker. Panels, forms, and tables stay calm.
6. **Playback is a cursor.** The user moves through a recorded event list. Nothing is simulated in the browser.

### 1.4 What to avoid (deliberate)

- Neon glow, bloom, animated gradients, particle trails, "hacker terminal" styling.
- Identical rounded cards stacked everywhere with the same shadow.
- Tracked-out ALL-CAPS eyebrow labels over every heading; middle-dot meta strings; "→" appended to buttons.
- Monospace everywhere. Mono is reserved for values that must align or be copied (section 4.2).
- A hero full of big-number stat tiles. The dashboard opens with the starter scenarios as questions (section 11).
- Fake charts or placeholder data presented as results (spec principle 8).

---

## 2. Design tokens

Implement as CSS custom properties in `frontend/src/styles/tokens.css`, mirrored in a typed `tokens.ts`. Components never hard-code hex values.

### 2.1 Color (dark theme, default)

Core palette (named, 6 values):

| Token | Hex | Role |
|---|---|---|
| `--nt-bg` | `#0E1726` | App and canvas background (deep navy) |
| `--nt-surface-1` | `#141F33` | Panels, top bar |
| `--nt-surface-2` | `#1B2A44` | Inputs, raised rows, menus |
| `--nt-line` | `#2E4263` | Borders and dividers |
| `--nt-text` | `#E8EEF8` | Primary text |
| `--nt-accent` | `#3CC8E8` | Selection, focus, active route, primary action |

Supporting tokens:

| Token | Hex | Role |
|---|---|---|
| `--nt-surface-3` | `#243652` | Hover and selected row |
| `--nt-line-strong` | `#4A6288` | Control borders, link lines at rest |
| `--nt-text-muted` | `#A3B2C9` | Secondary text |
| `--nt-text-faint` | `#8493AB` | Tertiary text (still must meet 4.5:1) |
| `--nt-accent-strong` | `#1FA8CB` | Pressed primary, accent on light surfaces |
| `--nt-on-accent` | `#06202B` | Text on accent fill |
| `--nt-grid-dot` | `#1D2C47` | Canvas dot grid |

Semantic tokens (each paired with an icon and a text label wherever used):

| Token | Hex | Meaning | Paired icon | Paired text |
|---|---|---|---|---|
| `--nt-ok` | `#3DDC97` | Delivered, valid | check in circle | "Delivered" |
| `--nt-bad` | `#FF6B6B` | Dropped, error | cross in circle | "Dropped" |
| `--nt-warn` | `#F5B84B` | Warning, TTL low, stale | triangle | "Warning" |
| `--nt-expired` | `#F5B84B` + clock | TTL expired | clock | "TTL expired" |
| `--nt-down` | `#7F8FA8` | Inactive / link down | slash | "Down" |
| `--nt-icmp` | `#F58AC6` | ICMP response packet | diamond marker | "ICMP" |
| `--nt-frag` | `#A58BFF` | IP fragments | small-square cluster | "Fragment" |

Rules:

- [ ] All text/background pairs meet **WCAG 2.2 AA** (4.5:1 normal text, 3:1 large text and UI graphics). Add `scripts/check-contrast.*` that computes ratios from `tokens.ts` and fails CI below threshold. Do not trust the hex values above blindly; the script is the authority.
- [ ] `--nt-down` and `--nt-text-faint` are never the only carrier of meaning.
- [ ] No pure black or pure white backgrounds.
- [ ] Light theme: `planned`. Tokens are structured so a second set can be added, but the first release ships dark only. Do not ship a half-themed light mode.
- [ ] High-contrast mode: honor `prefers-contrast: more` by raising `--nt-line-strong` and text-faint to full `--nt-text`.

### 2.2 Typography

Two families with clearly different jobs:

| Role | Family | Fallback stack | Used for |
|---|---|---|---|
| Interface | **Atkinson Hyperlegible Next** | `system-ui, "Segoe UI", Roboto, sans-serif` | All UI text, headings, forms, messages |
| Values | **Atkinson Hyperlegible Mono** | `ui-monospace, "SF Mono", Menlo, Consolas, monospace` | IP addresses, prefixes, MACs, packet IDs, reason codes, TTL numbers, byte counts |

Why: Atkinson Hyperlegible was designed to keep similar characters distinct (`0/O`, `1/l/I`), which matters when reading `10.0.0.1` and `l` vs `1` in IDs, and it supports the accessibility goal. Self-host through `@fontsource` packages (no runtime request to a font CDN, so the app works offline). If the families are unavailable, the fallbacks must not break layout; test with fallbacks forced.

Scale (base 14 px for dense tool UI; never below 12 px):

| Token | Size / line height | Weight | Use |
|---|---|---|---|
| `--nt-text-xs` | 12 / 16 | 400 | Captions, axis labels, badges |
| `--nt-text-sm` | 13 / 18 | 400 | Table rows, helper text |
| `--nt-text-md` | 14 / 20 | 400 | Default UI text and form fields |
| `--nt-text-lg` | 16 / 24 | 600 | Panel titles |
| `--nt-text-xl` | 20 / 28 | 600 | Page and dialog titles |
| `--nt-text-2xl` | 28 / 36 | 700 | Dashboard heading only |

Rules:

- [ ] Sentence case everywhere. No ALL CAPS labels. No letter-spacing tricks.
- [ ] Use `font-variant-numeric: tabular-nums` for TTL, time, byte counts, and metric values so columns align.
- [ ] Help text and explanations stay under about 70 characters per line (`max-width: 70ch`).
- [ ] Mono is applied by a `<Value>` component, not scattered `font-family` rules.

### 2.3 Spacing, radius, elevation

- **Spacing** (4 px base): `4, 8, 12, 16, 24, 32, 48`. Panels use 12 px inner padding; dense tables 8 px.
- **Radius (varies by role, not one value everywhere):** input 4 px, panel 6 px, dialog 10 px, node shapes 12 px, chips and badges fully round (999 px).
- **Elevation:** panels are separated by border and surface step, not shadow. Only floating layers (menus, popovers, dialogs, drawers) use a shadow: `0 8px 24px rgba(3, 8, 18, 0.55)`.
- **Focus ring:** 2 px `--nt-accent` outline with 2 px offset, always visible on keyboard focus, never removed.
- **Hit targets:** at least 24 × 24 px (WCAG 2.2); 32 px minimum for primary controls; 40 px on touch layouts.

### 2.4 Motion tokens

| Token | Value | Use |
|---|---|---|
| `--nt-dur-fast` | 120 ms | Hover, toggles |
| `--nt-dur-base` | 200 ms | Panel open/close, tab change |
| `--nt-ease` | `cubic-bezier(0.2, 0.7, 0.2, 1)` | All UI easing |
| `--nt-hop-base` | 700 ms | Packet travel per link at 1× speed |

Rules are in section 12.

---

## 3. Application layout

### 3.1 Workspace (desktop, ≥ 1280 px)

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Top bar: topology name · Save · Import · Export · [Validate ✓/!] · Run status │ 48px
├──────────┬─────────────────────────────────────────────────┬─────────────────┤
│ Left     │                                                 │ Right           │
│ 280px    │              Canvas (2D / 3D toggle)            │ 360px           │
│          │                                                 │                 │
│ Palette  │   hosts · switches · routers · cloud endpoint   │ Tabs:           │
│ ─────    │   links with state, route highlight, packet     │  Configure      │
│ Checks   │                                                 │  Inspect        │
│ (valid-  │   ┌ Playback: ⏮ ▶ ⏭  speed  ·  event 7 / 23 ┐   │  Compare        │
│  ation)  │   └─────────────────────────────────────────┘   │  Faults         │
├──────────┴─────────────────────────────────────────────────┴─────────────────┤
│ Bottom panel (resizable 160 px – 50% height): [TTL ribbon] [Timeline] [Metrics] │
└──────────────────────────────────────────────────────────────────────────────┘
```

- **Top bar** (48 px): topology name (editable inline), file actions, validation status chip, run status chip (idle, running, completed, failed), reset.
- **Left panel** (280 px, collapsible to a 48 px icon rail): node palette on top, validation list below. Selecting an issue selects the offending element on the canvas.
- **Canvas**: fills remaining space. The 2D/3D toggle and zoom/fit controls float at its top-right. Playback controls dock at its bottom center.
- **Right panel** (360 px, resizable 320–480): four tabs.
  - *Configure*: source, destination, scenario, algorithm, objective, constraints, packet settings, seed, Run button.
  - *Inspect*: Packet, Router, Link, and Explain sub-views for the current selection.
  - *Compare*: saved runs, inputs diff, metrics diff (M6).
  - *Faults*: fault list and editor (M7).
- **Bottom panel**: three tabs, TTL ribbon first (default), Timeline, Metrics. Collapsible; the run button and playback controls remain reachable when collapsed.

### 3.2 Tablet (768–1279 px)

- Left panel becomes an icon rail that opens as a drawer over the canvas.
- Right panel becomes a drawer opened from a "Panels" button; Run stays in the top bar.
- Bottom panel stays docked at 40% height.

### 3.3 Mobile (< 768 px)

Single column with a bottom tab bar: **Canvas · Run · Inspect · Events**.

- A sticky bar at the top of Canvas holds Run, Step, Pause, Reset. These are reachable on every tab.
- Editing a topology on a phone is `limited`: allow view, select, inspect, import/export, and run. Full link-drawing is a desktop/tablet feature; show a banner explaining that.
- 3D is offered only if WebGL is available and node count is small (section 14).

### 3.4 Pages

| Page | Route | Purpose |
|---|---|---|
| Dashboard | `/` | Start from a question or a blank topology |
| Workspace | `/lab/:topologyId` | Everything else |
| Help | drawer, any page | Glossary and model notes |

Dialogs: Import topology, Export run, Settings (default animation speed, reduced motion override, display units), Confirm destructive action.

---

## 4. Node and link visual language

### 4.1 Node silhouettes

Each node type has a **distinct outline**, so type reads at a glance even in monochrome. All are 56 × 56 px at 100% zoom, with the name below (14 px) and an optional IP line (mono, 12 px).

| Node | Silhouette | Notes |
|---|---|---|
| Host | Rounded rectangle with a small stand (screen) | Source/destination role shown by a corner tag: "Source" or "Destination" |
| Router | Circle with four short arrows pointing outward | Interfaces appear as small docked ports on its edge |
| Switch | Wide flat rectangle with a row of port ticks | No IP label (Layer 2) |
| Cloud endpoint | Cloud outline with a **dashed** border | Always carries a visible "Simulated" tag |

Icons are custom SVG (not an icon-font) with a 2 px stroke in `--nt-text`, fill `--nt-surface-2`. UI icons elsewhere use `lucide-react`.

### 4.2 Labels and values

- Name: interface font, 14 px, truncated with a tooltip at 18 characters.
- IP/prefix, MAC, interface names: mono, 12 px.
- Label visibility is a toggle: *Names*, *Names + IPs*, *Names + IPs + interfaces*. Default: Names + IPs.
- At low zoom, collapse to names only; hide interface labels first. Labels never overlap: apply collision nudging and prefer hiding to overlap.

### 4.3 Node states

| State | Visual | Text/badge |
|---|---|---|
| Normal | Solid outline | none |
| Selected | 2 px `--nt-accent` ring + resize handles for drag | none |
| Hovered | Ring at 50% accent | tooltip with role and primary IP |
| Inactive | Dashed outline, 60% opacity icon | "Down" badge with slash icon |
| Current (during playback) | Accent ring + soft inner fill | "Now" tag only while it is the cursor node |
| Delivered (terminal) | Check badge on destination | "Delivered" |
| Dropped (terminal) | Cross badge on the node or link where it happened | "Dropped" + reason code |
| TTL expired (terminal) | Clock badge on the router | "TTL expired" |
| Has warning | Small triangle badge | tooltip lists warnings |

Only one terminal badge per packet appears, and only after the cursor has reached the terminal event.

### 4.4 Links

| Property | Encoding |
|---|---|
| Up | Solid 2 px line, `--nt-line-strong` |
| Down | Dashed line with a "×" marker at the midpoint, plus "Down" label on hover/select |
| Directed | Arrowhead at the target end; bidirectional has none |
| Selected | 3 px accent line |
| Planned route (path-planning result) | Dashed accent line, 3 px |
| Followed route (packets actually crossed it) | Solid accent line, 4 px, drawn up to the cursor |
| Lossy (loss > 0) | Small percent tag on the line (text), e.g. "2%" |
| Fault active | Amber dotted underlay with a fault icon |
| Parallel links | Offset curves so each is separately clickable |

- Optional toggle **"Show bandwidth as thickness"** (off by default). When on, a legend shows the Mbps-to-thickness scale. Never use thickness silently.
- When the planned route and the followed route diverge, both are drawn so the difference is visible, and a banner in the result summary says so.

### 4.5 Packet markers

| Packet kind | Shape | Color token |
|---|---|---|
| Original IP packet | Circle | `--nt-accent` |
| ICMP response | Diamond | `--nt-icmp` |
| IP fragment | Small square (clustered, labeled "1/3") | `--nt-frag` |

- **TTL pips:** a ring of segmented pips around the marker, one per initial TTL step up to 8; above 8 the ring shows a proportion with the number in the marker tooltip. Pips consumed show as hollow. The exact TTL always appears as text in the packet label and the inspector.
- Marker has a text label (`pkt-0001`) shown on hover, selection, and at 100%+ zoom.
- During a link traversal the marker sits on the link; at a router it sits on the router with the lookup beat (section 12.2).

---

## 5. Signature element: the TTL ribbon

A horizontal strip in the bottom panel (default tab) that lays the packet's journey out as a ruler of hops.

```text
 Source        Switch    R1                    R2                    R3                 Cloud
 Host A  ─────  ·  ─────  [ TTL 3 → 2 ] ─────  [ TTL 2 → 1 ] ─────  [ TTL 1 ✕ ]
 192.168.1.10             next hop 10.0.0.2      next hop 10.1.0.2     expired here
 ●●●                      ●●○                    ●○○                   ○○○
                                                                       ▲ cursor
```

- **Cells:** one per router (large), thin tick for each switch pass (no TTL change), end cap for the terminal event.
- **Each router cell shows:** router name, TTL in, TTL out (or "expired"), selected next hop, and the matched prefix (e.g. `10.0.0.0/24`).
- **Pips** repeat the marker's TTL gauge under each cell, so the depletion across the path is visible.
- **Cursor** follows `currentEventIndex`; cells after the cursor are drawn faint and labeled "not reached yet" for assistive tech. Clicking a cell moves the cursor to that router's receive event.
- **Terminal cell:** delivered (check + "Delivered"), expired (clock + "TTL expired at R3"), dropped (cross + reason code).
- **ICMP and fragments:** a second lane under the original packet's lane for each related packet, labeled with its own ID and parent ("pkt-0002 ICMP Time Exceeded, reply to pkt-0001"). The ICMP lane starts at the router that generated it and runs back toward the source, making "this is a separate packet on its own return path" visible.
- **Honesty rules:** cells come only from `TTL_DECREMENTED`, `TTL_EXPIRED`, `ROUTE_SELECTED`, and forwarding events. If an event type is missing, the cell shows "Not modeled" instead of a guessed value.
- **Narrow screens:** the ribbon scrolls horizontally inside its own container; it never causes page-level horizontal scroll.
- **Path-planning-only runs:** the ribbon shows the planned hops with metrics per hop and the heading "Planned path (no packet sent)".

---

## 6. Canvas interaction (2D)

### 6.1 Mouse and touch

| Action | Behavior |
|---|---|
| Click node/link | Select; open Inspect tab |
| Drag node | Move; position saved in topology (draft only) |
| Drag from a node's port to another | Create link (editor mode) |
| Scroll / pinch | Zoom, anchored at pointer |
| Drag background | Pan |
| Double-click node | Open its property form |
| Shift-click | Add to selection |
| Marquee drag (Shift + drag background) | Multi-select |
| Right-click / long-press | Context menu: Edit, Duplicate, Delete, Set as source, Set as destination, Disable/enable |

### 6.2 Keyboard (canvas focused)

| Key | Action |
|---|---|
| Tab / Shift-Tab | Move between toolbar controls and the canvas |
| Arrow keys | Move focus to the nearest neighbor node in that direction |
| `Enter` | Open inspector for focused element |
| `Space` (on canvas element) | Toggle selection |
| `Delete` | Delete selected element (with confirmation if linked) |
| `F` | Fit to view |
| `+` / `-` | Zoom |
| `S` / `D` | Set focused host as source / destination |
| `Esc` | Clear selection, close menus |

Global playback shortcuts (when focus is not in a text field):

| Key | Action |
|---|---|
| `Space` | Play / pause |
| `→` / `←` | Step forward / back one event |
| `Home` / `End` | Jump to first / last event |
| `R` | Reset playback |
| `1`–`5` | Speed 0.25×, 0.5×, 1×, 2×, 4× |

Shortcuts are listed in Help and shown in tooltips (`<Kbd>` component). They never fire inside inputs.

### 6.3 Editing rules

- Topology remains editable at all times. If the draft differs from the snapshot used by the displayed result, a banner appears: **"Topology changed since this run. Rerun to see the effect."** with a Rerun button. The displayed result is not altered.
- Deleting a node that has links asks for confirmation and lists the links that will be removed.
- Invalid link attempts (interface already attached, same-node link) are rejected inline with the reason, not silently.
- Undo/redo: `planned`. Show the controls disabled with the tooltip "Planned, not available yet", or omit them. Never show a working-looking button that does nothing.

---

## 7. Forms and configuration controls

### 7.1 Field pattern

```text
Label (sentence case)                       [?] help
┌──────────────────────────────┬──────┐
│ value                        │ Mbps │   unit suffix inside the field
└──────────────────────────────┴──────┘
Helper text (optional, 1 line)
Error text (replaces helper): what is wrong and how to fix it
```

- Units are always visible as a suffix: Mbps, ms, bytes, %, hops. Internally stored as bps/ms/bytes/probability (D-02); conversion happens at the form boundary only.
- Validation runs on blur and on submit, with the error linked via `aria-describedby`. Errors never rely on color alone (icon + text).
- Inputs keep invalid values visible so users can fix them; they are not silently clamped.
- Numeric fields show min/max in the helper text (e.g. "1–255").

### 7.2 Configure tab (top to bottom)

1. **Endpoints:** Source and Destination selects (name + IP in mono).
2. **Scenario** (guided label only; selects a starter topology or a checklist of what the topology should contain).
3. **Mode:** segmented control: *Simulate a packet* (default) / *Plan a path only*.
4. **Algorithm:** BFS, Dijkstra, Bellman–Ford, Widest path. A one-line description appears under the select ("Fewest links. Ignores cost.").
5. **Objective (six criteria):** a six-option single-choice list, each with a one-line definition and a provenance tag: Cost, Packet-loss rate, Available bandwidth, Congestion / utilization, MTU compatibility, Router processing load. Invalid algorithm/objective pairs are disabled with a reason tooltip.
6. **Hard constraints:** min bandwidth, max loss, MTU feasibility, exclude inactive (always on). Collapsed by default. Labeled "Must satisfy" to separate it from the objective ("Optimize for").
7. **Packet:** protocol, initial TTL (1–255), packet size (20–65535 bytes), Don't Fragment (checkbox), ICMP errors enabled (checkbox), optional payload label.
8. **Reproducibility:** seed field with "Random" and "Use fixed" options. After a random run, the returned seed is shown with a copy button: "This run used seed 48213. Use it again to reproduce the same losses."
9. **Run button:** primary, full width, label **Run simulation** (or **Plan path** in path-only mode). Disabled with an inline reason list when inputs are invalid; the reasons are links to the offending fields.

Separate "Optimize for" and "Must satisfy" visually with a divider and different headings. Never merge them into one weighted slider.

### 7.3 Component states checklist

Every interactive component implements: default, hover, focus-visible, active/pressed, disabled (with reason), loading (where async), error, and read-only.

---

## 8. Inspectors

The Inspect tab follows the **cursor event** by default (a "Follow playback" toggle, on). Selecting an element or an event pins the inspector; a "Back to playback" chip unpins.

### 8.1 Packet inspector

Sections (collapsible, state remembered):

1. **Summary:** packet ID, kind (Original / ICMP / Fragment), status chip (icon + text), parent link ("Reply to pkt-0001", clickable).
2. **IP header** (only fields the engine models): source, destination, protocol, TTL (before → after at the cursor), identification, DF, MF, fragment offset (in 8-byte units with the byte offset in parentheses), total length. Checksum row shows **Not modeled**.
3. **Lengths:** three separate rows, each labeled: *Payload*, *IP total length*, *Frame length* (frame row shows "Not modeled" if L2 sizes are not simulated). This prevents confusing payload with total length.
4. **Link layer:** incoming and outgoing frame source/destination MAC, interfaces, EtherType, with a note "MAC addresses change on every routed link. IP addresses do not."
5. **Location:** current node, next hop, incoming/outgoing interface.
6. **Hop history:** compact list, click to move cursor.
7. **Outcome:** reason code (mono), plain explanation, responsible node/link.

Copyable values (IP, MAC, packet ID) use `<CopyableValue>`: click copies, announces "Copied" via `aria-live`.

### 8.2 Router inspector

1. Name, state (Up/Down), forwarding enabled, processing delay ("Not modeled" when null).
2. **Interfaces table:** name, IPv4/prefix, MAC, MTU, state.
3. **Forwarding table:** prefix, next hop, interface, source (connected / static / default / policy), preference, metric. The row matched by the *current* lookup is highlighted and labeled "Matched (longest prefix)". Rows considered but rejected show a muted "Rejected: <reason>".
4. **Current decision:** one sentence built from the engine's decision record.
5. **Recent events:** last 10 events at this router, click to jump.

### 8.3 Link inspector

State, directionality, endpoints, cost, bandwidth (capacity and available), propagation delay, loss (configured), MTU, queue ("Not modeled" if absent), and **observed** crossings in this run (count and drops), kept separate from configured values.

### 8.4 Explain panel (What / Where / Why / Evidence / What next)

A fixed five-part layout for the selected event, per `NETWORKING_RULES.md` §23:

```text
What happened   Router R2 selected 10.1.0.0/24 and forwarded the packet.
Where           R2, interface eth1 → link R2–R3
Why             Longest-prefix match: /24 is more specific than the default route /0.
Evidence        dst 10.1.0.20 · matched 10.1.0.0/24 · next hop 10.1.0.2 · TTL 2 → 1
What next       Forwarded to R3 (transmit started at t = 4.2 ms)
```

- Text comes from the engine's `message` and `details`; the UI only lays it out. The Evidence row renders structured `details` as mono key/value pairs.
- If an event has no explanation fields, show "Explanation not provided by the engine for this event." Never write one on the client.
- "What to try" (only on failures): a short static hint keyed by `reasonCode` (section 16.3). It is advice about configuration, not a claim about the outcome.

---

## 9. Timeline, metrics, and results

### 9.1 Event timeline (bottom panel tab)

A table, not a decorative list:

| Column | Content |
|---|---|
| # | Sequence number |
| Time | `timeMs` with unit (mono, tabular) |
| Layer | Text chip: Link, Network, Transport, Application, Physical (outlined, no fill color coding) |
| Where | Node and/or link name |
| Event | Event type in plain words ("TTL decremented"), code on hover |
| Detail | Engine `message` (truncated, expands on select) |

Behavior:

- Rows are in `sequence` order, always. Filtering never reorders.
- **Filters:** by packet (original / ICMP / fragments), by node, by layer, "Only failures". Filters hide rows; they never rewrite them. A "Showing 12 of 47 events" line appears when filtered.
- Current-cursor row has an accent left bar and the text "Current". Rows after the cursor are visible but dimmed and described to assistive tech as "upcoming".
- Click a row: moves the cursor (snaps, no animation) and pins the inspector.
- Keyboard: the table is a single tab stop; arrows move rows, `Enter` selects, `Home`/`End` jump.
- `aria-live="polite"` region announces the current event message during playback (throttled to avoid flooding at high speed).
- Virtualize rendering above 500 rows.
- This table is the **accessible alternative** to the animation: everything shown on the canvas must be derivable from it.

### 9.2 Result summary (top of Metrics tab and a compact banner on the canvas)

```text
┌ Result ───────────────────────────────────────────────────────────────┐
│ ✕ Dropped at R3 · TTL expired            reason: TTL_EXPIRED           │
│ Planned path: A – R1 – R2 – R3 – Cloud   Followed: A – R1 – R2 – R3    │
│ Router hops: 2 forwarded (3 on the planned path)   Links crossed: 3    │
│ Run completed · seed 48213 · engine 0.1.0 · schema 1.0                 │
│ This is a simulation. The cloud endpoint is simulated.                 │
└───────────────────────────────────────────────────────────────────────┘
```

- Packet outcome and run status are separate: run status "Completed" with packet outcome "Dropped".
- Always show both router hops and links crossed (D-05).
- Distinguish: **No route** (nothing was sent), **No feasible route** (constraints rejected every path; list which), **Dropped in transit** (sent, then lost), **Delivered**.

### 9.3 Metric tile

```text
Estimated end-to-end loss          [Estimated]
2.97 %                            ⓘ 1 − Π(1 − pᵢ) over 3 links, independent
```

- Fields: label, value + unit, **provenance tag**, definition tooltip.
- Provenance tags (text chip plus a distinct icon): **Configured** (from the topology), **Estimated** (computed from a model), **Observed** (from this run's events), **Not modeled** (no value).
- Missing values show "Not modeled" in `--nt-text-muted`, never `0`, never a dash that could read as zero.
- Estimated and observed values for the same quantity sit side by side (loss, delay) and are never merged.

### 9.4 Charts

- Only where a chart explains more than numbers: stacked bar of delay components (propagation / serialization / processing, queueing if modeled) per hop; compare view bars. Each segment has a label or pattern, not color alone.
- No chart for a single number. No decorative sparklines.
- Chart axes carry units. Data is exactly what the engine returned.

---

## 10. Comparison and fault views (M6, M7)

### 10.1 Compare tab

1. **Inputs diff first.** A table with rows for topology version, algorithm, objective, constraints, TTL, size, DF, seed. Cells that differ get a "Differs" tag and a left bar. If the topologies differ, a warning banner appears above everything: "These runs used different topologies. Differences below may come from the topology, not the setting you changed."
2. **Result diff:** outcome, path, router hops, link hops, cost, delay (estimated and observed), bottleneck, loss (estimated and observed), MTU status.
3. **Direction hints:** each metric says whether lower or higher is better ("lower is better") but the UI never declares an overall winner unless the user picked an objective; then it highlights that objective's row.
4. **Why paths differ:** a short generated list from engine data (e.g. "Widest path avoids L2 because its bandwidth is 100 Mbps").
5. Run list is limited to two or three columns; extra runs go in a picker. Each run has a user-editable label.

### 10.2 Faults tab

- List of faults with a toggle each, target, type, start (time or event number), duration, and a badge **Temporary (injected)** vs **Saved edit** to teach the difference.
- Adding a fault: choose target on canvas or from a list, choose type, set the changed value, preview "Before → after" (e.g. "MTU 1500 → 576").
- Fault events appear in the timeline with distinct text ("Fault activated") and amber markers on the affected link/node at the right moment in playback.
- **Before/after:** button "Compare with and without this fault" runs two requests with identical seeds and opens Compare.
- The UI never changes topology settings to make a scenario succeed.

---

## 11. Dashboard

The dashboard opens with the questions the product answers, because that is the user's entry point.

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ NetTrace 3D                                       [Open a saved topology]  │
│                                                                            │
│ Send a packet. See every decision.                                         │
│ A simulated network lab: build a topology, trace a packet hop by hop,      │
│ break something, and compare what changes.                                 │
│                                                                            │
│ Start with a question                                                      │
│ ┌──────────┐ Does a packet need a router to reach a neighbor?              │
│ │ mini     │ Same subnet · 1 switch · 2 hosts                [Open]        │
│ │ diagram  │                                                               │
│ ├──────────┤ Why does the host send to a gateway?                          │
│ │ mini     │ Two subnets · 1 router                          [Open]        │
│ │ diagram  │                                                               │
│ ├──────────┤ What happens when TTL runs out on a long path?                │
│ │ mini     │ Three routers · simulated cloud endpoint        [Open]        │
│ │ diagram  │                                                               │
│ └──────────┘                                                               │
│                                                                            │
│ Or start blank [New topology]        Import a file [Import]                │
│                                                                            │
│ Recent topologies                    Recent runs                           │
│ (local list, empty state below)      (local list)                          │
│                                                                            │
│ What's working                                                             │
│  Implemented: …   Partial: …   Planned / Not modeled: …                    │
│ This app simulates networks. It does not send real packets.                │
└────────────────────────────────────────────────────────────────────────────┘
```

- The mini diagrams are **static drawings of the topology**, not simulated results. Do not animate them or show success marks.
- Starter scenarios are rows with a diagram, a question, a one-line description, and an Open button. They are not a grid of identical cards.
- "What's working" is generated from `docs/status.md` data (a JSON export of the status board), so the dashboard never overclaims. Planned features are listed as planned.
- Empty states: "No saved topologies yet. Open a starter above or create a blank one."
- The simulation disclaimer is permanent text on this page, not a dismissible banner.

---

## 12. Playback and animation

### 12.1 Playback state machine

States: `empty → ready → playing ⇄ paused → ended`; `reset` returns to `ready`.

| From | Event | To | Notes |
|---|---|---|---|
| empty | result loaded | ready | cursor = 0, nothing highlighted |
| ready / paused | Play | playing | |
| playing | Pause | paused | progression stops immediately |
| ready / paused | Step | paused | cursor + 1 |
| playing | last event reached | ended | terminal badge appears |
| any | Reset | ready | cursor = 0, markers cleared |
| any | new run started | empty → ready | old result replaced only when the new one arrives |
| paused / ended | select timeline row | paused | cursor snaps to row |
| playing | user edits topology | playing | banner "Topology changed", result unchanged |

- `currentEventIndex` is the single source for 2D, 3D, ribbon, timeline, and inspectors.
- Speed multiplies only the *presentation* duration. It never changes timestamps, TTL, or outcomes.
- Step back (`←`) re-derives the view model at index − 1; it does not rerun the engine.

### 12.2 Beats

Animation groups events into **beats** for readability, while the cursor still moves per event:

| Beat | Events grouped | Presentation |
|---|---|---|
| Decide | host route lookup, neighbor resolved, frame encapsulated | Marker pulses once on the node; Explain panel updates |
| Travel | link transmit started → link receive | Marker moves along the link in `--nt-hop-base ÷ speed` |
| Process | route lookup, route selected, TTL decremented | Marker rests on the router; ribbon cell fills in; pips update |
| Terminal | delivered, dropped, expired | Badge settles in once (the single orchestrated moment) |

- Presentation time is **not** simulation time. A 50 ms link and a 1 ms link take the same time to animate at 1×. A small "Animation is not to scale" note sits in the playback tooltip. Optional "Scale to simulated time" mode is `planned`.
- Loss: the marker travels part of the link, stops, and shows the cross badge at the loss point. It never reaches the far node.
- Fragments: the marker splits into a labeled cluster at the router that fragmented.
- ICMP: a new diamond marker appears at the generating router and travels the return path as its own packet.

### 12.3 Motion rules

- Motion exists to show what changed: packet travel, selection, panel open/close, one terminal settle. No entrance animations on panels or rows, no hover bounce, no looping decorative animation.
- `prefers-reduced-motion: reduce` (and the Settings override): the marker **jumps** to each position with a brief highlight; no travel, no pulse. Playback still works.
- Never animate in a way that suggests success before the terminal event.
- Fast-forward at 4× keeps every event visible in the timeline; the canvas may skip intermediate beats but must still end at the correct state.

---

## 13. Honesty and labeling system

| Situation | Required UI |
|---|---|
| Any page with results | Persistent line: "Simulation. No real packets were sent." |
| Cloud endpoint | "Simulated" tag on the node, in lists, and in results ("Delivered to the simulated endpoint") |
| ARP abstracted | Event labeled "Neighbor resolved (ARP abstracted)" with a tooltip explaining the simplification |
| Switch loops (STP not modeled) | Warning "Layer-2 loop detected. Spanning tree is not modeled." |
| ICMP rate limiting omitted | Note in the ICMP packet inspector |
| Congestion model | Label "Simplified congestion model" next to any utilization value |
| Missing metric | "Not modeled" (never `0`, never blank) |
| Planned feature | Disabled control + tooltip "Planned. Not available yet." or hidden |
| Stale planned path | Banner "Plan is out of date because the topology or a fault changed." |
| Delivery wording | "IP delivered" unless application behavior is modeled; never "request succeeded" |
| Bellman–Ford | Description says "Shortest path with relaxation rounds. Not a RIP implementation." |

---

## 14. 3D view (M8)

Optional. Uses the same view model and the same `currentEventIndex`. 2D must work without any 3D code loaded.

### 14.1 Scene design

- **Background:** the same `--nt-bg` navy, with a very subtle vertical gradient toward `--nt-surface-1` at the horizon. No skybox, no bloom, no fog by default.
- **Subnet clusters:** each subnet is a flat translucent disc (the "floor plate") under its nodes, labeled with the prefix (mono). Router nodes sit at the edge where discs overlap or touch. Clusters are laid out by connectivity, with a fixed seed so layouts are stable.
- **Nodes:** the same four silhouettes extruded to low-relief solids, using the same outline logic. State badges are billboarded sprites (always face the camera) with icon + text.
- **Links:** lines, not glowing tubes. Down links are dashed with the "×" sprite. Planned/followed styles match 2D (dashed vs solid accent).
- **Packet marker:** same shape language (sphere, diamond/octahedron, small cubes), TTL pips as a ring, plus a billboarded text label.
- **Labels:** billboarded sprites with minimum on-screen size 12 px and a contrasting backing plate for legibility; auto-hide at distance.
- **Lighting:** one ambient + one soft directional light. No shadows by default.

### 14.2 Camera and controls

- Orbit (drag), pan (right-drag / two-finger), zoom (scroll / pinch).
- Buttons (top-right): **Reset camera**, **Top-down**, **Follow packet** (toggle; the camera eases to keep the current packet in view, instant under reduced motion), **Fit all**.
- Keyboard: arrow keys orbit when the canvas is focused, `+`/`-` zoom, `F` fit, `0` reset. Same global playback shortcuts as 2D.
- Click a node/link/packet: opens the same inspector. Selection and cursor persist when toggling 2D/3D; toggling never reruns.

### 14.3 Performance and fallback

- Lazy-load the 3D chunk on first toggle. Show a skeleton "Loading 3D view".
- No WebGL: show "3D needs WebGL, which isn't available here. Showing the 2D view." and stay in 2D.
- Above a node threshold (default 80) or low-power detection, show "This topology is large for 3D. The 2D view will be smoother." with a button to continue anyway. "Reduced effects" mode drops antialiasing and label sprites below a size.
- Record measured frame rates in `docs/testing.md`; do not claim smoothness without measurement.

---

## 15. Accessibility

Target: **WCAG 2.2 AA**.

- [ ] Every control has a visible label or an `aria-label`; icon-only buttons have tooltips and labels.
- [ ] Focus is always visible (section 2.3). Focus order follows visual order. Dialogs and drawers trap focus and restore it on close. `Esc` closes.
- [ ] The canvas is exposed as an application region with a text summary ("Topology with 2 hosts, 1 router, 1 switch, 4 links. Source Host A, destination Host B."). Nodes are focusable with accessible names that include type, name, IP, and state.
- [ ] The timeline is a full alternative to the animation (section 9.1). Anything visible on the canvas can be read there.
- [ ] State is conveyed by icon + text + shape; color is a reinforcement only. Test with a grayscale filter and with simulated color-vision deficiencies.
- [ ] Contrast checked by script (section 2.1). UI graphics (link lines, focus ring, pips) at least 3:1.
- [ ] Live regions: current-event announcements (polite, throttled), run completion ("Run completed. Packet dropped at R3, TTL expired."), copy confirmations. Errors use `role="alert"`.
- [ ] Motion: honors `prefers-reduced-motion`; no flashing above 3 Hz; no autoplay on load.
- [ ] Text can be zoomed to 200% without loss of function; layouts reflow at 320 px width for non-canvas panels.
- [ ] Touch targets at least 24 px, 40 px in touch layouts. Gestures have non-gesture alternatives (buttons for zoom/fit).
- [ ] Forms: labels programmatically associated, errors associated, required fields marked in text.
- [ ] Tooltips are reachable by keyboard focus and dismissible with `Esc`; essential information is never tooltip-only.
- [ ] Language attribute set; units spelled or abbreviated consistently, with full names in `aria-label` ("megabits per second").

---

## 16. Copy and tone

### 16.1 Voice

- Plain, specific, active, sentence case. Name things the way a networking student would.
- Buttons say what happens: **Run simulation**, **Plan path**, **Step**, **Reset**, **Save topology**, **Export run**. The same action keeps the same name in toasts and logs ("Run completed").
- Errors explain what happened and how to fix it, without apology or blame. Empty states point to the next action.
- No filler ("Oops!", "Awesome!"), no jokes in error states, no marketing language inside the lab.

### 16.2 Core strings

| Where | Text |
|---|---|
| Simulation notice | "Simulation. No real packets are sent and no cloud resources are created." |
| Cloud tag | "Simulated endpoint" |
| Unmodeled value | "Not modeled" |
| Run disabled | "Choose a source and destination to run." / "Fix 2 errors in the topology to run." (links to the list) |
| Stale result | "Topology changed since this run. Rerun to see the effect." |
| Seed note | "This run used seed {seed}. Use it again to repeat the same losses." |
| Planned feature | "Planned. Not available yet." |
| Backend unreachable | "Can't reach the simulation service. Check that the backend is running, then try again." |
| Engine failure | "The simulation engine stopped before finishing. Your topology is unchanged. Try again, or report this with run ID {id}." |
| Import too large | "That file is {size}. The limit is {limit}. Remove unused nodes or split the topology." |
| Import wrong version | "This file uses schema version {v}. This app reads version 1.x." |
| Empty timeline | "Run a simulation to see its events here." |
| Empty inspector | "Select a node, link, or event to see its details." |

### 16.3 "What to try" hints by reason code

Static advice shown under failures. The engine's message states what happened; this states what to check.

| `reasonCode` | Title | What to try |
|---|---|---|
| `NO_ROUTE` | No route to destination | Check the host's default gateway and the router's routes for the destination subnet. |
| `NO_FEASIBLE_ROUTE` | No path meets your constraints | Relax a constraint listed below, or add a link that satisfies it. |
| `GATEWAY_UNREACHABLE` | Gateway isn't on the host's subnet | Put the gateway address in the same subnet as the host interface. |
| `INVALID_GATEWAY` | Gateway address is invalid | Enter a valid IPv4 address for the default gateway. |
| `LINK_DOWN` | A link on the path is down | Enable the link, or add an alternate path and turn on rerouting. |
| `INTERFACE_DOWN` | An interface is down | Enable the interface on that device. |
| `TTL_EXPIRED` | TTL ran out before the destination | Raise the initial TTL above the number of routers on the path. |
| `MTU_EXCEEDED_DF_SET` | Packet too large and can't be fragmented | Reduce the packet size, clear Don't Fragment, or raise the link MTU. |
| `PACKET_LOSS` | Packet lost on a link | Lower the link's loss setting, or change the seed to see a different outcome. |
| `REASSEMBLY_TIMEOUT` | Fragments did not all arrive in time | Lower loss on the path, or avoid fragmentation by reducing packet size. |
| `QUEUE_OVERFLOW` | A queue was full | Raise queue capacity or link bandwidth. |
| `SIMULATION_LOOP_GUARD` | Safety guard stopped a forwarding loop | Check routes for a cycle between routers. |
| `EVENT_LIMIT_REACHED` | The run hit the event limit and was stopped | Reduce the packet count or raise the limit in Settings. |
| `UNSUPPORTED_PROTOCOL` | Not supported yet | Use IPv4, or see the status list for what is available. |

### 16.4 Glossary tooltips (Help drawer and inline `ⓘ`)

Each is one or two sentences. Written for a learner; no claim beyond the model.

- **TTL (time to live):** a counter in the IPv4 header. Each router that forwards the packet lowers it by one. At 1 on arrival, the router drops the packet instead of forwarding it.
- **Next hop:** the next router (or the destination) the packet is handed to. It is not necessarily the final destination.
- **Default gateway:** the router a host uses for destinations outside its own subnet.
- **Longest-prefix match:** when several routes match, the most specific one (longest prefix) wins.
- **MTU:** the largest IP packet, in bytes, that a link carries without fragmentation.
- **DF (Don't Fragment):** if set, a router must drop an oversized packet instead of fragmenting it.
- **Fragment offset:** where a fragment's data starts in the original packet, in units of 8 bytes.
- **ICMP Time Exceeded:** a separate message a router may send back when it drops a packet whose TTL ran out.
- **Bottleneck bandwidth:** the smallest available bandwidth on any link along the path.
- **Estimated vs observed:** estimated values come from a model of the configured network. Observed values come from what happened to packets in this run.
- **Router processing load (configured):** a simulation setting for how long a router takes per packet. It is not CPU usage.

---

## 17. Component inventory

Build in `frontend/src/components/` (generic) and `features/*` (domain). Each component has stories or unit tests covering its states (section 7.3).

**Generic:** `Button` (primary, secondary, quiet, danger), `IconButton`, `Field` (with unit suffix), `Select`, `SegmentedControl`, `Checkbox`, `Toggle`, `Tabs`, `Tooltip`, `Popover`, `Dialog`, `Drawer`, `Banner` (info, warning, error, stale), `Toast` (non-critical confirmations only), `Skeleton`, `EmptyState`, `Kbd`, `Chip`, `StatusBadge` (icon + text), `CopyableValue`, `Value` (mono wrapper), `DataTable` (virtualized), `ResizablePanel`.

**Topology:** `NodeShape` (4 types), `LinkLine`, `PortHandle`, `TopologyCanvas2D`, `PalettePanel`, `NodeForm`, `InterfaceEditor`, `LinkForm`, `RouteTableEditor`, `ValidationList`, `MiniTopology` (static, for the dashboard).

**Simulation:** `SimulationConfigForm`, `ObjectivePicker` (six criteria), `ConstraintsPanel`, `PlaybackBar`, `PacketMarker`, `TtlPips`, `TtlRibbon`, `EventTimeline`, `ResultSummary`.

**Inspectors:** `PacketInspector`, `RouterInspector`, `LinkInspector`, `ExplainPanel`.

**Metrics:** `MetricTile` (value, unit, provenance tag, definition), `ProvenanceTag`, `DelayBreakdownChart`, `ComparisonTable`, `InputsDiff`.

**Faults:** `FaultList`, `FaultEditor`, `BeforeAfterPreview`.

**3D (lazy):** `TopologyCanvas3D`, `SubnetPlate`, `Billboard`, `CameraControls`.

**Shell:** `AppShell`, `TopBar`, `Workspace`, `Dashboard`, `HelpDrawer`, `SettingsDialog`.

Rules:

- Renderers (`TopologyCanvas2D`, `TopologyCanvas3D`) accept only the shared view model and callbacks. They contain no routing, TTL, or outcome logic.
- Components render from props; data fetching lives in `api/` and feature hooks.
- No `dangerouslySetInnerHTML` with user-provided text. Labels are text nodes.

---

## 18. Design acceptance and testing

### 18.1 Design Definition of Done

- [ ] Tokens implemented once; no hard-coded colors in components.
- [ ] Contrast script passes for all token pairs.
- [ ] Four node silhouettes distinguishable in grayscale.
- [ ] Every state in sections 4.3–4.5 has icon + text, verified in grayscale.
- [ ] The TTL ribbon renders only from engine events and shows TTL in/out at every router for the three starter scenarios.
- [ ] Timeline contains everything shown on the canvas (spot-checked on all starter scenarios).
- [ ] Playback state machine implemented as specified; pause stops progression; reset restores the initial state.
- [ ] `prefers-reduced-motion` verified: no travel animation, playback works.
- [ ] Keyboard-only run completed end to end: load starter, configure, run, step through, inspect, export.
- [ ] Screen-reader pass (NVDA or VoiceOver) on Dashboard and Workspace; results recorded in `docs/testing.md`.
- [ ] Layouts verified at 1440, 1280, 1024, 768, and 390 px widths.
- [ ] No page-level horizontal scroll at any width.
- [ ] All empty, loading, error, stale, and unreachable-backend states implemented.
- [ ] "Not modeled" shown for every missing metric; no zero placeholders (search tests).
- [ ] Planned features are disabled with explanation or hidden; no dead buttons.
- [ ] 2D/3D toggle preserves selection and cursor; no rerun; same inspectors.
- [ ] Measured 2D and 3D performance recorded honestly.

### 18.2 Automated checks

- [ ] **axe** accessibility checks (Playwright) on Dashboard, Workspace idle, Workspace with result, Compare, Help.
- [ ] **Visual regression** screenshots for: each starter scenario at start, mid-run, and terminal; dropped and expired variants; narrow layout; reduced-motion; grayscale. Update baselines only with a recorded reason.
- [ ] **Contrast** script in CI.
- [ ] **Component tests** for states (disabled-with-reason, error, loading) and for `MetricTile` provenance tags.
- [ ] **Lint rule / grep test:** no client-side computation of routes, TTL, or delivery (`T-5-10`).
- [ ] **Font fallback** test: layout holds with fallback fonts.

### 18.3 Mapping to TASKS.md

| Design area | Tasks |
|---|---|
| Tokens, shell, dashboard, layout | M5-30 to M5-33 |
| Node/link visuals, editor | M5-08 to M5-16 |
| Playback, view model, timeline, inspectors, explain | M5-20 to M5-29 |
| TTL ribbon | M5-25 (extend) and T-5-03, T-5-04 |
| Metric tiles, compare, export | M6-15 to M6-23 |
| Faults, fragments in UI | M7-22 to M7-25 |
| 3D | M8-01 to M8-11 |
| Accessibility and visual QA | T-5-08, M9-14 |

---

## 19. Instructions for Antigravity (UI work)

1. Read `DESIGN.md` together with `NETWORKING_RULES.md` §23 before building any UI.
2. Implement tokens (section 2) and the component inventory (section 17) first, with state tests, before assembling screens.
3. Build in this order: tokens and shell → node/link visuals → editor → configure form → playback and view model → timeline → inspectors and Explain panel → TTL ribbon → result summary and metrics → compare → faults → dashboard polish → 3D.
4. Render only engine data. If the data for a field is absent, show "Not modeled" or hide the control. Never invent text, numbers, or states on the client.
5. Do not add animations, gradients, glows, or decorative effects not listed here. If something seems missing, add a note to `docs/decisions/` rather than improvising a style.
6. Verify with the checks in section 18 and report actual results (axe counts, contrast output, screenshots taken). Do not claim accessibility or performance without running the checks.
7. Keep the two languages of the UI separate: *configuration* (what the user set), *estimate* (a model's prediction), and *observation* (what happened in this run). Labels must make the difference clear.

### Prompt template

```text
Read DESIGN.md sections <N, M> and TASKS.md tasks <IDs>.
Build only these components: <list>. Use tokens from tokens.css; no hard-coded colors.
Implement every state in DESIGN.md 7.3. Add tests for them.
Show the exact commands run and results (unit tests, axe, contrast script).
List anything in the design you could not implement and why.
```

---

## 20. Open design decisions

Record outcomes in `docs/decisions/` when resolved.

- [ ] Final confirmation that Atkinson Hyperlegible Next/Mono are available as self-hostable packages in the chosen toolchain; fallback pair if not (e.g. a different humanist sans plus a mono with a slashed zero).
- [ ] Light theme: ship in v1 or defer (default: defer).
- [ ] "Scale to simulated time" animation mode: keep `planned` or include in M5.
- [ ] Whether bandwidth-as-thickness is worth shipping in v1 (default: ship, off by default, with legend).
- [ ] Guided tutorial / coach marks: `planned`; the dashboard questions plus the Explain panel carry v1 learning.
- [ ] Mobile editing scope: view + run only (current) vs limited editing.

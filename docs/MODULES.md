# Module status and feature notes

Status of the Platform_RTE modules and the design notes that accompany them. The README describes what
the product is and which interfaces it exposes; this file is the per-module status list.

Actively growing framework with SIL 4 safety focus.

**IMPLEMENTED (Production Ready):** 14 core modules
- Timer, IPC (base queue API only — see note below), Memory, NVM, Task/Thread, Logging, Reboot
- AppManager (lifecycle), SafeState (transitions), Status codes, Types, Buffer, Cast, String
- Suitable for SIL 1-3 systems; can be integrated with external redundancy solutions
- Note: `rte_ipc`'s pub/sub and request-reply variants
  (`src/ipc/rte_ipc_pubsub.c`, `rte_ipc_request_reply.c`) are TODO-only
  stubs, excluded from the build (see their own file headers and
  `CMakeLists.txt`'s `RTE_ENABLE_IPC` comment) — only the base queue
  API (`rte_ipc_create`/`_send`/`_receive`/`_destroy`) is implemented.

**IMPLEMENTED:** Watchdog (`include/rte/watchdog/rte_watchdog.h`,
`src/watchdog/rte_watchdog.c`)
- Fault detection and recovery actions (LOG/SAFESTATE/REBOOT/FAILOVER/CUSTOM)
  dispatched on timeout, real timer integration, full test coverage
- Depended on directly by `rte_checkpoint` and `rte_appmanager`'s
  optional checkpoint stage (ADR-024's dependency graph)

**IMPLEMENTED:** Redundancy Framework — vital channels, voting, checkpoints,
data integrity (ADR-008/ADR-017; supersedes the "design phase" framing
`docs/REDUNDANCY_ARCHITECTURE.md` originally described this as — that
document is now a design *record*, not a proposal still to be built)
- `rte_channel` — 2oo2/2oo3/NMR quorum voting across redundant
  channels, disagreement/health tracking
- `rte_checksum` — CRC-64 data integrity and the `rte_vital_message_t`
  envelope (sequence + sender + CRC) both of the modules below build on
- `rte_checkpoint` — bounded checkpoint-ID rendezvous across vital
  channels, correct even without wall-clock agreement between nodes;
  fills in `rte_channel_checkpoint()` as already specified (but not
  previously built) in `docs/REDUNDANCY_ARCHITECTURE.md`
- `rte_clocksync` — pluggable, diagnostic-only wall-clock offset/quality
  query (never the basis of vital-comparison correctness — see its header)
- Example: `examples/geo_distributed_checkpoint_sync.c`
- See `docs/architecture/ADR-017-checkpoint-and-clock-sync.md` section 1
  and ADR-024's dependency table for exactly what's wired to what.

**IMPLEMENTED:** AppManager cycle hooks + built-in checkpoint (ADR-019)
- `pre_execute`/`post_execute` — optional per-cycle hooks bracketing the
  existing mandatory `execute()`, so a cyclic application's "gather
  inputs" / "decide" / "send outputs" phases can be three named functions
  instead of one function with phase-numbered comments; both default to
  NULL (skipped) and are fully backward-compatible with every existing
  `rte_appmanager_operations_t` caller.
- Optional built-in checkpoint stage — `rte_appmanager_config_t::checkpoint`
  (NULL by default) wires a bounded `rte_channel_checkpoint()` (ADR-017)
  rendezvous into the loop automatically, ahead of `pre_execute()`, so a
  dual/multi-channel application no longer hand-rolls that call itself.
- Addendum (§5): relaxed `rte_channel_init()`'s `channel_count`
  floor from `>= 2` to `>= 1` (`RTE_VOTING_NMR`, `quorum_size == 1` only —
  2oo2/2oo3 floors unchanged) to support a single-physical-link topology;
  see `docs/architecture/ADR-019-appmanager-cycle-hooks-and-checkpoint.md`
  for the full retrofit writeup (including the multiplexed-frame wire
  protocol needed to carry checkpoint traffic on an existing link), first
  live-verified in `RBC_GP`'s A/B channel.

**IMPLEMENTED:** Structured message-trail logging (`rte_log_write_event()`)
- New addition to `rte_log` alongside the existing free-text
  `rte_log_write()`: emits a fixed, space-separated `Key=Value` line -
  `Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src> Destination=<dst>
  Type=<type> Info=<info>[ <extra_fields>]` - for logging an actual
  inter-channel message (a frame sent/received, a decision like AGREE/
  DISAGREE) — the mandatory fields a message-trail log needs, with room
  for caller-supplied extra `Key=Value` fields beyond those seven.
  Fixed-arity, no `<stdarg.h>` (MISRA C:2012 Rule 17.1); the same OSAdapter
  as `rte_log_write()` receives it, so no OSAdapter changes are required.
  First real consumer: `RBC_GP`'s A/B/C/SITE cyclic executives
  now log every AB_SAMPLE, M136, checkpoint REQUEST/REPLY, AGREE/
  DISAGREE, and SITE heartbeat this way.

**IMPLEMENTED:** Dual-transfer state negotiation (`rte_dual`, ADR-020)
- `rte_dual_state_t` — shared IDLE/UNKNOWN/ONLINE/HOTSTANDBY/COLDSTANDBY
  vocabulary for "which of two redundant instances is active, and how
  well-backed is the standby one", generalizing the ad hoc versions of
  this `RBC_GP`'s `site.c`/`channel_ab.c` each grew independently.
- `rte_dual_msgchannel_t` ("Channel") — one EN 50159-defended message
  channel over a single `rte_netlink_handle_t`, reusing the framework's
  existing `rte_vital_message_t` envelope (sequence/sender/CRC-64) plus a
  masquerade check against an expected peer ID.
- `rte_dual_channel_t` ("DualChannel") — wraps 1..N redundant Channels
  with always-send + bounded-ACK-wait delivery (the real payload traffic
  itself is the liveness signal, never gated by negotiated state),
  aggregate `DOWN`/`DEGRADED`/`FULL` connection-status tracking with an
  optional change callback, and a second fire-and-forget frame flow for
  carrying a negotiator's own STATE beacons on the same links.
- `rte_dual_negotiator_t` — drives one round of state negotiation per
  `execute()` call: older-startup-timestamp-wins tie-break for the initial
  ONLINE/STANDBY decision, and an asymmetric HOT/COLD rule where the
  *currently-ONLINE* side's own channel health (never the STANDBY side's
  self-report) determines the STANDBY side's HOTSTANDBY/COLDSTANDBY label.
  One-directional dependency: the negotiator depends on a DualChannel, a
  DualChannel has no knowledge of the negotiator.
- Framework-only in this pass — `RBC_GP`'s `site.c`/`channel_ab.c`
  keep their existing hand-rolled logic for now; retrofitting them to
  `rte_dual` is a deliberate follow-up (see ADR-020 §4 non-goals).
- See `docs/architecture/ADR-020-dual-transfer-state-negotiation.md`.

**Key Documentation:**
- Test Coverage Report — gcovr line/function/branch
  coverage (generated by CI on every push to `master`/`develop`; only
  live on the published GitHub Pages site, not in a plain repo checkout
  — run `./scripts/coverage.sh` locally for the same report)
- `docs/architecture/` — Architecture Decision Records (ADRs 001-008,
  016-019; 009-015 do not exist) + PlantUML diagrams
- `docs/requirements/SRS.md` — Consolidated requirements specification
- `docs/MISRA_COMPLIANCE_REPORT.md` — MISRA C:2012 conformance status
- `docs/EN_50128_ALIGNMENT.md` — **EN 50126/50128/50129 alignment & safety case** ← Start here for certification
  - EN 50128 (Software safety) — All 10 mandatory techniques
  - EN 50129 (Functional safety management) — Full support
  - EN 50126 (RAM - Reliability/Availability/Maintainability) — Design features
- `docs/REDUNDANCY_ARCHITECTURE.md` — Voting, checkpoints, site redundancy, what is implemented (includes the former 2oo2 guide)
- `docs/WATCHDOG_DESIGN.md` — System/task/channel/checkpoint watchdog & recovery
- `docs/HARDWARE_PATTERNS_GUIDE.md` — **Choose your SIL & Hardware** ← Start here to select a configuration
  - SIL 1-2: Single system (1oo1, 1oo1+Watchdog)
  - SIL 3: Dual-channel (2oo2, 2oo2D)
  - SIL 4: Triple-channel (2oo3) **← Recommended for ERTMS RBC**
  - SIL 4 alternatives: Online mode, Hot standby, NMR
  - Comparison matrix & decision tree
  - Cost estimates & implementation timeline
  - Real-world use case examples
- `docs/HARDWARE_PATTERNS_GUIDE.md` (configuration reference part) — **All 9 hardware setup options** ← Detailed technical specs
  - Single system (non-redundant)
  - 2oo2 dual-channel (SIL 3)
  - 2oo3 triple-channel (SIL 4)
  - NMR (N-modular)
  - Online mode (active-active cluster)
  - Hot standby (active-passive failover)
  - Heterogeneous systems (mixed processors)
  - Centralized voter topology
  - Distributed gossip topology
- `docs/FEATURE_EXPANSION.md` — Feature roadmap & design specs

OAL services, each in its own `include/rte/<feature>/` +
`src/<feature>/` directory (ADR-007): timer, non-volatile memory (NVM),
static memory reservation, task/thread scheduling, inter-process/inter-task
communication (IPC), logging/diagnostics, and controlled reboot.

Common, layer-agnostic facilities, same per-feature layout: status codes
(`status`) and fixed-width types (`types`), the cross-layer data buffer view
with endianness-safe multi-byte access (`buffer`), checked integer casting
between every fixed-width type and `size_t` (`cast`), safe-state transitions
/ checked assertions (`safestate`: `RTE_ASSERT`, `RTE_SAFESTATE`,
`RTE_REBOOT`), bounded string manipulation replacing strcpy/strcat/
sprintf/atoi/strtok (`string`), and application lifecycle management
(`appmanager`: single entry point with init→execute→shutdown pattern).

Every OAL service is reached through a **OSAdapter registered at startup**
(`rte_<service>_register_osadapter()`) rather than a hardcoded
implementation — this is how an integrator supplies their own
implementation (POSIX for host-side dev/test, an RTOS OSAdapter for target
hardware) without editing framework source. See
`docs/architecture/ADR-005-oal-osadapter-registration.md`.

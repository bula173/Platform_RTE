# Safe API Framework

A layered C/C++ API framework for building safety-related applications whose
layers communicate through well-defined software interfaces, so each layer
can be developed and verified independently. The reference application
driving the design is an ERTMS Radio Block Centre (RBC), a SIL 4 function
under CENELEC EN 50128 / EN 50129.

Applications never call OS/system APIs directly - every service is reached
through SAPI, so an application can be retargeted to a different OS/RTOS by
registering a different backend, without touching application code.

## Status

Actively growing framework with SIL 4 safety focus.

**IMPLEMENTED (Production Ready):** 14 core modules
- Timer, IPC (pubsub + request-reply), Memory, NVM, Task/Thread, Logging, Reboot
- AppManager (lifecycle), SafeState (transitions), Status codes, Types, Buffer, Cast, String
- Suitable for SIL 1-3 systems; can be integrated with external redundancy solutions

**IN PROGRESS:** Watchdog (System/Task/Channel/Checkpoint)
- API design complete (`include/safeapi/watchdog/sapi_watchdog.h`)
- Implementation pending (v0.3.0 target)
- Marks fault detection and recovery actions for SIL 4

**IMPLEMENTED:** Distributed channel synchronization (ADR-017)
- `sapi_checkpoint` — bounded checkpoint-ID rendezvous across vital
  channels, correct even without wall-clock agreement between nodes;
  fills in `sapi_channel_checkpoint()` as already specified (but not
  previously built) in `docs/REDUNDANCY_ARCHITECTURE.md`
- `sapi_clocksync` — pluggable, diagnostic-only wall-clock offset/quality
  query (never the basis of vital-comparison correctness — see its header)
- Example: `examples/geo_distributed_checkpoint_sync.c`
- Note: `sapi_vital_channel` and `sapi_checksum` (CRC-64) are also present
  in `include/`/`src/` and used by the two modules above, but are not yet
  reflected in this status summary's own categorization - see
  `docs/architecture/ADR-017-checkpoint-and-clock-sync.md` section 1 for
  what's actually wired together today.

**DESIGN PHASE:** Redundancy Framework (Vital Channels, Voting, Checkpoints)
- Documented in `docs/REDUNDANCY_ARCHITECTURE.md` as design proposal
- Planned for v0.4.0+
- Requires implementation of voting logic and checkpoint synchronization for full SIL 4 support
- See ROADMAP.md for detailed timeline

**IMPLEMENTED:** AppManager cycle hooks + built-in checkpoint (ADR-019)
- `pre_execute`/`post_execute` — optional per-cycle hooks bracketing the
  existing mandatory `execute()`, so a cyclic application's "gather
  inputs" / "decide" / "send outputs" phases can be three named functions
  instead of one function with phase-numbered comments; both default to
  NULL (skipped) and are fully backward-compatible with every existing
  `sapi_appmanager_operations_t` caller.
- Optional built-in checkpoint stage — `sapi_appmanager_config_t::checkpoint`
  (NULL by default) wires a bounded `sapi_channel_checkpoint()` (ADR-017)
  rendezvous into the loop automatically, ahead of `pre_execute()`, so a
  dual/multi-channel application no longer hand-rolls that call itself.
- Addendum (§5): relaxed `sapi_vital_channel_init()`'s `channel_count`
  floor from `>= 2` to `>= 1` (`SAPI_VOTING_NMR`, `quorum_size == 1` only —
  2oo2/2oo3 floors unchanged) to support a single-physical-link topology;
  see `docs/architecture/ADR-019-appmanager-cycle-hooks-and-checkpoint.md`
  for the full retrofit writeup (including the multiplexed-frame wire
  protocol needed to carry checkpoint traffic on an existing link), first
  live-verified in `safeAPIExample`'s A/B channel.

**IMPLEMENTED:** Structured message-trail logging (`sapi_log_write_event()`)
- New addition to `sapi_log` alongside the existing free-text
  `sapi_log_write()`: emits a fixed, space-separated `Key=Value` line -
  `Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src> Destination=<dst>
  Type=<type> Info=<info>[ <extra_fields>]` - for logging an actual
  inter-channel message (a frame sent/received, a decision like AGREE/
  DISAGREE) — the mandatory fields a message-trail log needs, with room
  for caller-supplied extra `Key=Value` fields beyond those seven.
  Fixed-arity, no `<stdarg.h>` (MISRA C:2012 Rule 17.1); the same backend
  as `sapi_log_write()` receives it, so no backend changes are required.
  First real consumer: `safeAPIExample`'s A/B/C/SITE cyclic executives
  now log every AB_SAMPLE, M136, checkpoint REQUEST/REPLY, AGREE/
  DISAGREE, and SITE heartbeat this way.

**IMPLEMENTED:** Dual-transfer state negotiation (`sapi_dual`, ADR-020)
- `sapi_dual_state_t` — shared IDLE/UNKNOWN/ONLINE/HOTSTANDBY/COLDSTANDBY
  vocabulary for "which of two redundant instances is active, and how
  well-backed is the standby one", generalizing the ad hoc versions of
  this `safeAPIExample`'s `site.c`/`channel_ab.c` each grew independently.
- `sapi_dual_msgchannel_t` ("Channel") — one EN 50159-defended message
  channel over a single `sapi_netlink_handle_t`, reusing the framework's
  existing `sapi_vital_message_t` envelope (sequence/sender/CRC-64) plus a
  masquerade check against an expected peer ID.
- `sapi_dual_channel_t` ("DualChannel") — wraps 1..N redundant Channels
  with always-send + bounded-ACK-wait delivery (the real payload traffic
  itself is the liveness signal, never gated by negotiated state),
  aggregate `DOWN`/`DEGRADED`/`FULL` connection-status tracking with an
  optional change callback, and a second fire-and-forget frame flow for
  carrying a negotiator's own STATE beacons on the same links.
- `sapi_dual_negotiator_t` — drives one round of state negotiation per
  `execute()` call: older-startup-timestamp-wins tie-break for the initial
  ONLINE/STANDBY decision, and an asymmetric HOT/COLD rule where the
  *currently-ONLINE* side's own channel health (never the STANDBY side's
  self-report) determines the STANDBY side's HOTSTANDBY/COLDSTANDBY label.
  One-directional dependency: the negotiator depends on a DualChannel, a
  DualChannel has no knowledge of the negotiator.
- Framework-only in this pass — `safeAPIExample`'s `site.c`/`channel_ab.c`
  keep their existing hand-rolled logic for now; retrofitting them to
  `sapi_dual` is a deliberate follow-up (see ADR-020 §4 non-goals).
- See `docs/architecture/ADR-020-dual-transfer-state-negotiation.md`.

**Key Documentation:**
- `docs/architecture/` — Architecture Decision Records (ADRs 001-008,
  016-019; 009-015 do not exist) + PlantUML diagrams
- `docs/requirements/SRS.md` — Consolidated requirements specification
- `docs/MISRA_COMPLIANCE_REPORT.md` — MISRA C:2012 conformance status
- `docs/EN_50128_ALIGNMENT.md` — **EN 50126/50128/50129 alignment & safety case** ← Start here for certification
  - EN 50128 (Software safety) — All 10 mandatory techniques
  - EN 50129 (Functional safety management) — Full support
  - EN 50126 (RAM - Reliability/Availability/Maintainability) — Design features
- `docs/REDUNDANCY_ARCHITECTURE.md` — Vital channels, voting, checkpoints (5-stage output gate)
- `docs/WATCHDOG_DESIGN.md` — System/task/channel/checkpoint watchdog & recovery
- `docs/HARDWARE_PATTERNS_GUIDE.md` — **Choose your SIL & Hardware** ← Start here to select a configuration
  - SIL 1-2: Single system (1oo1, 1oo1+Watchdog)
  - SIL 3: Dual-channel (2oo2, 2oo2D)
  - SIL 4: Triple-channel (2oo3) **← Recommended for ERTMS RBC**
  - SIL 4 alternatives: Online mode, Hot standby, NMR
  - Comparison matrix & decision tree
  - Cost estimates & implementation timeline
  - Real-world use case examples
- `docs/HARDWARE_CONFIGURATIONS.md` — **All 9 hardware setup options** ← Detailed technical specs
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

OAL services, each in its own `include/safeapi/<feature>/` +
`src/<feature>/` directory (ADR-007): timer, non-volatile memory (NVM),
static memory reservation, task/thread scheduling, inter-process/inter-task
communication (IPC), logging/diagnostics, and controlled reboot.

Common, layer-agnostic facilities, same per-feature layout: status codes
(`status`) and fixed-width types (`types`), the cross-layer data buffer view
with endianness-safe multi-byte access (`buffer`), checked integer casting
between every fixed-width type and `size_t` (`cast`), safe-state transitions
/ checked assertions (`safestate`: `SAPI_ASSERT`, `SAPI_SAFESTATE`,
`SAPI_REBOOT`), bounded string manipulation replacing strcpy/strcat/
sprintf/atoi/strtok (`string`), and application lifecycle management
(`appmanager`: single entry point with init→execute→shutdown pattern).

Every OAL service is reached through a **backend registered at startup**
(`sapi_<service>_register_backend()`) rather than a hardcoded
implementation — this is how an integrator supplies their own
implementation (POSIX for host-side dev/test, an RTOS backend for target
hardware) without editing framework source. See
`docs/architecture/ADR-005-oal-backend-registration.md`.

## Building

### Using CMake Presets (recommended)

CMake presets provide standardized build configurations. List available presets:

```sh
cmake --list-presets
```

**Common workflows:**

```sh
# Debug build with tests
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Release build (optimized)
cmake --preset release
cmake --build --preset release
ctest --preset release

# CI build (strict checks, fail on warnings)
cmake --preset ci
cmake --build --preset ci
ctest --preset ci

# Memory sanitizer (detect use-after-free, buffer overflows)
cmake --preset asan
cmake --build --preset asan
ctest --preset asan

# Undefined behavior sanitizer
cmake --preset ubsan
cmake --build --preset ubsan
ctest --preset ubsan

# Code coverage
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
```

**Available presets:**
- `debug` — Debug build, all warnings, tests enabled
- `release` — Optimized release build, tests enabled
- `ci` — CI strict mode (warnings→errors, stop on test failure)
- `asan` — AddressSanitizer (memory errors)
- `ubsan` — UndefinedBehaviorSanitizer (undefined behavior)
- `coverage` — Code coverage instrumentation
- `clang` / `gcc` — Explicit compiler selection
- `minimal` — Headers only, no tests
- `linux-native` — Native Linux build (POSIX OAL)
- `linux-release` — Linux release build
- `qnx` — QNX RTOS cross-compilation
- `qnx-release` — QNX RTOS release build

### Cross-Compilation (Linux, QNX, etc.)

For detailed cross-compilation instructions, see [CROSS_COMPILATION.md](docs/CROSS_COMPILATION.md).

**Quick start — Linux:**
```sh
./examples/build-linux-native.sh
```

**Quick start — QNX RTOS:**
```sh
export QNX_HOST=/opt/qnx7.0.0/host/linux/x86_64
export QNX_TARGET=/opt/qnx7.0.0/target/qnx7.0.0/x86_64
./examples/build-qnx.sh
```

### Manual CMake invocation

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Static Analysis (MISRA C:2012)

The project uses **cppcheck** with the MISRA addon to verify compliance with MISRA C:2012 Mandatory & Required rules.

**Run analysis:**
```sh
# Via convenience script
./scripts/run-cppcheck.sh

# Via CMake target (requires cppcheck installed)
cmake --build build --target cppcheck

# Direct cppcheck invocation
cppcheck --addon=misra --std=c99 --enable=all -I include src include
```

**Output formats:**
```sh
./scripts/run-cppcheck.sh              # Print to stdout
./scripts/run-cppcheck.sh --html report.html  # Generate HTML report
./scripts/run-cppcheck.sh --json report.json  # Generate JSON report
```

**Compliance status:**
See `docs/MISRA_COMPLIANCE_REPORT.md` for current conformance status and documented deviations.
Suppressions are managed in `.cppcheck-suppressions`.

## Layout

```
CMakeLists.txt                 top-level build
cmake/CompilerWarnings.cmake   shared warning/hardening flags
docs/architecture/             architecture decision records (ADRs)
docs/requirements/              consolidated requirements specification (SRS)
docs/MISRA_COMPLIANCE_REPORT.md MISRA C:2012 conformance status
include/safeapi/<feature>/     one public header per feature (ADR-007):
                                status, types, buffer, cast, safestate,
                                string, timer, nvm, memory, task, ipc, log,
                                reboot, appmanager, watchdog, checksum,
                                vital_channel, clocksync, checkpoint, dual
src/<feature>/                 matching implementation (ADR-007 directory
                                layout is unchanged); compiled into one of
                                4 grouped library targets - safeapi::core,
                                safeapi::oal, safeapi::channels,
                                safeapi::appmanager (ADR-023) - rather than
                                one target per feature
tests/<feature>/                matching CTest test file per feature
examples/                      standalone example programs (see
                                examples/README.md), e.g.
                                geo_distributed_checkpoint_sync.c (ADR-017)
```

## Design principles

- Pure C ABI (`extern "C"`) for every public interface.
- No dynamic memory allocation after initialization — callers own storage.
- Every fallible call returns an explicit `sapi_status_t`; no exceptions.
- MISRA C:2012-oriented source, aimed at SIL 3/4 (EN 50128) constraints.
- Every conversion between integer types goes through a checked `sapi_cast_*`
  function — no bare C-style casts.
- Requirement-ID tags (`REQ-...`) on public API elements for traceability,
  consolidated in `docs/requirements/SRS.md`.
- All public headers documented in Doxygen format (`@file`/`@brief`/
  `@param`/`@return`); see `Doxyfile` to generate HTML docs locally.

# ADR-032: Setup-phase lock coverage extended to backend registration and remaining setup-only constructors

Status: Accepted
Date: 2026-08-20
Applies to: `src/timer/rte_timer.c`, `src/ipc/rte_ipc.c`,
`src/task/rte_task.c`, `src/netlink/rte_netlink.c`,
`src/nvm/rte_nvm.c`, `src/log/rte_log.c`,
`src/clocksync/rte_clocksync.c`, `src/reboot/rte_reboot.c`,
`src/memory/rte_memory.c`, `src/safestate/rte_safestate.c`,
`src/ipc/rte_ipc_pubsub.c`, `src/ipc/rte_ipc_request_reply.c`.

## 1. Context

A downstream integrator (`RBC_GP`) asked this framework to review
itself end to end for exactly the class of gap ADR-026 introduced
`rte_lifecycle_check_setup_allowed()` to close: "can a resource with
process-lifetime scope still be created/registered after the application
has started running, when it should only ever be creatable during
setup." ADR-026 §2.2 enumerates the constructors it gated as a closed
list (`rte_timer_create`, `rte_channel_init`, `rte_voter_init`/
`_register_channel`, `rte_cross_comparator_init`/`_register_channel`,
`rte_watchdog_create`) and a separate, deliberate exclusion list
(`rte_netlink_open`, `rte_dual_channel_init`, `rte_dual_negotiator_init`
- legitimately re-invoked post-lock for reconnect). Neither list mentions
any `rte_*_register_backend()` function, `rte_mem_pool_create()`,
`rte_safestate_register_handler()`, or the two `rte_ipc_pubsub`/
`rte_ipc_request_reply` creation functions - these were simply not
considered when ADR-026 was written, not intentionally left open. A
repository-wide audit against ADR-026's own criteria ("is this a one-time
'wire this up' call, or a per-cycle 'do this now' call") confirmed all of
them are the former.

This matters specifically for the `_register_backend()` family: a
backend vtable is process-lifetime, integrator-supplied wiring - exactly
ADR-026's own "resource an application's own design did not already
account for" concern (§2.2). Before this ADR, nothing stopped
`execute()`/`pre_execute()` from swapping a running application's memory-
pool or NVM backend mid-run, which is a considerably sharper hazard than
adding a new timer would be (it can silently redirect already-in-flight
state to a different backend implementation).

## 2. Decision

Extend ADR-026 §2.2's gated list with the same guard, in the same place
(after NULL/param validation, before any real work), returning the same
`RTE_STATUS_INVALID_STATE` on a locked setup phase:

- `rte_osadapter_timer_register`, `rte_osadapter_ipc_register`,
  `rte_osadapter_task_register`, `rte_osadapter_netlink_register`,
  `rte_osadapter_nvm_register`, `rte_osadapter_log_register`,
  `rte_osadapter_clocksync_register`, `rte_osadapter_reboot_register`,
  `rte_osadapter_memory_register` - every `_register_backend()` function
  in the framework, with no exceptions (unlike §2.2's constructor list,
  none of these has a documented legitimate post-lock re-registration
  use case - an integrator swapping a live backend mid-run was never a
  supported pattern, only an unenforced one).
- `rte_mem_pool_create()` - same "fixed resource set, reserved once"
  rationale as every other gated constructor; REQ-OAL-MEM-001 already
  says pool reservation is expected during system initialization, this
  ADR is what actually enforces that expectation instead of only
  documenting it.
- `rte_safestate_register_handler()` - a safe-state level's reaction is
  process-lifetime configuration, the same class of resource as a
  watchdog or channel.
- `rte_ipc_pubsub_topic_create()`, `rte_ipc_rr_server_create()`,
  `rte_ipc_rr_client_create()` - both modules are still `TODO`-stub
  bodies (no real backend dispatch yet), gated now so the guard is
  already correct before their real implementations land, rather than
  retrofitting it later under time pressure.

`rte_nvm_open()` and `rte_log_init()` were evaluated and NOT gated -
see the "Deferred" note below.

**No new malloc-based default backend was added anywhere in the
framework as part of this ADR.** This was raised as a candidate (a
pluggable allocator, falling back to `malloc()` if no backend is
registered) but rejected once a repository search found
`RBC_GP/src/posix_osadapter/rte_posix_osadapter_memory.c` already
implements a complete, working `rte_osadapter_memory_t` using a static
compile-time-sized arena (`g_arena[RTE_POSIX_MEM_ARENA_SIZE]`) with a
bump allocator and intrusive free list - zero `malloc()`/`free()` calls.
Adding a malloc-backed default to the framework itself would reintroduce
dynamic allocation exactly where this framework's own `CLAUDE.md` ("No
dynamic memory: malloc, free, and realloc are strictly banned") and
ADR-005's "backend is integrator-supplied" philosophy both already keep
it out - a real regression in rigor for a convenience an already-
compliant integrator does not need. `CLAUDE.md`'s existing rule required
no amendment: it remains categorically true (zero `malloc`/`free`/
`realloc` call sites anywhere in `include/`/`src/`) rather than needing a
new stated exception.

### Deferred: `rte_nvm_open()`, `rte_log_init()`

Both are structurally the initialization half of a `_register_backend()`
pair, but neither has a settled answer to "is a post-lock re-open ever
legitimate" the way `rte_netlink_open()` does (ADR-026 §2.2's own
reconnect rationale, backed by a real downstream consumer that does it).
Gating them speculatively risks the same kind of break ADR-026 §2.2
already had to reason carefully about for netlink/dual - left ungated
until a real use case (or its absence) is confirmed, rather than guessed
at here.

## 3. Consequences

- Positive: closes the gap the audit found - every remaining one-time
  "wire this up" call in the framework now enforces the same fixed-and-
  auditable-resource-set property ADR-026 established for timers/
  channels/voters/cross-comparators/watchdogs.
- Positive: the `_register_backend()` gating in particular closes a
  sharper hazard than ADR-026's original list did (a live backend swap
  mid-run, not just a new resource of an already-used kind).
- Negative: `rte_ipc_pubsub_topic_create()`/`rte_ipc_rr_server_create()`/
  `_client_create()` are gated ahead of having real bodies - low present
  risk (nothing to protect yet) but means the guard's placement was
  chosen without a concrete implementation to validate ordering against;
  worth re-checking once those modules gain real backend dispatch.
- Neutral: `rte_nvm_open()`/`rte_log_init()` remain exactly as
  permissive as before this ADR - not a regression, just an explicitly
  acknowledged open question rather than a silent gap.

## 4. Verification

- `cmake --build build` + `ctest --test-dir build --output-on-failure`:
  full framework rebuild and test suite, confirming no gated function's
  existing legitimate call site (all of which already run during
  `init()`, per ADR-026 §3's own "zero source changes required for any
  existing consumer" finding) regressed.
- `RBC_GP` (downstream consumer): full clean rebuild, `ctest`,
  and `.claude/skills/run-RBC_GP/smoke.sh` matching its
  established baseline - confirms none of its own `_register_backend()`/
  `rte_mem_pool_create()`/etc. call sites (all setup-phase, per the same
  ADR-026 §3 finding) are affected either.

## 5. Location

- The fourteen functions enumerated in §2 above, across the ten files
  listed at the top of this document.
- `docs/architecture/ADR-026-application-setup-phase-lock.md` (base
  decision this one extends - not itself edited, kept as the historical
  record of what was gated when).

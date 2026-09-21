# ADR-026: Application Setup-Phase Lock and Single-Entry-Point Enforcement

Status: Accepted
Date: 2026-08-17
Applies to: `include/safeapi/lifecycle/rte_lifecycle.h` (new),
`src/lifecycle/rte_lifecycle.c` (new), `rte_appmanager.h`/`.c`,
`rte_timer.c`, `rte_channel.c`, `rte_voter.c`,
`rte_cross_comparator.c`, `rte_watchdog.c`, `rte_status.h`/`.c`.

## 1. Context

Three related integrator concerns, raised directly against this
framework's own design:

1. **Single entry point.** `rte_appmanager_run()` was already documented
   as "the single entry point for all applications" (`rte_appmanager.h`),
   but nothing enforced it - a second, concurrent (or accidentally
   re-entrant) call while a previous one was still mid-lifecycle would
   silently race on `rte_appmanager.c`'s own file-scope globals
   (`g_app_state`, `g_shutdown_requested`), corrupting whichever call
   "won."
2. **INIT phase vs. RUN phase, locked after start.** A safety-critical
   application's resource set (timers, channels, voters,
   cross-comparators, watchdogs) should be fixed and known ahead of time,
   not still able to grow while the application is executing cyclically -
   a requirement close to ARINC 653's own APEX partition model (`INIT`
   mode permits `CREATE_*` calls; `SET_PARTITION_MODE(NORMAL)` locks
   them out afterward). This framework had no equivalent: every
   constructor (`rte_timer_create()`, `rte_channel_init()`, etc.) was
   equally callable from `execute()` as from `init()`.
3. **No threads: how do channels/dual-channels get serviced?** Raised as
   a related but *not yet acted on* concern - this ADR does not change
   this framework's threading model (`rte_task` remains available and
   used, e.g. by `safeAPIRBC2oo2`'s own background reconnect tasks); it
   is recorded here because the discussion that produced this ADR
   surfaced it, but no design decision follows from it in this ADR.

## 2. Decision

### 2.1 A new, minimal `rte_lifecycle` module (REQ-LIFECYCLE-001/002)

A single process-wide flag (`include/safeapi/lifecycle/rte_lifecycle.h`,
implemented in `src/lifecycle/rte_lifecycle.c`, compiled unconditionally
into `safeapi_core` alongside `status`/`buffer`/`cast`/`safestate`/
`string` - it has no OS dependency of its own, matching that library's
own admission criteria): `rte_lifecycle_lock()`/`_unlock()`/
`_is_locked()`, plus `rte_lifecycle_check_setup_allowed()` returning
`RTE_STATUS_INVALID_STATE` (new status code, appended at value 12 -
`rte_status.h`'s own ABI-stability comment requires appending, never
renumbering) once locked.

`rte_appmanager_run()` is the sole caller of `lock()`/`unlock()`:
- `unlock()` at the very top of every call (existing convention: mirrors
  `g_app_state`'s own per-call reset) **and** again the moment the
  execution phase ends (entering `RTE_APP_STATE_SHUTTING_DOWN`) - not
  only at the next call's own top-of-run reset. The second unlock is
  required, not redundant: setup code that legitimately runs *between*
  two `rte_appmanager_run()` calls (this framework's own test suite
  does exactly this - see 2.3) would otherwise stay incorrectly locked
  out from the moment one run's execution phase begins until the next
  run's top-of-run reset.
- `lock()` the moment `ops->init()` returns `RTE_STATUS_OK` (state
  transitions to `RTE_APP_STATE_RUNNING`).

### 2.2 Which constructors are gated (REQ-LIFECYCLE-001)

Gated (each calls `rte_lifecycle_check_setup_allowed()` as one of its
first checks, after NULL/param validation, before doing any real work):
`rte_timer_create()`, `rte_channel_init()`, `rte_voter_init()`,
`rte_voter_register_channel()`, `rte_cross_comparator_init()`,
`rte_cross_comparator_register_channel()`, `rte_watchdog_create()`.

**Deliberately excluded**: `rte_netlink_open()`, `rte_dual_channel_init()`,
`rte_dual_negotiator_init()`. All three are legitimately re-invoked
*after* the setup phase locks by an application's own reconnect-after-
link-loss logic - concretely, `safeAPIRBC2oo2`'s
`channel_ab_io.c`'s peer/C-link background reconnect tasks and
`channel_ab_negotiate_reconnect()` (this framework's own consumer
project, built immediately before this ADR) all call `rte_netlink_open()`
- and the latter also `rte_dual_channel_init()`/`rte_dual_negotiator_init()`
- from *inside* the RUNNING phase, every time a dropped link needs to
come back. Locking these three would not be "prevent adding a resource
the application's own design never accounted for" (this lock's actual
purpose); it would break an already-verified, intentional pattern: an
application re-establishing a link it already owns is not the same
thing as an application growing new resources mid-run. This distinction
- not "was this called after RUN began" alone - is what REQ-LIFECYCLE-001
actually polices.

### 2.3 Single-entry-point enforcement (REQ-APPMANAGER-009)

`rte_appmanager_run()` now checks `g_app_state.state` **before** doing
anything else (before its own existing top-of-run reset): if a previous
call is still mid-lifecycle (`INITIALIZING`/`RUNNING`/`SHUTTING_DOWN`),
the new call is refused (`EXIT_FAILURE`, logged) without touching any of
the state the in-progress call owns. A call arriving only *after* a
previous one has fully returned (`SHUTDOWN`/`ERROR`) is unaffected - this
framework's own test suite (`tests/appmanager/test_rte_appmanager.c`)
already relies on calling `rte_appmanager_run()` sequentially, more than
once, in one process, and continues to after this ADR.

### 2.4 The `longjmp()` escape hatch: `rte_appmanager_reset_state()`

One existing test (`test_checkpoint_timeout_enters_safestate_before_pre_execute`)
verifies `RTE_SAFESTATE_LEVEL_SAFE`'s own documented never-returns
contract (`REQ-COMMON-SAFESTATE-002`) by registering a handler that calls
`longjmp()` - the only way to make an intentionally-infinite code path
observable in a bounded unit test. A `longjmp()` out of
`rte_appmanager_run()` skips its own SHUTDOWN phase entirely, leaving
`g_app_state`/the new setup lock stuck at `RUNNING`/locked forever - which
then caused every subsequent test's own `rte_appmanager_run()` call to
be rejected by 2.3's new guard (found live: `ctest` regressed from 26/26
to 25/26 the moment 2.3 was added, isolated to exactly this interaction).

`setjmp`/`longjmp` are already MISRA C:2012 Rule 21.4 (Required)
violations - `<setjmp.h>` is disallowed in production code - so this
situation cannot arise there; it is specific to this framework's own test
suite needing a way to exercise an otherwise-untestable code path.
Rather than leave the test suite (and any real integrator who, against
MISRA guidance, does the same at the application level) with no recovery
path, `rte_appmanager_reset_state()` was added: forcibly resets
`g_app_state` and the setup lock back to their initial condition. Its own
doc is explicit that this is not general-purpose API - calling it while a
`rte_appmanager_run()` call is genuinely still executing (not abandoned
via a non-local jump) corrupts that call's own state.

## 3. Consequences

**Positive:**
- A concurrent/accidental re-entrant `rte_appmanager_run()` call is now
  a defined, refused error instead of silent global-state corruption.
- A timer/channel/voter/cross-comparator/watchdog an application's own
  `init()` did not already create can no longer be created from
  `execute()`/`pre_execute()`/`post_execute()` - the resource set
  becomes fixed and auditable at the moment the application starts
  running, not just at the moment its source code is read.
- Zero source changes required for any existing consumer whose own
  setup already completes inside `ops->init()` (the overwhelmingly
  common, already-recommended pattern) - `safeAPIRBC2oo2` needed no
  changes at all (see Verification below).

**Negative / accepted trade-offs:**
- The netlink/dual exclusion (2.2) means this lock does **not** fully
  prevent a *misuse* of `rte_netlink_open()`/`rte_dual_channel_init()`/
  `rte_dual_negotiator_init()` to add a genuinely new link mid-run
  (as opposed to re-establishing an existing one) - this framework has
  no way to distinguish "reconnect" from "new resource" at the API
  level for these three calls. Left as an application-level discipline
  question, not enforced here.
- `rte_appmanager_reset_state()` is a real, if narrow, addition to the
  public API surface purely to keep a `longjmp()`-based test working.
  An integrator could misuse it to defeat 2.3's own guard; its doc
  comment states plainly that doing so while a run is genuinely still
  in progress corrupts that run's state.
- The setup lock is a single, process-wide flag (REQ-LIFECYCLE-002), not
  one per `rte_appmanager_config_t` - consistent with `g_app_state`'s
  own pre-existing single-instance assumption (this framework has no
  concept of more than one concurrently-running application per
  process today), but a real limit if that ever changes.

## 4. Verification

- `ctest --test-dir build`: 26/26 passing, including the pre-existing
  `test_rte_appmanager` sequential-multi-run suite and the `longjmp()`
  safestate test (now paired with `rte_appmanager_reset_state()`).
- `safeAPIRBC2oo2` (this framework's own downstream consumer) rebuilt
  against the updated framework and re-verified via
  `.claude/skills/run-safeAPIRBC2oo2/smoke.sh` (negotiation, cross-compare,
  and the full DISAGREE -> REBOOT -> TAKEOVER -> state-transfer failover
  chain) with zero source changes required - direct evidence that the
  netlink/dual exclusion (2.2) is scoped correctly for a real,
  already-verified reconnect-after-fault consumer.

## 5. Location

- `include/safeapi/lifecycle/rte_lifecycle.h`, `src/lifecycle/rte_lifecycle.c` (new)
- `include/safeapi/appmanager/rte_appmanager.h`, `src/appmanager/rte_appmanager.c`
- `include/safeapi/status/rte_status.h`, `src/status/rte_status.c`
- `src/timer/rte_timer.c`, `src/channel_link/rte_channel.c`,
  `src/voter/rte_voter.c`, `src/cross_comparator/rte_cross_comparator.c`,
  `src/watchdog/rte_watchdog.c`
- `tests/appmanager/test_rte_appmanager.c`,
  `tests/lifecycle/test_rte_lifecycle.c` (new)
- `docs/requirements/SRS.md` §3c, §3c-bis

# ADR-028: Channel-Down Escalation to REBOOT, and a Cyclic-Executive Starvation Fix

Status: Accepted, with a known open item (§2.5) - the reboot escalation
and its enabling framework fix are done and verified live; the
"operator must confirm ONLINE after a simultaneous dual-reboot with no
inter-site link" mechanism is not yet designed (blocked on the operator
interaction model) or implemented.
Date: 2026-08-18
Applies to: `safeAPIFreamwork`'s `src/appmanager/rte_appmanager.c`
(framework fix, REQ-APPMANAGER-011); `safeAPIRBC2oo2`'s
`src/application/AB/channel_ab.c`/`channel_ab_io.c`/
`channel_ab_negotiate.c`/`channel_ab_types.h`, `src/application/C/monitor_c.c`/
`monitor_c_io.c`/`monitor_c_types.h`, `src/application/common_config.h`,
`tests/robot/fault_injection.robot`.

## 1. Context

Explicit requirement from the framework's integrator: RTE shall enter a
safe state when a *channel* is down - not merely log it - so that this
site's own redundant cluster peer can take over. A channel may be backed
by several network links (`rte_dual_channel_t` already supports up to
`RTE_DUAL_CHANNEL_MAX_LINKS`); a single link going down should only be
*communicated*, not itself trigger a reaction - only the channel-level
"no link at all" condition should escalate.

Two distinct link categories exist in `safeAPIRBC2oo2`, and they needed
different treatment:

- **Same-site channels** (the A↔B peer link; each of A/B's own links
  to/from C): losing contact with your own redundant pair, or with the
  channel that supplies your own decision input, is unambiguous local
  damage - the same severity judgment this project already makes for a
  genuine cross-compare DISAGREE. REBOOT is the right reaction here, and
  is what §2.1 adds.
- **The inter-site negotiation link** (A-WEST↔A-EAST, B-WEST↔B-EAST):
  a real design trap, worked through directly with the integrator. This
  link connects two *different* sites, so "link down" looks identical
  from both ends - a naive channel-down→REBOOT rule here would reboot
  *both* sites simultaneously on a shared link outage, which is the
  opposite of failover and edges toward the exact split-brain risk
  ADR-008 exists to prevent. Resolved differently - see §2.2.

## 2. Decision

### 2.1 Same-site channels: REBOOT after a sustained outage

New `SAFEAPI_EXAMPLE_CHANNEL_DOWN_REBOOT_MS` (`common_config.h`, 20000ms)
- deliberately much longer than the existing
`SAFEAPI_EXAMPLE_LINK_STALE_TIMEOUT_COUNT`-based reconnect trigger
(ADR-027 Phase 2, ~6s): a channel that's merely reconnecting should get
real time to self-heal first. Each affected rx task
(`channel_ab_io_peer_rx_task_entry()`, `channel_ab_io_m136_rx_task_entry()`,
`monitor_c_io_rx_a_task_entry()`/`_rx_b_task_entry()`) marks a
`*_down_since_ms` timestamp on the transition to down (cleared on
recovery); once continuously down past the threshold, the owning
process calls `channel_ab_shutdown()`/`monitor_c_shutdown()` then
`RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_REBOOT, ...)` (new reason codes
`SAFEAPI_EXAMPLE_REASON_AB_PEER_LINK_DOWN`, `_C_LINK_DOWN`,
`_C_MONITOR_LINK_DOWN`).

**Deliberately excluded**: `monitor_c_io_rx_b_task_entry()`'s own link.
B is a pure fallback source C only consumes if A goes quiet - under
normal dual-mode operation B never sends anything at all, so continuous
receive timeout on that specific link is its ordinary healthy state, not
a liveness problem. Confirmed live (a 15s local run) that applying the
same staleness logic there produces false positives; removed for that
one link, kept for every other.

**The reboot decision must run on the main thread, never the rx task
that detected it**: `channel_ab_shutdown()`/`monitor_c_shutdown()` call
`rte_task_destroy()` on the very rx tasks whose own doc explains that
call blocks (`pthread_join()`) until they return - calling shutdown
*from* one of those tasks would self-join (undefined behavior, typically
a deadlock). Each rx task only ever *writes* its own down-since
timestamp; only the main thread's own per-cycle check reads it and acts
(`channel_ab_check_channel_down_reboot()` in `channel_ab_pre_execute()`,
`monitor_c_check_channel_down_reboot()` in `monitor_c_execute()`) - see
§2.4 for why this main-thread check itself needed a framework fix to be
reachable at all during a real sustained outage.

### 2.2 Negotiation link: HOT/COLD standby readiness (already built, just surfaced)

No reboot-on-down added here at all, per §1's split-brain concern. The
framework already ships exactly the right primitive for this, unused by
`safeAPIRBC2oo2` until now: `rte_dual_negotiator_t`'s own
`RTE_DUAL_STATE_HOTSTANDBY`/`COLDSTANDBY` (REQ-DUAL-NEGOTIATOR-004) -
a STANDBY instance's HOT/COLD label is derived from the *peer's own*
reported channel-degradation bit, not a self-report, computed inside
`rte_dual_negotiator_execute()` on every cycle already. An
app-level `standby_hot` tracker was drafted and then deliberately
reverted once this was found - it would have duplicated, with a cruder
heuristic, something the framework already computes more correctly.
`channel_ab_negotiate.c`'s existing `on_negotiator_state_change()`
callback (previously logging raw numeric state values) now uses
`rte_dual_state_to_string()`, so a HOTSTANDBY↔COLDSTANDBY transition is
directly visible in logs - satisfying "communicate a single link problem
to the user" without adding new state or new reboot logic. Confirmed
live: `peer IDLE->COLDSTANDBY` immediately after negotiation settles,
then `COLDSTANDBY->HOTSTANDBY` once the first real data exchange
confirms the link.

Note: this app's negotiation link currently has `link_count == 1`, so
`COLDSTANDBY` can only appear transiently at startup (before the first
confirming exchange), never from a genuinely degraded *subset* of
multiple redundant links - exercising that fully would mean wiring a
second physical link per negotiation pair, not done as part of this ADR.

### 2.3 Negotiation watchdog timeout was shorter than its own hysteresis

Found while tracing why `on_negotiation_link_lost()` (`channel_ab_negotiate.c`,
pre-existing - the ONLINE-only reboot-on-negotiation-loss reaction this
ADR's §2.1 deliberately does *not* replicate) sometimes fired on a single
dropped UDP datagram: `SAFEAPI_EXAMPLE_NEGOTIATION_WATCHDOG_TIMEOUT_MS`
was a fixed 2500ms, shorter than `rte_dual_channel_send()`'s own
`ack_timeout_ms` (3000ms) - meaning a single missed ACK could exceed the
watchdog's window before ADR-027 Phase 1's own `neg_consecutive_send_miss`
hysteresis (added specifically so one dropped datagram doesn't cause
drastic action) ever got a chance to run. Fixed by deriving the timeout
from that hysteresis's own worst-case timing instead of a bare constant:
`(SAFEAPI_EXAMPLE_NEG_SEND_MISS_THRESHOLD * SAFEAPI_EXAMPLE_LINK_TIMEOUT_MS)
+ SAFEAPI_EXAMPLE_RECONNECT_ATTEMPT_TIMEOUT_MS + SAFEAPI_EXAMPLE_AB_CYCLE_PERIOD_MS`
(10500ms), so the two can't silently drift back out of sync.

### 2.4 The real bug: the cyclic executive starves whenever checkpoint is paused

Both §2.1's new reboot check and §2.3's pre-existing watchdog reboot
initially failed to fire *at all* during a real, sustained Docker
network partition - confirmed live via `docker stats`: the affected
process sat at ~100% CPU for 24+ seconds straight (past both the 10.5s
negotiation-watchdog and 20s channel-down thresholds) with zero reboot.

Root cause, traced to `rte_appmanager_run()` itself (`safeAPIFreamwork`,
not this example): `rte_watchdog_t` has no independent timer or thread
of its own, by design (the integrator's own explicit direction: *"the
intention is to not have threads... each cycle we are checking whether
timer against start timestamp expired... this allows to have only one
thread"*) - its expiry check (`rte_watchdog_timer_tick()`) only runs
when the application itself calls it, which `channel_ab.c` does from
inside `channel_ab_execute()`. `rte_appmanager_run()`'s own per-cycle
loop calls its checkpoint/pre_execute/execute/post_execute stages in
strict sequence, each gated on the previous succeeding. When a link goes
down, `safeAPIRBC2oo2` deliberately pauses the built-in checkpoint
(`checkpoint_cfg.voter = NULL`) so a *known* outage doesn't also trip
checkpoint's own independent SAFE-halt - but `rte_channel_checkpoint(NULL, ...)`
returns `RTE_STATUS_INVALID_PARAM`, which the framework's own stage-
result handling treated as a *failed* stage: paced
(`rte_appmanager_pace_failed_checkpoint()`) and `continue`d, skipping
`pre_execute()`/`execute()`/`post_execute()` entirely for as long as
`voter` stayed `NULL`. This silently starved *every* per-cycle safety
check, not just the new one - including `rte_watchdog_timer_tick()`
itself, so no watchdog-driven reaction of any kind, old or new, could
ever fire during exactly the sustained-outage scenario it exists for.

**Fix (REQ-APPMANAGER-011, `rte_appmanager.c`)**: `voter == NULL` is now
treated identically to `config->checkpoint == NULL` - skip the stage
outright (no `rte_channel_checkpoint()` call, no pacing, no error
counted) and let every later stage run normally, every cycle, regardless
of how long checkpoint stays paused. This is a small, targeted change,
not a move toward threads or async timers: a watchdog's own expiry check
is still exactly "compare now against a saved start timestamp" (the
integrator's own stated design), it just now actually gets *asked* every
cycle again. Confirmed fixed live: the identical fault scenario that
previously spun at ~100% CPU with zero reboot now reboots correctly and
promptly (within ~6s, via `on_negotiation_link_lost()`), CPU returns to
idle afterward, and the framework's own test suite required updating two
tests that had encoded the old (buggy) starved-cycle behavior as
expected (`tests/appmanager/test_rte_appmanager.c` -
`test_checkpoint_paused_skips_stage_not_starves_cycle`,
`test_checkpoint_null_vital_channel_is_not_a_startup_error`).

### 2.5 Known open item: manual ONLINE confirmation on simultaneous dual-reboot

Integrator's own stated design, not yet implemented: *"note that there is
a special case when both systems reboot and at startup there is no link
between redundant clusters - in this case user must confirm whether
[this] machine can be ONLINE"* - i.e. the existing automatic
older-timestamp-wins startup tie-break (REQ-DUAL-NEGOTIATOR-003) should
defer to an operator when *neither* side can reach the other at boot,
rather than each side deciding independently with no way to detect a
genuine split-brain. Blocked on the operator-interaction mechanism itself
(environment variable set at launch vs. a file-based signal the process
waits on vs. something else) - not yet specified.

### 2.6 Known correlation with ADR-027 §2.5's open reconnect livelock

§2.1/§2.4's fix makes the reboot *fire* correctly on a sustained outage,
but full end-to-end *recovery* afterward can still be blocked by the
separate, already-open reconnect livelock (ADR-027 §2.5: two
independently-retrying UDP endpoints, each minting a fresh ephemeral port
per attempt, can keep missing each other indefinitely). Observed directly
while verifying this fix: after `a-west`'s negotiation-loss reboot fired
correctly, `a-west` cycled through several more Docker-level restarts
before eventually recovering once `b-west`'s own retry cadence happened
to line up - the same underlying issue as ADR-027 §2.5, not a new one.
`tests/robot/fault_injection.robot`'s own "Extended Network Partition
Triggers Self-Reboot" test cases (§3) may intermittently show this
correlation until §2.5 is fixed separately.

## 3. Verification

- `safeAPIFreamwork`'s own `ctest --test-dir build`: 27/27, including the
  two rewritten `test_rte_appmanager.c` cases above.
- `safeAPIRBC2oo2` local `smoke.sh`: consistently clean across many runs
  after every fix in this ADR, no false-positive reboots observed.
- Real 6-container Docker, manual `docker network disconnect` on
  `a-west` (ONLINE at the time): confirmed live, before §2.4's fix, the
  negotiation-watchdog reboot never fired (100% CPU, zero reboot lines,
  24+ seconds observed); after the fix, the same fault reboots within
  ~6s and CPU returns to idle. Full recovery after reconnect is subject
  to §2.6's own caveat.
- `tests/robot/fault_injection.robot`: two new "Extended Network
  Partition Triggers Self-Reboot" cases (`a-west`, `c-west`) added
  specifically to give this escalation standing regression coverage,
  run via `etc/run_robot_tests.sh --full` or VS Code's RobotCode test
  explorer. Timing constants there
  (`EXTENDED_PARTITION_SECONDS`/`EXTENDED_RECOVERY_TIMEOUT`) needed
  real-world calibration, not just the nominal threshold value - the
  "stale detection" delay (~6s) that precedes a down-since timestamp
  even being set is itself part of the real end-to-end time needed, and
  an initial attempt sized to exactly the nominal minimum reproducibly
  failed to trigger before the fault window closed.

## 4. Location

- `safeAPIFreamwork/src/appmanager/rte_appmanager.c` (REQ-APPMANAGER-011)
- `safeAPIFreamwork/tests/appmanager/test_rte_appmanager.c` (two tests
  rewritten)
- `safeAPIFreamwork/docs/requirements/SRS.md` (REQ-APPMANAGER-011,
  REQ-APPMANAGER-008 reworded)
- `safeAPIRBC2oo2/src/application/AB/channel_ab.c` (`channel_ab_check_channel_down_reboot()`),
  `channel_ab_io.c` (down-since marking, both rx tasks),
  `channel_ab_negotiate.c` (watchdog timeout formula, readable state-change logging),
  `channel_ab_types.h` (down-since fields)
- `safeAPIRBC2oo2/src/application/C/monitor_c.c` (`monitor_c_check_channel_down_reboot()`),
  `monitor_c_io.c` (down-since marking, B's link deliberately excluded),
  `monitor_c_types.h` (down-since fields)
- `safeAPIRBC2oo2/src/application/common_config.h`
  (`SAFEAPI_EXAMPLE_CHANNEL_DOWN_REBOOT_MS`, new reason codes,
  `SAFEAPI_EXAMPLE_NEGOTIATION_WATCHDOG_TIMEOUT_MS` reworked)
- `safeAPIRBC2oo2/tests/robot/fault_injection.robot` (two new extended-partition cases)

# ADR-017: Checkpoint Rendezvous and Clock Synchronization for Distributed Vital Channels

Status: Draft
Date: 2026-08-05
Applies to: `sapi_checkpoint` (new), `sapi_clocksync` (new), and how both
plug into the existing `sapi_channel` / `sapi_checksum` / `sapi_ipc`
/ `sapi_watchdog` modules.

## 1. Context

The vital channels that vote on a result (`sapi_channel`) can run on
separate physical machines, potentially in separate geographic locations
(the repo already has a `2oo2-geographic-redundancy` example for
active/standby site failover). This ADR addresses a different problem:
**within a single voting cycle**, how do independently-clocked,
independently-scheduled channels agree on *which* cycle's results they're
comparing, and what happens if a peer's result doesn't show up?

True wall-clock simultaneity across a network is not achievable and is
the wrong safety argument to depend on. What this ADR actually delivers
is a **bounded-time rendezvous on a logical checkpoint ID**, which is the
standard way distributed safety systems make this work: two channels are
treated as "at the same point" not because their clocks agree, but
because they both reached checkpoint N and confirmed it to each other
within a fixed timeout — and if that confirmation doesn't happen in time,
that's a fault, handled the same way any other vital fault is (safe-state
/ watchdog action), not silently ignored.

This closes three things already flagged elsewhere in this repo as
designed-but-not-built:

- `docs/REDUNDANCY_ARCHITECTURE.md` already specifies a
  `sapi_channel_checkpoint()` / `sapi_checkpoint_config_t` API (checkpoint
  ID, max delay, expected node count) in prose and example code, but
  `grep -rn "sapi_channel_checkpoint\|sapi_checkpoint_config_t" include
  src` returns nothing — it was never implemented.
- `include/safeapi/watchdog/sapi_watchdog.h` already has a
  `SAPI_WATCHDOG_CHECKPOINT` enumerator, unused by any real checkpoint
  logic.
- ADR-001's layered diagram names an empty "L1 Safety Communication Layer
  (future ADR)" slot between the OAL (`sapi_ipc`/`sapi_timer`) and the
  application layer (`sapi_channel` lives here); this ADR is that
  future ADR.
- ADR-008 (`sapi_channel`, a local two-channel comparator written before
  `sapi_channel`'s fuller 2oo2/2oo3/NMR implementation was known
  about) explicitly deferred "EN 50159-style message integrity if
  channels run on separate physical nodes... expected to build on
  `sapi_ipc` in a future ADR." This is that ADR. `sapi_checkpoint` and
  `sapi_clocksync` are designed to plug into `sapi_channel` (the
  more complete, actively-developed implementation), not `sapi_channel`.

## 2. Decision

### 2.1 Reuse the EN 50159 envelope that already exists — don't rebuild it

`sapi_checksum.h`'s `sapi_vital_message_t` + `sapi_checksum_vital_message_create()`
/ `sapi_checksum_vital_message_verify()` already implement most of what an
EN 50159-style message envelope needs: a monotonic `sequence_number`
(defends against repetition, deletion, insertion, re-sequencing), a
`sender_id`, a `timestamp_ms`, and CRC-64 (defends against corruption).
`sapi_checkpoint` (below) uses this wire format directly for the
checkpoint-arrival handshake instead of inventing a second envelope
format — the earlier plan to add a standalone `sapi_synclink` module is
dropped for exactly this reason.

### 2.2 `sapi_checkpoint`: bounded rendezvous, no new transport backend

`sapi_checkpoint` adds **no new backend of its own**. It operates on an
already-initialized `sapi_channel_t *` and reuses that instance's
already-registered `backend_send`/`backend_recv` callbacks (which is how
"pluggable" is achieved — whatever transport the integrator registered
for voting, e.g. `sapi_ipc` over POSIX/RTOS/a real network link, is what
checkpoint messages travel over too). This keeps the whole design
consistent with ADR-005's established pattern instead of adding a
parallel plug-in point.

```c
sapi_status_t sapi_channel_checkpoint(sapi_channel_t *handle,
                                       const sapi_checkpoint_config_t *config);
```

`sapi_checkpoint_config_t` keeps the field names already used in
`REDUNDANCY_ARCHITECTURE.md`'s examples (`checkpoint_id`, `max_delay_ms`,
`expected_node_count`) so those examples become literally true instead of
needing to be rewritten, plus an optional `watchdog` handle
(`sapi_watchdog_t`, may be NULL) that gets kicked on a successful
checkpoint.

Algorithm: broadcast a small checkpoint-arrival message (`checkpoint_id`
as both the `sapi_vital_message_t` sequence number and payload) to every
channel via the vital channel's own `backend_send`, then
`backend_recv`-poll each channel bounded by `max_delay_ms`, verifying
each reply with `sapi_checksum_vital_message_verify()` and discarding any
that fail CRC or carry the wrong `checkpoint_id` (defends against
corruption and cross-checkpoint mixups — a corrupted or stale reply must
never count toward quorum). If at least `expected_node_count` valid
replies arrive in time: kick the optional watchdog and return
`SAPI_STATUS_OK`. Otherwise: call
`sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE, ...)` directly (the same
pattern `sapi_channel_receive()` already uses on a voting
disagreement — see its own doc comment) and return
`SAPI_STATUS_TIMEOUT`. `SAPI_WATCHDOG_CHECKPOINT` now has real behavior
behind it: an integrator can additionally configure a watchdog of that
type with its own independent action (log/safe-state/reboot/failover) for
liveness monitoring across many checkpoints, on top of the immediate
safe-state entry a single failed checkpoint already triggers.

### 2.3 `sapi_clocksync`: diagnostic only, never the basis of correctness

A small pluggable backend (`sapi_clocksync_backend_t` +
`sapi_clocksync_register_backend()`, matching `sapi_timer`'s convention
exactly) exposing `sapi_clocksync_get_offset_ms()` and
`sapi_clocksync_get_quality()`. An integrator implements this against
whatever real synchronization mechanism their hardware has (PTP, GPS
discipline, NTP, or a custom link). Its purpose is diagnostics (log
correlation across sites) and *sizing* `max_delay_ms` for a given
deployment's known clock/network jitter — **not** determining whether two
channels are "at the same point in time." That correctness argument
comes entirely from section 2.2's checkpoint-ID rendezvous, which works
even with zero clock synchronization. This is stated explicitly in the
header so nobody later assumes "clocks report low offset" implies
"results are simultaneous" — they don't; only a confirmed matching
checkpoint ID within the timeout does.

## 3. Consequences

- Positive: no new envelope format, no new transport backend — both new
  modules are thin, and reuse everything already built and tested
  (`sapi_checksum`, `sapi_ipc`'s backend registration, `sapi_watchdog`,
  `sapi_safestate`).
- Positive: `REDUNDANCY_ARCHITECTURE.md`'s existing usage examples
  (checkpoint_id/max_delay_ms/expected_node_count) become real rather
  than illustrative-only.
- Negative / scope limit: `sapi_clocksync` provides no protocol
  implementation (no bundled PTP/NTP client) — only the pluggable
  interface and the explicit warning against misusing it for correctness.
  A real clock-sync protocol implementation is integrator-supplied,
  same as every other OAL backend.
- Negative / accepted risk: `sapi_checksum`'s `sender_id` is not
  cryptographically verified (no MAC/signature); on an open transmission
  system (untrusted network) this is a real masquerade-defense gap, not
  fully closed by this ADR. Flagged here rather than silently assumed
  away — a future ADR should address authenticated messaging for open
  transmission deployments per EN 50159 §5 if that deployment case is
  needed.
- Deferred: `sapi_checkpoint`'s relationship to `sapi_channel` (ADR-008)
  is not resolved by this ADR — `sapi_channel` remains a working, simpler
  same-build two-channel comparator; whether it should be deprecated in
  favor of `sapi_channel` + `sapi_checkpoint` is a separate decision
  for the team, not made here.

## 4. Location

`include/safeapi/checkpoint/sapi_checkpoint.h` +
`src/checkpoint/sapi_checkpoint.c` (target `safeapi::checkpoint`, links
`safeapi::vital_channel`, `safeapi::checksum`, `safeapi::watchdog`,
`safeapi::safestate`). `include/safeapi/clocksync/sapi_clocksync.h` +
`src/clocksync/sapi_clocksync.c` (target `safeapi::clocksync`, links
`safeapi::status`/`safeapi::types` only — no dependency on checkpoint or
vital_channel).

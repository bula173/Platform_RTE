# ADR-027: UDP Transport for `rte_netlink`

Status: Accepted, with a known open issue (§2.5) - the transport decision
and Phase 1 are done and verified; Phase 2's raw-link liveness/ordering
work is verified for most restart/partition scenarios but has a
reproducible reconnect livelock specific to `a-east` restarting, not yet
fixed. See §2.5 before treating this migration as fully closed.
Date: 2026-08-18
Applies to: `include/rte/netlink/rte_netlink.h` (doc contract only -
no API/ABI change), `RBC_GP`'s
`src/posix_osadapter/rte_posix_osadapter_netlink.c`,
`src/application/AB/channel_ab_negotiate.c`/`channel_ab_io.c`/
`channel_ab_types.h`, `src/application/C/monitor_c_io.c`/
`monitor_c_types.h`, `src/application/common_config.h`,
`tests/robot/fault_injection.robot`.

## 1. Context

`RBC_GP`'s 6-container RBC topology communicated over real TCP via
`rte_netlink`. A live field bug (a container whose peer restarted with a
fresh TCP connection was never detected as dead, because a framework-level
bug in `rte_dual_channel.c` was masking real transport failures as
generic timeouts - fixed separately, see the dual-channel hard-fault
propagation fix committed immediately before this ADR) prompted a broader
question: should this link's reliability semantics keep depending on
TCP's own connection-oriented guarantees at all?

Decision, made directly by the framework's integrator: move `rte_netlink`
to UDP, with message ordering, deduplication, and liveness detection
becoming explicitly the application/`rte_dual_*` layer's responsibility
rather than something the transport provides implicitly.

This ADR was executed in two phases, both covered here:
- **Phase 1**: swap the transport underneath `rte_netlink` and get the
  one link that already had a full application-layer reliability stack
  (`rte_dual_msgchannel`/`rte_dual_channel`, ADR-020 - the inter-site
  negotiation link) working correctly on top of it. Initially scoped to
  leave the raw, unframed links (`channel_ab_io.c`'s same-site peer link,
  `monitor_c_io.c`'s C-links) as explicit follow-on work (§2.1-2.3).
- **Phase 2**: real 6-container Docker testing of the Phase-1-only build
  (restarting a container mid-run, the same class of scenario that
  motivated this ADR in the first place) reproduced a genuine regression
  in exactly the links Phase 1 had deferred: with no transport-level
  disconnect signal under UDP, a restarted peer's counterpart could get
  stuck waiting forever, wedging both sides. Closed the same session,
  once demonstrated empirically rather than left as a theoretical gap
  (§2.4).

## 2. Decision

### 2.1 New UDP backend, same file, same accessor (REQ-OAL-NETLINK-014)

Per ADR-005/ADR-018 (concrete backends live in the consumer, not the
framework), `RBC_GP/src/posix_osadapter/rte_posix_osadapter_netlink.c`
was rewritten in place - same `rte_posix_osadapter_netlink()` accessor, so
registration and every include site needed zero changes. This is an
outright migration, not an opt-in toggle; no second backend file was kept.

**HELLO/HELLO_ACK handshake.** UDP's `connect()` performs no network I/O
and gives no signal the peer is reachable, unlike TCP's `connect()`/
`accept()`. Without a handshake, `rte_netlink_open()` would return
`RTE_STATUS_OK` for a CONNECT role even if nothing were listening yet,
silently breaking its own documented contract ("OK means usable").
Two fixed 1-byte magic datagrams (`HELLO`/`HELLO_ACK`) are exchanged
entirely before `rte_netlink_open()` returns a handle - LISTEN binds and
waits for the first `HELLO` (learning its peer's address from it, the UDP
analogue of `accept()`, then `connect()`s to lock that peer), CONNECT
dials immediately and retries `HELLO` on a short period until it sees
`HELLO_ACK`, bounded by `connect_timeout_ms` either way.

**One datagram per call, no byte accumulation.** The old TCP backend's
`transfer_all()` looped `poll()`+`recv()`/`send()` until exactly
`message_size` bytes transferred - correct for a byte stream, actively
wrong for UDP (a `recv()` call returns one whole datagram; looping to
"finish" a supposedly-partial receive would incorrectly wait for bytes
that will never arrive from a *different* datagram). `backend_send()`/
`backend_receive()` now do a single `send()`/`recv()` per call.
`backend_receive()` uses `MSG_TRUNC` so an oversized datagram is
detectable (real length reported even past `buffer_size`) rather than
silently truncated, and any length mismatch against the link's fixed
`message_size` is reported as `RTE_STATUS_DATA_CORRUPTION` - a wire
protocol violation, not a value to accept partial-length.

**`RTE_STATUS_HARDWARE_FAULT` becomes opportunistic, not primary.**
TCP's `recv()==0`/`ECONNRESET`/`EPIPE` gave every caller a reliable
"peer is gone" signal; UDP has no such thing. The one exception kept is
`ECONNREFUSED`, which a *connected* UDP socket can surface on a
subsequent `send()`/`recv()` after the kernel receives an ICMP
Port-Unreachable - real when it fires, but best-effort (silently
suppressed by NAT/firewalls, never fires for a merely slow/partitioned
peer). The header doc for `rte_netlink_send()`/`_receive()` was updated
to state this plainly rather than imply a guarantee no UDP backend can
make (new `REQ-OAL-NETLINK-014`: this service provides no ordering/
dedup/delivery guarantee of its own - any such guarantee is the caller's
job).

### 2.2 Negotiation link: no functional change required (validates ADR-020)

`channel_ab_negotiate.c`'s existing teardown trigger already closed the
link on a plain `RTE_STATUS_TIMEOUT` from the ACK wait - it never
depended on `HARDWARE_FAULT` specifically. This means `rte_dual_channel`/
`rte_dual_msgchannel`'s existing sequence+CRC+ACK machinery (ADR-020)
required **zero functional changes** to keep working correctly under UDP;
this is a validation of that design, not a coincidence.

**Added hardening** (small, `RBC_GP`-only, not a framework
change): a single dropped datagram is a routine, expected event under UDP
even on a healthy link (no transport-level retransmission), unlike a TCP
`ack_timeout_ms` genuinely meaning something was wrong. Tearing the
negotiation link down on the very first miss would reset
`rte_dual_negotiator_t`'s tie-break state machine (back to
`RTE_DUAL_STATE_IDLE`) far more often than a real fault warrants. A new
`ctx->neg_consecutive_send_miss` counter
(`RTE_EXAMPLE_NEG_SEND_MISS_THRESHOLD = 3`, `common_config.h`) requires
several consecutive misses before teardown fires; a receive-side hard
fault (a status that is neither `OK` nor `TIMEOUT`) still tears down
immediately, no hysteresis - that is never a routine event.

### 2.3 A UDP handshake has real, non-zero startup latency - two related fixes

Initial end-to-end testing (`smoke.sh`, the real 6-process topology)
surfaced that a UDP handshake, even a fast one, is not free the way TCP's
near-instant `accept()`/`connect()` was, and this interacted badly with
existing tuning:

- An earlier version of `open_listen_udp()` added a fixed 250ms
  "grace-drain" window after sending its own `HELLO_ACK`, to safely
  absorb any `HELLO` retransmit still in flight from the peer before
  `open()` returned. This unconditionally delayed *every* LISTEN-role
  `open()` by up to 250ms. Root-caused (via `sample`/backtrace on a
  process observed spinning at ~99% CPU) to the framework's own
  `rte_channel_checkpoint()` correctly entering
  `RTE_SAFESTATE_LEVEL_SAFE` (REQ-CHECKPOINT-003, an intentionally
  non-returning halt) after `RTE_EXAMPLE_AB_CHECKPOINT_MAX_DELAY_MS`
  (150ms, tuned for TCP) was blown by this added startup latency - this
  was the framework working exactly as designed, reacting correctly to a
  transport that had genuinely gotten slower to establish.
  **Fix**: removed the fixed grace-drain wait entirely; `open()` now
  returns as soon as the handshake completes. A stray duplicate `HELLO`/
  `HELLO_ACK` byte that outlives the handshake is instead tolerated
  cheaply, on demand, inside the steady-state `backend_receive()` path
  (a lone 1-byte datagram matching either magic value is silently
  dropped and the wait continues, rather than reported as
  `RTE_STATUS_DATA_CORRUPTION`) - paid only if such a datagram actually
  arrives, not unconditionally on every open().
- Even without that fixed delay, a multi-process startup cascades several
  sequential per-process link-establishment steps (e.g. a process opens
  its peer link before its negotiation link), and each CONNECT-role
  link's own `HELLO` retry period adds to that chain. `common_config.h`'s
  `RTE_EXAMPLE_AB_CHECKPOINT_MAX_DELAY_MS` was raised 150ms -> 300ms
  (still well under the 500ms cycle period) and the backend's own
  `POSIX_NETLINK_UDP_HELLO_PERIOD_MS` retry granularity was tightened
  100ms -> 20ms, so the worst-case startup convergence chain shrinks and
  the checkpoint budget has realistic headroom for it. Steady-state
  round-trip latency (once links are established) is unaffected - sub-
  millisecond on a healthy loopback link, confirmed by dedicated backend
  test coverage (§4).

### 2.4 Phase 2: raw-link liveness and ordering, closing a real Docker-confirmed regression

Real 6-container Docker testing of the Phase-1-only build (`docker
restart` on `a-west` mid-run - the exact scenario that originally
motivated this whole ADR) reproduced a genuine regression: `b-west`'s raw
peer link had no way to detect that `a-west`'s process had restarted
(under UDP there is no transport-level disconnect signal the way TCP's
`recv()==0`/`ECONNRESET` gave it), so it never redid the HELLO handshake,
and `a-west` was left waiting forever for a `HELLO` that never came -
`a-west` crash-looped, `b-west` went silent. Under the old TCP backend
this exact scenario self-healed automatically; under UDP-with-Phase-1-only
it did not. Closed the same session rather than shipped as a known gap,
once demonstrated empirically (see the "Recommended follow-up" pattern
this framework already uses elsewhere - a theoretical gap noted in an ADR
is not the same evidentiary bar as a reproduced failure).

**Staleness-timeout reconnect trigger**, mirroring §2.2's negotiation-link
pattern but for links with no ACK/sequence layer of their own: each raw
link's rx task (`channel_ab_io_peer_rx_task_entry()`,
`channel_ab_io_m136_rx_task_entry()`,
`monitor_c_io_rx_a_task_entry()`/`_rx_b_task_entry()`) now counts
consecutive `RTE_STATUS_TIMEOUT` results from its own bounded
`rte_netlink_receive()` call. `RTE_EXAMPLE_LINK_STALE_TIMEOUT_COUNT`
(`common_config.h`, = 2) consecutive timeouts - meaning
`RTE_EXAMPLE_LINK_TIMEOUT_MS` (3000ms) of total silence - closes the
link for reconnect, the same as an outright hard-fault status always did.
A single successful receive resets the counter to 0.

**Duplicate/replay rejection reusing existing wire fields, not a new
framework primitive.** An earlier design sketch (Phase 2, before
implementation) proposed a new generic-capacity sequence+CRC framework
module (`rte_dual_seqframe`), reasoning from `rte_vital_message_t`'s
248-byte payload cap being too small for the peer link's 273-byte frame.
That reasoning does not survive contact with what actually needs
ordering: `AB_SAMPLE`/`M136`/`ANSWER` already carry their own `cycle`
counter in their existing wire format (`channel_ab_wire.c`/
`monitor_c_wire.c`), which every consumer already decodes - no new bytes
on the wire, no new framework module, are needed to detect a stale or
duplicate frame. Each rx task now rejects (does not write into its
`rx_slot_t`/`answer_slot_t`) an incoming frame whose decoded `cycle` is
not strictly newer than the last one accepted
(`(int32_t)(decoded_cycle - last_accepted_cycle) <= 0`). A frame that
fails its *own* existing integrity check (`AB_SAMPLE`'s CRC-64; `M136`/
`ANSWER` have none, pre-existing and unrelated to this migration) is left
to pass through unchanged, preserving whatever existing corruption
handling already covers it - this check only ever suppresses a
validly-decoded but stale/duplicate frame. `CHECKPOINT_REQUEST`/`_REPLY`
(already self-checking via a full `rte_vital_message_t`) and
`SITE_STATE` (a level-triggered current-state broadcast with no natural
sequence field, self-correcting next cycle if a stale one is used - not
worth wrapping) are both left as-is.

**A second, empirically-found instance of the §2.3 budget problem - and
why a bigger budget alone wasn't the real fix.** Retesting in Docker
after the staleness-reconnect fix surfaced the same class of issue one
level up: the *reconnecting* side re-enabled the built-in checkpoint
(`ctx->checkpoint_cfg.voter`) the instant its own link's handshake
completed, immediately racing the first real round trip on a link that
had just come back - under real Docker bridge-network conditions (not
loopback), this occasionally still exceeded
`RTE_EXAMPLE_AB_CHECKPOINT_MAX_DELAY_MS`, triggering the same
intentional `RTE_SAFESTATE_LEVEL_SAFE` halt as §2.3, this time on the
*reconnecting* side rather than at startup. The budget was raised once
more (300ms -> 450ms, 90% of the 500ms cycle period) as a first attempt,
but a `docker restart` of `a-east` still reproduced the same halt on
`b-east` afterward - proving this was not (only) a margin problem: a
handshake completing proves the *socket* is usable, not that the peer's
own side has resumed *sending*, and no fixed budget reliably bridges
that gap under real jitter.

First redesign: `checkpoint_cfg.voter` no longer re-enabled at handshake
completion at all - a new `checkpoint_pending_reenable` flag deferred it
until the peer rx task's own first *successful receive* after reconnect,
i.e. until the link concretely proved it was carrying data. This traded
one bug for another: `rte_channel_checkpoint(NULL, ...)` returns
`RTE_STATUS_INVALID_PARAM` gracefully rather than SAFE-halting (by
design - see `rte_appmanager_run()`'s own comment on this), which
`rte_appmanager_pace_failed_checkpoint()` retries in a busy-wait paced
by the very budget this fix meant to stop racing. Gating checkpoint's own
*send* behind "wait for incoming data" turned out to be circular: if both
sides of a link reconnect around the same time, each one's checkpoint
stays paused waiting for the other to send first, and neither's
checkpoint (the only thing that would send) is enabled to do so - a
genuine mutual deadlock, reproduced live (`docker restart a-east`, both
`a-east` and `b-east` spinning at 100% CPU in the pacing busy-wait,
neither progressing). Fixed by adding a bounded fallback: a
`checkpoint_reenable_deadline_ms` set at reconnect time
(`RTE_EXAMPLE_CHECKPOINT_REENABLE_GRACE_MS` = 600ms) that re-arms
checkpoint on its own if the deadline passes with nothing received -
re-enable now fires on whichever of the two signals (data arrived, or
deadline elapsed) comes first, breaking the circularity while keeping the
stronger data-arrived signal as the fast path. Verified: the deadlock no
longer reproduces on the scenario that found it.

### 2.5 Known open issue: reconnect livelock between two independently-retrying endpoints

Retesting the same `a-east` restart scenario after the deadlock fix
surfaced a **further, deeper, and still-unresolved** issue: `a-east` and
`b-east` both settle into a perpetual cycle of "peer link stale -> close
-> reconnect -> (re)established -> stale again" every few seconds,
indefinitely (confirmed by direct log observation over 30+ seconds of
real wall time, not a test-harness artifact - `RestartCount` stays flat,
so this is not a crash loop, but cross-compare/checkpoint never
stabilizes either). This did **not** reproduce on the equivalent
`a-west` restart earlier in the same session, so it is not simply "any
container reboot" - there is some asymmetry or timing correlation
specific to this scenario not yet isolated.

Working theory, not yet confirmed: `open_connect_udp()` mints a fresh
local ephemeral UDP port on every reconnect attempt (a plain `socket()`
call, no `SO_REUSEPORT`/fixed local port). If both sides end up
reconnecting on independent, unsynchronized cadences (each driven by its
own `RTE_EXAMPLE_LINK_STALE_TIMEOUT_COUNT`-based detection, with no
coordination between them), the LISTEN side can `connect()` to (lock
onto) a peer address:port that is already stale by the time the CONNECT
side's *next* reconnect attempt fires from a *different* ephemeral port
- each side's retry can keep "just missing" the other's current attempt,
with no guarantee the pattern ever breaks on its own. This is a
liveness/scheduling problem, not a correctness bug in any single
exchange - distinct in kind from every other fix in this ADR, which were
all about a single round-trip's own timing or gating. A real fix likely
needs either a stable local port reused across a link's reconnect
attempts (so the LISTEN side's lock survives a CONNECT-side retry), or
deliberate backoff/jitter so retry cadences decorrelate rather than
staying in lockstep.

**Status: open.** `tests/robot/fault_injection.robot`'s
"Container Reboot Recovers - A East" test case (and by the same
mechanism, "Network Partition Recovers - A East") reliably reproduce it
and will fail until this is fixed - left failing deliberately rather
than adjusted to pass, so the suite keeps surfacing this until it's
actually resolved. Do not treat a green run of just the `a-west`/`b-west`
cases as evidence this class of issue is closed.

### 2.6 Deliberately not addressed by this ADR

`rte_safechannel.c`'s voter channels: confirmed zero consumers in
`RBC_GP` today, and already has no reconnect capability
regardless of transport. Not touched by either phase.

## 3. Verification

- `RBC_GP`'s `tests/posix_osadapter/test_rte_posix_osadapter.c`:
  three new cases against the real UDP backend over loopback -
  `test_netlink_round_trip` (full handshake + bidirectional exchange,
  proves `open()` returning `OK` means genuinely usable, not just that a
  socket call succeeded), `test_netlink_size_mismatch` (a 4-byte send
  against an 8-byte receive correctly yields `RTE_STATUS_DATA_CORRUPTION`,
  not silent truncation), `test_netlink_open_timeout`
  (`connect_timeout_ms` honored, not blocked indefinitely,
  REQ-OAL-NETLINK-002). `ctest --test-dir build`: 2/2 (this project has
  no ctest coverage of its own beyond the POSIX backend integration
  suite - framework-level dispatch logic is covered by
  `Platform_RTE`'s own `test_rte_netlink`, unaffected by this ADR
  since it only exercises the transport-agnostic layer via a mock
  backend).
- `Platform_RTE`'s own `ctest --test-dir build`: 27/27, unaffected -
  no framework source files changed in this phase (`rte_netlink.h`'s
  doc-comment-only update aside).
- `.claude/skills/run-RBC_GP/smoke.sh`, both scenarios (basic
  negotiation/cross-compare/checkpoint, and the DISAGREE -> REBOOT ->
  TAKEOVER -> state-transfer failover chain): consistently clean across
  many runs after the §2.3/§2.4 fixes, versus reproducible failures
  beforehand (checkpoint SAFE-halt on the very first rendezvous, an
  intermittent cross-compare stall, or the checkpoint-pacing deadlock) -
  this is the evidence base for those fixes, not a claim derived from
  code review alone. This suite runs entirely on localhost processes, not
  containers - it does **not** exercise §2.5's open livelock (which is
  specific to real cross-container network conditions/timing).
- Real 6-container `docker compose up` plus manual `docker restart`/
  `docker network disconnect` on each of the 6 RBC services: confirmed
  clean recovery for `a-west`/`b-west`/`b-east`-as-target scenarios;
  confirmed **reproducible failure** (§2.5's livelock) for `a-east` as
  the restarted container. Not yet re-tried for every one of the 6
  services x 2 fault types systematically - that systematic sweep is
  exactly what `tests/robot/fault_injection.robot` (§2.4) is for, run via
  `etc/run_robot_tests.sh --full` or VS Code's RobotCode test explorer
  (`tests/robot/` is already configured as this project's Robot
  Framework root, `.vscode/settings.json`) - expect its `A East` cases to
  fail until §2.5 is resolved.

## 4. Location

- `RBC_GP/src/posix_osadapter/rte_posix_osadapter_netlink.c`
  (rewritten)
- `RBC_GP/src/application/AB/channel_ab_types.h`,
  `channel_ab_negotiate.c` (`neg_consecutive_send_miss` hysteresis);
  `channel_ab_io.c` (peer-link staleness reconnect trigger, AB_SAMPLE/
  SITE_STATE dedup, `checkpoint_pending_reenable`/
  `checkpoint_reenable_deadline_ms` deferred-checkpoint-reenable)
- `RBC_GP/src/application/C/monitor_c_types.h`, `monitor_c_io.c`
  (same staleness/dedup pattern for C's two links)
- `RBC_GP/src/application/common_config.h`
  (`RTE_EXAMPLE_NEG_SEND_MISS_THRESHOLD`,
  `RTE_EXAMPLE_AB_CHECKPOINT_MAX_DELAY_MS` raised to 450ms,
  `RTE_EXAMPLE_LINK_STALE_TIMEOUT_COUNT`,
  `RTE_EXAMPLE_CHECKPOINT_REENABLE_GRACE_MS`)
- `RBC_GP/tests/posix_osadapter/test_rte_posix_osadapter.c` (new
  netlink cases)
- `RBC_GP/tests/robot/fault_injection.robot` (new - container
  reboot/network partition matrix, all 6 RBC services), `etc/run_robot_tests.sh`
  (`--full` flag to include it)
- `include/rte/netlink/rte_netlink.h` (doc contract: `REQ-OAL-NETLINK-014`,
  `HARDWARE_FAULT`/`DATA_CORRUPTION` semantics reworded for a
  transport-agnostic contract)
- `docs/requirements/SRS.md` §2.8 (backfilled `REQ-OAL-NETLINK-001..014`)

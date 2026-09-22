# ADR-029: RBC Train/IL/CTC Scenario - Real Multi-Train Protocol and Failover-Ready Session State

Status: Accepted (first simple logic pass, per the integrator's own
explicit scope - protocol depth and full route-topology fidelity are
deliberate follow-on work, not gaps in this pass).
Date: 2026-08-18
Applies to: `Platform_RTE`'s cross-compare/state-transfer contract is
unchanged (the widening described here is entirely `RBC_GP`-side
usage of existing framework primitives); `RBC_GP`'s
`src/application/C/*`, `src/application/AB/*`, `src/application/rbc_wire*`,
`sims/*.py`, `docker-compose.yml`.

## 1. Context

The integrator asked for a real (if deliberately simplified) slice of an
ERTMS-style RBC scenario: a Train reports its own position and gets a
Movement Authority; an Interlocking (IL) grants/extends that authority by
setting routes; a CTC observes both, one-way. Before this ADR, `C`
(`monitor_c.c`) self-generated a simulated M136 every cycle - standing in
for a train that did not yet exist - and the `sims/` Train/IL/CTC
processes were inert heartbeat-log placeholders with no sockets at all
(see those files' own pre-ADR-029 headers).

Critically, the integrator also required that a site failover (the
ONLINE channel failing, its STANDBY counterpart taking over - ADR-028's
own subject) must be invisible to the Train and IL: "Train and IL still
have contact - now with the new ONLINE channel." This meant the
existing single-scalar cross-site state-transfer snapshot
(`negotiate_extra_payload_t`, ADR-020) needed to become a real,
multi-train session table, and the sims themselves needed to stay
connected to *both* sites at once rather than reconnecting on failover.

## 2. Decision

### 2.1 Wire protocol - one shared, fixed-size envelope

Per this project's MISRA fixed-size-everything convention, every new
message kind (P0, M136, M24/M15, ROUTE_ADD, M3, M146, and the three CTC
indications) rides in ONE reused 28-byte struct (`rbc_envelope_t`,
`src/application/rbc_wire_types.h`) rather than N distinct wire structs -
fields not meaningful for a given kind are left zero. The codec
(`rbc_wire.c`/`.h`) lives at the `RBC_GP` application root (not
under `C/` or `AB/`) because BOTH C and A/B need it: this same envelope
now travels end-to-end, Train/IL -> C -> A/B -> C -> Train/CTC, with C
translating nothing - it relays the identical bytes it receives (see
2.3).

A Python mirror of this codec (`sims/rbc_wire.py`) exists because the
sims are a separate language with no shared schema - verified
byte-identical against the C encoder directly (same field values in,
identical hex out) before relying on it for any live test.

### 2.2 C is a pure relay - no session state of its own

`monitor_c.c` no longer originates anything: every connected Train
instance's P0/M136/M146 and every connected IL instance's ROUTE_ADD are
forwarded, unchanged, to BOTH A and B (mirroring the exact broadcast
discipline the old self-generated M136 already used - both channels must
see identical inputs to independently compute the same decision). A's
(or B's, in SINGLE mode) answer envelopes are relayed back out to the
right Train/CTC recipient by `train_id`. C holds no `train_session_t` of
its own (`monitor_c_types.h`'s own doc) - the MA/route decision lives
entirely in A/B, the vital 2-channel-voting decision-maker, exactly how
the M24/M15 decision already worked before this ADR. Putting MA
computation in C would put a safety-relevant decision somewhere
cross-compare can't see it.

Since a link can now carry more than one distinct event within a single
C report cycle (e.g. two trains' M136 landing the same cycle), the old
single-slot "latest value only matters" primitives (`answer_slot_t`,
`rx_slot_t`) were replaced on these links by a small fixed-capacity
single-producer/single-consumer queue (`rbc_envelope_queue_t`,
`rbc_wire_types.h`) drained fully every cycle, not read-latest-only - see
that type's own doc for the full rationale and its bounded-drop
behavior (no dynamic growth, CLAUDE.md).

**C does not block its own startup waiting for a Train/IL/CTC client.**
Unlike A/B (this project's own co-deployed containers, expected to be
reachable within `RTE_EXAMPLE_CONNECT_TIMEOUT_MS`), a Train/IL/CTC
sim is a genuinely external, opportunistically-connecting client that may
not even be running yet. `monitor_c_init()` starts each of these links'
background rx tasks with a NULL handle and lets that task's own
reconnect-forever loop establish the connection lazily, whenever a real
client first shows up - found live: the original implementation blocked
on the initial accept the same way A/B's links do, which meant an
`etc/run_all.sh`-style boot order where C starts before any sim connects
would fail `monitor_c_init()` outright with `RTE_STATUS_TIMEOUT`.

### 2.3 A/B: the train-session table

`train_session_t` (`channel_ab_types.h`) - a fixed
`RTE_EXAMPLE_MAX_TRAINS`-entry array (`sessions[]`) on
`channel_ab_context_t` - is the actual decision state: `in_use`,
`train_id`, `cycle`/`d_lrbg` (last position report), `granted_length`
(the running sum of every ROUTE_ADD so far - "first simple pass" per the
integrator: no real route topology, every ROUTE_ADD is worth a fixed
`RTE_EXAMPLE_MA_ROUTE_LENGTH_M`), `ma_seq`, `ma_acked`, plus two
local-only scratch flags (`ma_pending_send`, and the sticky
`ctc_connected_sent`/`ctc_ma_granted_sent` - see their own doc for why
these must be sticky rather than a per-cycle transient flag: a
transient "just connected this cycle" flag can be silently lost forever
if `should_forward_to_c` (needs AGREE) happens to be false on that exact
cycle - found live, a train's own CTC_CONNECTED notification never
arrived the first time this was tried).

`channel_ab_pre_execute()` drains the (possibly multi-train,
interleaved) inbound queue every cycle and applies each envelope to the
matching session by `train_id` - STANDBY sites drain-and-discard without
processing (they inherit sessions from the negotiate-transfer snapshot
instead, see 2.4, not from their own train stream).
`channel_ab_post_execute()` scans `sessions[]` for what changed and sends
the right downstream envelope(s) - possibly several per cycle now (one
M24/M15 per train with a fresh position report, one M3 per train with a
pending MA, the matching CTC indication) - a real, multi-envelope-per-
cycle send pattern replacing the old single-answer-per-cycle one.

### 2.4 Cross-site failover continuity - the actual point of this ADR

`channel_ab_negotiate.c`'s `negotiate_extra_payload_t` (the payload
continuously piggybacked on the existing inter-site negotiation
beacon, ADR-020) now carries the WHOLE `sessions[]` table, not the old
single-scalar `{cycle, dlrbg, decision}` snapshot. `apply_state_transfer()`
copies it into a promoted STANDBY's own `ctx->sessions[]`
**unconditionally** - unlike the transferred `cycle` counter (still
policy-gated by `RTE_EXAMPLE_TRANSFER_POLICY_ENV`, since it is
purely this channel's own iteration count), there is no "restart"
concept for a live Movement Authority: a promoted site refusing to
remember an in-flight MA would be actively unsafe, not an operator
preference (REQ-RBC-006). This unconditional session-table transfer is
the literal mechanism that makes "Train and IL still have contact"
true - the moment a site promotes, it already has every connected
train's granted MA length/sequence in memory, before the Train/IL sims
even notice anything happened.

Cross-compare (`channel_ab_crosscompare.c`) now votes on the whole
session table instead of one scalar M24/M15 value. `rte_cross_comparator_execute()`
does a raw `memcmp()`, so the compared payload is the WIRE-ENCODED form
(`channel_ab_wire_encode_sessions()`, no padding by construction), not
the raw `train_session_t` struct directly - that struct mixes
`bool`/`uint8_t`/`uint32_t` members, so the compiler is free to insert
padding bytes a raw `memcmp()` would treat as significant.

### 2.5 The real debugging story: three genuine synchronization bugs

Getting an actual end-to-end AGREE (not just code that compiles) required
finding and fixing three distinct, non-obvious timing bugs, each rooted
in the same underlying cause: this project's original constants
(`RTE_EXAMPLE_AB_CYCLE_SKEW_TOLERANCE`,
`_AB_DUAL_TRANSFER_TIMEOUT_MS`, `_LINK_STALE_TIMEOUT_COUNT`) were all
tuned around C's old near-every-cycle (~600ms) self-generated M136 -
once a real Train reports on its own realistic cadence
(`RTE_EXAMPLE_SIM_PERIOD_SECONDS`, 2000ms default) against the AB
cyclic executive's own faster 500ms pacing, "fresh data every cycle"
stopped being true and every one of these margins turned out to be too
tight. Found live, in this order:

1. **AB_SAMPLE was only sent on a cycle with fresh train data.** This
   left the PEER's own cached snapshot stale by several AB cycles
   routinely (not just at startup), so a per-train "is the peer's
   session in sync with mine" check almost never lined up. **Fixed** by
   sending AB_SAMPLE (and running cross-compare) every cycle while
   ONLINE, unconditionally - the same "always every cycle" convention
   `PEER_MSG_KIND_SITE_STATE` already used - rather than gating it on
   `have_channel_data`.
2. **`RTE_EXAMPLE_AB_CYCLE_SKEW_TOLERANCE` (3) and
   `_AB_DUAL_TRANSFER_TIMEOUT_MS` (3000ms) were far too tight** for the
   new ~2s real cadence, causing spurious "peer answered for a
   too-different cycle" skips and `dual_transfer_watchdog` firing
   (entering SINGLE mode) on a perfectly healthy link - which then
   tripped a NEGOTIATION MISMATCH reboot once the two channels' SINGLE-mode
   flags disagreed. Raised to 20 and `AB_CYCLE_PERIOD_MS * 20`
   respectively - see each constant's own doc (`common_config.h`) for
   the full reasoning.
3. **Per-train readiness gate needed to be bounded, not permanent.**
   Comparing session tables byte-for-byte while the two channels are
   still mid-catch-up on an asynchronously-arriving update (a train's
   own M136, or a discrete ROUTE_ADD) reports a spurious DISAGREE - both
   sides genuinely agree, they just have not both processed the SAME
   update yet. `channel_ab_crosscompare_execute()` now skips (not
   disagrees) a train whose session does not yet field-for-field match
   its peer's, but only for `RTE_EXAMPLE_XCOMPARE_SYNC_SKIP_LIMIT`
   (10) consecutive cycles - past that, it falls through to the real
   comparator, which correctly DISAGREEs. This distinction matters
   because a discrete event (ROUTE_ADD/M146) has no "next one" to
   fall back on if its single relay datagram is lost to only one
   channel (`RTE_EXAMPLE_LINK_STALE_TIMEOUT_COUNT` was ALSO raised,
   2 -> 4, after finding this exact loss occur during an ordinary
   staleness-reconnect window) - unlike M136's own cyclic, self-superseding
   value, a lost ROUTE_ADD is a genuine, permanent divergence once it
   happens, and this bound is what keeps that from being silently
   ignored forever instead of correctly (if rarely) triggering a fault
   reaction.

**Known, accepted limitation**: this link has no sequence/dedup
numbering (ADR-027's own limitation, inherited here), so a naive
redundant-send mitigation for the loss in (3) was considered and
rejected - resending the same ROUTE_ADD would be indistinguishable from
a genuinely new one on the receiving end and would double/triple-apply
it. A later pass needs real per-envelope sequencing before any
redundant-send mitigation is safe to add.

### 2.6 Full-stack Docker hardening: a permanent safe-halt and an undersized queue

The fixes in 2.5 were found against a 6-process local run; running the
full 11-container `docker compose` stack (2 trains, both routes active
simultaneously) surfaced two further bugs that only manifest under real
multi-container CPU contention and reconnect asymmetry, both root-caused
with `gdb -p 1` (installed at runtime via `apt-get` inside the affected
container, then attached with `docker exec --privileged` - the default
container capabilities block `ptrace` entirely, so this is not available
without that flag):

1. **A permanent, unrecoverable 100%-CPU hang.** Live `docker stats`
   showed an A or B process pinned at 100% CPU indefinitely after a
   peer-link stale/reconnect event; `gdb`'s `thread apply all bt` on the
   frozen main thread showed it stuck inside `rte_safestate_enter()`,
   called from the framework's own built-in `rte_channel_checkpoint()`
   (`Platform_RTE/src/checkpoint/rte_checkpoint.c`) - a deliberate,
   by-design, permanent halt (REQ-CHECKPOINT-003: once a checkpoint
   rendezvous fails to confirm within its budget, it MUST NOT silently
   continue). This is correct behavior for a genuinely dead peer, but
   `channel_ab_io.c`'s own re-enable-after-reconnect logic
   (`checkpoint_pending_reenable`) was force-re-arming the checkpoint
   after a fixed `RTE_EXAMPLE_CHECKPOINT_REENABLE_GRACE_MS` (600ms)
   fallback that only proves the LOCAL socket is usable again, not that
   the PEER's own independent reconnect (bounded by the unrelated
   `RTE_EXAMPLE_LINK_STALE_TIMEOUT_COUNT * RTE_EXAMPLE_LINK_TIMEOUT_MS`
   ~= 12s window) has also finished - under real Docker load the two
   sides' reconnects are not synchronized, so the checkpoint was routinely
   re-armed and immediately given only its own intentionally tight 450ms
   `RTE_EXAMPLE_AB_CHECKPOINT_MAX_DELAY_MS` fault budget against a
   link that could not yet possibly answer. **Fixed** by raising
   `RTE_EXAMPLE_CHECKPOINT_REENABLE_GRACE_MS` to 15000ms (comfortably
   past that ~12s worst case) - see that constant's own doc
   (`common_config.h`) for the full reasoning. `AB_CHECKPOINT_MAX_DELAY_MS`
   itself is deliberately left unchanged: catching a genuinely dead peer
   within one cycle is the whole point of that budget, and the real fix
   is not re-arming the checkpoint prematurely, not loosening the
   checkpoint itself. Verified fixed: repeated full-stack runs with
   normal reconnect churn no longer produce a stuck process (`docker
   stats` stays under 1% CPU on every container throughout).
2. **`ctx->m136_rx_queue` (the single queue, shared across both trains
   and both ILs, that carries everything C relays to A/B) genuinely
   filling and staying full across several consecutive drain cycles**
   under transient host scheduling pressure, silently and permanently
   dropping whatever envelope arrived in that window (this queue has no
   retry/dedup, same accepted-limitation class as 2.5's point 3) - found
   live when a commanded `ADD_ROUTE` never reached `A`/`B` despite `IL`'s
   own log confirming it was sent. **Fixed** by doubling
   `RBC_ENVELOPE_QUEUE_CAPACITY` from 8 to 16 (`rbc_wire_types.h`) - a
   purely defensive widening of an already-cheap (28 bytes/entry) fixed
   buffer, not a redesign.

**New open item found while verifying the above** (not yet fixed):
when `monitor_c_check_channel_down_reboot()` actually reboots a site's C
process (`RTE_EXAMPLE_CHANNEL_DOWN_REBOOT_MS`, pre-existing ADR-028
mechanism, still firing under sustained real reconnect instability on
this test host), every Train/IL sim's `dual_link.py` socket to that site
has no way to notice - from the client's own point of view nothing
failed, so `ensure_connected()`'s `if self.sockets[site] is not None:
continue` never re-runs the HELLO handshake the freshly-restarted C
process needs to (re)register that peer, and a command sent immediately
after such a reboot can be silently swallowed. This needs either a
liveness/keepalive check on the sim's own send path or a sequence-numbered
"session generation" the client can detect changed, before it can be
considered fixed; tracked as follow-on work, not blocking for this first
pass.

### 2.7 Dual-homed sims and a real command channel

Per the integrator's own explicit choice, every sim (`sims/train_sim.py`,
`il_sim.py`, `ctc_sim.py`) opens a UDP link to BOTH sites' C at once and
keeps both open for its whole run (`sims/dual_link.py`) - "which site is
ONLINE" is inferred purely from which site actually answers (a STANDBY
site's C has nothing to forward back), not a separate status message.
This is a hand-ported Python mirror of `rte_posix_osadapter_netlink.c`'s
own HELLO/HELLO-ACK UDP handshake (ADR-027) - the sims speak this
project's real wire transport, not a plain TCP socket (an early attempt
using `SOCK_STREAM` connected to nothing at all, silently, since this
project moved off TCP project-wide in ADR-027).

A separate addition, requested mid-implementation: Robot Framework
(`tests/robot/`) needed to *drive* specific sim actions on demand (grant
a route now, report a position now) rather than only ever observing
whatever an autonomous timer produced, for deterministic test scripting.
Each Train/IL sim now also runs a small line-based TCP command server
(`sims/control_server.py`, its own dedicated port and protocol,
deliberately separate from the RBC's own UDP wire protocol) - `REPORT
<d_lrbg>`/`PING` for Train, `ADD_ROUTE [length_m]`/`PING` for IL. IL's
route-adding is now purely command-driven (no autonomous schedule at
all); Train's own cyclic M136 reporting stays autonomous (a real train
does not wait to be asked), with `REPORT` available for tests that need
a specific value on demand. These control ports are the one deliberate
exception to this project's "no host ports" `docker-compose.yml`
convention, since Robot itself runs on the host, not inside the Docker
network.

## 3. Verification

- `sims/rbc_wire.py`'s `encode()` checked byte-for-byte identical to
  `rbc_wire_encode()`'s own output for the same field values - confirms
  the Python mirror is not just "probably compatible."
- Full local (non-Docker) 6-process run (`c-west`/`c-east`/`a-west`/
  `b-west`/`a-east`/`b-east`) plus `train_sim.py`/`il_sim.py`/`ctc_sim.py`
  pointed at `127.0.0.1`: confirmed, after the fixes in 2.5, a genuine
  end-to-end run with zero reboots across the full scenario - train
  connects (P0), gets M24/M15 acks every ~2s, a commanded `ADD_ROUTE`
  produces an M3 (`length=500m`) the train acks with M146 and CTC sees as
  CTC_MA_GRANTED, and a second commanded `ADD_ROUTE` correctly produces
  `length=1000m` (the running sum, not a fresh grant) that CTC sees as
  CTC_MA_EXTENDED specifically.
- `tests/robot/rbc_scenario/` (new, one test case per file): the same scenario driven
  against the real 11-container `docker compose` stack, commanding
  `il-west`'s real control port over the network and asserting on
  `train-west`'s and `ctc`'s own real container logs - not a mock of
  either. Confirmed passing against the live stack (8/8 across
  `tests/robot/containers/` + `tests/robot/rbc_scenario/`, `etc/run_robot_tests.sh`),
  after two test-side fixes beyond the section 2.6 production fixes:
  `tests/robot/__init__.robot`'s `SETTLE_SECONDS` raised 8 -> 30, and
  `rbc_scenario/02`/`03` now retry their own `ADD_ROUTE`-and-verify as one
  unit (up to 3 attempts, `Wait Until Keyword Succeeds ... 3x`) rather
  than a single fire-and-hope send - real 11-container runs on ordinary
  dev-laptop-class Docker still show occasional reconnect churn well
  past startup (not just a cold-start settling window), so a single
  command can still land in a bad window even after 2.6's own fixes;
  retrying the whole send-and-verify is safe here specifically because a
  genuinely lost `ROUTE_ADD` leaves no partial state to double-apply, and
  a full `PROPAGATION_TIMEOUT` (well beyond `channel_ab.c`'s own ~500ms
  decision cycle) is strong evidence of exactly that outcome, not
  ordinary log-flush lag - each test file's own comment carries the full
  reasoning, including why only the train-side check is inside the
  retry (a CTC-only lag after the train already confirms the grant must
  never trigger a resend, or it would double-grant the route).
- **Not yet re-verified in this pass**: a live failover
  (`docker network disconnect` on the ONLINE site's negotiation link,
  matching ADR-028's own established fault-injection pattern) with the
  Train/IL sims kept running throughout, confirming the promoted site's
  CTC indications correctly show the PRE-failover-granted MA length
  (not reset to 0). The state-transfer mechanism itself (2.4) is
  exercised and correct by construction (same `apply_state_transfer()`
  codepath ADR-028's own reboot-driven promotions already use, just
  carrying a richer payload now) and the encode/decode round-trip is
  unit-verified, but an actual live multi-site Docker run of this exact
  sequence is follow-on verification work, not yet performed.

## 4. Location

- `src/application/rbc_wire_types.h`, `rbc_wire.c`/`.h` (shared envelope
  + queue + codec)
- `src/application/site_config.h`/`.c` (Train/IL/CTC port helpers)
- `src/application/C/monitor_c_types.h`, `monitor_c_io.c`, `monitor_c.c`,
  `monitor_c_report.c` (pure-relay redesign, lazy-connect links)
- `src/application/AB/channel_ab_types.h` (`train_session_t`, session
  table, sync-skip counters), `channel_ab.c` (per-train pre/post_execute
  logic), `channel_ab_wire.c`/`.h` (session-array + AB_SAMPLE codec),
  `channel_ab_negotiate.c` (extra-payload widened, unconditional session
  transfer), `channel_ab_crosscompare.c` (session-table cross-compare,
  bounded sync-skip gate), `channel_ab_io.c` (envelope-queue rx, generic
  send-to-C)
- `src/application/common_config.h` (new message/timing constants - see
  each one's own doc for the specific live finding behind it)
- `sims/rbc_wire.py`, `dual_link.py`, `control_server.py`,
  `train_sim.py`, `il_sim.py`, `ctc_sim.py` (real dual-homed UDP clients
  + command channel; `common.py` retired - fully superseded)
- `docker-compose.yml` (Train/IL/CTC env vars + control-port host mappings)
- `tests/robot/rbc_scenario/` (new scenario test suite), `tests/robot/supportFunctions/*.resource` (Train/IL/CTC/RBC-common keyword libraries backing it and every other tests/robot/ suite)

## Addendum (2026-08-28) - chunked, active-only state transfer for up to 100 trains (ADR-036)

ADR-036 raises the concurrently-tracked train ceiling from 2 to 100 and
multiplexes all trains onto one simulator<->RBC link per relay kind. Two
parts of §2 change; everything else in this ADR stands.

### A. §2.3 session table - fixed size stays, live count is runtime

`RTE_EXAMPLE_MAX_TRAINS` becomes 100 and remains the compile-time
size of every `session[]` / `peer_sessions[]` array (no-malloc rule
intact). The number of trains a process actually services is a **runtime**
value - `RTE_EXAMPLE_DEFAULT_ACTIVE_TRAINS` (2), overridable by the
`RTE_RBC_ACTIVE_TRAINS` environment variable, clamped `1..MAX_TRAINS`.
Per-train loops in `C` and `A/B` iterate `0 .. active-1`. It is not a
wire field; each process reads it independently. Trains are identified
solely by `train_id` (nid_engine); the ADR-029 `west/east == slot`
coupling is dropped (see ADR-036 §5).

### B. §2.4 cross-site transfer - the full snapshot no longer fits one vital frame

`RTE_EXAMPLE_SITE_EXTRA_PAYLOAD_SIZE` + `RTE_EXAMPLE_DB_WIRE_SIZE`
encoded the whole session + whole runtime-route table every cycle inside
one `rte_vital_message_t` (248-byte payload, `uint8_t` size field). At
100 trains that is ~8.3 kB. Revised encoding:

- Only `in_use` sessions and `in_use` runtime routes are encoded; a
  silent train contributes nothing.
- The active set is transmitted as a round-robin **window** of
  `RTE_EXAMPLE_SITE_XFER_WINDOW` sessions (+ a matching route
  window) per cycle. `RTE_EXAMPLE_SITE_EXTRA_PAYLOAD_SIZE` /
  `_DB_WIRE_SIZE` are recomputed from `WINDOW` and
  `_Static_assert`-ed `<= 248`.
- The peer's session table converges within
  `ceil(active_trains / WINDOW)` cycles. Each windowed frame carries its
  base index and count so the receiver applies it to the right slots
  and never treats "not in this window" as "session ended".

### C. REQ-RBC-029A - promotion during an incomplete sweep

A STANDBY promoted to ONLINE before a full sweep completed holds a
**bounded-partial** table: every session it has received is
authoritative and answered normally; a `train_id` not yet swept is
handled by the existing new-train path (treated as a fresh P0 on its
next M136, which re-establishes the session within one cycle). Trains
present in a prior *completed* sweep see unbroken continuity; a train
that connected within the last incomplete sweep may see one delayed
cycle. This bounded relaxation replaces ADR-029 §2.4's "the promoted
site must already know every connected train" - an unbounded snapshot
inside a fixed vital frame is not achievable, and the convergence bound
is analysable (window size x cycle period). Verification: ADR-036 §3
items 2-3.

### D. Location delta (post GP/GA/SA split)

The paths in §4 predate the `RBC_GP` -> `RBC_GP/GA/SA`
split. Current equivalents: `channel_ab*` -> `RBC_GP/src/application/AB/GP/com/ab_gp_channel*`;
`monitor_c*` -> `RBC_GP/src/application/C/{gateway_c*,monitor_c*}`;
`src/application/{rbc_wire_types.h,common_config.h,site_config.*}` ->
`RBC_GP/src/application/{AB,C}/common/`; `sims/*.py` -> the
`RBC_Test_Sim_Core` + `RBC_Test_Sim_Train`/`RBC_Test_Sim_IL`/`RBC_Test_Sim_CTC` projects;
`tests/robot/` -> `RBC_Test_Env/robot/`.

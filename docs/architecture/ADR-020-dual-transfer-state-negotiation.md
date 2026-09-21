# ADR-020: Dual-Transfer State Negotiation (`rte_dual`)

## Status

Accepted

## Context

Two example-app files (`safeAPIRBC2oo2/src/application/SITE/site.c` and
`.../AB/channel_ab.c`) each independently grew ad hoc versions of the same
problem: "am I the active one or the standby, and how healthy is my own
redundancy right now?"

- `site.c` negotiates ONLINE vs STANDBY between two geographically
  separate sites over a heartbeat beacon (own role byte + timestamp tie
  break), persists the decided role to NVM, and reacts to heartbeat loss
  by declaring FAULTED and rebooting. It also logs (but does not track as
  a real state) a distinction between "Hot Standby" (the ONLINE peer's own
  A/B pair is fully redundant) and "Cold Standby" (the ONLINE peer's A/B
  pair has already dropped to SINGLE mode) - inferred from a single-mode
  flag byte riding along in the same beacon.
- `channel_ab.c` tracks its own A<->B peer link health via a dual-transfer
  watchdog and drops from DUAL to SINGLE mode on peer loss, all
  hand-rolled: raw `rte_netlink` sends, a bespoke `PEER_MSG_KIND` framing
  byte, a `pthread_mutex_t` guarding concurrent sends, a seqlock slot for
  the background rx task to hand data to the cycle thread.

Neither of these is reusable: a different application wanting the same
"which of my two redundant instances is active, and how degraded is my
own transport redundancy" behavior would have to reinvent both from
scratch. This ADR promotes the reusable parts into the framework as
`rte_dual` - a state negotiator plus a redundant, EN 50159-defended
messaging channel it can run over - while leaving the two application
files as they are for now (no retrofit in this pass; see "Non-goals").

## Decision

Add a new framework module, `rte_dual` (`include/safeapi/dual/`,
`src/dual/`), with three public types:

1. **`rte_dual_state_t`** - shared vocabulary for "what is this instance
   right now":

   | Value | Meaning |
   |---|---|
   | `RTE_DUAL_STATE_IDLE` | Never yet negotiated with a peer (startup transient). |
   | `RTE_DUAL_STATE_UNKNOWN` | Had a negotiated state before but contact with the peer has been lost - own/peer state can no longer be trusted. |
   | `RTE_DUAL_STATE_ONLINE` | This instance is the active one. |
   | `RTE_DUAL_STATE_HOTSTANDBY` | This instance is standby, and the peer's own redundancy is currently full (not degraded). |
   | `RTE_DUAL_STATE_COLDSTANDBY` | This instance is standby, and the peer's own redundancy is currently degraded (fewer of the peer's own links are up). |

2. **`rte_dual_msgchannel_t`** ("Channel") - one EN 50159-style defended
   message channel over one `rte_netlink_handle_t`.

3. **`rte_dual_channel_t`** ("DualChannel") - a wrapper over 1..N
   `rte_dual_msgchannel_t` redundant links, providing always-send +
   bounded-ACK-wait delivery, per-link and aggregate connection-status
   tracking with an optional change callback, and (optionally) hosting a
   `rte_dual_negotiator_t`'s own state-beacon traffic on the same links.

### 1. Two defensive layers, not one (EN 50159)

CENELEC EN 50159 assumes the transmission medium itself is not trusted
("open transmission system") and requires explicit defenses against seven
threats: repetition, deletion, insertion, resequencing, corruption, delay,
and masquerade. This module applies two layers of defense, matching the
two levels of "Channel" the user asked for:

**Layer 1 - `rte_dual_msgchannel_t` (the base Channel).** Every frame
sent/received on a single link is a `rte_vital_message_t`
(`rte_checksum.h`, already used the same way by `rte_channel_checkpoint()`
- ADR-017 - so this is reuse, not reinvention):

```c
typedef struct {
    uint32_t     sequence_number;  /* repetition / deletion / resequencing */
    uint32_t     sender_id;        /* masquerade */
    uint32_t     timestamp_ms;     /* delay / staleness */
    uint8_t      payload_size;
    uint8_t      padding;
    uint16_t     reserved;
    uint8_t      payload[248];
    rte_crc64_t crc64;            /* corruption */
} rte_vital_message_t;
```

`rte_checksum_vital_message_create()`/`_verify()` do the sequence-
continuity and CRC-64 checking; `rte_dual_msgchannel_send()`/`_receive()`
are thin wrappers that own the running sequence counter and the
`rte_netlink_handle_t` the frame travels over.

**Layer 2 - `rte_dual_channel_t` (DualChannel)'s own frame kind.** The
248-byte `rte_vital_message_t.payload` carries a small DualChannel-owned
header so that DATA, ACK, and STATE-negotiation traffic sharing the same
redundant links can never be misinterpreted as each other:

```c
typedef enum {
    RTE_DUAL_FRAME_KIND_DATA  = 0, /* application payload; expects an ACK back */
    RTE_DUAL_FRAME_KIND_ACK   = 1, /* acknowledges one DATA frame's sequence_number */
    RTE_DUAL_FRAME_KIND_STATE = 2  /* negotiator's own state beacon */
} rte_dual_frame_kind_t;

typedef struct { uint8_t kind; uint8_t reserved[3]; } rte_dual_frame_header_t; /* 4B, keeps what follows aligned */

typedef struct { rte_dual_frame_header_t header; uint32_t acked_sequence; } rte_dual_ack_frame_t;

typedef struct {
    rte_dual_frame_header_t header;
    uint8_t  state;             /* sender's own rte_dual_state_t */
    uint8_t  channel_degraded;  /* 0 = sender's own DualChannel is FULL, 1 = DEGRADED */
    uint16_t reserved;
    uint64_t timestamp_ms;      /* sender's own clock, for the same startup tie-break site.c uses today */
} rte_dual_state_frame_t;
```

An ACK's `acked_sequence` is checked against the sequence the DATA frame
was actually sent with (not just "some ACK arrived") - this specifically
defends against a stale or misdirected ACK being accepted as confirmation
of the wrong send, a gap the base `rte_vital_message_t` sequence check
alone does not close for a request/reply pattern.

### 2. Always send, wait for a bounded ACK - that IS the liveness signal

Per the requirement: `rte_dual_channel_send()` never gates on the
current negotiated state. It always transmits the DATA frame, on every
configured redundant link, and for each link waits up to
`config.ack_timeout_ms` for that link's ACK before moving to the next.
There is deliberately no separate "is it my turn to be liveness-checked"
beacon-only mode for DATA traffic - the real payload traffic itself is
the liveness check, avoiding a second, separately-timed protocol that
could disagree with the first about whether the peer is actually reachable.

Per link, the result (ACK arrived in time vs timed out) updates that
link's own UP/DOWN status. After all configured links have been tried:

- **Aggregate `rte_dual_channel_status_t`** = `FULL` (all links UP),
  `DEGRADED` (some but not all UP), or `DOWN` (none UP). A change from the
  previously reported aggregate status invokes the optional
  `status_callback` registered at `rte_dual_channel_init()` - this is
  the "indicate to the user via callback if registered" requirement.
- **Negotiator liveness event**: the negotiator does not attach to the
  channel or receive a push notification - the one-directional dependency
  (negotiator depends on channel, never the reverse; section 3) means
  `rte_dual_negotiator_execute()` itself pulls this signal each cycle, by
  calling `rte_dual_channel_get_status()` before sending its own beacon
  (`own_channel_degraded = status != FULL`) and by the beacon
  send/receive round trip itself timing out (or not) the same way any
  DATA send would. There is no separate registration step on the channel
  side for this.

This is the explicit answer to "when redundant link down then dual
channel is degraded but SITE state is still ONLINE and other STANDBY":
losing one of several redundant links only ever changes the *channel's*
own status (`FULL` -> `DEGRADED`); it does not by itself touch
`rte_dual_state_t` at all. Only total loss (aggregate `DOWN`, i.e. every
redundant link unresponsive) is a negotiator-relevant event. Partial
redundancy loss on the *peer's* side, however, is exactly what downgrades
*this* instance from HOTSTANDBY to COLDSTANDBY - see next section.

### 3. The negotiator keeps both its own and the peer's state

`rte_dual_negotiator_t` exposes both:

```c
rte_dual_state_t rte_dual_negotiator_get_own_state(const rte_dual_negotiator_t *neg);
rte_dual_state_t rte_dual_negotiator_get_peer_state(const rte_dual_negotiator_t *neg);
```

It is attached to a `rte_dual_channel_t` at init and, once per
`rte_dual_negotiator_execute()` call (intended to be driven once per
application cycle, e.g. from an `rte_appmanager` hook, the same way
`rte_channel_checkpoint()` is driven today), sends its own
`rte_dual_state_frame_t` over the attached DualChannel's redundant
links (piggy-backing on the same ACK/timeout machinery as any DATA send)
and processes whatever STATE frames arrived from the peer since the last
call.

State decision, each `execute()`:

- No STATE frame ever received from the peer -> `own_state` stays `IDLE`
  until the first one arrives (or the configured startup timeout elapses
  with none - see below).
- A STATE frame arrives: compare `(timestamp_ms, sender_id)` the same way
  `site.c`'s `decide_online()` does today (older timestamp wins,
  `sender_id` as a deterministic tie-break, decided once at startup and
  never re-run) to decide which side is ONLINE.
- **The HOT/COLD determination is asymmetric - it is never the STANDBY
  side's own self-reported degradation that decides its own HOT/COLD
  label, nor the ONLINE side's self-report for its own peer_state
  label.** Instead, in both directions, HOT/COLD reflects *the ONLINE
  side's own transport health*, because that health is what determines
  how well the STANDBY side is actually being backed up:
  - This instance decided ONLINE -> `own_state = ONLINE`; `peer_state`
    (the STANDBY side's label, as this instance sees it) is set from
    *this instance's own* `rte_dual_channel_get_status()`:
    `channel_degraded == 0 (FULL) -> HOTSTANDBY`,
    `channel_degraded == 1 (not FULL) -> COLDSTANDBY`.
  - This instance decided STANDBY -> `peer_state = ONLINE`; `own_state`
    (this instance's own label) is set from *the peer's* last-reported
    `channel_degraded` bit (the peer is the ONLINE side here, so its
    self-report is the authoritative one): `0 -> HOTSTANDBY`,
    `1 -> COLDSTANDBY`.
  - Put differently: whichever side is ONLINE, its own channel health is
    what determines the STANDBY side's HOT/COLD - never the STANDBY
    side's own health, and never the ONLINE side's opinion of itself
    when it is the one being labeled. Swapping which side's degradation
    bit feeds which label was an implementation bug caught during test
    design (see `rte_dual_negotiator.c`'s `negotiator_update_states()`)
    and is called out here explicitly so this ADR does not go stale
    relative to the fix.
  - Both directions refine on every subsequent `execute()` call the same
    way, once contact is established, not just at the initial decision.
- No STATE frame (and no DualChannel liveness event either) received
  within `config.peer_lost_timeout_ms` -> `peer_state = UNKNOWN`; if this
  instance itself has no other way to confirm its own role (e.g. it was
  never ONLINE), `own_state = UNKNOWN` too. An application that needs a
  harder reaction than "state is now UNKNOWN" (e.g. `site.c`'s own
  FAULTED+reboot escalation) still owns that decision - the negotiator
  raises the event, exactly as `rte_watchdog`'s failover callback and
  `rte_checkpoint`'s timeout do today; it does not itself call
  `rte_safestate_enter()`.

An optional `rte_dual_negotiator_state_change_callback_t` fires whenever
either `own_state` or `peer_state` changes, for the same "tell the user"
reason `rte_dual_channel_t`'s connection-status callback exists.

### 4. Non-goals for this ADR

- **No retrofit of `safeAPIRBC2oo2` in this pass.** `site.c` and
  `channel_ab.c` keep their existing hand-rolled logic; migrating them
  to `rte_dual` is a deliberate follow-up once this API has shipped and
  been exercised by its own tests, not bundled into the same change that
  defines the API.
- **No dynamic link count.** `RTE_DUAL_CHANNEL_MAX_LINKS` (fixed at 4)
  bounds every `rte_dual_channel_t`'s redundant-link array; no
  allocation, consistent with REQ-OAL-COMMON-010.
- **No new transport backend.** `rte_dual_msgchannel_t` sends/receives
  over an already-open `rte_netlink_handle_t`; opening/closing the
  underlying link(s) remains the caller's job via `rte_netlink_open()`/
  `_close()`, exactly as `rte_channel_checkpoint()` reuses
  `rte_channel_t`'s already-registered backend rather than owning
  a transport itself.
- **No automatic safety reaction.** As with every other OAL/negotiation
  primitive in this framework (`rte_watchdog`, `rte_checkpoint`), this
  module reports state/events; deciding what a sustained `UNKNOWN` means
  for safety (reboot? safe-state? degrade only?) stays an
  application-level policy decision, made via `rte_safestate.h`
  directly by the integrator.

## Consequences

- A new reusable state vocabulary (`rte_dual_state_t`) exists for "which
  of two redundant instances is active, and how well-backed is the
  standby one" - any future dual-site/dual-instance application can reuse
  it instead of reinventing `site.c`'s ad hoc ONLINE/STANDBY/FAULTED byte
  and separate Hot/Cold Standby log-only distinction.
- `rte_dual_channel_t` gives redundant-link fault tolerance
  (`FULL`/`DEGRADED`/`DOWN`) for free to anything sending real payload
  data over it, with the EN 50159 defensive envelope applied
  automatically rather than left to each integrator to reimplement (as
  `channel_ab.c` did for its own bespoke `PEER_MSG_KIND` framing).
- Two more fixed-size structs (`rte_dual_ack_frame_t`,
  `rte_dual_state_frame_t`) travel inside `rte_vital_message_t.payload`
  - both well under the 248-byte payload budget, no change to
  `rte_checksum.h` required.
- `safeAPIRBC2oo2` is unchanged by this ADR; its own dual-transfer logic
  and this new module will temporarily overlap in *purpose* (not in
  code) until the follow-up retrofit lands.

## Post-acceptance fix (ADR-022, SITE's live `rte_safechannel` migration)

`rte_dual_channel_send()`'s ACK-wait loop (`src/dual/rte_dual_channel.c`)
had only ever been exercised through `test_rte_dual_channel.c`'s mock
netlink backend before ADR-022 wired it to a real POSIX TCP link for the
first time (SITE's WEST/EAST heartbeat). Its "recompute remaining budget
from wall-clock elapsed time" step treated `now_ms == start_ms` (no
measurable progress on `rte_timer_now()`'s own millisecond tick) as "no
timer backend available, give up after one attempt". A real localhost
round trip - connect, send DATA, receive the peer's own DATA, auto-ACK
it, receive the peer's ACK - routinely completes inside a single
millisecond, so this guard misfired as a false abort after exactly one
poll, and WEST/EAST's very first negotiation exchange timed out every
run. Fixed by keeping `remaining` unchanged on a same-millisecond
iteration (instead of aborting) while capping the number of such
no-progress iterations at a fixed `RTE_DUAL_CHANNEL_STALL_POLL_LIMIT`
(32), so a genuinely unresponsive link or absent timer backend still
cannot spin unboundedly. Verified via `test_rte_dual_channel.c` (no
regression) and a live two-process SITE WEST/EAST run completing
negotiation and steady-state heartbeats over the real TCP backend.

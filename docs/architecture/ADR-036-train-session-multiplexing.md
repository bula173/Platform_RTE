# ADR-036: Train-session multiplexing - one link, many trains (up to 100)

## Status

Accepted - design of record. Implementation is phased (see §5); the
2-train behaviour of ADR-029 is a strict subset and stays green
throughout.

## Context

The integrator raised the target from 2 concurrently-tracked trains to
**up to 100 per site**, and asked whether the Train simulator should
itself serve many trains over **one connection** - "closer to FRMCS,
one link with several trains on it" - rather than one connection per
train.

ADR-029's transport is **one TCP connection per train instance**:

- `rte_netlink`'s `RTE_NETLINK_ROLE_LISTEN` accepts exactly **one
  peer per bound port** (`rte_netlink.h`), so N trains need N listen
  ports on `C`, N more for `IL`, and the C<->A and C<->B relay links
  are likewise one dedicated link per `{peer, kind, instance}`
  (`site_config.h`, `gateway_c_train_handler.h`:
  `relay_channel[GATEWAY_C_PEER_COUNT][SAFEAPI_EXAMPLE_MAX_TRAINS]`).
- The per-site port span is `4 + 9 * SAFEAPI_EXAMPLE_MAX_TRAINS`
  (`common_config.h`, `C_FOR_*_PORT_OFFSET` chain). At `MAX_TRAINS = 2`
  that is ~16 ports inside a 100-port WEST/EAST gap. At `MAX_TRAINS =
  100` it is **~904 ports per site** - it overruns the other site's
  base, the fixed negotiation ports (15021/15022), and (in the
  single-host local topology) silently mis-delivers UDP datagrams, the
  exact failure mode `common_config.h`'s own port-gap comment documents
  being hit before.

There is a second, independent ceiling. ADR-029 §2.4's cross-site
state-transfer snapshot (`SAFEAPI_EXAMPLE_SITE_EXTRA_PAYLOAD_SIZE` +
`SAFEAPI_EXAMPLE_DB_WIRE_SIZE`) encodes the **whole** session table plus
the **whole** runtime route table every cycle and rides inside one
`rte_vital_message_t`, whose payload is **248 bytes** and whose
`payload_size` field is a `uint8_t` (`rte_checksum.h:237-241`). Today
that snapshot is ~173 bytes. At `MAX_TRAINS = 100` (`MAX_ROUTES =
MAX_TRAINS * MAX_ROUTES_PER_TRAIN = 400`) it is **~8300 bytes** - 33x
over the cap, and not even expressible in `payload_size`.

The `rbc_envelope_t` wire frame already carries `train_id` (nid_engine)
in every message kind that is train-scoped (`rbc_wire_types.h`), so the
**application protocol is already multiplex-ready**; only the transport
wrapping (a socket per train) and the bulk inter-site sync are not.

## Decision

### 1. `SAFEAPI_EXAMPLE_MAX_TRAINS` is a compile-time ceiling, not the live count

`SAFEAPI_EXAMPLE_MAX_TRAINS` becomes **100** - it sizes every fixed
session/route array (`session[MAX_TRAINS]`, `peer_sessions[MAX_TRAINS]`,
the C gateway's per-slot liveness arrays) and stays a compile-time
constant, honouring the project's no-malloc/no-dynamic-list rule.

The number of trains actually brought up is a **runtime** value,
`SAFEAPI_EXAMPLE_DEFAULT_ACTIVE_TRAINS` (2) unless the environment
variable `RTE_RBC_ACTIVE_TRAINS` overrides it (clamped to
`1..MAX_TRAINS`). Every per-train loop in `C` and `A/B` runs
`0 .. active_trains-1`, not `0 .. MAX_TRAINS-1`. This keeps the
regression suite, CI, and a plain local bring-up paying only the
2-train cost; `N = 100` is an opt-in soak. It is not a wire field - each
process reads it independently, and a train_id outside the local active
range simply has no `in_use` session.

### 2. One multiplexed link per relay kind - and `C` is a pure pipe

The integrator's explicit refinement: **`C` accepts ONE session per
relay kind and never inspects `train_id`.** It is a byte pipe, exactly
as ADR-029 §2.2 already frames role C ("a pure relay - no session state
of its own"); multiplexing changes nothing about that, it only removes
the per-instance socket fan-out. Distinguishing which train an envelope
belongs to is **A/B's** job, by the `nid_engine` the envelope already
carries.

- **Sim-facing**: `C` binds **one** Train listen port and **one** IL
  listen port per site (CTC is already single). `site_config_c_port_for_train(site)`
  / `_for_il(site)` lose their `train_index` parameter.
- **C<->A / C<->B relay**: **one** link per `{peer, kind}`, not per
  `{peer, kind, instance}`. `relay_channel[GATEWAY_C_PEER_COUNT]` for
  Train, likewise for IL. `site_config_c_port_for_a/_b(site, kind)`
  lose the per-index expansion for TRAIN/IL.
- The Train simulator opens **one** connection per site and sends
  P0/M136/M146 for **every** train it hosts on that link, each envelope
  stamped with its own `train_id`. `C`'s Train gateway reads that one
  socket and forwards **each frame verbatim to both A and B** (the same
  broadcast discipline it already uses); it reads the one A relay and
  the one B relay and forwards their frames to the one sim socket
  (A-primary / B-fallback logic unchanged, just now per-link not
  per-train). **No `train_id` parsing, no per-train arrays, no
  per-train liveness in `C`.**
- `gateway_c_{train,il}_handler.c`: `sim_channel[MAX_TRAINS]` ->
  `sim_channel` (scalar); `relay_channel[PEER][MAX_TRAINS]` ->
  `relay_channel[PEER]`; the `a_stale_cycles` / `relay_down_since_ms` /
  `last_known_tag` / `a_seen_this_cycle` arrays -> scalars (per link).
  `_check_link_down` reports "the Train relay link", not "train[i]'s".
- **A/B** decode `nid_engine` from each inbound relay envelope and route
  it to `session[train_id - 1]` (`ab_gp_train.c` already indexes this
  way); outbound answer envelopes already carry `train_id` and go out
  the single relay link. The per-`{kind}` relay link in
  `ab_gp_channel_types.h` drops its `[MAX_TRAINS]` dimension;
  `ab_gp_channel_send_relay` / `_stage_relay` lose their `index`
  parameter; `on_relay_envelope_received` loses `index`.
- `RTE_RBC_ACTIVE_TRAINS` is read by **A/B** (session-table iteration
  bound) and the **sims** (how many trains to create). `C` does not
  read it - it relays whatever arrives.
- Channel resolver names lose their `-%u` suffix: `c-sim-train`,
  `c-relay-train-a`, `c-relay-train-b`, `ab-relay-train`, and the IL
  equivalents.

### 3. Port map collapse

With Train and IL each one port instead of `MAX_TRAINS`, the per-site
span drops from `4 + 9 * MAX_TRAINS` to a fixed **~10 ports**. The
`SAFEAPI_EXAMPLE_C_FOR_*_PORT_OFFSET` chain stops multiplying by
`MAX_TRAINS`. The WEST (15001) / EAST (15101) 100-port gap and the
15021/15022 negotiation ports keep enormous headroom at any train
count. No numeric base changes; only the offset arithmetic simplifies.

### 4. Inter-site state transfer: active-only + chunked (ADR-029 §2.4 amended)

See the **ADR-029 Addendum** appended to that file. In brief:

- Only `in_use` sessions and `in_use` runtime routes are encoded - a
  silent train contributes nothing.
- If the active set still exceeds a single vital payload, it is sent as
  a **bounded window** of `SAFEAPI_EXAMPLE_SITE_XFER_WINDOW` sessions
  (and a matching route window) per cycle, round-robin. The full table
  converges on the peer within `ceil(active_trains / WINDOW)` cycles.
- `SAFEAPI_EXAMPLE_SITE_EXTRA_PAYLOAD_SIZE` /
  `SAFEAPI_EXAMPLE_DB_WIRE_SIZE` are recomputed from `WINDOW`, not
  `MAX_TRAINS` / `MAX_ROUTES`, and are asserted `<= 248` at compile
  time.
- **Promotion semantics** (new REQ-RBC-029A): a STANDBY promoted to
  ONLINE mid-transfer holds a *bounded-partial* table - every session
  it has received is authoritative; sessions not yet in this window are
  filled in over the next `< WINDOW`-cycle sweep, during which the newly
  ONLINE site answers M136 for known trains immediately and treats an
  unknown `train_id` as a fresh P0 (the same path a genuinely new train
  takes). Continuity for trains that *were* in a prior completed sweep
  is unbroken; a train that connected within the last partial sweep may
  see one delayed cycle. This is the deliberate, bounded relaxation of
  ADR-029's "the promoted site must already know every connected train"
  - unbounded snapshot growth is not an option inside a fixed vital
  frame.

### 5. Simulators and topology

- **Flat roster**: trains are `train-1 .. train-N`, each with a
  configured `home_site` (WEST or EAST). The `west/east == slot`
  coupling of ADR-029 is dropped. `train_id` is the identity
  everywhere.
- **One process per role**: one `train-sim`, one `il-sim`, one `ctc`
  (was 2 + 2 + 1). Each `train-sim`/`il-sim` process is dual-homed
  (`SimCore.dual_link`) and hosts every train whose `home_site` it is
  responsible for; a single process may host both sites' trains in the
  local env. Config gains a `trains: [{nid_engine, home_site}, ...]`
  roster (or `active_trains: N` + `base_nid_engine` shorthand, expanded
  by `SimCore`).
- **Compose**: 11 containers -> **9** (6 RBC + `train` + `il` + `ctc`).
  `RTE_RBC_ACTIVE_TRAINS` is set once, shared by the RBC services and
  the sims.
- `setupLocalTestEnv.sh` generates 3 sim configs instead of 5 and
  exports `RTE_RBC_ACTIVE_TRAINS`.

### 6. FRMCS analogy - and its limit

Multiplexing many trains onto one simulator<->RBC link mirrors the
*shape* of an FRMCS bearer carrying many trains' communications, and is
the right model for a **test harness** driving a 100-train RBC. It is
**not** a claim that ETCS multiplexes trains onto one safe connection:
real on-board<->RBC communication keeps a **per-train EURORADIO safe
connection** (Subset-037) over the shared FRMCS/GSM-R bearer. This ADR
changes the simulator transport and the RBC's *sim-facing* gateway, not
the vitality model - A/B still cross-compare per-train decisions exactly
as before, and the inter-site transfer is still a vital `rte_dual_channel`
payload. A production RBC would terminate 100 EURORADIO connections;
this demo terminates one multiplexed test link and keeps 100 independent
vital sessions behind it.

## Verification

1. **N = 2 regression** - the full `rbc_scenario` suite is unchanged in
   outcome (15/16, the one failure being the pre-existing
   `16_ertms_temporary_speed_restriction` WIP), 0 A/B SAFE-states, both
   local (`setupLocalTestEnv.sh`) and `docker compose`.
2. **N = 10** - a new `rbc_scenario` case connects 10 trains, each gets
   an MA; a mid-run West->East failover leaves all 10 MAs intact
   (ADR-029 §2.4's property, now over a chunked transfer - assert
   convergence within the window bound).
3. **N = 100 soak** - `RTE_RBC_ACTIVE_TRAINS=100`, 100 trains connect
   and hold MAs for a 5-minute soak; 0 SAFE-states, 0 reboots, port
   scan shows the per-site span still inside the WEST/EAST gap,
   inter-site payload stays `<= 248` bytes.
4. Byte-level check that the Python `rbc_wire` mirror and the C codec
   still agree (unchanged frame, but re-run per ADR-029 §3).

## Consequences

- **Fewer moving parts at rest**: 9 containers not 11; ~10 ports/site
  not ~900; one relay link per kind not `MAX_TRAINS`.
- **Bounded eventual consistency** on failover instead of
  instantaneous-complete: a promoted site converges its session table
  within a known cycle bound rather than having it atomically. The
  trade is forced by the fixed vital-frame size and is bounded and
  analysed (REQ-RBC-029A), not open-ended.
- **`train_id -> slot` mapping** is a new small responsibility in `C`
  and `A/B` (direct index when `nid_engine` is dense `1..N`; a linear
  scan of `<= 100` entries otherwise - still O(1) per cycle amortised,
  no allocation).
- **Route pool**: `SAFEAPI_EXAMPLE_MAX_ROUTES` tracks
  `active_trains * MAX_ROUTES_PER_TRAIN` at runtime but is sized for
  the 100-train ceiling; the static site topology
  (`safeAPIRBC2oo2SA`'s `ab_site*_track_layout[]`) must define enough
  distinct routes for the largest `N` a scenario exercises - generated,
  not hand-listed, past a handful.
- **Simulator config schema change** (roster / `active_trains`) - a
  breaking change to `train-*.json` / `il-*.json`, versioned in
  `SimCore.sim_config`.
- A future real backend that wants genuine per-train EURORADIO
  termination is not precluded - it would add connections behind the
  same `session[]` table this ADR keeps.

## Location

- `safeAPIRBC2oo2GP/src/application/{AB,C}/common/common_config.h`
  (`MAX_TRAINS`, `DEFAULT_ACTIVE_TRAINS`, port-offset chain,
  `SITE_EXTRA_PAYLOAD_SIZE`, `DB_WIRE_SIZE`, `SITE_XFER_WINDOW`)
- `safeAPIRBC2oo2GP/src/application/C/common/site_config.{h,c}`
  (port helpers lose `train_index`)
- `safeAPIRBC2oo2GP/src/application/C/gateway_c*.{c,h}`,
  `monitor_c*.{c,h}` (single link per kind, demux)
- `safeAPIRBC2oo2GP/src/application/AB/GP/main/pre_execute.c`,
  `AB/GP/com/ab_gp_channel_io*.c`,
  `AB/GP/com/ab_gp_channel_negotiate.c` (relay demux, chunked
  encode/decode), `AB/common/rbc_wire_types.h` (unchanged frame,
  doc note)
- `safeAPIRBC2oo2GA/src/AB/il/ab_ga_il.c`, `AB/ctc/ab_ga_ctc.c`
  (relay codecs - per-instance iteration removed)
- `safeAPIRBC2oo2SA` site topology (route pool for large N)
- `SimCore/src/simcore/{sim_config,dual_link}.py`;
  `TrainRBCSim`, `ILRBCSim`, `CTCRBCSim` sims + configs
- `safeAPIRBC2oo2TestEnv/docker-compose.yml`,
  `etc/scripts/setupLocalTestEnv.sh`, `robot/rbc_scenario/*`
- This file; `ADR-029` Addendum.

# ADR-038: Status indication - a C-side pub/sub broker + an A/B↔C util channel

## Status

Accepted - design of record. Implementation is phased (§5). Follows
ADR-037 (role C decomposed into per-client service binaries) and ADR-036
(train multiplexing); precedes ADR-036 Phase 2.

## Context

Every unit of the system - the RBC channels A and B, the role-C gateway
services, and each simulator - should announce its own health **at
startup and periodically**, and the RBC as a whole should produce one
rolled-up per-site summary line, e.g.:

```
RBC West: OwnStatus=ONLINE OtherStatus=HOTSTANDBY A=up B=up ComServer=up
          ConnectedTrains=10 IL_West=up IL_East=down CTC_West=up CTC_East=down
TrainSim:  up  RBC_West=up  RBC_East=up  trains=10
```

No process today knows enough to compose that summary. The integrator's
decisions:

1. **A dedicated C-side service** listens for every other unit's status
   and generates the summary - a **publish/subscribe broker** in "the C
   computer", SomeIP-inspired: topic registration, runtime
   subscribe/unsubscribe, fan-out (**full broker**, not a minimal
   point-to-point status hack).
2. **A new util channel between A/B and C** carries the statuses A and B
   publish - a general A/B↔C side channel, extensible ("maybe in future
   something else"), built as a **4th relay kind** so it reuses the
   existing relay transport machinery.
3. Each sim also emits its own connection status **from its own point of
   view**, and pushes its status to **both** sites' brokers (it is
   already dual-homed).
4. Status is exposed **only as a structured log line** (no new external
   query surface) - startup + every ~5 s, `key=value` payload.
5. **Role C is non-vital** (ADR-001 §4). The broker / status service /
   aggregator may therefore be written in **C++**; A and B stay C99 /
   MISRA.

## Decision

### 1. `RTE_EXAMPLE_RELAY_KIND_UTIL` - the 4th relay kind

`rte_example_relay_kind_t` gains `..._UTIL = 3`; `..._RELAY_KIND_COUNT`
becomes 4. It reuses **everything** the train/il/ctc kinds already have:

- `common_config.h`: `C_FOR_A_UTIL_PORT_OFFSET` / `C_FOR_B_UTIL_PORT_OFFSET`
  appended to the A- and B-relay blocks, `C_FOR_UTIL_PORT_OFFSET` to the
  sim-facing block. Per-site port span grows from ~10 to ~13 - still far
  inside the 100-port WEST/EAST gap.
- `site_config_c_port_for_{a,b}(site, UTIL)` - no code change, the
  `switch` in `relay_port_offset()` gains a `UTIL` case.
- `ab_gp_channel.c`: resolver matches `ab-relay-util` (CONNECT to C),
  setup/recv/close loops already iterate `RELAY_KIND_COUNT`,
  `relay_channel[]` array already sized by it. The idle-keepalive
  discipline (ADR-036 Phase 1) applies unchanged.
- C side: the **status service** owns `c-relay-util-a` / `c-relay-util-b`
  (LISTEN) instead of the train/il/ctc gateways.

**Wire frame** - NOT `rbc_envelope_t` (that is train-domain shaped). A
new `rbc_util_frame_t`, fixed `RBC_UTIL_FRAME_SIZE` (256 B):

```
byte 0     util_msg_type   HELLO=1 SUBSCRIBE=2 UNSUBSCRIBE=3 PUBLISH=4 BYE=5 KEEPALIVE=0
byte 1     source_id       stable small id per unit (A_WEST, B_WEST, GW_TRAIN_WEST, TRAIN_SIM, ...)
byte 2..3  topic_id        u16 LE  (STATUS=1; room for more)
byte 4     seq             u8, wraps
byte 5     payload_len     u8  (0..250)
byte 6..   payload         key=value ASCII blob, `payload_len` bytes, unused tail zero
```

A pure C header (`rbc_util_frame.h`, `static inline` codec) shared by the
C99 A/B side and the C++ C side - `#ifdef __cplusplus extern "C"` guarded.

### 2. The broker - C++, hosted by the status service

`safeAPIRBC2oo2GP/src/application/C/status/` (C++17):

- `broker.hpp` / `broker.cpp` - `class Broker`:
  - `on_frame(ClientRef from, const rbc_util_frame_t&)` - dispatch on
    `util_msg_type`.
  - `HELLO` registers/refreshes a client (`std::unordered_map<uint8_t,
    Client>` keyed by `source_id`, last-seen timestamp).
  - `SUBSCRIBE` / `UNSUBSCRIBE` mutate
    `std::unordered_map<uint16_t, std::unordered_set<uint8_t>>`
    (topic → subscriber source_ids).
  - `PUBLISH` fans the frame out to every current subscriber of its
    `topic_id` (skipping the publisher), via the caller-supplied send
    functor.
  - `sweep(now)` - drops clients / subscriptions unseen for
    `> STATUS_STALE_MS`; a dropped client's last STATUS payload is what
    the aggregator ages out.
  - No SIL constraints (C is non-vital): STL containers, exceptions off
    by build flag, RAII. Deterministic-enough for a monitoring path.
- Transport under the broker is the existing `rte_channel_service`
  (netlink UDP). The status service binds:
  - `c-relay-util-a`, `c-relay-util-b` - A and B (one each).
  - `c-broker` - the local train/il/ctc gateway services (they CONNECT
    and speak the same `rbc_util_frame_t` protocol).
  - `c-sim-status` - **not** a new sim socket: sims send their status on
    their EXISTING sim link (see §4), the owning gateway republishes it
    to the broker.
  Each `rte_channel_service` LISTEN takes one peer; where more than one
  publisher must share (the 3 local gateways), the status service binds
  `c-broker-{train,il,ctc}` - three ports, same "one link per client"
  posture the rest of role C uses.

### 3. STATUS topic - payload schema

`topic_id = 1`. Each publisher sends `PUBLISH{STATUS, "<k=v ...>"}` every
`RTE_EXAMPLE_STATUS_PERIOD_MS` and once at startup. Keys are the
publisher's own view:

| source | payload |
|---|---|
| `A/WEST`, `B/WEST` | `unit=A/WEST up=1 role=ONLINE site=ONLINE other=HOTSTANDBY peer=up trains=10` |
| `GW-TRAIN/WEST` | `unit=GW-TRAIN/WEST up=1 sim=1 relayA=up relayB=up` |
| `GW-IL/WEST`, `GW-CTC/WEST` | same shape |
| `TRAIN-SIM` | `unit=TRAIN-SIM up=1 west=1 east=0 trains=10` |
| `IL-SIM`, `CTC-SIM` | `unit=IL-SIM up=1 west=1 east=0` / `unit=CTC-SIM up=1 west=1 east=1 indications=8` |

`role` / `site` / `other` are meaningful only from A and B (they run the
inter-site negotiator). `other=HOTSTANDBY|COLDSTANDBY` is the negotiator's
own REQ-DUAL-NEGOTIATOR-004 distinction, already computed - not a
self-report.

### 4. The aggregator - one summary line per site

Also in the status service. Keeps `latest[source_id] = {payload, ts}`.
Every `STATUS_PERIOD_MS` + at startup it composes and logs:

```
rte_log_write_event(INFO, site, 0, "STATUS", "-", "SUMMARY",
                     "RBC <site> health", "<rolled-up k=v>")
```

Rollup rules: `OwnStatus`/`OtherStatus` from A's (or B's, if A stale)
report; `A`/`B` = up iff their report is fresh; `ComServer` = up iff all
three local gateways' reports are fresh; `ConnectedTrains` = A's `trains`
(cross-checked against B's, mismatch flagged `trains=10/9!`); `IL_West` =
`IL-SIM west`, `IL_East` = `IL-SIM east` (straight from the sim's own
dual-homed report - it reached this broker, so at least one side is up);
likewise CTC. A stale source renders as `down`/`?`.

### 5. Per-unit self-status line

Independently of the broker, **every** unit logs its own one-liner at
startup + every `STATUS_PERIOD_MS`:

- A/B/gateways: `rte_log_write_event(INFO, site, cyc, unit, "-",
  "STATUS", "self", "<same k=v it publishes>")` from a cheap per-cycle
  check in the executive (`cycle % (STATUS_PERIOD_MS / period_ms) == 0`).
- Sims: a structured `[<sim>] [internal] [STATUS] [self] [<k=v>]` line
  from a wall-clock check in the main loop, plus a
  `PUBLISH{STATUS,...}` frame - which the sim sends inside its existing
  sim link as an `rbc_envelope_t` of a new kind `RBC_MSG_SIM_STATUS`
  (both `rbc_wire_types.h` copies; C stops decoding it and the owning
  gateway republishes the payload to the broker as a `rbc_util_frame_t`
  PUBLISH, rather than forwarding it to A/B).

### 6. Language / build

- **C99 / MISRA (unchanged)**: everything under `AB/`, `gateway_c_common`,
  `rbc_util_frame.h` (pure header), the `common/` headers, the relay/wire
  types. A and B are vital.
- **C++17 (new)**: `C/status/*.cpp` - the broker, the aggregator,
  `main_c_status.cpp`. `safeAPIRBC2oo2GP/CMakeLists.txt` gains `CXX` to
  `LANGUAGES`; the status target compiles as C++ and links
  `gateway_c_common` across an `extern "C"` boundary
  (`gateway_c_common.h` already C-linkage-clean; add the guard).
- New binary `rte_gateway_c_status` (GP - it is platform/transport
  infra, like the train gateway). The `c-<site>` container entrypoint
  (`gateway_c_entrypoint.sh`, safeAPIRBC2oo2SA) launches **four**
  services now.

### 7. Cadence

`#define RTE_EXAMPLE_STATUS_PERIOD_MS 5000U`
`#define RTE_EXAMPLE_STATUS_STALE_MS  15000U` (3 missed → down).

## Verification

- Every unit prints a `STATUS self` line within ~1 s of start, then every
  ~5 s (log-scrape in a new `robot/` check, both local + docker).
- The status service prints `RBC <site> health ...` every ~5 s; its
  fields match reality: kill `rte_gateway_c_il` → `ComServer=down`
  and `IL_West=down` within `STATUS_STALE_MS`; `docker network disconnect`
  the sim from EAST → `IL_East=down` / `CTC_East=down` in WEST's summary.
- `ConnectedTrains` tracks the active roster (ADR-036) as trains
  connect/disconnect.
- Broker unit test (C++, standalone): subscribe/publish/unsubscribe/
  sweep, fan-out to N subscribers, publisher-excluded, stale drop.
- N=2 `rbc_scenario` suite still 16/16, 0 A/B SAFE-states (the util
  relay + status traffic must not perturb the vital path).

## Consequences

- **C++ enters the C side.** The cross toolchains (LinuxMacOSToolchain,
  musl, QNX) must provide a C++ compiler; `build_toolchains.sh` /
  channel builds gain the status target. A/B and the framework are
  untouched C99.
- **+1 process per c-<site> container** (4 gateway services). Entrypoint,
  installer (`build_installers.sh`), compose command, `setupLocalTestEnv.sh`,
  and the merged-log `_ALL_SERVICES` all list a `status` service now.
- **+1 relay kind** everywhere `RELAY_KIND_COUNT` is used - all
  loop-bounded already, so mostly free; the port map shifts (regen any
  hard-coded sim `rbcWest/rbcEast.port` - the ADR-036 Phase 1 ports move
  again).
- A real broker component to own and test; its failure degrades
  monitoring only (never a relay or a decision).
- The util channel is deliberately general - a later feature (config
  push, remote command, structured fault reporting A→C→CTC) rides the
  same `rbc_util_frame_t` with a new `util_msg_type` / `topic_id` and no
  new link.

## Location

- `safeAPIFreamwork/docs/architecture/` - this file.
- `safeAPIRBC2oo2GP/src/application/{AB,C}/common/common_config.h` -
  `RELAY_KIND_UTIL`, port offsets, `STATUS_PERIOD_MS`/`_STALE_MS`.
- `.../C/common/site_config.{h,c}` - `RELAY_KIND_UTIL` case.
- `.../C/common/rbc_util_frame.h` - new frame + codec.
- `.../AB/GP/com/ab_gp_channel.c` - `ab-relay-util` resolver;
  `.../AB/GP/main/{pre,post}_execute.c` - publish A/B STATUS each period.
- `.../C/status/` (new, C++): `broker.{hpp,cpp}`, `aggregator.{hpp,cpp}`,
  `status_self.{hpp,cpp}`, `main_c_status.cpp`. `gateway_c_common.h` -
  `extern "C"` guard. `.../C/gateway_c_{train,il,ctc}_handler` +
  GA copies - publish self-status, republish sim `RBC_MSG_SIM_STATUS`.
- `.../{AB,C}/common/rbc_wire_types.h` - `RBC_MSG_SIM_STATUS`.
- `safeAPIRBC2oo2GP/CMakeLists.txt` - `LANGUAGES C CXX`, status target.
- `safeAPIRBC2oo2SA` - entrypoint (4 services), installer.
- `safeAPIRBC2oo2TestEnv` - compose, `setupLocalTestEnv.sh`,
  `_ALL_SERVICES`, new `robot/` status checks.
- `SimCore` / `TrainRBCSim` / `ILRBCSim` / `CTCRBCSim` - self-status line
  + `RBC_MSG_SIM_STATUS` publish.

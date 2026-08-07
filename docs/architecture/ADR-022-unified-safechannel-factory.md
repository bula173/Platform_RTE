# ADR-022: Unified `sapi_safechannel` Factory (Hiding IPC/Netlink from App Code)

Status: Accepted
Date: 2026-08-07
Applies to: safeAPIFreamwork (`sapi_safechannel`, new), safeAPIExample
(SITE/AB/C link usage).

## 1. Context

Feedback (continuing the ADR-021 "SAPI is too complicated" thread): the
low-level OAL transport services - `sapi_ipc` and `sapi_netlink` - should
not be something an application author reaches for directly. They matter
only to (a) a platform backend implementer, and (b) the channel-layer
code that sits on top of them. An application should be able to say "open
a channel of this type, connect it to that endpoint" without ever seeing
a `sapi_netlink_handle_t` or calling `sapi_netlink_open()` itself.

Investigating the current state surfaced two different problems, not one:

- `sapi_ipc` has **zero** consumers anywhere in either repository outside
  its own backend and its own unit test. Hiding it from the public,
  application-facing surface costs nothing - nothing legitimate depends
  on direct app access to it today.
- `sapi_netlink` is different: the framework's own higher-level channel
  abstraction, `sapi_dual_channel` (ADR-020), still requires the *caller*
  to open the netlink link(s) itself and hand over already-open
  `sapi_netlink_handle_t` values in its config. That gap is exactly why
  safeAPIExample's `channel_ab_types.h`, `monitor_c_types.h`, and
  `site.c` all include `safeapi/netlink/sapi_netlink.h` directly and call
  `sapi_netlink_open()`/`_send()`/`_receive()`/`_close()` themselves - the
  "channel" layer that was supposed to make this unnecessary doesn't
  reach far enough down.

Additionally, `channel_ab.c`/`monitor_c.c` predate `sapi_dual_channel`
entirely: they carry a hand-rolled EN 50159-style wire protocol (CRC-64,
sequence numbers, sender/peer ID checks - `channel_ab_wire.c`) built
directly over raw netlink, which ADR-020 section 4 explicitly scoped out
of retrofitting at the time. This ADR revisits that decision: the
retrofit is now in scope, driven by the "hide netlink" requirement rather
than by ADR-020 itself.

## 2. Decision

### 2.1 A new module, `sapi_safechannel`, not a repurposed `sapi_channel`

The obvious name, `sapi_channel`, is already taken: `include/safeapi/channel/sapi_channel.h`
is a live (if currently excluded-from-build) ADR-008 module with an
unrelated purpose - compile-time build-diversity identity and byte
comparison for a 2oo2 architecture, not a transport/handle abstraction.
Reusing that name for this ADR's factory would recreate exactly the kind
of "which `sapi_channel` do you mean" confusion this whole thread is
trying to remove. The new module is `sapi_safechannel`
(`include/safeapi/safechannel/sapi_safechannel.h`,
`src/safechannel/sapi_safechannel.c`), function prefix `sapi_safechannel_*`.
ADR-008's module is left as-is; its own disposition (fix-and-readopt vs.
retire) remains the open, separate decision it already was.

### 2.2 One handle, one type enum, one config, netlink opened internally

```c
typedef enum sapi_safechannel_type_e
{
    SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT = 0, /* wraps sapi_dual_channel_t */
    SAPI_SAFECHANNEL_TYPE_VITAL_VOTED    = 1  /* wraps sapi_vital_channel_t */
} sapi_safechannel_type_t;

typedef struct sapi_safechannel_endpoint_s
{
    sapi_netlink_role_t role;  /* LISTEN or CONNECT */
    const char         *host;  /* CONNECT: required. LISTEN: NULL = any. */
    uint16_t            port;
} sapi_safechannel_endpoint_t;
```

`sapi_safechannel_config_t` is a `type` tag plus a union of
`sapi_safechannel_dual_config_t` (endpoints[], link_count, sender_id,
expected_peer_id, connect/ack timeouts, optional status callback) and
`sapi_safechannel_vital_config_t` (endpoints[], link_count, voting
strategy, quorum, timeouts, optional disagreement callback) - mirroring
`sapi_dual_channel_config_t`/`sapi_vital_channel_config_t` field-for-field
except that **endpoints replace pre-opened handles**.
`sapi_safechannel_open()` opens every configured endpoint via
`sapi_netlink_open()` itself (retrying up to each config's
connect_timeout_ms, same pattern every current hand-rolled caller already
implements), then initializes the wrapped `sapi_dual_channel_t` or
`sapi_vital_channel_t` on top of the resulting handles. Storage is a
fixed, caller-owned struct (no dynamic allocation): an array of
`sapi_netlink_storage_t`/`sapi_netlink_handle_t` sized to
`SAPI_SAFECHANNEL_MAX_LINKS`, plus a union of the two wrapped types.

For `VITAL_VOTED`, `sapi_safechannel.c` supplies the
`backend_send`/`backend_recv` callbacks `sapi_vital_channel_config_t`
requires itself, implemented as thin casts to
`sapi_netlink_send()`/`_receive()` on the corresponding opened link. This
preserves `sapi_vital_channel.h`'s existing transport-agnostic design
(it still never mentions netlink) while giving the app a netlink-backed
instance without writing that bridge itself.

Uniform operations regardless of type:

```c
sapi_status_t sapi_safechannel_open(sapi_safechannel_t *channel, const sapi_safechannel_config_t *config);
sapi_status_t sapi_safechannel_send(sapi_safechannel_t *channel, const uint8_t *payload, size_t payload_size);
sapi_status_t sapi_safechannel_receive(sapi_safechannel_t *channel, uint8_t *out_payload, size_t max_size,
                                        sapi_duration_ms_t timeout_ms, size_t *out_size);
sapi_status_t sapi_safechannel_close(sapi_safechannel_t *channel);
sapi_safechannel_link_status_t sapi_safechannel_get_status(const sapi_safechannel_t *channel);
```

An application now includes exactly one header
(`safeapi/safechannel/sapi_safechannel.h`) and never includes
`safeapi/netlink/sapi_netlink.h` or `safeapi/ipc/sapi_ipc.h` at all.

### 2.3 Consequence for `sapi_ipc`/`sapi_netlink`'s status

Both remain real, tested OAL services with their own ADR-005 backend
registration - a platform integrator still implements
`sapi_netlink_backend_t`/`sapi_ipc_backend_t` exactly as before, and
`sapi_safechannel.c` itself is the one piece of framework code (besides
tests) that calls `sapi_netlink_open()`/`_send()`/`_receive()`/`_close()`
directly. Their headers are not moved (they still live under
`include/safeapi/`, since a backend implementer is a legitimate consumer
of the consumer-facing type/handle definitions, not just the backend
header) but the *expectation* is now explicit: application code goes
through `sapi_safechannel`; direct `sapi_netlink`/`sapi_ipc` use is a
backend-or-channel-layer-only concern, the same posture ADR-021 already
established for the backend vtables themselves.

### 2.4 Migration scope in safeAPIExample

All three of SITE's heartbeat link, `monitor_c`'s two listen links, and
`channel_ab`'s peer/C-forward links move to `sapi_safechannel`
(`SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT`, `link_count = 1` in every case -
none of these have actual redundant physical paths today; the redundancy
in this application lives at the A/B channel-pair level, not the link
level). This retires every direct `sapi_netlink_*` call from application
code.

- **SITE and `monitor_c`**: mechanical swap. Both are already a plain
  synchronous send-then-receive (or receive-only) pattern over one link
  per cycle; `sapi_dual_channel_send()`/`_receive()`'s DATA-frame
  semantics (send-and-wait-for-ACK, receive-most-recent-staged) satisfy
  this directly. Gains EN 50159 framing (CRC-64/seq/sender-id) on these
  two links as a side effect, where before they carried a raw
  application-defined struct with no framing at all.
- **`channel_ab`**: harder, because the one physical A<->B peer link
  today multiplexes two logical flows - periodic `AB_SAMPLE` broadcast
  data, and a synchronous `CHECKPOINT_REQUEST`/`REPLY` sub-protocol
  (`sapi_channel_checkpoint()`, ADR-017) - demuxed by a single background
  reader thread specifically to avoid a read race between that thread and
  the synchronous checkpoint call. `sapi_dual_channel_t` has no concept
  of "kind" beyond its own DATA/STATE frame split, and its `_receive()`
  documents returning only the *most recently staged* frame - i.e. it is
  still exactly one logical reader's responsibility to poll it, same
  constraint the current hand-rolled design already respects. The
  migration therefore keeps the existing architecture unchanged
  (one background thread owns all reads off the `sapi_safechannel_t`
  instance; `peer_send_mutex` still guards every send; the reconnect-owns-
  handle convention is unchanged) and only swaps the transport calls:
  - `channel_ab_wire.c`'s existing `PEER_MSG_KIND_*` tag byte moves from
    prefixing a raw netlink frame to prefixing the payload passed to
    `sapi_safechannel_send()`/returned by `_receive()` - unchanged
    encoding, different carrier.
  - `channel_ab_checkpoint.c`'s `sapi_vital_channel_t` backend adapter
    (`channel_ab_checkpoint_backend_send/recv`) now calls
    `sapi_safechannel_send()`/`_receive()` on the same instance the
    background thread reads from, instead of `sapi_netlink_send()`/
    `_receive()` directly - no change to the checkpoint call's own
    synchronization story, since it was never touching the socket
    directly even before this change (channel_ab_io.c always intermediated).
  - `channel_ab_wire.c`'s own hand-rolled CRC-64 signing on `AB_SAMPLE`
    payloads is **left in place**, even though `sapi_dual_msgchannel`
    now adds a second, framework-level CRC-64/seq/sender-id envelope
    around the same bytes. This is deliberately redundant-but-harmless
    rather than a further simplification bundled into this change - a
    safety-critical wire format's defenses should be removed in their
    own separately-reviewed, separately-verified change, not as a
    side effect of a transport-hiding refactor.
  - Because the wire format changes (raw bytes -> EN 50159-framed DATA
    frames) on every link, and both ends of every link are this same
    codebase, there is no external interop/backward-compatibility
    concern - old and new binaries are simply not run against each other.

### 2.5 Verification bar

SITE and `monitor_c`'s migrations are lower-risk (no multiplexing, no
safety-decision logic riding on the link itself) and are verified with a
build + a live run of the affected roles. `channel_ab`'s migration
touches the DISAGREE-triggered reboot path, the dual-transfer watchdog's
single-mode degradation, and the checkpoint rendezvous - the same
surface a prior session already stress-tested end-to-end (long-duration
fault -> reboot -> recover run). That same rigor is repeated after this
migration, not skipped in favor of a shorter smoke test, given what rides
on this specific path.

## 3. Consequences

- Positive: no application code anywhere in safeAPIExample includes
  `sapi_netlink.h`/`sapi_ipc.h` or calls their functions directly after
  this change; "open a channel, pick a type" is now literally the API.
- Positive: SITE's and `monitor_c`'s links gain EN 50159 defended-messaging
  framing they previously lacked, for free.
- Negative: `sapi_safechannel_config_t`'s per-type union, plus the
  existing per-type configs it wraps, is a third layer of configuration
  struct for the dual-redundant case (safechannel config -> dual_channel
  config -> netlink config internally) - more indirection than calling
  `sapi_netlink_open()` directly, traded for not needing to know netlink
  exists at all.
- Negative: `channel_ab`'s migration is real surgery on already-hardened
  safety logic and requires full fault-injection re-verification, not a
  quick build check.
- Deferred, not done here: retiring `channel_ab_wire.c`'s now-partially-
  redundant hand-rolled CRC layer (2.4); ADR-008's `sapi_channel`
  disposition; extending `sapi_safechannel` to types other than
  `DUAL_REDUNDANT`/`VITAL_VOTED` if a future consumer needs them.

## 4. Status

Framework module (`sapi_safechannel` + tests): done. SITE's WEST/EAST
heartbeat migration: done - `site.c` no longer includes
`safeapi/netlink/sapi_netlink.h` or holds a `sapi_netlink_handle_t`;
its one heartbeat link is a `SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT`
`sapi_safechannel_t`. This migration was also what surfaced and drove
the fix to `sapi_dual_channel_send()`'s ACK-wait loop documented in
ADR-020's "Post-acceptance fix" section - the first time that module was
exercised over a real (not mocked) transport. Verified via a live
two-process WEST/EAST run: negotiation completes, steady-state
heartbeats continue exchanging role/single-mode status every cycle.
`monitor_c` and `channel_ab` migrations remain pending; see the MISRA
compliance report for build/test verification detail as each lands.

# ADR-019: AppManager Cycle Hooks and Built-In Checkpoint Rendezvous

Status: Accepted
Date: 2026-08-06
Applies to: `sapi_appmanager` (extended, backward-compatible), reusing
`sapi_checkpoint`/`sapi_channel` (ADR-017) without changes to either.

## 1. Context

Every application that uses `sapi_appmanager_run()` today writes its own
`execute()` callback as one undivided block, and if it is part of an A/B
dual-channel pair it must hand-roll its own cross-channel cycle-sync check
inside that block (see `safeAPIRBC2oo2/src/application/AB/channel_ab.c`,
which currently has phases for reading, waiting for the peer's dual
transfer, comparing, and forwarding, all inlined into one `execute()`).
Two related requests came out of that experience:

1. A dual-channel application wants to know, every cycle, "is my peer at
   the same point I am" — not after the fact by comparing results (that
   is what `sapi_channel`'s voting already does), but as a guard
   *before* that cycle's decision logic runs. `sapi_checkpoint`
   (ADR-017) already does exactly this — `sapi_channel_checkpoint()` is a
   bounded rendezvous that safe-states on timeout (REQ-CHECKPOINT-003) —
   but nothing wires it into the app's main loop automatically; every app
   that wants it must call it by hand at the right place, every cycle.
2. Splitting a cycle into "read/prepare inputs", "the actual decision
   logic", and "send outputs/cleanup" is a shape nearly every cyclic
   safety application already has (RBC included). Today `execute()` is a
   single opaque callback, so every application reimplements this
   structure inside its own body instead of the framework expressing it.

This ADR extends `sapi_appmanager` to cover both, without introducing any
new synchronization mechanism — it wires the existing `sapi_checkpoint`
primitive into the existing lifecycle loop, and splits the existing
`execute()` slot into three optional stages.

## 2. Decision

### 2.1 Three cycle hooks, `execute` unchanged in meaning

`sapi_appmanager_operations_t` gains two new **optional** members,
`pre_execute` and `post_execute`, with the same signature as the existing
`execute`:

```c
typedef struct {
    sapi_status_t (*init)(void *context);
    sapi_status_t (*pre_execute)(void *context);   /* NEW, may be NULL */
    sapi_status_t (*execute)(void *context);        /* unchanged */
    sapi_status_t (*post_execute)(void *context);  /* NEW, may be NULL */
    sapi_status_t (*shutdown)(void *context);
    const char *(*get_name)(void);
    const char *(*get_version)(void);
} sapi_appmanager_operations_t;
```

`execute()`'s contract does not change — it remains the mandatory "main"
stage; existing applications that only set `.execute` (every application
in `safeAPIRBC2oo2` today) keep compiling and behaving identically, since
a C struct's unset members are zero-initialized in a designated
initializer and `sapi_appmanager_run()` treats a NULL `pre_execute`/
`post_execute` as "skip this stage", not an error.

Per-cycle order inside `sapi_appmanager_run()`'s loop:

```
[checkpoint, if configured — see 2.2]
  -> pre_execute()  (skipped if NULL)
  -> execute()
  -> post_execute() (skipped if NULL)
```

`pre_execute`/`post_execute` returning non-OK is handled identically to
`execute()` returning non-OK today: logged, `error_count` incremented,
`error_threshold` checked. This keeps one error-handling path instead of
three.

### 2.2 Checkpoint is opt-in, reuses `iteration_count` as the checkpoint ID

`sapi_appmanager_config_t` gains one new optional field:

```c
typedef struct {
    const sapi_appmanager_operations_t *ops;
    void *context;
    uint32_t max_iterations;
    uint32_t error_threshold;
    const sapi_appmanager_checkpoint_config_t *checkpoint; /* NEW, NULL = disabled */
} sapi_appmanager_config_t;

typedef struct {
    sapi_channel_t *vital_channel;   /* checkpoint target */
    sapi_duration_ms_t    max_delay_ms;    /* forwarded to sapi_checkpoint_config_t */
    uint32_t              expected_node_count;
    sapi_watchdog_t       watchdog;        /* optional, may be NULL */
} sapi_appmanager_checkpoint_config_t;
```

(ADR-025 superseded the checkpoint target's type: `vital_channel`
became `voter`, of type `sapi_voter_t *` - a voter with its channels
already registered, rather than a raw multi-channel handle. The
decision described in this section - checkpoint runs first, every
cycle, opt-in via `config->checkpoint == NULL` - is otherwise
unchanged; only the target type's shape moved.)

`config->checkpoint == NULL` (the default) disables the feature entirely
— zero behavior change and zero added latency for every application that
is not part of a synchronized multi-channel group (in `safeAPIRBC2oo2`
today, that is SITE and C; only A/B would set this). This has to be
opt-in rather than unconditional for two reasons: `sapi_channel_checkpoint()`
is a *blocking* call bounded by `max_delay_ms`, so turning it on
unconditionally would add that latency to every AppManager application's
every cycle whether or not it needed cross-channel agreement; and not
every AppManager application is part of an A/B pair at all.

When `config->checkpoint != NULL`, `sapi_appmanager_run()` builds a
`sapi_checkpoint_config_t` each cycle using
`state.iteration_count` as `checkpoint_id` — `sapi_checkpoint.h` already
documents this field as "e.g. a per-cycle counter"
(`include/safeapi/checkpoint/sapi_checkpoint.h`), and AppManager already
tracks `iteration_count` for its own stats (`sapi_appmanager_get_stats()`),
so no new counter is introduced — and calls
`sapi_channel_checkpoint(config->checkpoint->vital_channel, &built_config)`
before `pre_execute()`. Checkpointing first, ahead of any stage that
might act on this cycle's data, means a desynced peer is caught before
either channel does decision work that cycle — not discovered afterward
by comparing results that were already computed against different
inputs.

A checkpoint timeout is not given a special reaction here:
`sapi_channel_checkpoint()` has already called
`sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE, SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT)`
itself (REQ-CHECKPOINT-003) before returning `SAPI_STATUS_TIMEOUT`, and
`sapi_appmanager_run()` handles that `SAPI_STATUS_TIMEOUT` exactly like
any other failed stage — logged, `error_count` incremented,
`pre_execute`/`execute`/`post_execute` skipped for that cycle,
`error_threshold` checked — rather than inventing a second reaction path.

**This accounting path is defensive, not the expected outcome.** In the
framework's shipped `sapi_safestate.c`, `SAPI_SAFESTATE_LEVEL_SAFE` is an
unconditional, permanent halt (REQ-COMMON-SAFESTATE-002): after invoking
any registered handler, `sapi_safestate_enter()` always spins forever
regardless of whether that handler returns, so `sapi_channel_checkpoint()`
returning `SAPI_STATUS_TIMEOUT` to its caller at all is not reachable
through a normal handler return — the only way is a handler that itself
diverts control flow (e.g. `longjmp`, as `tests/checkpoint/test_sapi_checkpoint.c`
already does to verify the SAFE-entry without hanging the test suite).
`sapi_appmanager_run()`'s handling of that return value exists so the
accounting is *correct if it is ever reached* (a test harness diverting
around the halt, or a future/alternate safestate backend that does not
halt unconditionally) — not because a real SIL2/SIL3 deployment using the
default handler is expected to see it, since that deployment halts at
the `sapi_safestate_enter()` call before `sapi_channel_checkpoint()` ever
returns.

### 2.3 No change to `sapi_checkpoint` or `sapi_channel`

This ADR adds no new fields or behavior to either module (ADR-017). This
is deliberately a composition, not a modification: `sapi_appmanager`
becomes a caller of `sapi_channel_checkpoint()`, the same way any
integrator's own `execute()` could call it by hand today. An application
that wants checkpoint semantics AppManager does not yet cover (e.g.
per-channel `checkpoint_id` values other than the iteration counter) can
still call `sapi_channel_checkpoint()` directly from its own
`pre_execute()` and leave `config->checkpoint` NULL.

## 3. Consequences

- Positive: existing applications are unaffected — `pre_execute`/
  `post_execute`/`checkpoint` are all optional and default to
  off/skipped.
- Positive: a dual-channel application's main-loop boilerplate shrinks —
  cycle-sync guarding moves from hand-written code in every `execute()`
  into one config field, and the read/decide/send split becomes three
  named functions instead of one function with internal phase comments
  (see `channel_ab.c`'s existing phase-numbered comments as the
  motivating example).
- Positive: reuses `iteration_count`, already-tested `sapi_checkpoint`,
  and the existing error/threshold accounting — no new counters, no new
  error-handling path, no new safe-state trigger.
- Negative / accepted cost: `sapi_appmanager_config_t` and
  `sapi_appmanager_operations_t` grow by one and two pointer-sized fields
  respectively. Source-compatible for every caller using designated
  initializers (the only style used anywhere in this codebase's own
  examples); not binary-compatible with a prebuilt caller using
  positional initialization against the old struct layout — flagged
  here rather than silently assumed away, consistent with this
  project's practice of stating deviations rather than hiding them.
- Negative / scope limit: checkpoint hook ordering is fixed
  (checkpoint before `pre_execute`); an application that needs the
  checkpoint *after* reading fresh inputs instead of before must call
  `sapi_channel_checkpoint()` itself from `pre_execute()` and leave
  `config->checkpoint` NULL, per 2.3.

## 4. Location

`include/safeapi/appmanager/sapi_appmanager.h` +
`src/appmanager/sapi_appmanager.c` (existing target `safeapi::appmanager`,
gains a link dependency on `safeapi::checkpoint` for the optional
checkpoint path only).

## 5. Addendum: retrofitting `safeAPIRBC2oo2/src/application/AB/channel_ab.c`

Section 2's design was validated by actually wiring it into A/B's real
topology, not just by unit test. Two gaps surfaced that were not visible
from the framework side alone, both resolved with the user's direction,
and both are framework-level changes this addendum records for
traceability.

### 5.1 `sapi_channel_init()`'s `channel_count >= 2` floor

`sapi_channel_checkpoint()` requires a `sapi_channel_t`, and that
constructor originally rejected `channel_count < 2` — it modeled "this
node has 2+ redundant transport paths to its peer(s)". A/B's actual
topology is exactly one physical TCP link to exactly one peer, which does
not fit that constructor without either faking a second entry pointing at
the same live socket (a double-send/double-read correctness bug, not a
shortcut) or adding a genuinely separate second transport path (out of
scope for this retrofit).

Decision (user-directed): relax `sapi_channel_init()`'s floor to
`channel_count >= 1`, reachable only via `SAPI_VOTING_NMR` with
`quorum_size == 1` — 2oo2 and 2oo3 keep their existing floors of exactly 2
and exactly 3 respectively, so this does not weaken any existing voting
strategy's own guarantee, it only makes a degenerate 1-channel NMR
instance constructible for callers (like this checkpoint transport) that
are not doing cross-channel voting at all, just using `sapi_channel`
as `sapi_checkpoint`'s required abstraction over "a channel with a
backend". See `include/safeapi/channel_link/sapi_channel.h`'s own
`@pre channel_count` doc for the exact conditions.

### 5.2 One physical link, two message protocols, and a missing reply

Even with 5.1, `sapi_channel_checkpoint()`'s wire format
(`sapi_vital_message_t`, ~272 bytes) does not fit A/B's existing peer
link, which was opened with a fixed 16-byte `AB_SAMPLE` frame size
(`sapi_netlink`'s `message_size` is fixed per link at `sapi_netlink_open()`
time — every message on that link must be exactly that size), and a
background task already exclusively calls `sapi_netlink_receive()` on
that link — a second, synchronous reader for checkpoint traffic would
race it.

Decision (user-directed: "the same channel shall support different types
of message"): multiplex both protocols onto the one existing link instead
of opening a second one. Every frame now carries a 1-byte kind tag
(`PEER_MSG_KIND_AB_SAMPLE`, `PEER_MSG_KIND_CHECKPOINT_REQUEST`,
`PEER_MSG_KIND_CHECKPOINT_REPLY`) ahead of its payload, sized to the
larger of the two payloads; the link's existing single background reader
(`peer_rx_task_entry()`) demultiplexes by that tag instead of a second
task being added.

This surfaced a second gap during live verification, not from code
reading alone: `sapi_channel_checkpoint()` sends its own arrival marker
and then blocks waiting for an explicit reply confirming that *same*
`checkpoint_id` — nothing in `sapi_checkpoint` or in this retrofit's first
version ever generated that reply, so both A and B hung permanently after
their first cycle (`sapi_channel_checkpoint()`'s `SAPI_STATUS_TIMEOUT`
path is only reachable via a diverting handler per 2.2's own note — the
default handler's unconditional halt fired instead, silently, with no
further output). Fixed by splitting the checkpoint tag into
`_REQUEST`/`_REPLY`: `peer_rx_task_entry()` now also acts as this
checkpoint's own auto-responder, echoing a `_REPLY` back the instant it
sees the peer's `_REQUEST`, and only ever treats an actual `_REPLY` as the
signal `peer_checkpoint_backend_recv()` is waiting on.

Making the background thread a sender as well as a reader introduced a
third, independent hazard: `sapi_netlink`'s POSIX backend `send()` is a
partial-write retry loop, not one atomic syscall, so the background
thread's `_REPLY` echo and the main thread's own sends (`_REQUEST`,
`AB_SAMPLE`) could now interleave on the same file descriptor and corrupt
a frame. A `pthread_mutex_t` (`peer_send_mutex`) added directly to
`channel_ab_context_t` — not exposed as a new framework primitive —
guards every `sapi_netlink_send()` call site on that link.

### 5.3 Safety-policy reconciliation, not a framework change

`sapi_channel_checkpoint()`'s mandatory SAFE-halt on timeout
(REQ-CHECKPOINT-003) is intentionally more aggressive than this
particular application's own deliberately lenient peer-loss policy
(reconnect, degrade to SINGLE mode, never halt just because a peer went
briefly quiet — see `channel_ab.c`'s own file-header doc on
`on_dual_transfer_lost()`). Resolved entirely on the application side, not
by weakening either policy: `ctx->checkpoint_cfg.vital_channel` is
toggled to `NULL` in lockstep with the peer link's own down state, since
`sapi_channel_checkpoint(NULL, ...)` returns the harmless, recoverable
`SAPI_STATUS_INVALID_PARAM` (per 2.2, handled like any other failed
stage) rather than ever reaching the halt-triggering branch. This also
uncovered and fixed a real ordering bug in `sapi_appmanager_run()` itself
(not specific to this retrofit): the original code validated
`config->checkpoint->vital_channel != NULL` before calling `ops->init()`,
which rejects the realistic pattern where an integrator's own `init()` is
what populates that field. That startup validation was removed; the
per-cycle NULL handling already described in 2.2 is what actually needs
to be correct, and now is the only check.

### 5.4 Result

Live-verified with the full 8-process WEST+EAST demo (`etc/run_all.sh`'s
topology): all four A/B processes progress cycle over cycle with
`AGREE`/`0 errors`, no checkpoint timeouts, no `APPMANAGER ERROR` lines —
confirming the `_REQUEST`/`_REPLY` auto-responder and `peer_send_mutex`
fix resolved the hang found in the first wiring attempt.

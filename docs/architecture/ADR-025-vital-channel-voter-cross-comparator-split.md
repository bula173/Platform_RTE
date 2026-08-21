# ADR-025: Split `sapi_vital_channel` into `sapi_channel` + `sapi_voter` + `sapi_cross_comparator`

Status: Accepted
Date: 2026-08-16
Applies to: `include/safeapi/vital_channel/` -> `include/safeapi/channel_link/`,
new `include/safeapi/voter/`, new `include/safeapi/cross_comparator/`,
`src/vital_channel/` -> `src/channel_link/`, new `src/voter/`, new
`src/cross_comparator/`, `src/checkpoint/`, `src/safechannel/`,
`src/appmanager/`, the top-level `CMakeLists.txt` and
`tests/CMakeLists.txt`, and the retirement of the old, dead ADR-008
`include/safeapi/channel/` / `src/channel/` / `tests/channel/`.

## 1. Context

Before this ADR, `sapi_vital_channel_t` conflated two separate
responsibilities into one type:

1. **A single redundant link** - an opaque transport handle plus
   send/recv callbacks, with its own health/error bookkeeping.
2. **An N-way voting bundle** - `sapi_vital_channel_init()` took a whole
   array of backend handles plus a `voting_strategy` and internally
   managed `channel_count` sub-links, running majority-vote logic
   inside `sapi_vital_channel_receive()`.

This made it impossible to express two use cases that are conceptually
distinct but were forced through the same API:

- **N-modular redundancy voting** (2oo2/2oo3/NMR) on multiple redundant
  copies of *the same* computation, feeding a single arbitrated result
  back to the caller.
- **Cross-comparison** between exactly *two independent* peer
  computations (e.g. an A/B pair), where the interesting output is
  "did they agree," not a voted single value - a pattern
  `safeAPIRBC2oo2`'s hand-rolled `channel_ab_crosscompare.c` had
  already reimplemented outside the framework because no equivalent
  primitive existed here.

Exploration during this redesign also surfaced a real bug in the old
voting algorithm: `sapi_vital_channel_receive()` compared every
channel's data *pairwise against the first successful channel's data
only*. With 3 channels where channel B and C agree but A disagrees,
this reported `SAPI_VOTING_DISAGREED` instead of the correct 2-of-3
majority result. This was not an edge case found by inspection alone -
it is the direct consequence of "compare everyone to channel 0" not
being equivalent to "find the largest group of mutually-agreeing
channels," and it would misbehave on every 2oo3/NMR(>2) deployment
where the *first* configured channel happens to be the outlier.

Additionally, `sapi_vital_channel_t`'s old multi-channel shape was
directly and deeply embedded in two other modules, meaning any
redesign had to account for a ripple effect, not just the type itself:

- `sapi_channel_checkpoint()` took a `sapi_vital_channel_t *handle`
  directly and reached into `handle->channel_count`,
  `handle->channels[i]`, `handle->config.backend_send`/`backend_recv`.
- `sapi_safechannel_t`'s `SAPI_SAFECHANNEL_TYPE_VITAL_VOTED` path
  embedded one whole multi-channel `sapi_vital_channel_t` directly
  inside its `impl` union and called the old
  `sapi_vital_channel_init/_send/_receive/_get_aggregated_health` on it
  with all links passed at once.

Separately, the framework already had a long-dead, never-compiled
ADR-008 module at `include/safeapi/channel/` / `src/channel/` (excluded
from every build since before ADR-023, per that ADR's own §2.1/§5)
whose intended purpose - a simple 2-channel comparator - is exactly
what `sapi_cross_comparator` now provides for real, tested and
buildable. That dead module has been removed outright as part of this
ADR rather than left to bit-rot alongside its live replacement.

**Explicitly out of scope for this ADR**: the dual-transfer
online-to-standby callback and timer/task RESTART/CONTINUE/ASK_USER
switchover policy that motivated this whole investigation. Exploration
found zero precedent for it anywhere in either repository -
`sapi_dual_channel` is deliberately negotiator-unaware per ADR-020 §3,
and no timer/task transfer mechanism exists in any form yet. That
remains a separate, future piece of work.

## 2. Decision

Split the old `sapi_vital_channel` into three modules along the
responsibility boundary described above.

### 2.1 `sapi_channel` (renamed from `sapi_vital_channel`, single link)

`include/safeapi/channel_link/sapi_channel.h` /
`src/channel_link/sapi_channel.c` - reduced to exactly one redundant
link: an opaque `channel_handle` plus `send`/`recv` callbacks, with its
own `sapi_channel_health_t` (send/recv counts, error counts,
`is_healthy`). No voting, no quorum, no multi-channel bookkeeping - just
validate-then-dispatch, the same shape already established by
`sapi_timer`/`sapi_nvm`.

```c
sapi_status_t sapi_channel_init(sapi_channel_storage_t *storage, const sapi_channel_config_t *config);
sapi_status_t sapi_channel_send(sapi_channel_t *handle, const void *data, size_t size);
sapi_status_t sapi_channel_receive(sapi_channel_t *handle, void *data, size_t size, uint32_t timeout_ms);
sapi_status_t sapi_channel_get_health(const sapi_channel_t *handle, sapi_channel_health_t *out_health);
sapi_status_t sapi_channel_set_healthy(sapi_channel_t *handle, bool is_healthy);
sapi_status_t sapi_channel_destroy(sapi_channel_t *handle);
```

Renamed from `sapi_vital_channel` to `sapi_channel` because it is no
longer specific to "vital" (safety-rated) computers - it is a generic
single-link primitive any two endpoints can use, vital or not. Placed
in a **new** directory, `channel_link/`, rather than reusing the
now-freed `channel/` name, to avoid any risk of confusion with the just
-retired ADR-008 module during the transition and to keep "channel" as
a word free for use in prose without a specific-directory implication.

### 2.2 `sapi_voter` (new) - N-way voting, with the majority-vote bug fixed

`include/safeapi/voter/sapi_voter.h` / `src/voter/sapi_voter.c` -
registers up to `SAPI_VOTER_MAX_CHANNELS` (8) `sapi_channel_t`
instances and performs 2oo2/2oo3/NMR voting over them.

```c
sapi_status_t sapi_voter_init(sapi_voter_storage_t *storage, const sapi_voter_config_t *config);
sapi_status_t sapi_voter_register_channel(sapi_voter_t *voter, sapi_channel_t *channel);
sapi_status_t sapi_voter_send(sapi_voter_t *voter, const void *data, size_t size);
sapi_status_t sapi_voter_receive(sapi_voter_t *voter, void *data, size_t size,
                                  sapi_voting_result_t *result, size_t *bytes_received);
sapi_status_t sapi_voter_get_aggregated_health(const sapi_voter_t *voter, uint32_t *healthy_count,
                                                uint32_t *total_disagreements);
uint32_t sapi_voter_get_channel_count(const sapi_voter_t *voter);
sapi_channel_t *sapi_voter_get_channel(const sapi_voter_t *voter, uint32_t index);
sapi_status_t sapi_voter_destroy(sapi_voter_t *voter);
```

**Bug fix (behavior change, deliberate):** `sapi_voter_receive()`
groups the N successfully-received buffers by equality (via the
configured `compare` callback, or `memcmp` by default - see §2.4) and
picks the *largest* group, reporting `SAPI_VOTING_AGREED` with that
group's data if its size meets the configured strategy's quorum,
`SAPI_VOTING_DISAGREED` otherwise. This replaces the old
compare-everyone-to-channel-0 logic and changes observable behavior for
any 2oo3/NMR(>2) deployment where the first-registered channel was the
one in the minority - that deployment now correctly returns AGREED with
the majority's data instead of a spurious DISAGREED.

`get_channel_count()`/`get_channel()` were added only because
`sapi_channel_checkpoint()` needs to iterate a voter's registered
channels directly (§2.5) - this is a deliberately narrow accessor pair,
not a general-purpose iteration API.

### 2.3 `sapi_cross_comparator` (new) - 2-way peer comparison

`include/safeapi/cross_comparator/sapi_cross_comparator.h` /
`src/cross_comparator/sapi_cross_comparator.c` - structurally similar
to `sapi_voter` but fixed at exactly 2 registered channels and no
strategy/quorum configuration (only "both agree" is meaningful at
N=2). A 3rd `sapi_cross_comparator_register_channel()` call returns
`SAPI_STATUS_RESOURCE_EXHAUSTED`.

```c
sapi_status_t sapi_cross_comparator_init(sapi_cross_comparator_storage_t *storage,
                                          const sapi_cross_comparator_config_t *config);
sapi_status_t sapi_cross_comparator_register_channel(sapi_cross_comparator_t *cmp, sapi_channel_t *channel);
sapi_status_t sapi_cross_comparator_execute(sapi_cross_comparator_t *cmp, size_t data_size,
                                             sapi_voting_result_t *result, void *out_data, size_t *out_size);
sapi_status_t sapi_cross_comparator_get_aggregated_health(const sapi_cross_comparator_t *cmp,
                                                           uint32_t *healthy_count,
                                                           uint32_t *total_disagreements);
sapi_status_t sapi_cross_comparator_destroy(sapi_cross_comparator_t *cmp);
```

**Why this is a separate module, not just "Voter with N=2":** this is a
conceptual distinction the framework's API should make explicit, not
merely a technical one. A voter arbitrates redundant copies of *the
same* computation down to one authoritative answer - the caller wants a
single value back. A cross-comparator checks whether two *independent*
peer computations (e.g. an active/standby pair, or two different
algorithms computing the same safety-relevant quantity) still agree -
the caller wants a yes/no consistency signal, and in many real uses
(such as `safeAPIRBC2oo2`'s original hand-rolled A/B pattern this
generalizes) doesn't even need the arbitrated value, just the
disagreement signal itself. Sharing one type under a `strategy=2OO2`
flag would blur that intent at every call site; keeping them separate
means a reader immediately knows which relationship between the
registered channels is meant.

### 2.4 Shared comparison semantics

Both `sapi_voter` and `sapi_cross_comparator` accept an optional
`sapi_voter_compare_fn compare` callback (`bool (*)(const void *a, const
void *b, size_t size, void *user_ctx)`). If not registered, the default
is a full byte compare (`memcmp(a, b, size) == 0`) of the received
payloads. This is a deliberate default, not a placeholder: CRC64
(`sapi_checksum`) already covers transport-integrity verification
elsewhere in the stack and is not itself "the comparison" - conflating
the two would silently accept two payloads that pass their own
individual integrity checks but differ from each other, which is
exactly the disagreement case these two modules exist to catch.

Both modules are **caller-driven**: there is no internal timer or
thread inside either component. "Cyclic" checking, if a caller wants
it, means calling `sapi_voter_receive()` / `sapi_cross_comparator_execute()`
periodically from the caller's own loop - the same pattern
`sapi_channel_checkpoint()` already used from `sapi_appmanager`'s cycle
hook (ADR-019) before this ADR, and unchanged by it.

### 2.5 Rewiring `sapi_checkpoint`

`sapi_channel_checkpoint()`'s first parameter changed from
`sapi_vital_channel_t *handle` to `sapi_voter_t *voter`. Internals swap
the old direct field reads
(`handle->channel_count`/`handle->channels[i]`/
`handle->config.backend_send`/`backend_recv`) for
`sapi_voter_get_channel_count()`/`sapi_voter_get_channel()` plus
`sapi_channel_send()`/`sapi_channel_receive()` on each individual
channel. `sapi_appmanager_checkpoint_config_t`'s field was renamed from
`vital_channel` to `voter` to match
(`include/safeapi/appmanager/sapi_appmanager.h`,
`src/appmanager/sapi_appmanager.c`).

### 2.6 Rewiring `sapi_safechannel`'s voted path

`sapi_safechannel_t`'s `impl.vital` union member changed from one
embedded multi-channel `sapi_vital_channel_t` to:

```c
struct {
    sapi_channel_t channels[SAPI_SAFECHANNEL_MAX_LINKS];
    sapi_voter_t   voter;
} vital;
```

`safechannel_open_vital()` now opens each endpoint, creates the voter,
then creates one `sapi_channel_t` per opened endpoint and registers
each into the voter. `sapi_safechannel_send/_receive/_get_status/_close`
call `sapi_voter_send/_receive/_get_aggregated_health/_destroy` instead
of the old `sapi_vital_channel_*` equivalents. The
`on_disagreement` callback signature changed from
`(void *, const sapi_voting_result_t *)` to `(void *,
sapi_voting_result_t)` (by value) to match `sapi_voter_config_t`'s own
simplified signature.

### 2.7 Retirement of the old ADR-008 `channel` module

`include/safeapi/channel/`, `src/channel/`, and `tests/channel/` (the
dead, never-built ADR-008 2-channel comparator ADR-023 §2.1/§5 had
already flagged as excluded from every build) are removed outright.
`sapi_cross_comparator` (§2.3) covers its intended use case for real,
tested, and buildable. This closes the "retire-or-fix" open question
ADR-023 deferred for that module.

### 2.8 Build configuration (ADR-024)

`SAFEAPI_ENABLE_VITAL_CHANNEL` renamed to `SAFEAPI_ENABLE_CHANNEL_LINK`.
Two new options added, both default `ON`:

```
SAFEAPI_ENABLE_VOTER            needs CHANNEL_LINK, LOG
SAFEAPI_ENABLE_CROSS_COMPARATOR needs CHANNEL_LINK, VOTER, LOG
```

`SAFEAPI_ENABLE_CHECKPOINT`, `SAFEAPI_ENABLE_SAFECHANNEL`, and
`SAFEAPI_ENABLE_APPMANAGER` each gained a dependency on
`SAFEAPI_ENABLE_VOTER` (previously only on `VITAL_CHANNEL`/
`CHANNEL_LINK`). See ADR-024 §2.1 for the updated full dependency graph.

## 3. Consequences

- Positive: the API now matches the mental model an integrator actually
  reasons in - "one link," "vote across N links," "cross-check 2
  independent peers" are three distinct, independently testable
  concerns instead of one overloaded type.
- Positive: the majority-vote bug fix (§2.2) is a real correctness
  improvement for every existing or future 2oo3/NMR(>2) deployment, not
  just a naming cleanup.
- Positive: `sapi_cross_comparator` gives `safeAPIRBC2oo2` (and any
  future integrator with an A/B peer-comparison need) a tested framework
  primitive instead of requiring a hand-rolled reimplementation.
- Positive: the old, dead ADR-008 module's disposition (an open question
  since ADR-023) is now closed - removed, superseded.
- Negative: this is a breaking API change to `sapi_vital_channel`'s
  public surface (rename, signature changes, struct-shape changes
  rippling into `sapi_checkpoint`/`sapi_safechannel`/`sapi_appmanager`).
  Pre-implementation exploration concluded `safeAPIRBC2oo2` did not
  consume this API directly - that turned out to be **wrong**: its
  `channel_ab*.c` files use `sapi_vital_channel_t` directly, for the
  ADR-019 addendum's built-in-checkpoint wiring over the A/B peer link
  (one `sapi_vital_channel_t` with `channel_count == 1`, the
  `SAPI_VOTING_NMR`/`quorum_size == 1` "single link" case), not through
  `channel_ab_crosscompare.c`'s hand-rolled cross-compare logic as
  assumed (that part genuinely is independent, as originally thought).
  This was caught by a `safeAPIRBC2oo2` build failure after the rename
  landed, not by the original exploration - `channel_ab_types.h`,
  `channel_ab.c`, `channel_ab_io.c`, and `channel_ab_checkpoint.h`/`.c`
  needed the same rewiring as this framework's own `sapi_checkpoint`:
  one `sapi_channel_t` (`channel_ab_context_t::checkpoint_vc`)
  registered into one `sapi_voter_t`
  (`channel_ab_context_t::checkpoint_voter`, `SAPI_VOTING_NMR`,
  `quorum_size == 1`), with `sapi_appmanager_checkpoint_config_t::voter`
  replacing the old `::vital_channel` field throughout. Fixed as part of
  this same change; verified via `safeAPIRBC2oo2`'s own
  `ctest` and `.claude/skills/run-safeAPIRBC2oo2/smoke.sh` (which
  explicitly checks for "A/WEST checkpoint traffic" / "B/WEST checkpoint
  traffic" in the running processes' logs, so it exercises the live
  checkpoint path, not just that the binary links). Lesson for future
  ADRs with a similar blast radius: a downstream repo's own build is
  part of verification, not just its exploration - grep-based
  exploration of a downstream consumer can miss a real dependency that
  a full build immediately surfaces.
- Neutral: the dual-transfer online-to-standby callback and timer/task
  switchover policy that originally motivated this investigation remains
  out of scope (§1) - a future ADR, not this one.

## 4. Verification

- `cmake -S . -B build -DSAFEAPI_BUILD_TESTS=ON && cmake --build build`:
  clean build, zero warnings (default `SAFEAPI_WARNINGS_AS_ERRORS=ON`).
- `ctest --test-dir build --output-on-failure`: all 26 test executables
  pass, including new `test_sapi_voter`/`test_sapi_cross_comparator` and
  the rewired `test_sapi_channel` (formerly `test_sapi_vital_channel`),
  `test_sapi_checkpoint`, `test_sapi_appmanager`, `test_sapi_safechannel`.
- `test_sapi_voter`'s `test_receive_majority_vote_fix` specifically
  exercises the §2.2 bug fix: 3 channels where channel 0 disagrees but
  channels 1 and 2 agree - asserts the voter returns the channels-1/2
  majority data, not channel 0's.
- `safeAPIRBC2oo2` (downstream consumer, separate repo, consumed via
  `add_subdirectory()`): `cmake --build build` clean;
  `ctest --test-dir build` (2/2 tests) pass;
  `.claude/skills/run-safeAPIRBC2oo2/smoke.sh` passes, including its
  explicit "A/WEST checkpoint traffic" / "B/WEST checkpoint traffic"
  log checks - the live, rewired `sapi_channel_t`/`sapi_voter_t`
  checkpoint path.
- Manual repository-wide grep for `sapi_vital_channel`/
  `SAFEAPI_ENABLE_VITAL_CHANNEL` confirmed no remaining reference to a
  symbol or option name that no longer exists in any file the build
  actually compiles (`src/`, `include/`, `tests/`, `CMakeLists.txt`,
  `tests/CMakeLists.txt`).

## 5. Location

- `include/safeapi/channel_link/`, `src/channel_link/`, `tests/channel_link/`
- `include/safeapi/voter/`, `src/voter/`, `tests/voter/`
- `include/safeapi/cross_comparator/`, `src/cross_comparator/`, `tests/cross_comparator/`
- `src/checkpoint/sapi_checkpoint.c`, `include/safeapi/checkpoint/sapi_checkpoint.h`
- `src/safechannel/sapi_safechannel.c`, `include/safeapi/safechannel/sapi_safechannel.h`
- `src/appmanager/sapi_appmanager.c`, `include/safeapi/appmanager/sapi_appmanager.h`
- `CMakeLists.txt` (top-level), `tests/CMakeLists.txt`
- `docs/architecture/ADR-024-configurable-feature-build.md` (dependency
  graph, §2.1, updated)
- `docs/architecture/ADR-023-consolidate-cmake-libraries.md` (§5,
  pointer to this ADR added noting the old `channel/` module's removal)
- `safeAPIRBC2oo2` (separate repo): `src/application/AB/channel_ab_types.h`,
  `channel_ab.c`, `channel_ab_io.c`, `channel_ab_checkpoint.h`/`.c` -
  the downstream consumer this ADR's exploration missed (§3)

# MISRA C:2012 Compliance Report

Date: 2026-08-27 (see "update 21" note below)

**2026-08-27, update 21 (ADR-035: new `sapi_platform` OAL service - one-shot
real-time platform bring-up moved out of a direct `safeAPIBackendPosix`
call in `safeAPIRBC2oo2GP` startup):** no automated `cppcheck` run - the
tool is not installed in the environment this change was made in (same
"state plainly when no tool was available" posture as updates 4/5/13).
Manual MISRA C:2012 review of the change:

- **`src/oal/platform/sapi_platform.c`** (new, in `safeapi_oal`): a
  validate-then-dispatch service structurally identical to
  `src/oal/reboot/sapi_reboot.c` (its template) - fixed-width types
  (`uint32_t`), one named constant (`SAPI_PLATFORM_RT_PRIORITY_MAX 99U`),
  no dynamic memory, no recursion, single-level pointer use, `const`
  backend pointer. The early-return validation style (Rule 15.5,
  Advisory) is the established, already-deviated pattern for every OAL
  `*_register_backend()` / dispatch function in this project - not a new
  deviation. One function-pointer call through the registered vtable,
  same as `sapi_reboot`/`sapi_timer`.
- **`include/safeapi/oal/platform/sapi_platform.h` +
  `include/safeapi_backend/platform/sapi_platform_backend.h`** (new):
  consumer/backend header split per ADR-021, Doxygen `@file`/`@brief`,
  full `@param`/`@return`, include guards, `extern "C"` wrappers - matches
  the reboot pair verbatim in shape.
- **`tests/platform/test_sapi_platform.c`** (new): `<assert.h>`-based, in
  the already-out-of-scope `tests/` tree (section 4).
- No change to any existing `safeapi_core`/`safeapi_oal`/`safeapi_channels`
  translation unit; the static finding total is unchanged from update
  20's **1229** (the new `.c` was not run through the tool, so it
  contributes 0 counted findings - flagged here rather than silently
  folded in).
- Backend side (`safeAPIBackendPosix`, integrator code, not this
  framework's SIL scope): the `mlockall`/`SCHED_FIFO` body was **moved**
  from `sapi_posix_backend.c` to a new `sapi_posix_backend_platform.c`
  behind the vtable, with one behavioural fix (gate `MCL_FUTURE` on
  `getrlimit(RLIMIT_MEMLOCK)`). Its POSIX-API use (`errno`, `strerror`,
  `snprintf`) is pre-existing and unchanged in kind, consistent with the
  rest of that project's backend files.

**2026-08-26, update 20 (dynamic analysis tooling added to the build:
`SAFEAPI_ENABLE_ASAN`/`SAFEAPI_ENABLE_UBSAN` CMake options + a Valgrind
`ctest -T memcheck` target - see `.claude/skills/run-safeAPIFreamwork/SKILL.md`'s
new "Sanitizers (ASan/UBSan) and Valgrind" section):** this update is
about *dynamic* analysis (real execution under instrumentation), not
`cppcheck`'s static MISRA pass - the total static finding count above
still moved (1228 -> 1229, +1) purely from the one-line `memset()` fix
below, not from anything sanitizer-related. Full ctest suite run under
all three tools:

- **AddressSanitizer + LeakSanitizer** (`-DSAFEAPI_ENABLE_ASAN=ON`):
  29/29 clean *after* two fixes, both real, both pre-existing (not
  introduced by this session's other work):
  - `tests/voter/test_sapi_voter.c`: `g_mock[8]` was one element too
    small - `test_register_channel()` legitimately needs 9 mock channel
    backends (`channels[9]`, indices 0-8) to exercise the
    `SAPI_STATUS_RESOURCE_EXHAUSTED` path one past `SAPI_VOTER_MAX_CHANNELS`
    (8). `reset_mocks(9)` was writing `g_mock[8]` out of bounds into
    whatever global happened to follow it in memory
    (`g_handler_calls`) - a real global-buffer-overflow, silent under a
    plain build, caught immediately by ASan's global redzones. Fixed:
    `g_mock[9]`.
  - `tests/checkpoint/test_sapi_checkpoint.c`:
    `test_correct_sequence_wrong_payload_does_not_count()` deterministically
    crashes on its own epilogue under ASan (confirmed via `lldb`: `lr`
    correctly points back into this same function's `setjmp()` call
    site - the expected post-`longjmp()` state, not a corrupted return
    address elsewhere) - a documented AddressSanitizer/`setjmp`+`longjmp`
    limitation (ASan's per-function stack-redzone epilogue bookkeeping
    doesn't replay correctly across a `longjmp()`-restored frame), not a
    memory-safety defect in `sapi_checkpoint.c` itself: production code
    never calls `setjmp`/`longjmp` (REQ-COMMON-SAFESTATE-002's real
    handler never returns at all - this is test-only diversion tooling).
    The other three `setjmp`/`longjmp` tests in the same file, same
    pattern, don't trigger it. Fixed with a scoped
    `__attribute__((no_sanitize("address")))` on just that one function,
    with the full reasoning recorded in the file's own header comment
    for anyone who hits it again.
- **UndefinedBehaviorSanitizer** (`-DSAFEAPI_ENABLE_UBSAN=ON`,
  `-fno-sanitize-recover=undefined` so a detected UB aborts the offending
  test rather than reporting and continuing): 29/29 clean, no fix
  needed.
- **Valgrind memcheck** (`ctest -T memcheck`, verified in a Linux
  container - Valgrind does not support macOS/arm64, the primary dev
  platform here, but Linux is this project's actual deployment target
  per every `dist/<platform>` Docker/toolchain path elsewhere in this
  workspace): found one real, genuine "Use of uninitialised value of
  size 8" inside `sapi_checksum_crc64()`, reached from
  `sapi_dual_channel_send_heartbeat()`. Root cause:
  `sapi_dual_heartbeat_frame_t` has a compiler-inserted 4-byte alignment
  gap between its 4-byte `header` and its 8-byte-aligned `timestamp_ms`
  - unlike every other frame type in `sapi_dual_frames.h`, whose fields
  happen to sum to an already-8-aligned offset before their own trailing
  `uint64_t`. The function set every *named* field but never the struct
  as a whole, so that 4-byte gap stayed indeterminate stack content -
  and `sapi_dual_msgchannel_send()` hashes/transmits `sizeof(frame)` raw
  bytes, not just the named fields, so the indeterminate gap was read by
  `sapi_checksum_crc64()` and folded into a live, on-wire checksum. A
  real "no uninitialized variables" violation (CLAUDE.md), not merely a
  Valgrind nag - fixed with a `memset(&frame, 0, sizeof(frame))` before
  the field assignments in `sapi_dual_channel_send_heartbeat()`
  (`src/redundancy/dual/sapi_dual_channel.c`). Re-ran the full
  `ctest -T memcheck` suite after the fix: 0 defects across all 29
  tests, confirmed in the same Linux container. Along with the ASan fix
  above (both are runtime findings, not static ones), this changes the
  static `cppcheck` total below by nothing beyond the memset line
  itself - see the "Total MISRA findings" delta immediately below.
- Every fix above is a test-file or production-`.c` change already
  covered by this repo's own existing `ctest` entries (no new test was
  needed to exercise `sapi_dual_channel_send_heartbeat()` - the existing
  `test_send_heartbeat` already called it; Valgrind is what made the
  latent bug visible, not new test coverage).
- Total MISRA findings: **1229** (up from update 19's 1228, +1) - the
  single `memset()` call added to
  `src/redundancy/dual/sapi_dual_channel.c` above. Manual review: this
  is a defensive, whole-struct zero-init immediately followed by the
  same explicit per-field assignments the file already used everywhere
  else - no deviation, no new finding type.
- Verified separately (not `cppcheck`): full `cmake --build build/native`
  and `ctest --test-dir build/native` on macOS - **29/29**, unchanged, and
  a full downstream rebuild of `safeAPIRBC2oo2GP` (pulls in
  `safeAPIBackendPosix`/`safeAPIRBC2oo2GA`/`safeAPIRBC2oo2SA`
  transitively) confirming no regression from the `sapi_dual_channel.c`
  fix.

**2026-08-26, update 19 (channel-by-name lookup, plus the new opt-in
safety-violation notification handler - see `docs/requirements/SRS.md`
sections 1.5.1/3d.1/3d.2):** automated checker run performed (`cppcheck`,
`cmake --build build --target cppcheck`, `.cppcheck-suppressions`
unchanged):

- Total MISRA findings: **1228** (`build/native/cppcheck-report.txt`) - up
  from update 18's **1211** (+17). Two source changes landed in this
  repo since update 18's snapshot, neither of which had been through
  `cppcheck` yet:
  - **Channel-by-name lookup** (`sapi_channel_config_t::name` +
    `sapi_channel_get_name()` in `sapi_channel.c`;
    `sapi_voter_get_channel_by_name()` in `sapi_voter.c`, linear
    `strcmp()` scan). Confirmed by line-level correlation against the
    report: both new functions land exactly one 15.5 (single point of
    exit, the trailing `return NULL;`/`return handle->config.name;`) and
    one 17.7 apiece - the same two-finding-per-function shape every
    other function in these two files already carries (visually
    confirmed against the surrounding functions' own finding lines) -
    not a new finding *type* introduced by this code.
  - **`sapi_safety_violation` module** (new
    `include/safeapi/utils/safestate/sapi_safety_violation.h` +
    `src/utils/safestate/sapi_safety_violation.c` - single fixed handler
    slot, REQ-COMMON-SAFETYVIOLATION-001/002) plus instrumenting the
    three existing primitive families with
    `sapi_safety_violation_report()` calls at their pre-existing failure
    branches: `sapi_safe_ptr_get()`/`_offset()` (`sapi_safe_ptr.c`, 3
    call sites) and all six `sapi_cast_checked_*` functions plus
    `sapi_cast_bounds_check()` (`sapi_cast.c`, 7 call sites). The new
    module itself contributes exactly 2 findings (15.5, on its own two
    early-return guard clauses); the instrumentation calls add no new
    findings of their own, since each call sits *before* a `return`
    statement that already existed (and was already counted) prior to
    this change - the branch structure of `sapi_cast.c`/`sapi_safe_ptr.c`
    is unchanged, only a function call was inserted into already-existing
    branches.
- Manual review: both changes match this codebase's already-documented
  15.5 deviation (consistent guard-clause/single-early-return style, not
  new); the new module has no dynamic allocation, fixed-width types
  throughout, a single static handler slot, and full Doxygen with
  `REQ-COMMON-SAFETYVIOLATION-001/002` citations. No deviation notes
  needed.
- Verified separately from `cppcheck` (build-system/test-count concerns,
  not MISRA-rule content): full rebuild + `ctest` - **29/29** (up from
  update 18's 28 - one new binary, `test_sapi_safety_violation`, covering
  no-handler-is-a-no-op (REQ-COMMON-SAFETYVIOLATION-002), NULL-handler
  rejection, correct dispatch of kind/file/line/message, and
  re-registration replacing rather than stacking). Downstream repos
  (`safeAPIBackendPosix`/GP/GA/SA) and Docker were not rebuilt for this
  specific update - the safety-violation module's default (no handler
  registered) behavior is provably unchanged from the three primitives'
  existing return-code contracts, so no downstream consumer is affected
  until one opts in by calling `sapi_safety_violation_register_handler()`,
  which nothing yet does.

**2026-08-26, update 18 (module reorg into `common`/`utils`/`oal`/`redundancy`/`app`
tiers, plus three new MISRA-safety primitives - see `docs/requirements/SRS.md`
sections 1.4/2.3.1):** automated checker run performed (`cppcheck`,
`cmake --build build --target cppcheck`, `.cppcheck-suppressions` unchanged):

- Total MISRA findings: **1211** (`build/cppcheck-report.txt`) - up from update
  17's **1183** (+28). This delta is fully attributable to the three new
  primitives' new code (`sapi_safe_ptr.h`/`.c`, and the six new
  `sapi_cast_checked_*`/`sapi_cast_bounds_check` functions added to
  `sapi_cast.c`) - the module reorg itself moved existing files into deeper
  directories without changing a single line of their content, so it
  contributed zero findings on its own (directory depth is not
  MISRA-relevant).
- Manual review of the new code: no dynamic allocation
  (`sapi_safe_ptr_t` wraps caller-owned memory, never allocates); fixed-width
  types throughout; explicit, checked overflow detection in every
  `sapi_cast_checked_*` function (widen-then-compare for the `u32` variants,
  `SIZE_MAX`-relative checks for the `size_t` variants, a zero-operand guard
  before the multiplication overflow check's own division); single point of
  exit preserved; full Doxygen blocks with `REQ-COMMON-CAST-004/005` and
  `REQ-OAL-SAFEPTR-001/002/003` citations. No deviation notes needed.
- The reorg's own mechanical correctness (every `#include`/build-system path
  updated, nothing left pointing at a pre-move location) was verified
  separately, not by `cppcheck`: a full rebuild + `ctest` (28/28, up from 27 -
  the two new test binaries) of this repo, then of every downstream consumer
  (`safeAPIBackendPosix`, `safeAPIRBC2oo2SA`, `safeAPIRBC2oo2GA`,
  `safeAPIRBC2oo2GP`, confirming `safeAPIRBC2oo2GA`'s `.so` still has zero
  unresolved symbols referencing GP), then a live Docker redeploy of the full
  11-container stack (all 6 RBC roles reaching real cycle-390 cross-compare
  AGREE/checkpoint traffic, zero reboots over a 3-minute observation window)
  - a build-system/path change like this has no MISRA-rule content of its
    own to check; the real regression risk was "does it still link and run,"
    not "does it still comply."

**2026-08-26, update 17 (ADR-034: checkpoint-signature marks, stage
reorder, stage-then-commit output - see `docs/requirements/SRS.md`
section 3c and this file's own root-cause context in the session that
wrote it):** automated checker run performed (`cppcheck` available this
session, `cmake --build build --target cppcheck`,
`.cppcheck-suppressions` unchanged):

- Total MISRA findings: **1183** (`build/cppcheck-report.txt`) - up from
  update 16's **1108** (+75). `src/appmanager/sapi_appmanager.c` alone now
  accounts for 70 findings in this run. This session does not have a
  precise pre-change, per-file baseline for that file (only the prior
  session's repo-wide total, 1108) to separate "genuinely new violation
  from this session's code" from "a pre-existing violation on unchanged
  code that simply moved line numbers" (this file grew by ~150 lines:
  the mark API, the signature reset/fold helpers, the reordered cycle
  loop, and the `on_checkpoint_result` stage). Spot-checked several: the
  finding at the `while (!g_shutdown_requested && ...)` loop condition
  (misra-c2012-10.4/12.1) is on unchanged, pre-existing code that shifted
  down from an earlier line - not new. The dominant rule IDs in this
  file (17.7 unused return value, 21.6 stdio.h, 10.4/12.1 arithmetic
  type/precedence, 15.5 multiple return points) are the same widespread,
  already-accepted style-level patterns present throughout this codebase
  before this session (e.g. `src/cast/sapi_cast.c` alone carries 150 of
  the repo's 1183 findings, `src/string/sapi_string.c` 68 - neither
  touched this session) - not a new category this change introduced.
- Manual review of the actual new code (not just the checker's
  style-level output) for the conventions this project cares about most:
  no dynamic allocation (the checkpoint-mark encode buffer
  (`fold_buf[16]`), the mark text buffer (`text[128]`), and
  `safeAPIRBC2oo2GP`'s new staged-send queue
  (`ab_gp_staged_send_t[AB_GP_MAX_STAGED_SENDS_PER_CYCLE]`) are all
  fixed-size, sized generously, checked with `SAPI_STATUS_RESOURCE_EXHAUSTED`
  rather than silently overflowing); fixed-width types throughout
  (`uint64_t` signature, `uint32_t` folded `checkpoint_id`); explicit,
  checked casts for the 64-to-32-bit signature fold
  (`sapi_appmanager_checkpoint_fold_signature()` - an XOR of the two
  32-bit halves, not an implicit truncating assignment, per this
  project's own CLAUDE.md rule); single point of exit preserved in the
  new/changed functions; full Doxygen blocks with ADR-034 citations on
  every new public function/type/macro in `sapi_appmanager.h`/
  `ga_interface.h`/`ab_gp_channel_stage.h`. No deviation notes needed for
  the new code itself.
- `safeAPIRBC2oo2GP`'s/`safeAPIRBC2oo2GA`'s own changes (the stage-then-
  commit queue, `SAPI_CHECKPOINT_MARK()` call sites, the `ga_interface.h`
  `checkpoint_mark` field) are out of this repo's `cppcheck` scan scope
  per ADR-018's own established convention (3a's/15's own entries) -
  manual review only, same conventions confirmed above.

**2026-08-18, update 16 (ADR-029: RBC Train/IL/CTC scenario - real
multi-train protocol, session-table cross-compare/failover-transfer
widening; see `docs/requirements/SRS.md` section 3h and
`docs/architecture/ADR-029-rbc-train-il-ctc-scenario.md`):** automated
checker run performed (`cppcheck` was available this session,
`cmake --build build --target cppcheck`, `.cppcheck-suppressions`
unchanged):

- Total MISRA findings: **1108** (`build/cppcheck-report.txt`) -
  UNCHANGED from update 15's own count. This ADR's entire change set
  lives in `safeAPIRBC2oo2` (new files `rbc_wire.c`/`.h`/
  `rbc_wire_types.h`; extensive changes to `src/application/C/*` and
  `src/application/AB/*`; no `safeAPIFreamwork` source file touched at
  all) - per ADR-018's own established scope, `safeAPIFreamwork`'s
  `cppcheck` target only scans this repo's own `include/`/`src/`, so a
  purely-`safeAPIRBC2oo2`-side change is expected to leave this repo's
  own finding count exactly unchanged, and it did.
- Manual review only for the `safeAPIRBC2oo2`-side files, same posture
  as update 15's own RBC-adjacent (ADR-028) entry: all new/changed code
  follows this codebase's already-established conventions - no dynamic
  allocation (the train-session table and every new queue are fixed-size
  arrays sized by `SAFEAPI_EXAMPLE_MAX_TRAINS`/`RBC_ENVELOPE_QUEUE_CAPACITY`,
  never grown), fixed-width types throughout the new `rbc_envelope_t`/
  `train_session_t` structs, explicit bounds-checking on every
  wire-supplied `train_id` before it indexes an array
  (`channel_ab_train_id_to_index()`/`train_id_to_index()`, REQ-RBC-005),
  single point of exit in each new function, full Doxygen blocks on
  every new public function/type with REQ-ID citations
  (`rbc_wire.h`/`rbc_wire_types.h`). No deviation notes needed.
- One new MISRA-relevant pattern worth flagging explicitly (not a
  violation, a design note): `channel_ab_crosscompare.c`'s cross-compare
  payload is now built via manual field-by-field comparison
  (`xcompare_recv_local()`/`_peer()` and the new per-train sync-skip
  gate) rather than a single primitive-type `memcpy()`, specifically
  *because* `train_session_t` mixes `bool`/`uint8_t`/`uint32_t` members
  and a raw struct `memcmp()` would risk comparing compiler-inserted
  padding bytes alongside the real fields - resolved by encoding to a
  packed wire form first (`channel_ab_wire_encode_sessions()`) rather
  than comparing the struct directly. Documented in REQ-RBC-007 and this
  file's own reasoning is preserved there rather than repeated per-call-
  site.
- **Addendum, same date**: full 11-container Docker testing (beyond the
  6-process local run this update's own entry above was written against)
  found two further live bugs, both root-caused with `gdb` and fixed by
  constant retunes only - no new code paths, no new MISRA-relevant
  pattern. See ADR-029 section 2.6 for the full story:
  `SAFEAPI_EXAMPLE_CHECKPOINT_REENABLE_GRACE_MS` (600ms -> 15000ms,
  `common_config.h`) and `RBC_ENVELOPE_QUEUE_CAPACITY` (8 -> 16,
  `rbc_wire_types.h`). Both remain fixed-width (`uint32_t`/`#define`
  unsigned-suffixed literals), no dynamic allocation introduced, no
  control-flow change - manual review only, no re-run of `cppcheck`
  needed (a `safeAPIRBC2oo2`-side numeric constant change, same
  out-of-scope reasoning as this update's main entry above).

**2026-08-18, update 15 (ADR-028: cyclic-executive checkpoint-starvation
fix - REQ-APPMANAGER-011 - and its `safeAPIRBC2oo2` channel-down-reboot/
HOT-COLD-standby consumer; see `docs/requirements/SRS.md` and
`docs/architecture/ADR-028-channel-down-reboot-and-executive-starvation-fix.md`):**
automated checker run performed (`cppcheck` was available this session,
`cmake --build build --target cppcheck`, `.cppcheck-suppressions`
unchanged):

- Total MISRA findings: **1108** (`build/cppcheck-report.txt`), up from
  1107 at update 14's era - a one-finding increase from tightening
  `sapi_appmanager.c`'s checkpoint-stage gating condition
  (`config->checkpoint->voter != NULL`, REQ-APPMANAGER-011); no new
  files, no new rule category. `sapi_appmanager.c` itself carries 84
  MISRA findings post-change, same already-triaged-or-accepted buckets
  as update 13/14 (15.5 single-exit, 12.1/10.4 mixed-type loop
  conditions) - this change added a boolean-AND term to an existing
  `if`, not a new loop or a new pattern.
  `tests/appmanager/test_sapi_appmanager.c`'s two rewritten test cases
  (`test_checkpoint_paused_skips_stage_not_starves_cycle`,
  `test_checkpoint_null_vital_channel_is_not_a_startup_error`) are test
  code, out of this report's production-code scope (same posture as
  every other `tests/` file - see section 1a's scope note).
- The rest of this round's changes - `channel_ab.c`/`channel_ab_io.c`/
  `channel_ab_negotiate.c`/`channel_ab_types.h` (peer/C-link
  channel-down-reboot, HOT/COLD standby log surfacing, negotiation
  watchdog timing fix), `monitor_c.c`/`monitor_c_io.c`/
  `monitor_c_types.h` (C-side channel-down-reboot), and
  `common_config.h` (new timing constants/reason codes) - all live in
  `safeAPIRBC2oo2`, which per ADR-018's own scope has no formal MISRA
  report or automated checker wired up (`safeAPIFreamwork`'s
  `cppcheck` target only scans this repo's own `include/`/`src/`).
  Manual review only for those files: all follow the same conventions
  already established elsewhere in `safeAPIRBC2oo2` - no dynamic
  allocation, fixed-width types (`sapi_timestamp_ms_t` for every new
  down-since field), explicit `NULL`-pointer checks before every new
  dereference, single point of exit in each new function
  (`channel_ab_check_channel_down_reboot()`,
  `monitor_c_check_channel_down_reboot()`,
  `channel_ab_io_mark_link_down()`, `monitor_c_io_mark_link_down()`).
  No deviation notes needed - nothing in this batch required
  bypassing a convention the rest of the codebase already follows.
- No re-triage of section 1a/section 5's still-open un-triaged rule list
  performed this pass either - same deferred item as updates 13/14.

**2026-08-18, update 14 (ADR-026: application setup-phase lock and
single-entry-point enforcement, new `sapi_lifecycle` module; plus the
`sapi_dual_channel.c` hard-fault-propagation fix - REQ-DUAL-CHANNEL-008 -
and its test coverage; see `docs/requirements/SRS.md`):** automated
checker run performed (`cppcheck` was available this session,
`cmake --build build --target cppcheck`, `.cppcheck-suppressions`
unchanged):

- Total MISRA findings: **1107** (`build/cppcheck-report.txt`), up from
  1083 at update 13's era.
- The new module contributes almost nothing to that increase:
  `src/lifecycle/sapi_lifecycle.c` has exactly **one** MISRA finding
  (rule 8.6 - an external identifier without a single external
  definition site cppcheck can resolve; not yet triaged real-vs-deviation,
  same "not yet triaged" status as every other rule in section 1a/5's
  list below), and `include/safeapi/lifecycle/sapi_lifecycle.h` has
  **zero** - only an informational (non-MISRA) `missingIncludeSystem`
  note from `<assert.h>` not being resolvable in this sandbox, which
  cppcheck's own message text explains is expected and harmless
  ("Standard library headers do not need to be provided to get proper
  results").
- The remaining ~23-finding increase is spread across this session's
  other in-flight, not-yet-individually-MISRA-reviewed changes bundled
  into this same update: `sapi_appmanager.c`'s setup-phase lock/
  reentrancy-guard additions (ADR-026 §2.1/2.3) and its new
  `sapi_appmanager_reset_state()`; the seven newly-gated constructors
  (`sapi_timer_create()`, `sapi_channel_init()`, `sapi_voter_init()`,
  `sapi_voter_register_channel()`, `sapi_cross_comparator_init()`,
  `sapi_cross_comparator_register_channel()`, `sapi_watchdog_create()`);
  `sapi_dual_channel.c`'s hard-fault-propagation fix; and the new/
  extended test files for all of the above
  (`tests/lifecycle/test_sapi_lifecycle.c` new;
  `tests/appmanager/test_sapi_appmanager.c`,
  `tests/dual/test_sapi_dual_channel.c` extended). Per-file attribution
  of that portion was not performed this pass - same posture update 13
  already took ("a spot check... without confirming that check used the
  exact same methodology... left as-is rather than guess"); none of
  these files introduce a new rule *category* not already present
  elsewhere in this codebase (same validate-then-dispatch/early-return
  style throughout).
- No re-triage of section 1a/section 5's still-open un-triaged rule list
  performed this pass either - same deferred item as update 13.

**2026-08-17, update 13 (ADR-025: `sapi_vital_channel` split into
`sapi_channel`/`sapi_voter`/`sapi_cross_comparator`, retirement of the
dead ADR-008 `channel/` module, and a `sapi_appmanager.c` busy-loop fix
- REQ-APPMANAGER-008, see `docs/requirements/SRS.md`):** automated
checker run performed (`cppcheck` was available this session,
`cmake --build build --target cppcheck`, `.cppcheck-suppressions`
unchanged):

- Total MISRA findings: **1083** (`build/cppcheck-report.txt`), up from
  1052 at update 12's era. The increase is from three new production
  modules (`src/voter/`, `src/cross_comparator/`, and the renamed/
  redesigned `src/channel_link/`) plus their new test files, and the
  small `sapi_appmanager_pace_failed_checkpoint()` addition to
  `sapi_appmanager.c` (REQ-APPMANAGER-008) - not a regression in any
  pre-existing file. The old, already-dead-and-excluded-from-every-build
  ADR-008 `src/channel/` module was removed outright as part of this
  same change (see ADR-025 §2.7), which would otherwise have partially
  offset the new-module increase, but that module was never scanned by
  `compile_commands.json` in the first place (excluded from the build),
  so its removal changed nothing in this count either way.
- No new rule *categories* introduced: the new modules follow the same
  established patterns (validate-then-dispatch, early-return-on-invalid-
  param) as every other OAL/channels-layer module in this codebase, so
  their findings land in the same already-documented, already-triaged-or-
  accepted rule buckets as the rest of the tree (15.5 single-exit,
  17.7 ignored return values on defensive `(void)`-uncast calls, etc. -
  see section 3/5 below), not a new kind of finding this report hasn't
  already characterized.
- Rules 12.1 and 10.4 (both already tracked in section 1a's "not yet
  triaged" list below) gained a handful of hits each from
  `sapi_appmanager_pace_failed_checkpoint()`'s own bounded-poll loop -
  same already-accepted mixed-`&&`/arithmetic-type style already used
  throughout this file's pre-existing loop conditions (e.g. the main
  `while` loop's own `!g_shutdown_requested && (...)` a few lines away,
  unchanged by this update), not a new style introduced by this function.
- No re-triage of section 1a/section 5's still-open un-triaged rule list
  (8.7, 17.7, 12.1, 10.4, 11.5, 5.9, 20.9, 10.8, 8.9, 21.16, 10.2, 8.4)
  was performed as part of this update either - same still-open item
  update 12 already deferred. The per-rule counts in that list's own
  table row below are from update 12's run and were NOT re-verified this
  pass; only the report-wide total above was re-measured. A spot check
  during this update found materially different per-rule counts under a
  quick manual `grep`, but without confirming that check used the exact
  same methodology (dedup, file scope) as whatever originally produced
  the table's numbers, publishing a mismatched replacement risked making
  this compliance document *less* trustworthy, not more - left as-is
  rather than guess.

**2026-08-15, update 12 (ADR-024 configurable feature build, a real
`-Wcast-qual` fix, the checksum module's `REQ-ID` cleanup, and a large
test-coverage push - see the project's own task history for the full
change list; MISRA-relevant subset only, below):** automated checker run
performed (`cppcheck` was available this session -
`cmake --build build --target cppcheck`, `.cppcheck-suppressions`
unchanged):

- Total MISRA findings: **1052** (`build/cppcheck-report.txt`), up from
  865 at the last real run (update 10's era). The increase is from
  `--project=compile_commands.json` now also scanning many new/expanded
  `tests/<module>/test_sapi_<module>.c` files this session's coverage
  work added (test code isn't held to the same production-path MISRA
  bar, but cppcheck scans it identically since it appears in
  `compile_commands.json`) - not a regression in `src/`. Top rules
  unchanged in kind from prior runs (15.5 single-exit still dominates at
  481 hits, matching the long-standing documented deviation in section 3).
- Rule 21.6 (`<stdio.h>`): now confirmed to appear in exactly
  `src/appmanager/sapi_appmanager.c` (still open, as section 5 already
  said) and `tests/log/test_sapi_log.c` (test-only, not a production
  finding) - no other file. Fixes the report's own prior
  self-contradiction where section 5 additionally still named
  `sapi_watchdog.c` here despite update 3 already saying it was fixed;
  corrected in place (see that note).
- The real `-Wcast-qual` fix this update made
  (`sapi_voter_get_aggregated_health()`'s first parameter widened
  to `const sapi_channel_t *`, `src/channel_link/sapi_channel.c`/
  `include/safeapi/channel_link/sapi_channel.h`) removes a compiler
  warning, not a cppcheck/MISRA finding - it doesn't change any count
  above, but is worth noting here since it was found via the same
  strict-warnings build this report's own tooling depends on.
- No re-triage of section 1a/section 5's still-open un-triaged rule list
  (8.7, 17.7, 12.1, 10.4, 11.5, 5.9, 20.9, 10.8, 8.9, 21.16, 10.2, 8.4) was
  performed as part of this update - out of scope for this pass, still an
  open item.

**2026-08-07, update 11 (build-system reorganization only, no code
change - ADR-023 consolidates 22 CMake library targets into 4):** no
automated re-run performed (same caveat as prior updates - still no
`cppcheck` in this sandbox session, and no `cmake` binary either for this
specific change, see below); reasoned manually since this change touches
zero `.c`/`.h` files:

- The 21 per-feature static libraries (`safeapi_status` through
  `safeapi_safechannel`) are now compiled into 3 grouped libraries -
  `safeapi_core`, `safeapi_oal`, `safeapi_channels` - plus
  `safeapi_appmanager` unchanged as a 4th. Every `.c`/`.h` file stays at
  its exact ADR-007 path; only which `.a` its object code lands in
  changed. No new casts, no new control flow, no new dynamic behavior -
  every existing MISRA finding tied to a specific source file is
  unaffected by which library that file compiles into.
- 20 per-feature `src/<feature>/CMakeLists.txt` files were deleted; their
  `add_library()` calls now live directly in the top-level
  `CMakeLists.txt`. `src/appmanager/CMakeLists.txt` and the deliberately
  unbuilt `src/channel/CMakeLists.txt` (ADR-008, unaffected) are
  untouched as files, appmanager's link line updated to the 3 new names.
- Every consumer of the old per-feature target names was updated:
  `tests/CMakeLists.txt` (19 tests), the top-level `install(TARGETS ...)`
  list, safeAPIRBC2oo2's `src/posix_backend/CMakeLists.txt` and top-level
  `CMakeLists.txt`, and the `examples/qnx-rtos-app`/`examples/linux-posix-app`
  integration templates plus `examples/build-qnx.sh`'s doc string.
  Verified via a repository-wide grep for every old `safeapi::<feature>`
  name in both repos after the change - the only remaining hits are in
  `src/channel/CMakeLists.txt`, which was already excluded from the build
  before this change and stays that way.
- Verified via manual `gcc -std=c99 -Wall -Wextra -Wpedantic` rebuild of
  all 19 framework unit tests (all pass) plus a full safeAPIRBC2oo2
  rebuild and live two-process SITE WEST/EAST smoke run, compiling
  exactly the source-file groupings the new `CMakeLists.txt` targets
  specify. **Caveat specific to this update**: because the change is to
  the CMake build files themselves, this source-level verification
  cannot by itself prove the new `add_library()`/`target_link_libraries()`
  CMake syntax is free of configuration-time errors - no `cmake` binary
  (and no network access to install one) was available in this sandbox
  session. An actual `cmake --build` on a real toolchain remains the
  outstanding verification step for this specific change.
- ADR-023 status: framework, safeAPIRBC2oo2, and example templates all
  updated; see ADR-023 for the full rationale and dependency-cluster
  reasoning.

**2026-08-07, update 10 (new module `sapi_safechannel` (ADR-022), a
real bug fix in `sapi_dual_channel_send()` (ADR-020) it surfaced, and
SITE's migration off direct `sapi_netlink` use):** no automated re-run
performed (same caveat as prior updates - still no `cppcheck` in this
sandbox session); reasoned manually plus full test-suite + live-run
verification:

- `sapi_safechannel` (`include/safeapi/safechannel/sapi_safechannel.h`,
  `src/safechannel/sapi_safechannel.c`): new module, same conventions as
  every other feature - no dynamic allocation (fixed
  `SAPI_SAFECHANNEL_MAX_LINKS`-sized arrays), explicit casts at every
  narrowing point (e.g. `sapi_safechannel_send()`'s `payload_size >
  UINT8_MAX` guard before the `(uint8_t)` cast for the DUAL_REDUNDANT
  path), single point of exit is not used throughout (early-return-on-
  invalid-param is this codebase's established idiom, consistent with
  every other OAL service), no recursion, `const`-correct where the
  wrapped `sapi_dual_channel`/`sapi_channel` APIs allow it (one
  explicit, commented `const`-cast in `sapi_safechannel_get_status()`
  because `sapi_voter_get_aggregated_health()` itself takes a
  non-const handle for a read-only query - a pre-existing constraint of
  the wrapped API, not introduced here).
- Real bug found and fixed in `sapi_dual_channel_send()`
  (`src/dual/sapi_dual_channel.c`, unchanged since ADR-020): its
  ACK-wait loop's "no measurable elapsed time" check aborted the wait
  after exactly one poll instead of allowing further polls within the
  same millisecond tick - see ADR-020's "Post-acceptance fix" section
  for the full explanation and the bounded-retry-count fix
  (`SAPI_DUAL_CHANNEL_STALL_POLL_LIMIT`, a fixed cap of 32 - no dynamic
  behavior, no new casts). This module had only ever been exercised
  through a mock netlink backend (`test_sapi_dual_channel.c`) before
  this pass wired it to a real transport for the first time.
- `safeAPIRBC2oo2/src/application/SITE/site.c`: removed its own direct
  `sapi_netlink_open()`/`_send()`/`_receive()`/`_close()` calls and the
  manual CONNECT-retry loop it hand-rolled around them; now opens one
  `sapi_safechannel_t` (`SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT`,
  `link_count = 1`) and calls `sapi_safechannel_send()`/`_receive()`/
  `_close()` instead. No change to `encode_beacon()`/`decode_beacon()`
  or the negotiation/promotion/demotion/re-negotiation decision logic -
  only the transport calls moved.
- Verified via manual `gcc -std=c99 -Wall -Wextra -Wpedantic` rebuild of
  all 17 framework unit tests (all pass, including
  `test_sapi_dual_channel`, `test_sapi_dual_msgchannel`,
  `test_sapi_dual_negotiator`, and the new `test_sapi_safechannel`) plus
  a live two-process SITE WEST/EAST run over the real POSIX TCP netlink
  backend: negotiation completes and steady-state heartbeats continue
  exchanging role/single-mode status every cycle - see ADR-022 section 4.
- ADR-022 status: framework module and SITE's migration done;
  `monitor_c` and `channel_ab` migrations remain pending.

**2026-08-06, update 9 (header restructuring only, no behavior change -
ADR-021 consumer/OS-backend header split completed for the remaining 8
modules: `nvm`, `memory`, `task`, `ipc`, `log`, `reboot`, `netlink`,
`clocksync`):** no automated re-run performed (same caveat as prior
updates - still no `cppcheck` in this sandbox session); reasoned manually
since this change is a pure declaration move, not new logic, identical in
nature to update 8's `sapi_timer` pilot:

- Same pattern as the pilot: each module's `sapi_<feature>_backend_t` and
  `sapi_<feature>_register_backend()` *declarations* moved from
  `include/safeapi/<feature>/sapi_<feature>.h` to the new
  `include/safeapi_backend/<feature>/sapi_<feature>_backend.h`; each
  service's `.c` implementation (`src/<feature>/sapi_<feature>.c`) is
  byte-for-byte unchanged apart from the added `#include`. No new casts,
  no new control flow.
- `sapi_clocksync.h` was the one header where backend material was
  interleaved with consumer functions (vtable/register between the
  quality enum and the two consumer accessor functions) rather than
  trailing them as in the other 8 - the extraction was still a pure cut,
  no reordering of surrounding consumer declarations.
- Every call site that referenced a vtable type or `_register_backend()`
  gained the corresponding new `#include`: framework tests
  (`test_sapi_nvm`, `test_sapi_reboot`, `test_sapi_netlink`,
  `test_sapi_clocksync`, `test_sapi_log`, `test_sapi_dual_msgchannel`,
  `test_sapi_dual_channel`, `test_sapi_dual_negotiator`), the
  `examples/geo_distributed_checkpoint_sync.c` sample, and
  safeAPIRBC2oo2's umbrella `sapi_posix_backend.h` (now includes all 9
  backend headers alongside their 9 consumer headers). No site needed a
  logic change; `task`, `ipc`, and `memory` have no dedicated framework
  unit tests, so only their `.c` implementation and the POSIX backend
  umbrella header needed the new include.
- Verified via manual `gcc -std=c99 -Wall -Wextra -Wpedantic` rebuild of
  all 16 framework unit tests (all pass, including the 8 directly
  touched by this change) plus a full rebuild and live 8-process run of
  safeAPIRBC2oo2 (0 errors, clean shutdown, identical AGREE/checkpoint
  behavior to before) - see ADR-021 section 2.3.
- ADR-021 status: all 9 backend-bearing OAL modules now have the
  consumer/backend header split in place.

**2026-08-06, update 8 (header restructuring only, no behavior change -
ADR-021 consumer/OS-backend header split, `sapi_timer` pilot):** no
automated re-run performed (same caveat as prior updates - still no
`cppcheck` in this sandbox session); reasoned manually since this change
is a pure declaration move, not new logic:

- `sapi_timer_backend_t` and `sapi_timer_register_backend()`'s
  *declarations* moved from `include/safeapi/timer/sapi_timer.h` to the
  new `include/safeapi_backend/timer/sapi_timer_backend.h`; their
  *implementation* in `src/timer/sapi_timer.c` is byte-for-byte
  unchanged, only its `#include` list gained the new header. No new
  casts, no new control flow, no new dynamic behavior - the existing
  Rule 8.x (declaration consistency), 17.x, and 21.x findings already
  covering `sapi_timer.c` in prior updates are unaffected.
- Every call site that referenced the vtable type
  (`tests/timer/test_sapi_timer.c`, `tests/watchdog/test_sapi_watchdog.c`,
  `tests/dual/test_sapi_dual_negotiator.c`, `tests/log/test_sapi_log.c`,
  safeAPIRBC2oo2's `sapi_posix_backend.h`/`sapi_posix_backend_timer.c`)
  gained the new `#include` and were rebuilt; no site needed a logic
  change.
- Verified via manual `gcc -std=c99 -Wall -Wextra -Wpedantic` rebuild of
  all four affected framework unit tests (all pass) plus a full rebuild
  and live 8-process run of safeAPIRBC2oo2 (0 errors, clean shutdown) -
  see ADR-021 section 2.3.
- **Not yet done:** the same split for the other eight backend-bearing
  modules (`nvm`, `memory`, `task`, `ipc`, `log`, `reboot`, `netlink`,
  `clocksync`) - ADR-021 is a pilot on `sapi_timer` only as of this
  update. This report will gain a further update per module as each is
  split.

**2026-08-06, update 7 (new module, `sapi_dual` - ADR-020 dual-transfer
state negotiation):** no automated re-run performed (same caveat as
updates 4-6 - still no `cppcheck` in this sandbox session); reasoned
manually against the four new files (`sapi_dual_types`, `sapi_dual_frames`,
`sapi_dual_msgchannel`, `sapi_dual_channel`, `sapi_dual_negotiator`):

- No dynamic allocation: every instance (`sapi_dual_msgchannel_t`,
  `sapi_dual_channel_t`, `sapi_dual_negotiator_t`) is caller-owned storage;
  `sapi_dual_channel_t`'s redundant links are a fixed
  `[SAPI_DUAL_CHANNEL_MAX_LINKS]` array (4), never a dynamically-sized one.
- No recursion, no `<stdio.h>`/`<assert.h>`/`<errno.h>`/`<setjmp.h>`,
  fixed-width types throughout, `const` on every read-only parameter -
  consistent with the rest of the codebase; `grep -rln
  "stdio.h\|assert.h\|errno.h\|setjmp.h" src/dual include/safeapi/dual`
  returns zero hits.
- The one `switch` in the new code (`sapi_dual_channel.c`'s frame-kind
  dispatch in `dual_channel_poll_link_once()`) has an explicit `default`
  clause (Rule 16.1/16.4) - an unrecognized `kind` is defensively ignored,
  not treated as an error, since it isn't reachable through a conforming
  sender.
- **Open finding, not yet fixed - narrows section 2's existing blanket
  claim.** Section 2's Rule 10.1-10.8 row states "no bare narrowing casts
  exist elsewhere in `include`/`src`"; that is no longer accurate as of
  this module. `sapi_dual_channel.c` uses bare C-style casts (not
  `sapi_cast_*`) to narrow `size_t sizeof(...)` expressions and
  `raw_size - sizeof(header)` arithmetic down to `uint8_t` at ~8 call
  sites (e.g. `(uint8_t)sizeof(raw)`, `(uint8_t)(raw_size -
  (uint8_t)sizeof(header))`, `(uint8_t)sizeof(frame)`), plus two
  `uint8_t`<->`sapi_dual_frame_kind_t` enum casts for the frame-kind
  header byte. Risk assessment: every one of these is a compile-time-fixed
  struct size (`sapi_dual_frame_header_t` = 4B, `sapi_dual_ack_frame_t` and
  `sapi_dual_state_frame_t` well under 32B, `SAPI_DUAL_MSGCHANNEL_MAX_PAYLOAD`
  = 248) or a value already bounded by an earlier `SAPI_STATUS_INVALID_PARAM`
  check (`payload_size <= SAPI_DUAL_CHANNEL_MAX_PAYLOAD` before the
  `sizeof(header) + payload_size` cast) - none can actually overflow
  `uint8_t` today, so this is assessed as low-risk, not a live defect. It
  is recorded here rather than silently claimed compliant because the
  project's own CLAUDE.md convention (checked-cast helper preferred over a
  bare C-style cast, precisely so a *future* change - e.g. a larger frame
  struct - fails loudly via `sapi_cast_size_to_u8()`'s
  `SAPI_STATUS_VALUE_OUT_OF_RANGE` instead of silently truncating) was not
  followed for this module. **Follow-up recommendation:** route each site
  through `sapi_cast_size_to_u8()`/`sapi_cast_u32_to_u8()` (already exist,
  ADR-003) with the same guard-clause-on-failure style used throughout the
  rest of this module, as a dedicated, separately-tested change rather than
  bundled into this one. `(size_t)payload_size`/`(size_t)payload_max_size`
  widening casts in `sapi_dual_msgchannel.c` are unaffected by this finding
  - they match the pre-existing widening-cast precedent already in
  `sapi_watchdog.c` (section 2), which is safe by construction (no value
  loss possible widening `uint8_t`/`int` into `size_t`) and was never part
  of the narrowing-cast claim this finding corrects.
- New tests (`tests/dual/test_sapi_dual_msgchannel.c`,
  `test_sapi_dual_channel.c`, `test_sapi_dual_negotiator.c`) cover NULL
  rejection, basic send/receive roundtrips, masquerade rejection (wrong
  `sender_id`), sequence-continuity `DATA_CORRUPTION` and recovery via
  `sapi_dual_msgchannel_reset_sequence()`, all three aggregate
  `FULL`/`DEGRADED`/`DOWN` outcomes with the status callback firing only on
  change, redundant-link independence (a broken link never counts toward
  another's ACK), inbound-frame auto-ACK, STATE-frame roundtrip, the
  older-startup-timestamp-wins tie-break, the asymmetric HOT/COLD
  determination (REQ-DUAL-NEGOTIATOR-004), and lost-peer-contact
  degradation to `UNKNOWN` with the "already-`ONLINE`-stays-`ONLINE`"
  exception - all passing (manual `gcc -std=c11 -Wall -Wextra` build +
  run, exit 0 each; no `cmake`/`cppcheck` in this sandbox session, same
  standing caveat as every other update in this report).

**2026-08-06, update 6 (structured event logging, `sapi_log_write_event()`):**
no automated re-run performed (same caveat as updates 4/5). New construct,
reasoned manually:
- `sapi_log.c`/`.h`: new `sapi_log_write_event()` and
  `sapi_log_level_to_string()`. Deliberately **not** a variadic function -
  MISRA C:2012 Rule 17.1 (required) prohibits `<stdarg.h>`; the optional
  "more fields" requirement is instead a single fixed `extra_fields`
  parameter that the caller pre-formats with `safeapi::string`'s own
  bounded helpers (same primitives this function uses internally to build
  the rest of the line) - `grep -rn "stdarg.h" include src` confirms zero
  hits, unchanged by this addition.
- Fixed-size stack buffers only (`char line_storage[SAPI_LOG_EVENT_LINE_MAX_LEN]`,
  a small numeric-formatting scratch buffer) - no dynamic allocation, same
  as every other module (Dir 4.12 in section 2).
- New dependencies for this module only: `safeapi::string` (bounded
  concatenation/formatting) and `safeapi::timer` (`sapi_timer_now()` for
  the TIMESTAMP field) - both already-reviewed leaf OAL/common modules
  (section 2); no new external header, no new banned construct introduced
  by depending on them.
- Every field-append is best-effort (`(void)`-cast `sapi_string_concat()`/
  `sapi_string_from_u32()`/`sapi_string_from_u64()` return values) -
  consistent with REQ-OAL-LOG-001's "must never affect caller control
  flow": a `SAPI_STATUS_RESOURCE_EXHAUSTED` from an oversized field is
  accepted as truncation, not propagated as an error (this function
  returns `void`, matching `sapi_log_write()`'s own existing contract).
- Verified against the existing "no `<stdio.h>`/no recursion/no
  uninitialized locals/single-point-of-exit-preferred guard-clause style"
  conventions by direct code read - no deviation from any of those.
- New tests (`tests/log/test_sapi_log.c`) cover field order/delimiters,
  `NULL` `info`/`extra_fields` handling, `TIMESTAMP` degrading to `"0"`
  with no `sapi_timer` backend registered, and the pre-existing
  `sapi_log_write()`/no-backend silent-no-op contract being unaffected -
  all passing (manual `gcc` build, exit 0; no `cmake`/`cppcheck` in this
  sandbox, same standing caveat as every other update in this report).

**2026-08-06, update 5 (ADR-019: AppManager cycle hooks + built-in
checkpoint):** no automated re-run performed (same caveat as update 4 -
still no `cppcheck` in this sandbox); reasoned manually against the
already-open findings instead of introducing new ones:
- `sapi_appmanager.c`/`.h`: adds `pre_execute`/`post_execute` (two more
  optional function-pointer members, same type/NULL-check pattern already
  used for `execute`) and a checkpoint stage that calls
  `sapi_channel_checkpoint()` (already-reviewed under ADR-017, no change
  to that module - see 2.3 of ADR-019). New `#include
  "safeapi/checkpoint/sapi_checkpoint.h"` and a new link dependency on
  `safeapi::checkpoint`; no new banned construct (no dynamic memory, no
  recursion, no new `<stdio.h>`/`errno`/`assert` use beyond what section
  1a's un-triaged `sapi_appmanager.c` `21.6` finding already covers). Both
  modules remain inside this report's pre-existing "Known gap" paragraph
  (section header above) - this update does not close that gap, it adds
  to what's inside it.
- `sapi_channel.c`: `sapi_channel_init()`'s `channel_count`
  floor relaxed from `>= 2` to `>= 1` (only reachable via
  `SAPI_VOTING_NMR` with `quorum_size == 1` - `SAPI_VOTING_2OO2`/
  `SAPI_VOTING_2OO3` floors unchanged). A parameter-validation bound
  change, not a new construct - no new type, no new header, no new
  control-flow shape; the existing `switch` on `voting_strategy` still has
  its `default` clause (Rule 16.1/16.4, section 2).
- **`safeAPIRBC2oo2/src/application/AB/channel_ab.c` (separate repo,
  already out of this report's stated scope, called out here only for
  completeness since it's the first real consumer of both changes
  above):** adds a `pthread_mutex_t` (`ctx->peer_send_mutex`) guarding
  every `sapi_netlink_send()` call on the shared peer link, once the
  background receive task also needed to send (a checkpoint
  request/reply auto-responder echo - see ADR-019 §5.2). This is a
  genuinely new construct for that project (no prior direct pthread
  primitive exposed in application code; `sapi_task`/`sapi_ipc` already
  wrap pthreads internally in `posix_backend`, but this is the first
  direct use in `src/application/`). Framework-level deviation: **N/A**
  (out of this report's scope, per its own stated boundary); flagged here
  as a heads-up for that project's own eventual MISRA pass, not resolved
  by this update.

**2026-08-06, update 4:** two small, targeted changes, no automated
re-run performed (see caveat below):
- `sapi_watchdog.c`: `SAPI_WATCHDOG_ACTION_FAILOVER` now dispatches to
  `config->custom_action(config->context)` on timeout - identical code
  path to the already-reviewed `SAPI_WATCHDOG_ACTION_CUSTOM` case (same
  function pointer type, same call site, same NULL-guard), plus a matching
  `custom_action != NULL` check added to `sapi_watchdog_create()`'s
  existing validation block for `CUSTOM`. No new construct, no new banned
  header, no new dynamic allocation - MISRA posture unchanged from what
  was already reviewed for `CUSTOM`.
- `sapi_appmanager.c`: added `#define _POSIX_C_SOURCE 200809L` before any
  header include. This is a feature-test-macro fix, not a new construct -
  `struct sigaction`/`sigaction()`/`sigemptyset()` were already present
  and already covered by section 3's Rule 21.5 deviation entry below; this
  fix is what makes that already-documented, already-reviewed code path
  actually compile under glibc's strict-C99 mode (caught by a real Linux
  CI failure - the omission had been silently masked on macOS, whose libc
  does not gate these declarations the same way). See section 3's Rule
  21.5 entry for the updated note.
- **Caveat:** neither change was re-verified with a fresh `cppcheck
  --addon=misra` run (section 1a's own counts are therefore unchanged and
  slightly stale as of this update) - both are small, mechanically
  reasoned as MISRA-neutral above rather than tool-confirmed. A fresh run
  is still recommended before treating section 1a's numbers as current.

**2026-08-05, update 3:** `sapi_watchdog.c` has been rewritten from a
non-functional stub (every function was a no-op or empty `/* TODO */`) to
a real, working implementation: a fixed-size static pool of watchdog
slots, timed via `sapi_timer_now()` (no dynamic allocation, no new
OS-specific code of its own). `sapi_watchdog_timer_tick()` now genuinely
scans for expired watchdogs and dispatches the configured recovery action
(`LOG` via `sapi_log_write()`, `SAFESTATE`/`REBOOT` via
`sapi_safestate_enter()`, `CUSTOM` via the caller's callback). As part of
this, the Rule 21.6 finding recorded below for `sapi_watchdog.c`
(`<stdio.h>`/`fprintf` use) is now fixed - the new implementation has no
`<stdio.h>` dependency at all, using only `sapi_log_write()` with static
string literals (the same convention already used by `sapi_channel.c`).
`sapi_appmanager.c`'s own Rule 21.6 finding is unrelated and still open.
Verified via a new real test suite (`tests/watchdog/test_sapi_watchdog.c`,
13/13 framework tests passing) using a mock `sapi_timer` backend with a
test-controlled clock: confirms a watchdog does NOT fire while kicked
regularly, DOES fire once its deadline is genuinely passed, and that each
of the `LOG`/`SAFESTATE`/`CUSTOM` actions dispatch correctly (SAFESTATE
verified via the same setjmp/longjmp-diverting-handler technique as
`tests/safestate/test_sapi_safestate.c`, since `SAPI_SAFESTATE_LEVEL_SAFE`
is documented to never return). Also fixed a real, previously-latent
linking bug this work surfaced: `src/watchdog/CMakeLists.txt` only linked
`safeapi_status`/`safeapi_log`, even though the (stub) implementation's
public header already implied a dependency on `safeapi_timer`; a
standalone consumer of `safeapi::watchdog` alone (e.g. the new unit test)
would have failed to link. Now links `safeapi_safestate`/`safeapi_timer`
too.

**2026-08-05, later same day - update 2:** the automated checker described
as unavailable in section 1 below is now actually running for the first
time (see new section 1a) - both because a real macOS `cmake`/`cppcheck`
environment became available (this report's original section 1 was
written from inside a network-isolated sandbox with neither), and because
two real bugs in this project's own tooling were fixed that had silently
prevented the `cppcheck` CMake target from ever doing anything despite
printing success (`CMAKE_EXPORT_COMPILE_COMMANDS` was never set, so
`compile_commands.json` never existed for `--project=...` to open; then,
once set, it was set as a plain `set()` instead of a `CACHE` variable,
which only affects the setting directory and its descendants - meaning it
never took effect for a downstream project's own targets when this
framework is consumed via `add_subdirectory()`, e.g. by `safeAPIRBC2oo2`).
Section 1a records what the first real run actually found. The three
CRC-64 lookup tables flagged as incomplete placeholders in section 2 below
have also been fixed (full, correctly generated 256-entry tables) - see
`sapi_checksum.c`'s own file-level note for detail.

Scope: `include/` and `src/` (shipped library code only - `tests/` is
verification tooling, not a deliverable, and is called out separately in
section 4).

**Known gap, not closed by this update:** this report has not been
re-verified against every module in the current tree - `sapi_watchdog`,
`sapi_channel`, `sapi_appmanager`, and most of `sapi_checksum`'s
pre-existing logic were added by work outside the review that originally
produced this document and have not had a MISRA pass done against them,
beyond what was necessary to make `sapi_checksum.c` compile at all (see
below). Treat sections 2-3 below as covering the modules present when
this report was first written, plus the two ADR-017 additions - not the
whole current `src/` tree. Section 1a's real tool run *does* cover the
whole tree, and is the more reliable source for anything it disagrees
with below.

## 1. How this report was originally produced (manual review)

**No automated MISRA checker was available to run this review.** This
sandbox has no network/package-install access (`apt-get`, `pip install`
all fail with no route to a package index), and no static analysis tool
(cppcheck, clang-tidy, or a commercial MISRA checker) is preinstalled. What
follows is a **manual review** against the specific rules judged most
relevant to this codebase, backed by `grep`-based evidence collected
directly against the source tree (commands and results below each rule),
plus a design-level read of every file. It is not a substitute for a real
MISRA tool and should not be treated as certification evidence on its own
- see section 5 for how to get one.

## 1a. First real automated run (`cppcheck --addon=misra`), 2026-08-05

Run via `cmake --build build --target cppcheck` (real macOS environment,
`cppcheck` from Homebrew) after the two tooling fixes described above.
Output: `build/cppcheck-report.txt`/`.xml`. Note: no `--rule-texts=<file>`
was supplied (that requires a licensed copy of the MISRA C:2012 document
to map rule numbers to their actual wording) - findings below are
identified by rule number only, per cppcheck's own limitation without
that file; verifying rule *text* against a licensed copy is still required
before treating any of this as compliance evidence, exactly as section 5
already said.

Findings against `safeAPIFreamwork/src/*` (826 total, by rule, top ones):

| Rule | Count | Note |
|---|---:|---|
| 15.5 (single point of exit) | 387 | Matches the deviation already documented in section 3 - consistent guard-clause style across the codebase, not new. |
| 8.7 (internal linkage) | 138 | Needs manual triage - section 2 claims this rule is compliant-by-construction (`static` on every backend/handler table); a real tool disagreeing with that specific claim across 138 sites needs to be reconciled, not assumed to be a tool false-positive. Not yet triaged as part of this update. |
| 17.7 (ignored return value) | 34 | Needs triage - some are likely legitimate (`(void)`-cast calls the addon still flags), some may be real. |
| 21.6 (banned `<stdio.h>`) | 28 (as originally counted) | **Confirmed real, not a tool artifact:** both hits were in `sapi_appmanager.c` and `sapi_watchdog.c` - exactly the two modules this report's own "Known gap" paragraph already named as never having been reviewed. **Update 3:** the `sapi_watchdog.c` contribution to this count is now fixed (real rewrite, no `<stdio.h>` dependency, see the update-3 note above) - a fresh cppcheck run confirms no `21.6`/`missingIncludeSystem <stdio.h>` finding remains for that file. `sapi_appmanager.c`'s `<stdio.h>` use is unrelated to this task and remains open. |
| 12.1, 10.4, 11.5, 5.9, 20.9, 10.8, 8.9, 21.16, 10.2, 8.4 | 25/18/13/9/8/4/3/2/1/1 | Not yet triaged. |
| `unusedFunction` | 139 | Not a MISRA rule - cppcheck's own dead-code detector. Expected for a library where most public API functions aren't called from within the library itself (they're called by consumers like `safeAPIRBC2oo2`); not necessarily a real problem, but not yet individually verified either. |

**This is the first ground truth this project has had from a real tool.**
It broadly confirms section 3's documented 15.5 deviation at scale, and
confirms the "Known gap" paragraph's suspicion about `appmanager`/
`watchdog` was correct. The remaining rules (8.7, 17.7, and the smaller
counts) are **not yet triaged** - listed here rather than silently
dropped, per this report's own standing instruction not to go stale. Given the
scale (826 raw findings, many likely legitimate deviations once triaged
individually, some likely tool noise without `--rule-texts`), a proper
triage pass is a dedicated follow-up, not something folded into this
update.

`safeAPIRBC2oo2` (a separate project, out of this report's own stated
scope) was also checked incidentally by the same run (496 findings) -
its own compliance posture is that project's own responsibility to
document; its README already discloses "No SIL-level V&V, no MISRA/
cppcheck pass has been run against this project's own source."

## 2. Compliant by construction

These rules were designed in from the start (see the relevant ADRs), not
retrofitted, and are verified here by direct search of the shipped source:

| Rule | Topic | Evidence |
|---|---|---|
| Dir 4.12 (mandatory-equivalent per CLAUDE.md) | No dynamic memory allocation | `grep -rn "malloc\|free(\|realloc" include src` -> zero real hits (only a comment describing the policy). Every stateful object uses caller-owned static storage (ADR-001 section 3.4). |
| Rule 10.1-10.8 | Essential type model / implicit conversions | Every conversion between fixed-width types and `size_t` goes through a checked `sapi_cast_*` function (ADR-003); no bare narrowing casts exist elsewhere in `include`/`src` (`grep` for stray native `int`/`long`/`unsigned` outside `sapi_types.h`'s byte-array storage macro returned nothing). |
| Rule 17.2 / CLAUDE.md "no recursion" | No recursion | Manual review: no function in `src/` calls itself directly or indirectly; the call graph is flat (public API -> backend dispatch, one level). |
| Rule 21.6 (required) | No `<stdio.h>` | `grep -rln "stdio.h" include src` -> zero hits. |
| Rule 21.4 (required, contextual) / CLAUDE.md "no assert in production" | No `<setjmp.h>`/`<assert.h>`/`<errno.h>` in shipped code | `grep -rln "assert.h\|errno.h\|setjmp.h" include src` -> zero hits. (`tests/` uses both `<assert.h>` and, in `test_sapi_safestate.c`, `<setjmp.h>` - see section 4.) |
| Rule 20.13/2.1 | No `goto`, no unreachable code | `grep -rn "goto" include src` -> zero hits. The one intentionally-infinite `for (;;)` (`sapi_safestate.c`, the defensive halt) is the last statement in its function, nothing follows it. |
| Rule 19.2 (advisory) | Avoid `union` | `grep -rn "union" include src` -> zero hits. |
| Rule 8.7 | Objects/functions used only within one translation unit shall have internal linkage | Every per-service backend pointer (`s_backend`) and the safestate handler table (`s_handlers`) is declared `static`. |
| Rule 16.1/16.4 (required) | Every `switch` shall have a `default` | Both `switch` statements in the codebase (`sapi_safestate.c`, `sapi_status.c`) have an explicit `default` clause. |
| Fixed-width types | Use `<stdint.h>` types, not native `int`/`long` | Every public API uses `uint8_t`.."uint64_t"/`int8_t`.."int64_t"/`size_t`/`bool`; no bare `int`/`long`/`short` appears in any public signature. |
| `const` correctness | Immutable pointer targets marked `const` | Every read-only buffer/backend-vtable parameter is declared `const` (e.g. `const void *buffer`, `const sapi_timer_backend_t *backend`). |
| Rule 21.6 (required) | No `<stdio.h>` (ADR-017 addition) | `sapi_checkpoint.c`/`sapi_clocksync.c`: zero hits. `sapi_checksum.c` previously included `<stdio.h>` for printf-style logging calls that didn't compile against the real `sapi_log_write()` signature (no varargs) - both the calls and the now-dead include were removed as part of making this file compile at all (see the file-level comment in `sapi_checksum.c`). |
| Explicit status codes, no invented enum values | `sapi_checksum.c` fix | The pre-fix file referenced `SAPI_STATUS_ERROR`/`SAPI_STATUS_INVALID`, neither a member of `sapi_status_t` - this alone was a hard compile error, not a style issue. Remapped to the closest real code by meaning: `SAPI_STATUS_DATA_CORRUPTION` for CRC/sequence failures (matches the enum's own documented purpose - "Integrity check ... failed"), `SAPI_STATUS_INVALID_PARAM` for bad arguments, `SAPI_STATUS_ALREADY_INITIALIZED` for double-init. |

**Update (2026-08-20): new module, `include/safeapi/notify/sapi_notify.h`
(ADR-034).** Header-only, no `src/` file to run the normal cppcheck pass
against - reviewed manually instead. Rule 11.1 (function-pointer/other-type
conversion) is the rule this module exists specifically to avoid violating:
`SAFEAPI_DECLARE_CALLBACK_LIST` declares only a `{callback_fn_type fn; void
*context;}` storage shape with the callback field kept at its own real,
concrete function-pointer type throughout - no `void *`-typed function
pointer, no cast between function-pointer types anywhere in the header (see
the header's own doc for why a fully generic, signature-agnostic dispatcher
was rejected for exactly this reason). Rule 17.7 (no ignored return values)
is satisfied by construction in the header's own worked dispatch example: a
veto-gate loop folds every registered validator's return value into a single
`result` variable (nothing discarded), and NULL-guards every slot before
calling it (`sapi_watchdog_create()`'s own established custom-callback
precedent, cited directly in the header doc). No dynamic memory, no
recursion, no `<stdio.h>`/`<assert.h>`/`<errno.h>` - the header includes
only `<stdint.h>`. Standalone compile check (`gcc -std=c99 -Wall -Wextra
-Wpedantic`) of a worked instantiation: clean, zero warnings.

**Update (2026-08-20): new module, `include/safeapi/memory/sapi_mem_util.h`
(ADR-031).** Header-only, no `src/` file to run the normal cppcheck pass
against - reviewed manually instead. `sapi_mem_set()`/`sapi_mem_copy()`/
`sapi_mem_compare()` are thin `static inline` wrappers over `memset()`/
`memcpy()`/`memcmp()` - the ONE sanctioned call site for each, so
downstream code (this session: `safeAPIRBC2oo2`, all ~90 direct
`memset`/`memcpy`/`memcmp` call sites) routes through this header instead
of including `<string.h>` itself. No dynamic memory, no recursion, no
`<stdio.h>`/`<assert.h>`/`<errno.h>` - the header includes only
`<stddef.h>`/`<string.h>` (the latter is this module's own reason to
exist, not something it hides from a reviewer). Every parameter is
documented with its caller-ownership/NULL contract; `sapi_mem_compare()`'s
own doc explicitly tells callers to rely only on the zero/nonzero
distinction, never the sign, avoiding a fragile-comparison MISRA posture
issue at every call site rather than in just one place. Standalone
compile check (`gcc -std=c99 -Wall -Wextra -Wpedantic`): clean, zero
warnings.

**Update (2026-08-21): new module, `include/safeapi/mutex/sapi_mutex.h` +
`include/safeapi_backend/mutex/sapi_mutex_backend.h` + `src/mutex/sapi_mutex.c`
(ADR-033).** Added after an architecture review of `safeAPIRBC2oo2` found
it calling `pthread_mutex_init()`/`_lock()`/`_unlock()`/`_destroy()`
directly on a raw `pthread_mutex_t` application struct field - a real
violation of this framework's own OAL premise (an application should
depend only on this framework's portable API, never a platform threading
primitive directly). `cmake --build build --target cppcheck` run against
the new `src/mutex/sapi_mutex.c`: only rule 15.5 (single point of exit,
the same accepted guard-clause deviation already documented above at
scale - see the 387-count table entry) and `unusedFunction` (the same
established false-positive class every other OAL dispatch file gets,
since cppcheck's single-project analysis cannot see these public API
functions called from a downstream consumer like `safeAPIRBC2oo2`) - no
new rule category introduced beyond what `sapi_timer.c` (this module's
own template) already carries; in fact strictly fewer findings than
`sapi_timer.c`, since the pointer-cast rules (11.5/11.6/8.9) that
`sapi_timer.c` triggers live only in the POSIX backend implementation
for mutex (`safeAPIRBC2oo2/src/posix_backend/sapi_posix_backend_mutex.c`,
outside this repo's own cppcheck scope), not in the dispatch file itself.
`cmake --build build` + `ctest --test-dir build`: full rebuild, 27/27
pass. `safeAPIRBC2oo2` (downstream): migrated off `pthread_mutex_t`
entirely (`channel_ab_types.h`/`channel_ab.c`/`channel_ab_checkpoint.c`/
`channel_ab_io.c`), full rebuild, `ctest` (2/2), and the real Docker-based
`safeAPITestEnv` Robot Framework suite (11/11) all re-verified clean.

**Update (2026-08-20): setup-phase lock coverage extended (ADR-032).**
Fourteen functions across ten files (`sapi_timer_register_backend`,
`sapi_ipc_register_backend`, `sapi_task_register_backend`,
`sapi_netlink_register_backend`, `sapi_nvm_register_backend`,
`sapi_log_register_backend`, `sapi_clocksync_register_backend`,
`sapi_reboot_register_backend`, `sapi_mem_pool_register_backend`,
`sapi_mem_pool_create`, `sapi_safestate_register_handler`,
`sapi_ipc_pubsub_topic_create`, `sapi_ipc_rr_server_create`,
`sapi_ipc_rr_client_create`) now call
`sapi_lifecycle_check_setup_allowed()` as one of their first checks,
closing a gap an integrator-requested audit found: these are the same
class of one-time "wire this up" setup call ADR-026 already gated for
timers/channels/voters/cross-comparators/watchdogs, just never covered
when that ADR was written. `sapi_nvm_open()`/`sapi_log_init()` were
evaluated and deliberately left ungated - see ADR-032's own "Deferred"
note. `cmake --build build` + `ctest --test-dir build`: full rebuild and
suite pass. `safeAPIRBC2oo2` (downstream): full rebuild, `ctest`, and
`smoke.sh` re-verified against its established baseline. No new
`malloc()`/`free()`/`realloc()` call site was added anywhere - a
candidate malloc-backed default memory-pool backend was considered and
rejected (see ADR-032 §2) once `safeAPIRBC2oo2/src/posix_backend/sapi_posix_backend_memory.c`
was confirmed to already implement a complete, malloc-free (static
arena, bump allocator, intrusive free list) backend - the zero-`malloc`
count for this repo's own `include/`/`src/` (section 1a's grep sweep)
remains unchanged.

**Update (2026-08-05, same day as the section 1a tooling fix): fixed.**
`sapi_checksum.c`'s three CRC-64 lookup tables (`g_crc64_ertms_table`
etc.) were incomplete placeholders - only ~24 of 256 entries populated
for ERTMS, 2 of 256 for ISO/XZ, the rest implicitly zero per C array
initialization rules, explicitly commented in the source as placeholders,
and explicitly called out here as not real integrity protection. All
three are now full, correctly generated 256-entry tables (standard
reflected/right-shifting CRC table-generation algorithm against each
polynomial already declared in `sapi_checksum.h`) - see `sapi_checksum.c`'s
own file-level note for detail, and `safeAPIRBC2oo2`'s A<->B peer-link
CRC-64 integrity check (`channel_ab.c`) for the first real consumer of
this fix.

## 3. Known, documented deviations

Being transparent about these rather than silently non-compliant:

- **Rule 15.5 (advisory) - single point of exit.** CLAUDE.md states single
  point of exit is "preferred," not mandatory, and this codebase
  consistently uses early-return guard clauses for parameter validation
  instead (e.g. `sapi_timer_create` has 4-5 `return` statements: one per
  invalid-argument check, then the dispatch). This is a deliberate,
  consistent choice across every function in the codebase: guard clauses
  keep each validation check flat and independently readable rather than
  nesting every subsequent check inside the previous one's `else` branch,
  which is the more common accepted alternative to strict single-exit in
  modern safety-critical C style guides (e.g. NASA/JPL's own C guidelines
  make the same trade). Flagged here as a formal deviation rather than
  left implicit, since CLAUDE.md's own wording is a preference, not a
  hard rule, and this project has consistently chosen the guard-clause
  reading of it.
- **Rule 8.13 (advisory) - pointer parameters that could be `const` are
  not always.** Output parameters (`*out_handle`, `*out_value`, etc.) are
  correctly non-`const`; this has not been exhaustively re-verified
  parameter-by-parameter across all 72 `sapi_cast_*` functions plus every
  OAL service by a tool, only by design review during authoring.
- **Rule 21.5 (required) - `<signal.h>` shall not be used.** Violated,
  deliberately, in exactly one place:
  `sapi_appmanager_install_default_signal_handlers()`
  (`src/appmanager/sapi_appmanager.c`, guarded by
  `SAPI_APPMANAGER_HAVE_POSIX_SIGNALS`). This is a real, confirmed hit
  (`misra-c2012-21.5`, cppcheck), not a tool artifact - `<signal.h>` is
  genuinely included and `sigaction()` genuinely called. Rationale: this
  is an explicitly opt-in, POSIX-only convenience for stopping a
  long-running `sapi_appmanager_run()` loop from an operator (Ctrl+C) or
  process manager (SIGTERM) - see the function's own doc in
  `sapi_appmanager.h` for the full reasoning, including why this module
  (already not backend-dispatched, unlike the seven ADR-005 OAL services)
  was judged the least-bad place for a narrow, explicitly-named exception
  rather than every downstream POSIX integrator reimplementing the same
  handful of lines. It is compiled out entirely (returns
  `SAPI_STATUS_NOT_SUPPORTED`, no `<signal.h>` include at all) on any
  target where `SAPI_APPMANAGER_HAVE_POSIX_SIGNALS` is not defined, so a
  SIL-rated build targeting a real RTOS/bare-metal backend never compiles
  this code path in the first place. The handler itself is minimal by
  design (writes one `volatile int`, calls nothing else - see
  `sapi_appmanager_signal_handler()`'s own comment) specifically to avoid
  the underlying hazard Rule 21.5 exists to prevent (unbounded/unsafe
  work in signal-handler context). Not currently caller-configurable
  (always exactly `SIGINT`+`SIGTERM` -> `sapi_appmanager_request_shutdown()`);
  a caller needing different signals or additional handler logic should
  install their own via `sigaction()` directly rather than using this
  convenience function.
  **Update 2026-08-06:** this code path did not actually compile on
  Linux/glibc under strict C99 (`struct sigaction`/`sigaction()`/
  `sigemptyset()` are POSIX.1-2001, hidden by glibc without an explicit
  feature-test macro) - a real GitHub Actions CI failure caught this,
  masked locally because macOS's libc does not gate these declarations the
  same way. Fixed by adding `#define _POSIX_C_SOURCE 200809L` before any
  header include in `sapi_appmanager.c` (same pattern already used in
  `safeAPIRBC2oo2`'s own POSIX application files). No change to the
  deviation itself - `<signal.h>` is still genuinely included and still
  guarded by the same `SAPI_APPMANAGER_HAVE_POSIX_SIGNALS` compile-time
  gate; this only fixes a portability bug in code that was already
  supposed to work.

## 4. Explicitly out of scope: `tests/`

Test code is not shipped as part of the framework and is not held to the
same rules:

- `tests/safestate/test_sapi_safestate.c` and, per the same pattern,
  `tests/checkpoint/test_sapi_checkpoint.c` (ADR-017 - `sapi_checkpoint`
  also triggers safe-state, on an insufficient-confirmation timeout) use
  `<setjmp.h>`/`longjmp` to safely exercise the documented "does not
  return" contract for `SAFE`/`REBOOT` levels without hanging the test
  process (see the comment at the top of each file for the rationale).
  This would be a Rule 21.4 violation in production code; it is
  intentionally confined to test-only tooling.
- All test files use `<assert.h>` per normal unit-test practice.

## 5. Recommended follow-up

Section 1a is that real tool run - it is no longer unavailable, at least
in a real macOS/Linux dev environment (`cmake --build build --target
cppcheck`, or directly: `cppcheck --project=build/compile_commands.json
--addon=misra --std=c99 --enable=all --inconclusive`). What is still
missing:

- **Rule-text mapping.** No `--rule-texts=<file>` was supplied, so section
  1a's findings are rule-number-only; getting the actual rule wording
  still requires a licensed copy of MISRA C:2012 (or a commercial checker
  that bundles it) - the mapping cppcheck's addon does for free is not a
  substitute for verifying against real rule text.
- **Triage of section 1a's un-triaged rules** (8.7, 17.7, 12.1, 10.4,
  11.5, 5.9, 20.9, 10.8, 8.9, 21.16, 10.2, 8.4) - each needs a per-site
  decision: real violation needing a fix, or a documented deviation to
  add to section 3, or (less likely, but possible without `--rule-texts`)
  a tool false-positive.
- **`sapi_appmanager.c`'s confirmed `<stdio.h>` use** (Rule 21.6) - a
  real, now-confirmed finding, not yet fixed. (`sapi_watchdog.c`'s own
  contribution to this finding was fixed as of update 3 above - this
  section previously named both files together, which contradicted that
  update; corrected to name only the file that's actually still open.)
- A commercial checker (LDRA, Parasoft C/C++test, PC-lint Plus, Polyspace)
  for certification-grade evidence, which EN 50128 SIL 3/4 verification
  will ultimately require rather than cppcheck's free addon or this
  manual review.

This report should be regenerated (or have section 1a re-run and
refreshed) whenever a new module is added or the un-triaged rule list
above is worked through - don't let the "not yet triaged" framing above
become permanent.

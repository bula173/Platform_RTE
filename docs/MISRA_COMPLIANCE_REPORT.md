# MISRA C:2012 Compliance Report

Date: 2026-08-06 (updated for `sapi_watchdog.c`'s new
`SAPI_WATCHDOG_ACTION_FAILOVER` dispatch and a real portability bug fix in
`sapi_appmanager.c` - see "update 4" note below)

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
string literals (the same convention already used by `sapi_vital_channel.c`).
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
framework is consumed via `add_subdirectory()`, e.g. by `safeAPIExample`).
Section 1a records what the first real run actually found. The three
CRC-64 lookup tables flagged as incomplete placeholders in section 2 below
have also been fixed (full, correctly generated 256-entry tables) - see
`sapi_checksum.c`'s own file-level note for detail.

Scope: `include/` and `src/` (shipped library code only - `tests/` is
verification tooling, not a deliverable, and is called out separately in
section 4).

**Known gap, not closed by this update:** this report has not been
re-verified against every module in the current tree - `sapi_watchdog`,
`sapi_vital_channel`, `sapi_appmanager`, and most of `sapi_checksum`'s
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
| `unusedFunction` | 139 | Not a MISRA rule - cppcheck's own dead-code detector. Expected for a library where most public API functions aren't called from within the library itself (they're called by consumers like `safeAPIExample`); not necessarily a real problem, but not yet individually verified either. |

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

`safeAPIExample` (a separate project, out of this report's own stated
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

**Update (2026-08-05, same day as the section 1a tooling fix): fixed.**
`sapi_checksum.c`'s three CRC-64 lookup tables (`g_crc64_ertms_table`
etc.) were incomplete placeholders - only ~24 of 256 entries populated
for ERTMS, 2 of 256 for ISO/XZ, the rest implicitly zero per C array
initialization rules, explicitly commented in the source as placeholders,
and explicitly called out here as not real integrity protection. All
three are now full, correctly generated 256-entry tables (standard
reflected/right-shifting CRC table-generation algorithm against each
polynomial already declared in `sapi_checksum.h`) - see `sapi_checksum.c`'s
own file-level note for detail, and `safeAPIExample`'s A<->B peer-link
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
  `safeAPIExample`'s own POSIX application files). No change to the
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
- **`sapi_appmanager.c`/`sapi_watchdog.c`'s confirmed `<stdio.h>` use**
  (Rule 21.6) - a real, now-confirmed finding, not yet fixed.
- A commercial checker (LDRA, Parasoft C/C++test, PC-lint Plus, Polyspace)
  for certification-grade evidence, which EN 50128 SIL 3/4 verification
  will ultimately require rather than cppcheck's free addon or this
  manual review.

This report should be regenerated (or have section 1a re-run and
refreshed) whenever a new module is added or the un-triaged rule list
above is worked through - don't let the "not yet triaged" framing above
become permanent.

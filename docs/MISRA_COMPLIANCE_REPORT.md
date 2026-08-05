# MISRA C:2012 Compliance Report

Date: 2026-08-05 (updated for `sapi_checkpoint`/`sapi_clocksync`, ADR-017;
also fixed `sapi_checksum.c`, which did not compile prior to this pass -
see section 2's new row)
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
whole current `src/` tree.

## 1. How this report was produced

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

**Separate, more significant finding in `sapi_checksum.c`, not fixed by
this pass:** its three CRC-64 lookup tables (`g_crc64_ertms_table` etc.)
are incomplete placeholders - only ~24 of 256 entries populated for
ERTMS, 2 of 256 for ISO/XZ, the rest implicitly zero per C array
initialization rules, and explicitly commented in the source as
placeholders. Compute-then-verify round-trips are still internally
self-consistent (useful for testing `sapi_checkpoint`'s logic), but this
is **not a mathematically correct CRC-64** and must not be treated as
real integrity protection until proper tables are generated for all
three polynomials.

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

This report is a stand-in until a real tool can be run. Once network/tool
access is available (locally or in CI), the recommended path is:

```sh
# cppcheck's MISRA addon (free tool; the addon maps warnings to rule
# numbers but the official rule *text* still requires a MISRA license to
# display verbatim - the mapping itself is free to use):
cppcheck --enable=all --inconclusive --addon=misra \
         --std=c99 -I include src
```

or a commercial checker (LDRA, Parasoft C/C++test, PC-lint Plus, Polyspace)
for certification-grade evidence, which EN 50128 SIL 3/4 verification will
ultimately require rather than this manual review. This report should be
regenerated (or replaced by real tool output) whenever a new module is
added - the deviation list in section 3 in particular should be reviewed
by whoever runs the first real static analysis pass, since a tool may
surface additional Rule 15.5/8.13 instances this manual pass missed.

# ADR-023: Consolidate 22 CMake Library Targets into 4

Status: Accepted
Date: 2026-08-07
Applies to: safeAPIFreamwork's top-level `CMakeLists.txt`,
`tests/CMakeLists.txt`, every `src/<feature>/CMakeLists.txt`; downstream
safeAPIExample's `CMakeLists.txt` and `src/posix_backend/CMakeLists.txt`;
the `examples/*-app` integration templates.

## 1. Context

ADR-007 gave every feature its own directory and its own CMake static
library target (`safeapi_<feature>`, aliased `safeapi::<feature>`),
arguing that "an integrator who only needs `sapi_cast` and
`sapi_safestate` links exactly those two targets." That granularity has
grown to 21 feature targets plus `appmanager` (22 total; the dead,
excluded-from-build ADR-008 `sapi_channel` module is not counted). Most
of these targets are tiny - `status` is 33 lines of `.c`, `reboot` 34,
`types` is header-only - and none of them are ever linked "à la carte"
in practice: every real consumer (safeAPIExample's `posix_backend` and
its own top-level `CMakeLists.txt`, every framework unit test) already
links a large, overlapping subset of them, not one or two in isolation.
The promised fine-grained linking benefit isn't actually being used
anywhere in either repository, while the cost - 22 tiny `CMakeLists.txt`
files, a 22-name `foreach()` in the top-level file, a 22-name
`install(TARGETS ...)` list, and a `target_link_libraries()` line in
every test and every consuming `CMakeLists.txt` that has to know which
of the 22 names it needs - is paid on every change.

The dependency graph between the 22 targets is not arbitrary; it falls
into three non-overlapping clusters plus one top-level orchestrator that
sits above all three:

- **Core primitives** (`status`, `types`, `buffer`, `cast`, `safestate`,
  `string`): zero dependency on anything OS-related (ADR-002 section 4's
  rule, restated by ADR-007 section 2.3) - `string` is the only one with
  an inter-feature dependency at all (`buffer`, `cast`).
- **OS Abstraction Layer** (`timer`, `nvm`, `memory`, `task`, `ipc`,
  `netlink`, `log`, `reboot`, `watchdog` - ADR-001/ADR-005's
  backend-dispatch modules): `log` depends on `string`+`timer`;
  `watchdog` depends on `status`+`log`+`safestate`+`timer`. Every OAL
  module's `.c` file only ever calls core-primitive or same-layer
  functions - never a channel-layer one.
- **Safety-comms / channel layer** (`checksum`, `vital_channel`,
  `clocksync`, `checkpoint`, `dual`, `safechannel` - ADR-008/017/020/022):
  genuinely interdependent (`safechannel` alone already pulls in `dual`,
  `checksum`, and `vital_channel`; `checkpoint` pulls in `checksum`,
  `watchdog`, `vital_channel`), and every module here reaches down into
  both core primitives (`status`, `types`, `safestate`, `buffer`) and OAL
  services (`log`, `timer`, `netlink`).
- **`appmanager`**: depends on `status` (core), `log` (OAL), and
  `checkpoint` (channels) - sits above all three clusters, so it stays
  its own target regardless of how the three below it are grouped.

## 2. Decision

Collapse the 21 feature targets into 3 static libraries along the
cluster boundaries above, keep `appmanager` as a 4th, and delete
per-feature `CMakeLists.txt` files entirely - the three new libraries'
`add_library()` calls are declared directly in the top-level
`CMakeLists.txt`, which already lists every source file's path
unambiguously (`src/<feature>/sapi_<feature>.c`).

```
safeapi_core     : status, buffer, cast, safestate, string
                   (+ types' header, which needs no .c and is already
                   covered by the shared PUBLIC include directory - no
                   separate INTERFACE target needed for it anymore)
safeapi_oal      : timer, nvm, memory, task, ipc, netlink, log, reboot,
                   watchdog          -- PUBLIC links safeapi_core
safeapi_channels : checksum, vital_channel, clocksync, checkpoint,
                   dual (all 4 of its .c files), safechannel
                                     -- PUBLIC links safeapi_core, safeapi_oal
safeapi_appmanager (unchanged file) -- PUBLIC links safeapi_core,
                                        safeapi_oal, safeapi_channels
```

Aliased `safeapi::core`, `safeapi::oal`, `safeapi::channels`,
`safeapi::appmanager` - same alias convention as before, just four names
instead of twenty-two.

### 2.1 What does *not* change

- **No file moves.** Every `.c`/`.h` stays exactly where ADR-007 put it
  (`src/<feature>/sapi_<feature>.c`, `include/safeapi/<feature>/sapi_<feature>.h`).
  This ADR only changes which compiled library a `.c` file's object code
  ends up in - it is not a reversal of ADR-007's directory-per-feature
  layout (section 2.1), only of section 2.2's "one CMake target per
  feature" corollary. Directory-level navigation ("find everything about
  the timer service") is unaffected.
- **No `#include` path changes anywhere** - not in framework tests, not
  in safeAPIExample, not in the example templates. A header include
  never depended on which library the corresponding `.c` was compiled
  into.
- **No public API, status code, or behavior change** in any module.
- The backend header split (ADR-021, `include/safeapi_backend/`) is
  unaffected - it was already header-placement-only, not a separate set
  of build targets.
- ADR-008's `sapi_channel` module (`src/channel/`) stays excluded from
  the build exactly as it was; its own `CMakeLists.txt` is untouched and
  its retire-or-fix disposition remains the separate open decision ADR-020
  already described it as.
- `sapi_ipc_pubsub.c`/`sapi_ipc_request_reply.c` (pre-existing stub files
  under `src/ipc/`, never wired into `safeapi_ipc`'s source list and
  including a nonexistent `safeapi/log.h` path) are **not** added to the
  new `safeapi_oal` library's source list either - this ADR preserves
  exactly what was and wasn't compiled before it, and does not fix that
  pre-existing, unrelated gap. Worth a separate ticket, not bundled here.

### 2.2 Consumers updated

Every place that linked an old per-feature target now links one of the
four new ones instead:

- `tests/CMakeLists.txt`: each test links whichever of `safeapi::core` /
  `safeapi::oal` / `safeapi::channels` / `safeapi::appmanager` actually
  covers the module under test (e.g. `test_sapi_nvm` now links
  `safeapi::oal`, `test_sapi_checkpoint` now links `safeapi::channels`) -
  `target_link_libraries` being `PUBLIC` throughout the new libraries
  means linking one name still pulls in everything transitively required,
  exactly as linking the old fine-grained set did.
- `install(TARGETS ...)` in the top-level `CMakeLists.txt`: four names
  instead of twenty.
- safeAPIExample's `src/posix_backend/CMakeLists.txt` (was: `status`,
  `types`, `timer`, `ipc`, `netlink`, `task`, `log`, `nvm`, `reboot`,
  `memory`) now links `safeapi::core` and `safeapi::oal`.
- safeAPIExample's top-level `CMakeLists.txt` (was 12 individual names)
  now links `safeapi::posix_backend`, `safeapi::core`, `safeapi::oal`,
  `safeapi::channels` (for `checksum`), `safeapi::appmanager`.
- `examples/qnx-rtos-app/CMakeLists.txt` and
  `examples/linux-posix-app/CMakeLists.txt` (integration templates, not
  built as part of this repo's own test suite) updated the same way, plus
  `examples/build-qnx.sh`'s doc string.

## 3. Consequences

- Positive: 22 `CMakeLists.txt` files and a 22-entry `foreach()`/
  `install(TARGETS ...)` collapse to 4 `add_library()` blocks and a
  4-entry install list; every consumer's own link list shrinks by the
  same ratio (safeAPIExample's top-level file: 12 names -> 5).
- Positive: the dependency direction is now a single, obvious linear
  chain (core -> oal -> channels -> appmanager) instead of an
  ad hoc per-feature graph a reader had to reconstruct from 22 separate
  `target_link_libraries()` calls to understand.
- Negative: the fine-grained "link only `sapi_cast`" story ADR-007
  section 2.2 promised is gone - the smallest unit link-able now is
  `safeapi::core` (all six primitives together). This was judged an
  acceptable trade since no consumer in either repository ever actually
  exercised that fine-grained linking; if a future integrator genuinely
  needs, say, `sapi_cast` alone without the rest of `core` (e.g. a
  size-constrained bootloader stage), splitting a narrower fifth library
  back out remains possible without touching directory layout again.
- Negative (documented, not treated as blocking): this makes each
  module's *EN 50128 configuration/verification item* boundary slightly
  fuzzier at the CMake-target level than "22 independently linkable
  units" implied - in practice this was already only nominal (every real
  consumer already linked overlapping supersets), and each `.c` file is
  still individually reviewed, tested (`tests/<feature>/test_sapi_<feature>.c`
  is unchanged, one file per feature, unaffected by this ADR), and
  MISRA-checked regardless of which `.a` its object code lands in.
- Neutral: this ADR supersedes ADR-007 section 2.2 specifically (one
  CMake target per feature) - section 2.1 (per-feature directory layout)
  and every other ADR are otherwise unaffected. A pointer to this ADR has
  been added at ADR-007 section 2.2.

## 4. Verification

No `cmake` binary is available in this sandbox session (same
longstanding constraint as every other change in this project's history
here); verification is therefore: (a) manual review of the new
`add_library()` source lists against `find src -name "*.c"`'s actual
output, confirmed to match ADR-007's original per-feature source lists
exactly except for the grouping; (b) a repository-wide grep for every old
`safeapi::<feature>` target name after the change, confirming no leftover
reference to a name that no longer exists; (c) manual
`gcc -std=c99 -Wall -Wextra -Wpedantic` builds of all 17 framework unit
tests and a full safeAPIExample rebuild, compiling the exact same `.c`
files a real `cmake --build` would select for the new targets (this
verification method builds from source-file lists directly and was
already insensitive to which library a file was grouped into, so it does
not by itself prove the new `CMakeLists.txt` is free of CMake syntax
errors - an actual `cmake --build` on a real toolchain remains the
outstanding verification step, same caveat noted for every prior
CMake-only change in this project).

## 5. Location

- `CMakeLists.txt` (top-level, safeAPIFreamwork)
- `tests/CMakeLists.txt`
- `src/appmanager/CMakeLists.txt` (link line only)
- `src/channel/CMakeLists.txt` (untouched, listed here only to make clear
  it was considered and deliberately left alone)
- safeAPIExample: `CMakeLists.txt`, `src/posix_backend/CMakeLists.txt`
- `examples/qnx-rtos-app/CMakeLists.txt`,
  `examples/linux-posix-app/CMakeLists.txt`, `examples/build-qnx.sh`

# ADR-024: Per-Module Feature Selection at Configure Time

Status: Accepted
Date: 2026-08-15
Applies to: Platform_RTE's top-level `CMakeLists.txt`,
`tests/CMakeLists.txt`, `cmake/RTEHelpers.cmake`,
`cmake/RteFrameworkConfig.cmake.in`, `src/appmanager/CMakeLists.txt`.

## 1. Context

ADR-023 collapsed 22 per-feature CMake targets into 4 grouped static
libraries (`rte_core`/`oal`/`channels`/`appmanager`) because no
consumer ever linked a feature target in isolation - every real
consumer already linked a large, overlapping subset. That consolidation
was about *linking* granularity, not *compiling* granularity: every
module's `.c` file was, and until this ADR still is, compiled into its
group's library unconditionally, regardless of whether a given
integrator's application ever calls into it.

That matters for two reasons beyond binary size:

- **SIL verification scope.** Under EN 50128, code that ships in a
  safety-related build has to be verified to the target SIL - reviewed,
  tested, MISRA-checked, traced to a requirement. A module an
  integrator never uses but that still gets compiled into their binary
  is a module they still have to account for in that verification
  scope, whether they exercise it or not.
- **Explicit configuration.** An integrator building a train-protection
  core that has no need for, say, `rte_reboot` or the West/East
  negotiation machinery in `rte_dual` should be able to say so at
  configure time and have that module genuinely absent from the build,
  not merely unreferenced.

ADR-023's own "Negative" consequence already flagged this as a known,
accepted trade at the time ("if a future integrator genuinely needs...
splitting a narrower library back out remains possible without
touching directory layout again"). This ADR is that follow-up, done
without reversing ADR-023's target consolidation: the 4 library names
stay exactly as they are, but what goes *into* each one's source list
is now selectable.

## 2. Decision

Add one `option(RTE_ENABLE_<NAME> ... ON)` per toggleable module to
the top-level `CMakeLists.txt`. Default **ON** for every option, so an
existing consumer (RBC_GP, the example templates) keeps
building exactly what it built before with zero CMake changes on their
side. `CORE` (`status`, `types`, `buffer`, `cast`, `safestate`,
`string`) has no option and is always compiled - it is the one cluster
with no OS/backend dependency of its own (ADR-002 section 4) and every
other module needs it, directly or transitively.

### 2.1 The dependency graph

Derived from actual `#include` usage (grepped, not assumed) across
`src/` and `include/rte/`:

```
CORE (always on): status, types, buffer, cast, safestate, string

OAL layer (each an independent option, default ON):
  TIMER, NVM, MEMORY, TASK, NETLINK, IPC, REBOOT   -- need only CORE
  LOG                                               -- needs TIMER
  WATCHDOG                                          -- needs LOG, TIMER

Channels layer (each an independent option, default ON):
  CLOCKSYNC                                         -- needs only CORE
  CHECKSUM                                          -- needs TIMER
  CHANNEL_LINK                                      -- needs LOG
  VOTER          -- needs CHANNEL_LINK, LOG
  CROSS_COMPARATOR -- needs CHANNEL_LINK, VOTER, LOG
  CHECKPOINT     -- needs CHECKSUM, TIMER, VOTER, WATCHDOG
  DUAL           -- needs CHECKSUM, NETLINK, TIMER
  SAFECHANNEL    -- needs CHECKSUM, DUAL, NETLINK, VOTER

App layer (option, default ON):
  APPMANAGER     -- needs VOTER, WATCHDOG, CHECKPOINT
                    (hard #include dependency in rte_appmanager.h/.c -
                    the checkpoint/watchdog integration is a *runtime*
                    NULL check, but the types must still be compiled in
                    either way, so this is a real build-time dependency
                    even though usage is optional)
```

(As of ADR-025, `VITAL_CHANNEL` was split into `CHANNEL_LINK` - a single
point-to-point link primitive - plus the new `VOTER` and
`CROSS_COMPARATOR` options; `CHECKPOINT`, `SAFECHANNEL`, and
`APPMANAGER`'s dependency each moved from `VITAL_CHANNEL` to `VOTER`
accordingly. See ADR-025 for the full rationale.)

`IPC` covers only the real `rte_ipc.c` queue API. The pre-existing
`rte_ipc_pubsub.c`/`rte_ipc_request_reply.c` stub files (TODO-only,
never wired into any target - see ADR-023 section 2.1) stay excluded
regardless of `RTE_ENABLE_IPC`; this ADR does not change that.

### 2.2 Enforcement: explicit failure, not silent auto-enable

`cmake/RTEHelpers.cmake` gains `rte_require_feature(<FEATURE>
DEPENDS <dep1> <dep2> ...)`: if `<FEATURE>` is `ON` and any `<depN>` is
`OFF`, configure fails with `FATAL_ERROR` naming exactly which
`-DRTE_ENABLE_<dep>=ON` to pass. This was chosen over silently
force-enabling the missing dependency (a common CMake pattern
elsewhere) because a build a SIL verification package will point to
should never end up with extra code compiled in that nobody explicitly
asked for at configure time - the whole reason a `-D` flag was flipped
matters, so a mistaken flag combination should be a build failure the
integrator sees immediately, not a silently-widened build. Every edge
in section 2.1 has a matching `rte_require_feature()` call in the
top-level `CMakeLists.txt`.

### 2.3 What changes mechanically

- Each grouped library's `add_library(... STATIC ...)` source list is
  now built up conditionally (`if(RTE_ENABLE_X) list(APPEND ...)
  endif()` per source) instead of being a fixed list. If every option
  in a group ends up `OFF`, configure fails with a clear message
  (`rte_oal`/`rte_channels` cannot be an empty library) rather
  than producing a broken zero-source target.
- `tests/CMakeLists.txt`: each optional module's `rte_add_test()`
  call is gated behind the same option, so a disabled module doesn't
  leave a test target that fails to link.
- `src/appmanager/CMakeLists.txt`'s inclusion becomes conditional on
  `RTE_ENABLE_APPMANAGER` (previously gated only on the
  subdirectory's `CMakeLists.txt` existing at all).
- A configure-time summary (`message(STATUS "RTE_ENABLE_<X>:
  <value>")` per option) so `cmake -B build` output shows the full
  selection without needing `cmake -L`.
- `install(TARGETS ...)` only lists `rte_appmanager` when
  `RTE_ENABLE_APPMANAGER` actually created that target
  (`if(TARGET rte_appmanager)`).

### 2.4 What does *not* change

- The 4 library/alias names (`rte_core`/`oal`/`channels`/
  `appmanager`, `rte::core` etc.) from ADR-023 are unchanged - this
  ADR only changes what goes *into* each one's source list, never their
  names or the fact there are 4 of them.
- No file moves, no `#include` path changes, no public API or behavior
  change in any enabled module.
- Default behavior with no `-D` flags passed is unchanged: every module
  builds, exactly as before this ADR.

## 3. Consequences

- Positive: an integrator can now genuinely exclude a module from
  their build (`-DRTE_ENABLE_WATCHDOG=OFF -DRTE_ENABLE_REBOOT=OFF
  ...`), narrowing both binary size and SIL verification scope to what
  their application actually uses.
- Positive: the dependency graph in section 2.1, once written down,
  turned out to double as documentation of the framework's actual
  internal coupling - useful independent of the build-configuration
  motivation (e.g. it makes explicit that `rte_appmanager` cannot be
  used without `rte_watchdog` being compiled in, which was previously
  only discoverable by reading `rte_appmanager.c`'s `#include` list).
- Negative: 15 new CMake options is more surface for a downstream
  `CMakeLists.txt` or CI matrix to have to reason about than the
  previous "it's all compiled in" default. Mitigated by every option
  defaulting `ON` (opt-out, not opt-in) and by `rte_require_feature()`
  failing loudly on a misconfiguration rather than producing a build
  that silently differs from what was requested.
- Neutral: this ADR supersedes none of ADR-023's decisions; it composes
  with it directly (ADR-023's 4 targets are the unit this ADR's options
  gate the *contents* of).

## 4. Verification

- `cmake -B build && cmake --build build && ctest --test-dir build`
  with every option at its default (`ON`) - equivalent to the pre-ADR-024
  full build, all 19+ existing tests pass unchanged.
- A reduced-feature configure/build
  (`-DRTE_ENABLE_WATCHDOG=OFF -DRTE_ENABLE_CHECKPOINT=OFF
  -DRTE_ENABLE_APPMANAGER=OFF`) succeeds and produces libraries with
  those modules' object files genuinely absent (`ar t
  librte_oal.a` no longer lists `rte_watchdog.c.o`).
- A misconfigured combination (e.g. `-DRTE_ENABLE_SAFECHANNEL=ON
  -DRTE_ENABLE_DUAL=OFF`) fails configure with a `FATAL_ERROR`
  naming the exact flag to add, rather than failing later at compile or
  link time with a confusing missing-header/undefined-symbol error.
- RBC_GP (which builds this project via `add_subdirectory()`
  with every option left at its `ON` default) still builds and its own
  smoke test (`.claude/skills/run-RBC_GP/smoke.sh`) still
  passes unchanged.

## 5. Location

- `CMakeLists.txt` (top-level, Platform_RTE) - option declarations,
  dependency enforcement, conditional source lists, install target list.
- `cmake/RTEHelpers.cmake` - `rte_require_feature()`.
- `cmake/RteFrameworkConfig.cmake.in` - unaffected by which options
  were used to build the installed package, beyond `rte::appmanager`
  only existing in the exported targets file if it was enabled.
- `tests/CMakeLists.txt` - per-module test gating.
- `src/appmanager/CMakeLists.txt` - unaffected in content, only in
  whether the parent `add_subdirectory()` call reaches it at all.

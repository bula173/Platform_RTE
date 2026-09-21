# ADR-007: Per-Feature Directory Layout

Status: Draft
Date: 2026-08-02
Applies to: repository-wide directory structure

## 1. Context

Through ADR-001 to ADR-006 the repository grew a two-tier grouping:
`common/` (layer-agnostic facilities) and `os/` (OAL services), each
holding every feature's files together — `src/os/rte_timer.c` sat next
to `rte_nvm.c`, `rte_ipc.c`, and five others in the same directory. That
grouping reflected an architectural distinction worth keeping *conceptually*
(ADR-001/002 still explain why some things depend on the OS and some
don't), but as more features were added, finding "everything about the
timer service" meant filtering one file out of a flat directory of seven.

## 2. Decision

Every feature gets its own directory, mirrored across all three trees:

```
include/safeapi/<feature>/rte_<feature>.h
src/<feature>/rte_<feature>.c          (+ CMakeLists.txt)
tests/<feature>/test_rte_<feature>.c
```

Features: `status`, `types` (header-only, no `.c`/test), `buffer`, `cast`,
`safestate`, `string`, `timer`, `nvm`, `memory`, `task`, `ipc`, `log`,
`reboot`.

### 2.1 The `common`/`os` grouping folder is dropped, not renamed

Rather than nesting feature folders one level deeper under `common/`/`os/`
(e.g. `src/common/buffer/`), the grouping folder is removed entirely and
every feature sits directly under `src/`, `include/safeapi/`, `tests/` as
a sibling of every other feature. The common-vs-OAL distinction that
motivated the original grouping is still real and still documented (every
ADR still explains which features have an OS dependency and which don't;
`rte_buffer`/`rte_cast`/`rte_safestate`/`rte_string` still have zero
dependency on any OAL feature, per ADR-002 section 4's original rule), it's
just no longer encoded as a directory level. One flat namespace of
feature folders is simpler to navigate than a two-level one for a project
this size, and the conceptual grouping is one `grep`/ADR-read away when it
matters (e.g. auditing "does anything in `common` accidentally depend on
`os`" is still a valid and answerable question — see section 2.3).

### 2.2 One CMake target per feature

**Superseded by ADR-023** (2026-08-07): the 21 per-feature targets this
section describes were later collapsed into 4 grouped static libraries
(`safeapi_core`/`oal`/`channels`/`appmanager`) because the fine-grained
linking this section promised was never actually exercised by any real
consumer. The directory-per-feature layout described in section 2.1
above is unaffected and remains current.

Each `src/<feature>/CMakeLists.txt` builds a small static library
`safeapi_<feature>`, aliased `safeapi::<feature>`. Most features have zero
inter-feature dependencies and link nothing but the shared include path;
`rte_string` is the one exception (it calls `rte_buffer_*` and
`rte_cast_*` functions directly) and links `safeapi::buffer` and
`safeapi::cast` publicly. This was already implicit in ADR-001 section 3.5's
"consumer can link the whole OAL or a single service" goal — this ADR is
what actually delivers it, down to individual-feature granularity rather
than whole-layer granularity.

### 2.3 What stays true from earlier ADRs despite the directory change

- ADR-002 section 4's "no OS dependency" rule for `rte_buffer` etc. is a
  statement about `#include`s and function calls, not about directory
  nesting - it still holds and is still checkable (no feature under
  `status/types/buffer/cast/safestate/string` includes or links against
  `timer/nvm/memory/task/ipc/log/reboot`).
- ADR-005's backend-registration pattern, ADR-003's checked-cast module,
  ADR-004's safe-state levels, and ADR-006's string/endian design are
  unaffected — this ADR only moves files and updates `#include` paths and
  build files accordingly; no function signature, status code, or behavior
  changed.

## 3. Consequences

- Positive: one directory per feature is easy to browse, easy to reason
  about in isolation, and easy to link independently (an integrator who
  only needs `rte_cast` and `rte_safestate` links exactly those two
  targets).
- Positive: adding a new feature is now a mechanical, well-understood
  three-folder-plus-one-CMakeLists pattern.
- Negative: every earlier ADR's "Location" section, written before this
  reorganization, still says `src/common/...`/`src/os/...` — those
  sections are **not** rewritten (ADRs are historical decision records);
  this ADR is the authoritative statement of the current physical layout,
  and each earlier ADR's Location section carries a one-line pointer here.
- Negative: `tests/CMakeLists.txt` and the top-level `CMakeLists.txt` both
  needed updating to the new per-feature target names
  (`safeapi::<feature>` instead of `safeapi::common`/`safeapi::os`) - done
  as part of this change, verified by a full rebuild and test run.

## 4. Location

This ADR describes the whole-repository layout; there is no single
"location" for it beyond the repository root itself.

# ADR-021: Consumer / OS-Backend Header Separation

Status: Accepted - applied to all 9 backend-bearing modules
Date: 2026-08-06
Applies to: safeAPIFreamwork, every OAL service with a backend vtable
(ADR-005): `timer`, `nvm`, `memory`, `task`, `ipc`, `log`, `reboot`,
`netlink`, `clocksync`.

## 1. Context

Feedback on the framework was that SAPI has become too complicated, and
the first concrete symptom named was header structure: every OAL service
(ADR-005) currently declares two very different audiences' APIs in one
file. Using `sapi_timer.h` as the representative example, a single
155-line header held both:

- **Consumer API** - `sapi_timer_create/start/stop/destroy/now()`, the
  opaque handle/storage/config types - what a real application (e.g.
  safeAPIExample's `channel_ab.c`) includes and calls.
- **Backend adaptation API** - `sapi_timer_backend_t` (the vtable) and
  `sapi_timer_register_backend()` - what a platform/RTOS integrator
  implements once, per target, and nobody else ever calls.

A consuming application has no reason to see the vtable shape at all; it
never populates one or calls the register function. Mixing the two in one
file makes the header read as more complex than the piece of it any given
reader actually needs, and makes it easy for consumer code to
(accidentally or not) reach into backend-only internals.

## 2. Decision

### 2.1 Two top-level include trees

- `include/safeapi/<feature>/sapi_<feature>.h` - consumer-facing only.
  Everything a real application includes and links against
  (`safeapi::<feature>`). Never declares a backend vtable type or a
  `_register_backend()` function.
- `include/safeapi_backend/<feature>/sapi_<feature>_backend.h` -
  backend-facing only. Declares the `sapi_<feature>_backend_t` vtable and
  `sapi_<feature>_register_backend()`. Includes the consumer header for
  the shared types the vtable's function pointers reference (e.g.
  `sapi_timer_storage_t`, `sapi_timer_config_t`). A real application
  should never need to include anything under `safeapi_backend/`; only
  the one piece of integration code that registers a concrete backend
  does.

A separate top-level tree (not a subfolder of `include/safeapi/<feature>/`)
was chosen over a same-directory `_backend.h` suffix so the two audiences
are physically, not just conventionally, separated - an integrator can
grep or browse `include/safeapi_backend/` to see exactly the surface they
need to implement, without any consumer-only material mixed in, and vice
versa. The cost is a second top-level directory and a second
`install(DIRECTORY ...)` rule per distributable tree; both are one-time,
structural costs, not ones that recur per feature added later.

### 2.2 What still lives where

- The `.c` file implementing a service (e.g. `src/timer/sapi_timer.c`)
  includes *both* headers: the consumer header (it implements the
  consumer API) and the backend header (it holds the single
  `static const sapi_<feature>_backend_t *s_backend` slot and validates/
  dispatches through it, per ADR-005 section 2.2). This is the one place
  in the framework itself that legitimately needs both.
- A concrete backend implementation (e.g. safeAPIExample's
  `sapi_posix_backend_timer.c`) includes only the backend header (via its
  own umbrella `sapi_posix_backend.h`) - it never needs the bulk of the
  consumer header's doc comments describing how an application should
  call the service, only the vtable shape it must fill in.
- Framework unit tests that register a mock backend (e.g.
  `tests/timer/test_sapi_timer.c`, `tests/watchdog/test_sapi_watchdog.c`)
  include both, the same as a real backend integrator would for its
  registration call plus any consumer calls the test also exercises.

### 2.3 Rollout: piloted on `sapi_timer`, then replicated to all 9

Given the scale of this change (9 modules, every downstream include),
`sapi_timer` was split first as a pilot and verified (framework unit
tests for timer/watchdog/log/dual-negotiator rebuilt and passed; the full
safeAPIExample 8-process demo rebuilt from source and re-run with
identical behavior to before the split). The same mechanical split -
move the vtable struct and `_register_backend()` declaration out, add the
new backend header, update every file that referenced the vtable type -
was then applied unchanged to the remaining eight: `nvm`, `memory`,
`task`, `ipc`, `log`, `reboot`, `netlink`, and `clocksync`. No per-module
wrinkle turned up (no service has more than one vtable; `sapi_clocksync`
was the only header where the backend material was interleaved with
consumer functions rather than trailing them, but the split was still
mechanical). Verified the same way as the pilot: every affected framework
unit test (16 total: status, buffer, cast, safestate, string, timer, nvm,
reboot, vital_channel, clocksync, checkpoint, netlink, watchdog,
appmanager, log, and the three `dual/` tests) rebuilt via manual `gcc`
and passed, and a full safeAPIExample rebuild + live 8-process WEST/EAST
demo run showed 0 errors and identical AGREE/checkpoint/negotiation
behavior to before the split.

## 3. Consequences

- Positive: a consuming application's transitive include graph no longer
  pulls in backend vtable shapes it never uses; browsing
  `include/safeapi/` answers "what can my application call," and
  `include/safeapi_backend/` answers "what must my platform integration
  implement," with no cross-contamination.
- Positive: makes the ADR-005 backend-registration pattern more visible
  as its own concern, not an implementation detail buried at the bottom
  of the consumer header.
- Negative: every existing `#include "safeapi/<feature>/sapi_<feature>.h"`
  in a file that ALSO uses the backend vtable now needs a second
  `#include "safeapi_backend/<feature>/sapi_<feature>_backend.h"` line -
  a one-time, mechanical, per-file update; a file that only calls the
  consumer API is unaffected.
- Negative: two `install(DIRECTORY ...)` rules and two logical install
  components (`development` vs `backend-development`) instead of one,
  though `target_include_directories` needs no change since both trees
  already live under the same `include/` root added to every target's
  public include path.

## 4. Status

All 9 backend-bearing modules are split as of this update:
`include/safeapi/<feature>/sapi_<feature>.h` (consumer) and
`include/safeapi_backend/<feature>/sapi_<feature>_backend.h` (backend)
exist side by side for `timer`, `nvm`, `memory`, `task`, `ipc`, `log`,
`reboot`, `netlink`, and `clocksync`. safeAPIExample's POSIX backend
(`include/safeapi/posix_backend/sapi_posix_backend.h`) includes both
headers for every service it implements.

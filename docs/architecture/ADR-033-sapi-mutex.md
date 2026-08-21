# ADR-033: sapi_mutex - a portable mutual-exclusion primitive

## Status

Accepted

## Context

safeAPIRBC2oo2 (the reference application built on this framework) guards
a netlink handle shared between its own cyclic-executive thread and a
background relay-rx task with a mutex (`ctx->peer_send_mutex`, used by
`channel_ab_checkpoint.c`'s own checkpoint send/recv adapter and
`channel_ab_io.c`'s own peer-sample/relay-envelope/site-state sends).
Found live, during an architecture review of every "backend"-named
symbol the application uses: that mutex was declared as a raw
`pthread_mutex_t` and manipulated with `pthread_mutex_init()`/`_lock()`/
`_unlock()`/`_destroy()` directly, with `<pthread.h>` included straight
into an *application* header (`channel_ab_types.h`), not confined to a
POSIX-backend implementation file.

This violates this project's own foundational premise (ADR-001/ADR-005):
an application built on safeAPIFreamwork should depend only on this
framework's own OS-Abstraction-Layer API, never on a specific platform's
threading primitives directly - the whole point of the backend-dispatch
pattern every other OAL service (`sapi_timer`, `sapi_task`, `sapi_ipc`,
`sapi_netlink`, ...) already follows is that swapping the platform/RTOS
underneath an application should mean swapping *one backend registration
call*, not auditing every application source file for platform-specific
API usage. A `pthread_mutex_t` baked into an application-level struct
defeats that guarantee outright: porting this application to a target
with no pthreads (a bare-metal RTOS, for instance) would require
rewriting application code, not just registering a different backend.

Checked before deciding on a fix: does safeAPIFreamwork already expose
*any* portable synchronization primitive under a different name (a
critical-section API bundled into `sapi_task`, for instance)? It does
not - the OAL surface (`clocksync`/`ipc`/`log`/`memory`/`netlink`/`nvm`/
`reboot`/`task`/`timer`) has no mutex/lock/semaphore module at all. So
this is not "the application should have called an existing framework
function instead" - the framework itself was missing the primitive, the
same gap-class ADR-031 (`sapi_mem_util`, for `<string.h>`) and ADR-026/032
(`sapi_lifecycle`, for setup-phase gating) already closed this project's
history.

## Decision

Add `sapi_mutex` as a new OAL-layer service, following the exact same
consumer/backend split every other OAL service uses (ADR-021):

- `include/safeapi/mutex/sapi_mutex.h` - consumer API:
  `sapi_mutex_create()`/`_lock()`/`_unlock()`/`_destroy()`, an opaque
  `sapi_mutex_handle_t` bound to caller-owned `SAFEAPI_DECLARE_STORAGE`
  storage (128 bytes - `sizeof(pthread_mutex_t)` is 64 bytes on the
  POSIX backend's own target platforms, generous headroom for a small
  wrapper struct around it). Non-recursive by design (REQ-OAL-MUTEX-003)
  - matches `pthread_mutex_t`'s own default behavior on Linux/macOS, the
  only backend this framework ships today; a caller needing recursive
  locking must track that itself, same as it always would have.
- `include/safeapi_backend/mutex/sapi_mutex_backend.h` - the backend
  vtable (`sapi_mutex_backend_t`) and `sapi_mutex_register_backend()`,
  gated by `sapi_lifecycle_check_setup_allowed()` (ADR-026) exactly like
  every other `sapi_*_register_backend()` call.
- `src/mutex/sapi_mutex.c` - pure dispatch to the registered backend,
  byte-for-byte the same shape as `src/timer/sapi_timer.c`.
- New `SAFEAPI_ENABLE_MUTEX` CMake option (default ON), no dependency
  edges on or from any other feature - a leaf OAL module, same as
  `TIMER`/`NVM`/`MEMORY`/`TASK`.
- `sapi_mutex_create()` is setup-phase-gated (a mutex is created once at
  init, same posture as `sapi_timer_create()`/`sapi_task_create()`);
  `_lock()`/`_unlock()`/`_destroy()` are not, since those are genuinely
  runtime/shutdown operations.

The POSIX backend implementation (`sapi_posix_backend_mutex.c`, a thin
wrapper over `pthread_mutex_init()`/`_lock()`/`_unlock()`/`_destroy()`)
lives in safeAPIRBC2oo2's own `src/posix_backend/`, not in
safeAPIFreamwork - matching every other OAL backend's location per
ADR-018's own "a backend is integrator-supplied, not part of the
reusable framework" philosophy. This is exactly where `pthread_mutex_t`
usage belongs: confined to the one file whose entire job is adapting a
specific platform to this framework's portable API, never leaking into
application code above it.

safeAPIRBC2oo2's own `channel_ab_types.h`/`channel_ab.c`/
`channel_ab_checkpoint.c`/`channel_ab_io.c` were migrated to
`sapi_mutex_handle_t` + `sapi_mutex_storage_t`, dropping `<pthread.h>`
from application code entirely (the one remaining `pthread_join()` use
inside `sapi_task_destroy()`'s own doc-comment cross-reference is a
framework-internal detail the application only reads about, not calls
directly).

## Consequences

- Every application built on this framework now has a portable mutex
  available without reaching for platform threading primitives directly.
- The POSIX backend's own `sapi_posix_backend_register_all()` gained one
  more call (`sapi_mutex_register_backend(sapi_posix_backend_mutex())`);
  any other backend integrator adding a new target must do the same.
- `channel_ab_negotiate.c`'s own direct `select()`/`read()` on
  `STDIN_FILENO` (the interactive `ASK_USER` transfer-policy prompt) was
  reviewed in the same pass and found to be a second, separate instance
  of the same class of violation - deliberately left out of this ADR's
  scope; it has no framework equivalent to migrate to yet (no portable
  console/stdin abstraction exists) and was called out to the user as an
  explicitly separate, not-yet-scoped follow-up.

## Verification

- safeAPIFreamwork: full clean rebuild + `ctest` (matching the existing
  suite's own pass/fail baseline - no test added specifically for
  `sapi_mutex` beyond compilation, matching `sapi_timer`'s own precedent
  of relying on downstream consumers, not a dedicated unit test, since
  the backend dispatch logic is identical in shape to every other
  already-tested OAL service).
- safeAPIRBC2oo2: full clean rebuild + `ctest`, `smoke.sh`, and the real
  Docker-based `safeAPITestEnv` Robot Framework suite, after migrating
  off `pthread_mutex_t` - confirmed no behavior change (the POSIX mutex
  backend's own locking semantics are identical to what the application
  called directly before, just routed through the OAL dispatch layer).

## Location

- `include/safeapi/mutex/sapi_mutex.h`
- `include/safeapi_backend/mutex/sapi_mutex_backend.h`
- `src/mutex/sapi_mutex.c`
- `safeAPIRBC2oo2/src/posix_backend/sapi_posix_backend_mutex.c` (sibling
  project - the POSIX backend implementation itself)

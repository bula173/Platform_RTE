# ADR-035: `sapi_platform` OAL service for real-time bring-up

## Status

Accepted

## Context

`safeAPIRBC2oo2GP`'s process startup (`app_main_common.c`, both the AB and
C variants) called `sapi_posix_backend_init_realtime(80U)` directly - a
symbol exported by `safeAPIBackendPosix`, not by `safeAPIFreamwork`. That
is a layering violation: ADR-001 §3.5 and ADR-005 require application code
to reach the OS *only* through the framework's OAL API, with the single
exception of the composition-root wiring that registers a concrete backend
(`sapi_*_register_backend()`). `sapi_posix_backend_init_realtime()` is not
registration - it is behaviour (POSIX `mlockall()` + `SCHED_FIFO`) the
application was invoking by reaching around the framework, and there was
no OAL API for it to use instead.

It also hid a real bug. The POSIX implementation called
`mlockall(MCL_CURRENT | MCL_FUTURE)` and treated only *failure* of that
call as noteworthy. On a host with a bounded `RLIMIT_MEMLOCK` (a stock
Ubuntu VM: 8 MiB soft/hard cap) the call *succeeds*, and `MCL_FUTURE` then
forces every subsequent mapping - including each new 8 MiB thread stack -
to be locked. The next `pthread_create()` fails with `EAGAIN`, so
`sapi_timer`'s backend cannot start its cyclic-timer thread,
`sapi_appmanager` init fails with `INTERNAL_ERROR`, and every RBC process
exits at startup. 13 of 16 end-to-end scenario tests failed as a result,
all with the same downstream symptom (no RBC indications ever produced).

## Decision

### 1. New OAL service `sapi_platform` (framework)

A minimal validate-then-dispatch service in the same shape as
`sapi_reboot` (ADR-004/005) and split consumer/backend headers per
ADR-021:

- `include/safeapi/oal/platform/sapi_platform.h` - consumer surface:
  `sapi_status_t sapi_platform_realtime_init(uint32_t rt_priority);`
- `include/safeapi_backend/platform/sapi_platform_backend.h` - vtable
  `sapi_platform_backend_t { sapi_status_t (*realtime_init)(uint32_t); }`
  and `sapi_platform_register_backend()`.
- `src/oal/platform/sapi_platform.c` - range-checks `rt_priority` (0..99),
  then dispatches; `NOT_INITIALIZED` if no backend, `NOT_SUPPORTED` if the
  vtable slot is `NULL`. Registration is setup-phase-gated (ADR-026), the
  same as every other `*_register_backend()`.
- Gated by `SAFEAPI_ENABLE_PLATFORM` (default ON, ADR-024), depends only
  on CORE.

The name is `platform`, not `realtime`, so the one service can absorb
other one-shot platform bring-up concerns later without another ADR; the
only operation today is `realtime_init`.

`realtime_init` is defined as **best-effort** (REQ-OAL-PLATFORM-001): a
backend that cannot get RT scheduling or full memory residency still
returns `SAPI_STATUS_OK` - and, critically, must not leave the process
unable to create threads afterwards. That last clause is what the POSIX
backend's old code violated.

### 2. POSIX implementation moves behind the vtable (`safeAPIBackendPosix`)

- New `src/sapi_posix_backend_platform.c`: the `mlockall` / stack
  pre-fault / `SCHED_FIFO` body moves here from
  `sapi_posix_backend.c::sapi_posix_backend_init_realtime()`, as the
  `realtime_init` vtable function, plus a `sapi_posix_backend_platform()`
  accessor (matching `sapi_posix_backend_timer()` etc.).
- `sapi_posix_backend_register_all()` now also registers this backend.
- The old public `sapi_posix_backend_init_realtime()` is **removed** - it
  had exactly one caller (GP), now migrated.

**The `RLIMIT_MEMLOCK` fix**: before `mlockall`, the backend now calls
`getrlimit(RLIMIT_MEMLOCK)`. If the limit is not `RLIM_INFINITY`, it locks
with `MCL_CURRENT` only (never `MCL_FUTURE`) and logs a WARNING explaining
why. A privileged / `memlock=unlimited` deployment is unaffected and still
gets `MCL_CURRENT | MCL_FUTURE`; an unprivileged dev/CI host keeps its
current footprint resident but no longer poisons later thread creation.

### 3. GP calls the framework

`app_main_common.c` (AB and C) replaces
`sapi_posix_backend_init_realtime(80U)` with
`sapi_platform_realtime_init(80U)` and includes
`safeapi/oal/platform/sapi_platform.h`. It still includes
`sapi_posix_backend.h` for `sapi_posix_backend_register_all()` and
`sapi_posix_backend_reboot_set_argv()` - those are the sanctioned
composition-root wiring; the RT call no longer is.

## Consequences

- Application code no longer invokes any `safeAPIBackendPosix` *behaviour*
  symbol - only the registration seam. `sapi_posix_backend_channel_service_register_resolver()`
  (in `gateway_c.c` / `ab_gp_channel.c`) and `sapi_posix_backend_reboot_set_argv()`
  are the remaining direct backend touches; folding those behind
  framework seams is left as follow-up work, out of scope here.
- One more OAL service to keep in build/test/install wiring. The
  `install(DIRECTORY include/safeapi_backend ...)` rule already copies the
  whole tree, so no install change was needed.
- The bounded-`RLIMIT_MEMLOCK` path trades some determinism (future
  allocations may fault) for the process actually being able to run. This
  is the correct trade on an unprivileged host, which is not an RT target
  anyway; a real RT deployment runs with `memlock` unlimited and gets the
  full behaviour. A future refinement could additionally cap timer/task
  thread stack sizes so `MCL_FUTURE` stays viable under a bounded limit -
  not done here.
- Backends other than POSIX (a future `safeAPIBackendQNX`, bare-metal
  FreeRTOS) can leave `realtime_init` as `NULL` and callers transparently
  get `SAPI_STATUS_NOT_SUPPORTED`, which `app_main_common.c` already
  ignores (the call is `(void)`-cast).

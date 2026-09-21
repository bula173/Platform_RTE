# ADR-005: OAL Backend Registration via Callbacks

> **Terminology update (2026-09):** "backend" is now called **OSAdapter**. `rte_<service>_backend_t` is `rte_osadapter_<service>_t`,
> `rte_<service>_register_backend()` is `rte_osadapter_<service>_register()`, headers moved from `safeapi_backend/` to
> `safeapi_osadapter/`, and the POSIX implementation is `rte_posix_osadapter_*` (`Platform_OS_POSIX`). The text below keeps the
> original wording as a historical record.

Status: Draft
Date: 2026-08-02
Applies to: safeAPIFreamwork, all `include/safeapi/os/*.h` services

## 1. Context

ADR-001 section 3.5 anticipated this decision but deferred it: "Each
service's implementation is reached through a function-pointer table
... selected at build/link time ... This ADR only defines the public API;
backend selection/registration is left to a follow-up ADR." Until now, the
only "implementation" of any OAL service was the stub `.c` file shipped
with the framework itself (always returning `RTE_STATUS_NOT_IMPLEMENTED`),
which meant an integrator's only option was to edit those files directly -
not swappable, not testable with a mock backend, and every target
(POSIX dev host, an RTOS, bare metal) would fight over the same files.

This ADR resolves that open item: **runtime callback registration**, the
same pattern already used for `rte_safestate`'s per-level handlers
(ADR-004 section 2.2), applied uniformly to all seven OAL services (timer,
NVM, memory, task, IPC, log, reboot).

## 2. Decision

### 2.1 One backend vtable type and one registration function per service

Each service defines a struct of function pointers matching its
operations, and a single registration entry point:

```c
typedef struct rte_osadapter_timer_s
{
    rte_status_t (*create)(rte_timer_storage_t *storage,
                             const rte_timer_config_t *config,
                             rte_timer_handle_t *out_handle);
    rte_status_t (*start)(rte_timer_handle_t handle);
    rte_status_t (*stop)(rte_timer_handle_t handle);
    rte_status_t (*destroy)(rte_timer_handle_t handle);
    rte_status_t (*now)(rte_timestamp_ms_t *out_now_ms);
} rte_osadapter_timer_t;

rte_status_t rte_osadapter_timer_register(const rte_osadapter_timer_t *backend);
```

An integrator provides their own implementation by populating a
`static const rte_osadapter_timer_t my_posix_timer_backend = { ... };` and
calling `rte_osadapter_timer_register(&my_posix_timer_backend)` during
system startup, before any other `rte_timer_*` call. This is the answer
to "how does a user provide their own implementation": no framework source
file needs editing, and multiple backends (POSIX for host-side testing, an
RTOS backend for target hardware) can live side by side in an integrator's
codebase, selected by which one gets registered.

One global slot per service (a single `static const rte_*_backend_t
*s_backend` in each service's `.c` file), not one per handle/instance -
consistent with there being exactly one real OS underneath a given build,
mirroring the one-handler-per-level design already used for
`rte_safestate`. Registering again replaces the previous backend (no
error on re-registration), matching the same choice made for
`rte_safestate_register_handler`.

### 2.2 The framework still validates; the backend only executes

Every public API function keeps its existing defensive parameter
validation (null checks, config sanity checks) *before* touching the
backend, and only then dispatches:

```c
rte_status_t rte_timer_create(rte_timer_storage_t *storage,
                                 const rte_timer_config_t *config,
                                 rte_timer_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->callback == NULL) || (config->period_ms == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, config, out_handle);
}
```

This means a backend author only has to implement the actual mechanism
correctly for already-validated inputs - it never has to re-derive the
framework's own precondition checks, and a backend that only implements a
subset of a service's optional operations can leave the corresponding
vtable slot `NULL` rather than providing a dummy function.

### 2.3 Status code semantics change slightly

- `RTE_STATUS_NOT_INITIALIZED`: no backend has been registered for this
  service yet. Previously unused by the OAL services (only by
  handle-lifecycle checks); now also means "you forgot to call
  `rte_<service>_register_backend()` at startup."
- `RTE_STATUS_NOT_SUPPORTED`: a backend **is** registered, but the
  specific vtable slot for this operation is `NULL` - the backend
  implements the service but not this particular call.
- `RTE_STATUS_NOT_IMPLEMENTED`: no longer produced by any OAL service
  (it was the placeholder stub's blanket answer). The value stays in
  `rte_status_t` (status codes are append-only, ADR-001 section 3.6) but
  is no longer emitted by framework code; it remains available for a
  backend author to return from its own vtable function if that specific
  meaning is useful to them.

### 2.4 Why this isn't a virtual-dispatch / C++ redesign

This is still a pure C ABI (ADR-001 section 3.1): a vtable here is an
ordinary `struct` of function pointers, not a C++ `vtable`/virtual class.
No dynamic allocation is introduced - the vtable itself is a `static
const` struct the integrator defines (typically at file scope), and the
single backend-pointer slot per service is a `static` variable inside the
framework, exactly like `rte_safestate`'s handler array.

### 2.5 `rte_log` and `rte_reboot`

`rte_log` gets the same treatment for consistency even though its default
(no backend registered) behavior is identical to today's stub: writes are
silently dropped (matches REQ-OAL-LOG-001's "never blocks or fails the
caller" requirement regardless of whether a backend exists yet).
`rte_reboot` (new in ADR-004 section 3) is built with this pattern from
the start - no separate stub-then-retrofit step.

## 3. Consequences

- Positive: resolves the open item from ADR-001 section 7 and ADR-004
  section 2.3/4; integrators never edit framework source; multiple
  backends can coexist and be swapped by changing which one is registered;
  unit tests can register a mock backend to exercise the dispatch path
  without any real OS underneath.
- Positive: parameter validation is centralized in the framework and
  cannot be skipped by a careless backend implementation.
- Negative: an extra indirect call per operation (one function-pointer
  dispatch) versus a direct call - negligible next to actual OS-call
  latency (timers, NVM I/O, IPC) for every service this applies to.
- Negative: every existing OAL test that asserted `RTE_STATUS_NOT_IMPLEMENTED`
  for valid-parameter cases now asserts `RTE_STATUS_NOT_INITIALIZED`
  instead (no backend registered in the test); tests that want to exercise
  the dispatch path register a small mock backend first.

## 4. Location

> **Superseded by ADR-007.** See below for the original paths; each OAL
> service's backend type/registration function now lives in its own
> per-feature header (`include/safeapi/<feature>/rte_<feature>.h`) and
> `.c` file (`src/<feature>/rte_<feature>.c`), not a shared `os/` folder.

Every `include/safeapi/os/*.h` gains a `rte_<service>_backend_t` type and
a `rte_<service>_register_backend()` declaration; every
`src/os/rte_*.c` is rewritten to validate-then-dispatch instead of always
returning `RTE_STATUS_NOT_IMPLEMENTED`.

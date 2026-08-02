# ADR-001: OS Abstraction Layer (OAL) for the Safe API Framework

Status: Draft
Date: 2026-08-02
Applies to: safeAPIFreamwork, first abstraction layer (RBC core <-> Operating System)

## 1. Context

The goal of this project is a universal API that lets independently developed
layers of a safety-related application communicate through well-defined
software interfaces, so each layer can be developed, verified and (where
required) certified in isolation. The reference application used to drive the
design is an ERTMS Radio Block Centre (RBC), a SIL 4 function under
EN 50129 / EN 50128 (CENELEC).

This ADR defines the first abstraction layer: the boundary between the RBC
core (the ERTMS application logic) and the underlying operating system /
board support package. The RBC core must never call OS or vendor APIs
directly; it calls only this layer. This keeps the safety-related core
portable across OS/hardware targets and keeps the OS-facing code isolated so
it can be qualified or replaced independently.

## 2. Layered architecture

```
+-----------------------------------------------------+
| L2  Application Layer  (RBC core, ERTMS functions)   |
+-----------------------------------------------------+
| L1  Safety Communication Layer (future ADR)          |
|     inter-layer / inter-process safety-related        |
|     messaging, EN 50159 style defenses                |
+-----------------------------------------------------+
| L0  OS Abstraction Layer (OAL)   <-- this ADR         |
|     timers, NVM, memory reservation, task/thread      |
|     scheduling, IPC, logging/diagnostics               |
+-----------------------------------------------------+
| OS / RTOS / BSP  (Linux, QNX, VxWorks, bare-metal...)|
+-----------------------------------------------------+
```

Only L0 (OAL) is scoped for this ADR. L1 is referenced so that OAL's IPC
primitives are designed to be usable as a transport underneath it, but L1's
message-level safety mechanisms (sequence numbers, timeouts, CRC/ authentication,
per EN 50159) are out of scope here.

## 3. Decision

### 3.1 Interface style: pure C ABI

All OAL interfaces are exposed as a **pure C ABI** (`extern "C"`), not C++
classes or virtual interfaces.

Rationale:
- Matches the project's C/C++ stack while remaining callable from either.
- Avoids vtable dispatch, which complicates WCET analysis and is restricted
  or banned outright by some SIL 3/4 coding guidelines and qualified
  toolchains.
- Function-pointer tables (see 3.5) give us swappable backends without C++
  runtime polymorphism.
- Easiest form to bind to other qualified languages/tools if ever required.

### 3.2 Target integrity level: SIL 3/4

The OAL is designed to the constraints implied by EN 50128 SIL 3/4 software,
since the RBC core it serves is SIL 4:

- MISRA C:2012 compliant source (mandatory/required rules; documented
  deviations only where justified).
- **No dynamic memory allocation after initialization.** Callers provide
  storage for every handle (see 3.4); the OAL never calls `malloc`/`free`
  internally past init.
- Deterministic, bounded execution time for every API call (no unbounded
  loops, no recursion).
- Every function validates its parameters and returns an explicit status
  code; no exceptions, no `errno`-style side channels.
- Every public API element carries a requirement-ID tag (`REQ-OAL-...`) in
  its doxygen comment for traceability to a future requirements
  specification, per EN 50128 verification/traceability expectations.
- Safety-related and non-safety-related code are kept in separate
  translation units/services (e.g. `sapi_log` is diagnostic/non-safety,
  clearly marked as such).

### 3.3 Naming conventions

- Public functions/types: `sapi_<service>_<verb>`, e.g. `sapi_timer_create`.
- Public macros/constants: `SAPI_<SERVICE>_<NAME>`.
- Status/error enum: `sapi_status_t`, values `SAPI_STATUS_*`.
- Each service gets one public header under `include/safeapi/os/`.
- Header guards: `SAFEAPI_OS_<SERVICE>_H`.

### 3.4 Handle pattern (no dynamic allocation)

Every stateful OAL object (timer, NVM region, memory pool, task, IPC
channel) uses a **caller-owned static storage** pattern instead of
heap allocation:

```c
sapi_timer_storage_t   timer_storage;   /* caller-owned, e.g. static or on a
                                            long-lived stack frame */
sapi_timer_handle_t    timer;           /* opaque handle bound to storage */

sapi_status_t st = sapi_timer_create(&timer_storage, &config, &timer);
```

`sapi_timer_storage_t` is an opaque, fixed-size, aligned byte buffer defined
in the public header (size is part of the ABI). The implementation places
its internal state inside that buffer. This gives static, analyzable memory
usage while keeping the internal layout hidden from callers.

### 3.5 Backend indirection

Each service's implementation is reached through a function-pointer table
(`sapi_<service>_backend_t`) selected at build/link time (e.g. POSIX
backend, FreeRTOS backend, vendor BSP backend). This is the C-ABI-compatible
equivalent of a strategy pattern, chosen instead of C++ virtual dispatch for
the reasons in 3.1. This ADR only defines the public API; backend
selection/registration is left to a follow-up ADR.

### 3.6 Error handling

All fallible functions return `sapi_status_t`. Common codes (defined once in
`sapi_status.h` and shared by every service):

| Code | Meaning |
|---|---|
| `SAPI_STATUS_OK` | success |
| `SAPI_STATUS_INVALID_PARAM` | null pointer / out-of-range argument |
| `SAPI_STATUS_NOT_INITIALIZED` | used before `_create`/`_init` |
| `SAPI_STATUS_ALREADY_INITIALIZED` | double init |
| `SAPI_STATUS_TIMEOUT` | blocking call exceeded its deadline |
| `SAPI_STATUS_RESOURCE_EXHAUSTED` | static pool/storage full |
| `SAPI_STATUS_NOT_SUPPORTED` | valid request, backend can't do it |
| `SAPI_STATUS_NOT_IMPLEMENTED` | stub only (current skeleton state) |
| `SAPI_STATUS_HARDWARE_FAULT` | backend reported a HW-level fault |
| `SAPI_STATUS_DATA_CORRUPTION` | NVM integrity check (CRC) failed |
| `SAPI_STATUS_INTERNAL_ERROR` | should-never-happen / defensive catch-all |

## 4. Scope of the first abstraction layer (OAL services)

Six services, one header each:

1. **Timer** (`sapi_timer.h`) — periodic/one-shot timers with
   millisecond-resolution deadlines, needed for ERTMS movement authority
   timeouts, cyclic supervision, watchdog-style deadlines.
2. **NVM** (`sapi_nvm.h`) — non-volatile storage of safety-related persistent
   data (e.g. train/track database state) with integrity checking
   (CRC/redundant storage) on read.
3. **Memory reservation** (`sapi_memory.h`) — static memory pool
   reservation/partitioning at init time; no `malloc` in the safety path.
4. **Task/thread scheduling** (`sapi_task.h`) — creation of periodic/cyclic
   safety tasks with fixed priorities, matching the cyclic processing model
   typical of RBC implementations.
5. **Inter-process/inter-task communication** (`sapi_ipc.h`) — bounded
   message queues/channels between safety tasks, usable as the transport for
   a future L1 safety communication layer.
6. **Logging/diagnostics** (`sapi_log.h`) — explicitly **non-safety-related**
   black-box-style event logging; must never be on any safety execution
   path (e.g. must not block or fail a caller).

## 5. Directory / build structure

> **Superseded by ADR-007.** The `include/safeapi/common/` /
> `include/safeapi/os/` (and matching `src/`/`tests/`) grouping described
> below was the original layout. ADR-007 replaced it with one directory
> per feature (`include/safeapi/timer/`, `src/timer/`, `tests/timer/`,
> etc.), dropping the `common`/`os` grouping folder while keeping the
> conceptual distinction it represented (see ADR-007 section 2.1). Left
> here unmodified as the historical record of this decision.

```
safeAPIFreamwork/
  CMakeLists.txt                 top-level, options, adds subdirs
  cmake/CompilerWarnings.cmake   shared warning/hardening flags
  docs/architecture/             ADRs (this file)
  include/safeapi/common/        sapi_status.h, sapi_types.h
  include/safeapi/os/            the six service headers
  src/os/                        stub backend implementations + CMakeLists.txt
  tests/                         CTest scaffold, one test file per service
```

Each service is built as its own CMake object/static library target under a
top-level `safeapi_os` interface library, so a consumer can link the whole
OAL or a single service.

## 6. Consequences

- Positive: portable RBC core, independent V&V of the OAL, no heap
  fragmentation/non-determinism risk, straightforward MISRA-C audit surface,
  swappable backends per target OS.
- Negative: caller-owned storage pattern pushes sizing knowledge to call
  sites (mitigated by `sizeof`-based static asserts in headers); pure C ABI
  means no compile-time generics/templates for convenience wrappers (a thin
  optional C++ RAII wrapper can be layered on top later without touching the
  ABI).
- Deferred to follow-up ADRs: backend registration/build selection, L1
  safety communication layer design, formal requirements specification and
  MISRA deviation register, unit test framework selection for SIL 3/4
  qualification evidence.

## 7. Open items

- Confirm target OS/RTOS backends for the first reference implementation
  (POSIX/Linux is the natural first backend for host-based development and
  testing; a deterministic RTOS backend would follow for target hardware).
- Define `sapi_timer_storage_t` etc. sizes once a first backend fixes real
  internal state layout (skeleton currently uses a conservative fixed size
  plus a compile-time size assertion hook).
- Decide unit test framework (Unity/CMock are common choices for MISRA-C
  SIL 3/4 projects) — current skeleton uses a minimal assert-based harness
  under CTest as a placeholder.

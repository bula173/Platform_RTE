# Safe API Framework

A layered C/C++ API framework for building safety-related applications whose
layers communicate through well-defined software interfaces, so each layer
can be developed and verified independently. The reference application
driving the design is an ERTMS Radio Block Centre (RBC), a SIL 4 function
under CENELEC EN 50128 / EN 50129.

Applications never call OS/system APIs directly - every service is reached
through SAPI, so an application can be retargeted to a different OS/RTOS by
registering a different backend, without touching application code.

## Status

Early skeleton, actively growing. The first abstraction layer — the
**OS Abstraction Layer (OAL)**, sitting between the RBC core and the
operating system — is defined as a pure C ABI. See
`docs/architecture/` for the full set of ADRs (design rationale for every
decision below); `docs/requirements/SRS.md` for the consolidated
requirements specification; `docs/MISRA_COMPLIANCE_REPORT.md` for the
current MISRA C:2012 conformance status.

OAL services, each in its own `include/safeapi/<feature>/` +
`src/<feature>/` directory (ADR-007): timer, non-volatile memory (NVM),
static memory reservation, task/thread scheduling, inter-process/inter-task
communication (IPC), logging/diagnostics, and controlled reboot.

Common, layer-agnostic facilities, same per-feature layout: status codes
(`status`) and fixed-width types (`types`), the cross-layer data buffer view
with endianness-safe multi-byte access (`buffer`), checked integer casting
between every fixed-width type and `size_t` (`cast`), safe-state transitions
/ checked assertions (`safestate`: `SAPI_ASSERT`, `SAPI_SAFESTATE`,
`SAPI_REBOOT`), and bounded string manipulation replacing strcpy/strcat/
sprintf/atoi/strtok (`string`).

Every OAL service is reached through a **backend registered at startup**
(`sapi_<service>_register_backend()`) rather than a hardcoded
implementation — this is how an integrator supplies their own
implementation (POSIX for host-side dev/test, an RTOS backend for target
hardware) without editing framework source. See
`docs/architecture/ADR-005-oal-backend-registration.md`.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Layout

```
CMakeLists.txt                 top-level build
cmake/CompilerWarnings.cmake   shared warning/hardening flags
docs/architecture/             architecture decision records (ADRs)
docs/requirements/              consolidated requirements specification (SRS)
docs/MISRA_COMPLIANCE_REPORT.md MISRA C:2012 conformance status
include/safeapi/<feature>/     one public header per feature (ADR-007):
                                status, types, buffer, cast, safestate,
                                string, timer, nvm, memory, task, ipc, log,
                                reboot
src/<feature>/                 matching implementation + CMakeLists.txt,
                                one static library target safeapi::<feature>
tests/<feature>/                matching CTest test file per feature
```

## Design principles

- Pure C ABI (`extern "C"`) for every public interface.
- No dynamic memory allocation after initialization — callers own storage.
- Every fallible call returns an explicit `sapi_status_t`; no exceptions.
- MISRA C:2012-oriented source, aimed at SIL 3/4 (EN 50128) constraints.
- Every conversion between integer types goes through a checked `sapi_cast_*`
  function — no bare C-style casts.
- Requirement-ID tags (`REQ-...`) on public API elements for traceability,
  consolidated in `docs/requirements/SRS.md`.
- All public headers documented in Doxygen format (`@file`/`@brief`/
  `@param`/`@return`); see `Doxyfile` to generate HTML docs locally.

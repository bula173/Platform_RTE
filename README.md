# Platform_RTE

**Product:** Platform_RTE, the Safe Computing Platform core / RTE of the RBC_Template workspace
(repository `safeAPIFreamwork`). C99, MISRA C:2012, EN 50128 SIL 4 target.
**Layer:** RTE. **Assessment unit:** Platform core (part of the RTE composition, see
[../../docs/safety/CERTIFICATION_PLAN.md](../../docs/safety/CERTIFICATION_PLAN.md)).

## Responsibility

Platform_RTE is the only layer through which safety applications reach the OS, the network and each
other. It owns:

- **Execution:** the cyclic application manager (init, execute, shutdown, optional pre/post hooks and a
  built-in checkpoint stage) and the setup-phase lock.
- **Redundancy:** N-way voting, 2-way cross-comparison, checkpoint rendezvous, dual-instance
  ONLINE/STANDBY negotiation, state transfer on promotion, redundancy configuration.
- **Safe state:** the single place where a transition to SAFE or REBOOT happens. Applications only
  register a cleanup handler.
- **Communication seam:** named channels and Flows over pluggable OSAdapters, checksums and the
  vital-message envelope.
- **OS abstraction (OAL):** timer, NVM, memory, task, mutex, IPC, log, reboot, netlink, platform
  and clock sync, each reached through an OSAdapter registered at startup (ADR-005).
- **Common utilities:** status codes, fixed-width types, checked casts, bounded strings, buffer views.

It does **not** implement any operating-system OSAdapter (Platform_OS_POSIX), any transport protocol
(Platform_Protocol_*), any railway logic (RBC_*), or any ERTMS message handling.

## Interfaces exposed to other products

| Interface | Header directory | Consumed by | Notes |
|---|---|---|---|
| **PI-API** (`rte_flow_*`) | `include/safeapi/oal/flow/` | RBC_GP, RBC_GA | Name-addressed publish/subscribe endpoint shaped after OCORA's `flows.h`; the intended public API of the platform |
| **Application manager** | `include/safeapi/app/appmanager/` | RBC_GP, RBC_GA | Lifecycle, stages, cycle hooks, checkpoint result hook |
| **Safe state** | `include/safeapi/utils/safestate/` | all | `RTE_ASSERT`, `RTE_SAFESTATE`, `RTE_REBOOT`, cleanup handler registration |
| **Redundancy services** | `include/safeapi/redundancy/{voter,cross_comparator,checkpoint,channel_link,dual,safechannel,checksum,watchdog}/` | RBC_GP | Used directly today; narrowing to the PI-API is tracked in the root `TODO.md` |
| **Channel service** | `include/safeapi/redundancy/channel_service/` | RBC_GP, gateways | Named channel setup, read, send, close; includes the Flow-backed variant |
| **Redundancy configuration** | `include/safeapi/redundancy/config/` | integrator | Loads a JSON file (topology, replicas, quorum, `standby_mode`, roles, channel definitions) and calls the application's registered capability callback |
| **State registration** | `include/safeapi/redundancy/state_transfer/` | integrator | Application lists the fields that must survive a promotion |
| **OSAdapter seams** | `include/safeapi/oal/<service>/*_osadapter.h`, `oal/flow`, `oal/protocol` | Platform_OS_POSIX, Platform_Protocol_DDS | One vtable per OAL service, `rte_osadapter_flow_t`, `rte_protocol_adapter_ops_t` |
| **Utilities** | `include/safeapi/utils/` | all | Status, types, cast, buffer, string |
| **Build artifacts** | `dist/<platform>/`, CMake package `safeAPIFramework`, Conan `safeapiframework/0.1.0` | all C products | Targets `safeapi::core`, `::oal`, `::channels`, `::appmanager` |

Every public function has a Doxygen block with pre-conditions, post-conditions and requirement IDs.
See [docs/DOCUMENTATION_INDEX.md](docs/DOCUMENTATION_INDEX.md) for the per-module guides.

## Interfaces required

Only the C standard library and, at run time, one registered OSAdapter per OAL service used. A build
that links no OSAdapter cannot run the services; `Platform_OS_POSIX` is the reference OSAdapter.

## Build and verify

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
cmake --preset ci                        # warnings as errors
cmake --preset asan | ubsan | coverage   # sanitizers and coverage
cmake --build build --target cppcheck    # MISRA C:2012 analysis (or ./scripts/run-cppcheck.sh)
cmake --install build --prefix .         # produces dist/<platform>/ for consumers
```

Cross-compilation targets and toolchain files: [docs/CROSS_COMPILATION.md](docs/CROSS_COMPILATION.md)
and `platform/toolchains/`. Coding rules: [CLAUDE.md](CLAUDE.md). Conformance evidence:
[docs/MISRA_COMPLIANCE_REPORT.md](docs/MISRA_COMPLIANCE_REPORT.md),
[docs/COVERAGE_REPORT.md](docs/COVERAGE_REPORT.md), [docs/EN_50128_ALIGNMENT.md](docs/EN_50128_ALIGNMENT.md),
[docs/SAFETY_APPLICATION_CONDITIONS.md](docs/SAFETY_APPLICATION_CONDITIONS.md).

## Layout

```text
include/safeapi/<area>/<feature>/   public header per feature (ADR-007)
src/<feature>/                      implementation, built into 4 library targets (ADR-023)
tests/<feature>/                    one CTest file per feature
docs/                               guides, ADRs, requirements (SRS), safety, templates
examples/                           standalone example programs
```

## Where to read next

- Module status and feature notes: [docs/MODULES.md](docs/MODULES.md)
- Architecture: [docs/architecture/SYSTEM_OVERVIEW.md](docs/architecture/SYSTEM_OVERVIEW.md) and the ADRs
- Workspace context: [../../docs/architecture/ARCHITECTURE.md](../../docs/architecture/ARCHITECTURE.md)

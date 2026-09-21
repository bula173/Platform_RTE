/**
 * @page system_architecture System Architecture Overview
 *
 * @section system_architecture_layers Layering
 *
 * The framework is organized in three layers. Applications only ever call
 * into Platform_RTE - never directly into the OS/RTOS - so retargeting to a
 * different platform means registering a different OSAdapter (ADR-005), not
 * rewriting application code.
 *
 * -# **OS Abstraction Layer (OAL)** - one service per OS/RTOS primitive:
 *    @ref TIMER, @ref NVM, @ref MEMORY, @ref TASK, @ref IPC, @ref NETLINK,
 *    @ref LOG, @ref REBOOT. Every service validates parameters, then
 *    dispatches to an integrator-supplied OSAdapter (ADR-005); the framework
 *    itself ships no OS-specific code.
 * -# **Common utilities** - layer-agnostic building blocks with no OS
 *    dependency: @ref BUFFER, @ref STRING, @ref CAST, @ref TYPES,
 *    @ref STATUS, @ref SAFESTATE.
 * -# **Redundancy / vital communication** - built on top of the first two
 *    layers: @ref channel_link (one point-to-point redundant link),
 *    @ref voter (N-way 2oo2/2oo3/NMR voting across registered
 *    @ref channel_link instances), @ref cross_comparator (2-way
 *    consistency check between two independent peer channels, ADR-025),
 *    @ref CHECKPOINT (bounded checkpoint-ID rendezvous for cross-node
 *    agreement), @ref CLOCKSYNC (diagnostic-only wall-clock offset,
 *    never a correctness dependency), @ref CHECKSUM (CRC-64 data
 *    integrity), and @ref WATCHDOG (hang detection and recovery,
 *    including RTE_WATCHDOG_ACTION_FAILOVER for redundant-peer-loss
 *    reactions).
 *
 * @ref APPMANAGER sits above all three layers, giving an application a
 * single init/execute/shutdown lifecycle entry point.
 *
 * @section system_architecture_ccf Common-Cause-Failure Mitigation
 *
 * For SIL 3/4 dual-channel deployments, ADR-008 covers build-diversity
 * mitigation (two independently-configured toolchains compiling channel A
 * and channel B); @ref cross_comparator (ADR-025) is the framework
 * primitive that answers "do channel A's and channel B's results agree?"
 * for exactly that kind of independent peer pair - it supersedes the
 * ADR-008-era standalone comparator module, which was never built and has
 * since been removed outright.
 *
 * @section system_architecture_docs Where to Go Next
 *
 * - Getting started integrating an OSAdapter: @ref appmanager_integration
 * - Per-module architecture and user guides: see the "Related Pages" list
 *   (each module under `include/safeapi/<module>/` ships an
 *   ARCHITECTURE.md and USER_GUIDE.md pair).
 * - IPC transport choice: @ref ipc_transport_selection,
 *   @ref channel_configuration
 * - Channel topologies: @ref vital_channel_topologies
 * - Architecture Decision Records (design rationale, one per major
 *   decision): `docs/architecture/ADR-*.md`
 * - Requirements traceability: @ref safeapi_srs
 * - MISRA C:2012 conformance status: `docs/MISRA_COMPLIANCE_REPORT.md`
 * - Cross-compilation (Linux, QNX, etc.): `docs/CROSS_COMPILATION.md`


 +-------------------------------------------------------------------------+
|                  Generic Platform (RBC_GP)                    |
|      (ERTMS Procedures, Handover FSM, Route Management, 2oo2 Voter)     |
+-------------------------------------------------------------------------+
                                    │
                                    ▼ (rte_channel_open / rte_flow_write)
+-------------------------------------------------------------------------+
|                Safe Computing Platform (Platform_RTE)               |
+-------------------------------------------------------------------------+
                                    │
           ┌────────────────────────┼────────────────────────┐
           ▼                        ▼                        ▼
+---------------------+  +---------------------+  +---------------------+
| safeApiProtocol-    |  | safeApiProtocol-    |  | safeApiProtocol-    |
| AdapterDDS          |  | AdapterSS098        |  | AdapterEuroRadio    |
| (Trackside Bus)     |  | (RBC-RBC Fixed IP)  |  | (EVC-RBC Radio)     |
+---------------------+  +---------------------+  +---------------------+
           │                        │                        │
           │                        └────────────┬───────────┘
           │                                     ▼
           │                     +--------------------------------------+
           │                     |          safeCommFramework           |
           │                     | (SaF State Machine, 3DES MAC, KSMAC, |
           │                     |  CFM, ALE, Future RaSTA Engine)      |
           │                     +--------------------------------------+
           ▼                                     ▼
+-------------------------------------------------------------------------+
|                       OSAdapter (Platform_OS_POSIX)                   |
|                   (Timers, Memory Pools, Raw OS Sockets)                |
+-------------------------------------------------------------------------+
 */

/**
 * @page system_architecture System Architecture Overview
 *
 * @section system_architecture_layers Layering
 *
 * The framework is organized in three layers. Applications only ever call
 * into SAPI - never directly into the OS/RTOS - so retargeting to a
 * different platform means registering a different backend (ADR-005), not
 * rewriting application code.
 *
 * -# **OS Abstraction Layer (OAL)** - one service per OS/RTOS primitive:
 *    @ref TIMER, @ref NVM, @ref MEMORY, @ref TASK, @ref IPC, @ref NETLINK,
 *    @ref LOG, @ref REBOOT. Every service validates parameters, then
 *    dispatches to an integrator-supplied backend (ADR-005); the framework
 *    itself ships no OS-specific code.
 * -# **Common utilities** - layer-agnostic building blocks with no OS
 *    dependency: @ref BUFFER, @ref STRING, @ref CAST, @ref TYPES,
 *    @ref STATUS, @ref SAFESTATE, @ref CHANNEL.
 * -# **Redundancy / vital communication** - built on top of the first two
 *    layers: vital_channel (voting across 2oo2/2oo3/NMR), @ref CHECKPOINT
 *    (bounded checkpoint-ID rendezvous for cross-node agreement),
 *    @ref CLOCKSYNC (diagnostic-only wall-clock offset, never a
 *    correctness dependency), @ref CHECKSUM (CRC-64 data integrity), and
 *    @ref WATCHDOG (hang detection and recovery, including
 *    SAPI_WATCHDOG_ACTION_FAILOVER for redundant-peer-loss reactions).
 *
 * @ref APPMANAGER sits above all three layers, giving an application a
 * single init/execute/shutdown lifecycle entry point.
 *
 * @section system_architecture_ccf Common-Cause-Failure Mitigation
 *
 * For SIL 3/4 dual-channel deployments, ADR-008 covers build-diversity
 * mitigation (two independently-configured toolchains compiling channel A
 * and channel B) and the sapi_channel comparator that answers "which
 * channel is this binary?" and "do the two channels' results agree?".
 *
 * @section system_architecture_docs Where to Go Next
 *
 * - Getting started integrating a backend: @ref appmanager_integration
 * - Per-module architecture and user guides: see the "Related Pages" list
 *   (each module under `include/safeapi/<module>/` ships an
 *   ARCHITECTURE.md and USER_GUIDE.md pair).
 * - IPC transport choice: @ref ipc_transport_selection,
 *   @ref channel_configuration
 * - Vital channel topologies: @ref vital_channel_topologies
 * - Architecture Decision Records (design rationale, one per major
 *   decision): `docs/architecture/ADR-*.md`
 * - Requirements traceability: `docs/requirements/SRS.md`
 * - MISRA C:2012 conformance status: `docs/MISRA_COMPLIANCE_REPORT.md`
 * - Cross-compilation (Linux, QNX, etc.): `docs/CROSS_COMPILATION.md`
 */

# safeAPIFramework Feature Expansion

## Document Purpose

This document details proposed feature additions to the safeAPIFramework and outlines a configuration system allowing users to select which features to include. It serves as the specification reference for roadmap items and complements the detailed ADRs in `docs/architecture/`.

**Status:** This is a living document. It is updated as features are designed, implemented, and released.

**Last Updated:** 2026-08-02

---

## Table of Contents

1. [Current Modules](#current-modules)
2. [Tier 1: High Fit](#tier-1-high-fit)
3. [Tier 2: Strong Fit](#tier-2-strong-fit)
4. [Tier 3: Specialized Use](#tier-3-specialized-use)
5. [Configuration System Design](#configuration-system-design)
6. [Summary & Recommendations](#summary--recommendations)

---

## Current Modules

The framework currently provides 13 core modules:

**Common, Layer-Agnostic Utilities:**
- `status` — Status codes and error handling
- `types` — Fixed-width type definitions
- `buffer` — Cross-layer data buffer view with endianness-safe access
- `cast` — Checked integer casting between all fixed-width types and `size_t`
- `safestate` — Safe-state transitions and checked assertions
- `string` — Bounded string manipulation (replaces strcpy/strcat/sprintf/atoi/strtok)

**OS Abstraction Layer (OAL) Services:**
- `timer` — Timing and delays
- `nvm` — Non-volatile memory (flash)
- `memory` — Static memory reservation
- `task` — Task/thread scheduling
- `ipc` — Inter-process/inter-task communication
- `log` — Logging and diagnostics
- `reboot` — Controlled reboot with reason codes

---

## Tier 1: High Fit

Features that strongly align with RBC (ERTMS Radio Block Centre) safety requirements and address core architectural gaps.

### Hierarchical State Machine (HSM)

**Motivation:** The RBC must handle complex protocol state machines (ERTMS messaging, mode transitions, safe-state sequencing). Flat conditionals lead to state-explosion bugs and are difficult to verify.

**What it does:**
- Table-driven, event-driven state machine
- Nested states with entry/exit actions
- Orthogonal regions (AND states) for independent concurrent behaviors
- Static transition table allocation (no dynamic dispatch, MISRA-compliant)

**Why it matters:**
- Protocol handling becomes declarative and traceable to requirements
- Reduces branching complexity; easier to analyze for SIL 4 certification
- Entry/exit actions prevent state-transition bugs
- Orthogonal regions eliminate code duplication for parallel protocols

**Tradeoffs:**
- Adds indirection in critical path (mitigated by inline transition table)
- Requires discipline in event handler design
- Testing must cover all state/event combinations

**API Sketch:**
```c
typedef struct sapi_hsm_state {
    const char *name;
    sapi_status_t (*on_entry)(void *context);
    sapi_status_t (*on_exit)(void *context);
    sapi_status_t (*on_event)(void *context, sapi_hsm_event_t event);
    // Orthogonal regions, parent state, transitions...
} sapi_hsm_state_t;

sapi_status_t sapi_hsm_dispatch(sapi_hsm_t *hsm, sapi_hsm_event_t event);
```

**MISRA Considerations:**
- Avoid function pointers; use indexed dispatch tables instead
- All state transitions must be explicitly listed in transition matrix
- Entry/exit actions must be idempotent (reentrance-safe)

**Related:** Message Queue (consumes events), Watchdog (monitors HSM responsiveness)

---

### Event/Message Queue

**Motivation:** Safe, decoupled async communication between tasks without raw IPC complexity.

**What it does:**
- Bounded circular queue for message passing
- Multiple producer, single consumer (MPSC) pattern
- Overflow/underflow detection with error status
- Integration with task scheduler wake-on-message

**Why it matters:**
- Decouples task producers from consumers
- Proven safety pattern in automotive/avionics
- Pairs naturally with HSM (events in queue, HSM dispatches them)

**Tradeoffs:**
- Queue size must be statically allocated (no dynamic growth)
- Overflow handling adds complexity (drop, block, or fail?)

**API Sketch:**
```c
#define SAPI_MSGQUEUE_CAPACITY 128

typedef struct {
    uint32_t msg_type;
    uint8_t payload[64];
} sapi_msg_t;

sapi_status_t sapi_msgqueue_send(sapi_msgqueue_t *q, const sapi_msg_t *msg);
sapi_status_t sapi_msgqueue_recv(sapi_msgqueue_t *q, sapi_msg_t *msg);
sapi_status_t sapi_msgqueue_is_empty(const sapi_msgqueue_t *q, bool *empty);
```

**MISRA Considerations:**
- Validate queue capacity at compile time (static assertion)
- Overflow behavior must be deterministic and documented

**Related:** Hierarchical State Machine (consumes events), Watchdog (monitors queue depth)

---

### Watchdog & Health Monitor

**Motivation:** Safety-critical systems must detect and recover from task starvation, deadlocks, and timeout violations without human intervention.

**What it does:**
- Per-task heartbeat tracking
- Timeout violation detection
- Automatic safe-state transition on health failure
- Integration with reboot layer (optional controlled restart)

**Why it matters:**
- Unattended operation requires automatic failure recovery
- Mandatory for SIL 4 certification
- Bridges task and reboot layers naturally

**Tradeoffs:**
- Requires RTOS timer support for callbacks
- False positives possible if task takes longer than expected (requires tuning)
- Must not itself deadlock

**API Sketch:**
```c
typedef struct {
    uint32_t task_id;
    sapi_timer_duration_t timeout_ms;
    sapi_status_t (*on_timeout)(uint32_t task_id, void *context);
} sapi_watchdog_task_config_t;

sapi_status_t sapi_watchdog_heartbeat(uint32_t task_id);
sapi_status_t sapi_watchdog_register_task(const sapi_watchdog_task_config_t *cfg);
```

**MISRA Considerations:**
- All state transitions must be atomic
- Timeout handler must be reentrant

**Related:** Timer (tracks deadlines), Reboot (triggers restart), Task (monitors scheduler)

---

### Protected Data / Synchronized Access

**Motivation:** Multi-threaded access to shared data requires protection against race conditions, but MISRA forbids dynamic locks.

**What it does:**
- Read-write guards (using hardware atomics where available, or spinlocks on single-core)
- Deadlock prevention via compile-time lock ordering
- Atomic compare-and-swap helpers for lock-free algorithms

**Why it matters:**
- Prevents data corruption from concurrent access
- Natural integration with task layer
- Supports both blocking and lock-free approaches

**Tradeoffs:**
- Lock ordering analysis required (tooling can automate)
- Spinlocks waste cycles on contention
- Can hide performance issues if over-used

**API Sketch:**
```c
typedef struct {
    uint8_t lock_id;  // For deadlock prevention
    uint32_t data;
} sapi_protected_u32_t;

sapi_status_t sapi_protected_read(const sapi_protected_u32_t *p, uint32_t *out);
sapi_status_t sapi_protected_write(sapi_protected_u32_t *p, uint32_t value);

// For lock-free patterns:
sapi_status_t sapi_atomic_cas(volatile uint32_t *addr, uint32_t expected, 
                               uint32_t new_val, bool *success);
```

**MISRA Considerations:**
- Lock IDs must form a strict total order (static verification)
- All atomic operations must use explicit functions (no inline asm)

**Related:** Task (uses locks), Synchronized access across multiple tasks

---

## Tier 2: Strong Fit

Features that are valuable for observability, testing, and configuration but not strictly necessary for baseline RBC operation.

### Diagnostic Ring Buffer

**Purpose:** Forensic logging for post-mortem analysis and certification auditing.

**Features:**
- Bounded circular buffer for timestamped, structured events
- No allocation after initialization
- Survives task restart (persists in NVM or reserved RAM)
- Query API for off-target log analysis

**MISRA Fit:** Excellent — completely static allocation, no pointers.

**Estimated Effort:** 1–2 weeks

---

### Safe Configuration Manager

**Purpose:** Runtime tuning with schema validation, corruption recovery, and NVM backing.

**Features:**
- Schema-based validation at encode/decode
- Automatic corruption detection and safe recovery (revert to defaults)
- Rollback support (keep two generations)
- Built on NVM layer

**MISRA Fit:** Good — requires careful state machine to prevent partial writes.

**Estimated Effort:** 2–3 weeks

---

### Checksum & CRC Utilities

**Purpose:** Data integrity verification for protocol payloads and NVM.

**Features:**
- Pre-computed CRC32 lookup tables
- Fletcher-16/32 checksums
- Protocol payload verification
- NVM integrity checking

**MISRA Fit:** Excellent — static tables, no allocation.

**Estimated Effort:** 1 week

---

### Cyclic Scheduler / Time Slot Allocator

**Purpose:** Deterministic, fully predictable task scheduling for hard real-time systems.

**Features:**
- Static time-slot allocation per task
- Integration with timer layer
- Cyclic schedule generation and validation
- WCET analysis support

**Trade-off:** Reduces scheduling flexibility (no preemption within a cycle).

**MISRA Fit:** Excellent — completely static schedule.

**Estimated Effort:** 2–3 weeks

---

### Mock Backend Harness

**Purpose:** Build-time selectable mock backends for all OAL services, enabling fast CI/unit testing on the host.

**Features:**
- Fake timer (time control via API)
- Fake NVM (in-memory storage)
- Fake task scheduler (single-threaded event pump)
- Fake IPC (local pipes)
- Failure injection (simulate timeouts, corruption)

**MISRA Fit:** Good — mocks are test-only code.

**Estimated Effort:** 2 weeks

**Critical:** Must not be linked into production firmware.

---

## Tier 3: Specialized Use

Defer unless specific requirements emerge.

| Feature | Motivation | Effort | Risk |
|---------|-----------|--------|------|
| **Performance Counters** | WCET analysis, task profiling | 1–2 wks | Low — atomic counters only |
| **Binary Serialization (ASN.1/TLV)** | Compact protocol payloads | 2–3 wks | Medium — schema complexity |
| **CAN Bus Utilities** | Automotive/rail integration | 1–2 wks | Low — straightforward |
| **Authenticated Encryption** | Secure payloads (if threat model requires) | 1–2 wks | Medium — crypto integration |

---

## Configuration System Design

### Goal

Allow users to include only the features their application needs, reducing binary size, build time, and MISRA verification scope.

### Approach 1: CMake Feature Flags (Recommended Start)

**Mechanism:** Each feature is an optional CMake `option()`. Disabled features are not compiled or linked.

**Pros:**
- Simple to understand and use
- Ties directly to existing build infrastructure
- Reduces binary size immediately
- Link-time errors if dependencies are missing

**Cons:**
- Requires separate build for each configuration
- Not suitable for multi-variant firmware in the same binary

**Example:**
```cmake
option(SAFEAPI_ENABLE_HSM "Enable Hierarchical State Machine" ON)
option(SAFEAPI_ENABLE_MSGQUEUE "Enable Event/Message Queue" ON)
option(SAFEAPI_ENABLE_WATCHDOG "Enable Watchdog & Health Monitor" ON)
option(SAFEAPI_ENABLE_PROTECTED_DATA "Enable Protected Data Sync" OFF)
option(SAFEAPI_ENABLE_DIAGNOSTICS "Enable Diagnostic Ring Buffer" ON)
option(SAFEAPI_ENABLE_CONFIG "Enable Safe Config Manager" OFF)
option(SAFEAPI_ENABLE_CHECKSUM "Enable CRC/Checksum Utils" OFF)
option(SAFEAPI_ENABLE_SCHEDULER "Enable Cyclic Scheduler" OFF)

if(SAFEAPI_ENABLE_HSM)
    add_subdirectory(src/hsm)
endif()
```

**Usage:**
```bash
cmake -S . -B build \
  -DSAFEAPI_ENABLE_HSM=ON \
  -DSAFEAPI_ENABLE_WATCHDOG=ON \
  -DSAFEAPI_ENABLE_MOCKS=ON
cmake --build build
```

---

### Approach 2: Runtime Feature Registry (Phase 2)

**Mechanism:** Features are built into the framework, but can be enabled/disabled at startup via API.

**Pros:**
- Single binary can support multiple configurations
- Graceful degradation (disabled features return `SAPI_STATUS_NOT_AVAILABLE`)
- Useful for multi-variant firmware

**Cons:**
- Adds runtime overhead
- Harder to guarantee behavior (disabled feature still in binary)

**API:**
```c
typedef enum {
    SAPI_FEATURE_HSM,
    SAPI_FEATURE_MSGQUEUE,
    SAPI_FEATURE_WATCHDOG,
    // ...
} sapi_feature_id_t;

sapi_status_t sapi_enable_feature(sapi_feature_id_t id, bool enabled);
sapi_status_t sapi_is_feature_available(sapi_feature_id_t id, bool *available);
```

---

### Approach 3: Hybrid — Static Config Header + CMake (Phase 3)

**Mechanism:** A generated or hand-written `safeapi_config.h` defines static `#define SAFEAPI_ENABLE_HSM 1` flags, which control both compilation and registration. Best for locked certification workflows.

**Pros:**
- Static configuration, fully traceable
- CMake verifies consistency
- Suitable for SIL 4 certification (configuration is fixed before testing)

**Cons:**
- Requires header generation
- Least flexible

**Example:**
```c
// safeapi_config.h (generated or hand-written)
#define SAFEAPI_ENABLE_HSM 1
#define SAFEAPI_ENABLE_MSGQUEUE 1
#define SAFEAPI_ENABLE_WATCHDOG 1
#define SAFEAPI_ENABLE_PROTECTED_DATA 0
#define SAFEAPI_ENABLE_DIAGNOSTICS 1
```

---

### Recommendation

**Start with Approach 1 (CMake Feature Flags):**
- Simplest to implement and understand
- Provides immediate value (reduced scope, faster builds)
- Can migrate to Approach 3 as certification requirements solidify

**Migrate to Approach 3 (Hybrid) before SIL 4 certification:**
- Locks configuration before final testing
- Generates compliance matrix (what's enabled, why, traced to requirements)

**Approach 2 (Runtime) is optional** — useful only if you need multi-variant firmware in a single binary.

---

## Summary & Recommendations

### Priority Ranking

| Feature | Tier | Fit | Complexity | Recommendation |
|---------|------|-----|------------|-----------------|
| **Hierarchical State Machine** | 1 | Excellent | Medium | **Add first** (highest value) |
| **Event/Message Queue** | 1 | Excellent | Low | **Add early** |
| **Watchdog & Health Monitor** | 1 | Excellent | Medium | **Add early** |
| **Protected Data / Sync** | 1 | Good | Medium | Add if multi-task heavy |
| **Diagnostic Ring Buffer** | 2 | Good | Low | Add for observability |
| **Safe Configuration Manager** | 2 | Good | Medium | Add if runtime config needed |
| **Checksum & CRC Utils** | 2 | Good | Low | Add for protocol compliance |
| **Cyclic Scheduler** | 2 | Moderate | Medium | Add if determinism critical |
| **Mock Backend Harness** | 2 | Good | Low | **Invest for testing** |
| Tier 3 features | 3 | Moderate | Varies | Defer unless specified |

### Implementation Sequence

**v0.2.0 (Next Release, Q3 2026):**
1. Configure system (CMake Feature Flags — Approach 1)
2. Hierarchical State Machine
3. Event/Message Queue
4. Watchdog & Health Monitor
5. Diagnostic Ring Buffer
6. Checksum & CRC Utilities
7. Mock Backend Harness

**v0.3.0 (Q4 2026):**
1. Protected Data / Synchronized Access
2. Safe Configuration Manager
3. Cyclic Scheduler
4. Runtime Feature Registry (Approach 2, optional)

**v0.4.0+ (2027):**
1. Tier 3 features as needed
2. Hybrid static config system (Approach 3, for certification)
3. Performance tuning

---

## Tracking & Governance

- **ROADMAP.md** — Detailed status, effort estimates, GitHub issue links
- **GitHub Issues** — One issue per feature for design discussion and tracking
- **GitHub Projects** — Visual board for status by tier and release
- **ADRs** — Architecture Decision Records in `docs/architecture/ADR-NNN-*.md`
- **SRS** — Updated `docs/requirements/SRS.md` with feature requirements

---

## Document Storage

This document is maintained in two places:

1. **GitHub** — `docs/FEATURE_EXPANSION.md` (canonical, versioned)
2. **Claude.ai Artifact** — Interactive view with better formatting
   - Link: [Feature Expansion Artifact](https://claude.ai/code/artifact/b75f23a4-842a-4c2c-8292-bfe2ecc94716)
   - This artifact is regenerated when GitHub content is updated

Both are kept in sync; the GitHub version is authoritative.

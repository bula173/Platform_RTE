# safeAPIFramework Feature Expansion

## Document Purpose

This document details proposed feature additions to the safeAPIFramework and outlines a configuration system allowing users to select which features to include. It serves as the specification reference for roadmap items and complements the detailed ADRs in `docs/architecture/`.

All features are designed for **EN 50126/50128/50129** compliance, supporting **SIL 4** railway applications.

**Status:** This is a living document. It is updated as features are designed, implemented, and released.

**Last Updated:** 2026-08-02

**Compliance Standards:**
- ✅ EN 50126:2017 (RAM - Reliability/Availability/Maintainability) — Deterministic design, fault tolerance, graceful degradation
- ✅ EN 50128:2011 (Software safety) — All 10 mandatory techniques, MISRA C:2012
- ✅ EN 50129:2018 (Functional safety management) — V&V planning, safety case, requirements traceability
- ✅ MISRA C:2012 (Code quality) — Mandatory & required rules, no unsafe constructs

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

### IPC Enhancements (Communication Patterns)

**Motivation:** Current IPC provides basic point-to-point send/receive. RBC requires sophisticated communication patterns: request-reply (RPC), broadcast, publish-subscribe, priority handling, and flow control.

**What it adds:**

1. **Request-Reply Pattern (Issue #17)**
   - Synchronous RPC-style communication
   - Client blocks on reply with timeout
   - Server processes and sends reply
   - Essential for command/control scenarios

2. **Publish-Subscribe (Issue #18)**
   - One publisher, multiple subscribers
   - Decouple publishers from subscribers
   - Broadcast signals (e.g., track status changes)
   - Static subscriber registration

3. **Message Filtering & Routing (Issue #19)**
   - Route messages by type/tag
   - Filter by criteria (e.g., train_id >= 100)
   - Reduce message queue congestion
   - Enable selective message consumption

4. **Priority Queues (Issue #20)**
   - Process high-priority messages first
   - Prevent low-priority messages from blocking critical ones
   - Safety-critical: emergency stops before routine updates
   - Static priority levels (0-7)

5. **Flow Control (Issue #21)**
   - Back-pressure handling (queue full)
   - Producer blocking vs. message drop strategies
   - Configurable: drop oldest, drop newest, or block
   - Prevent silent message loss

6. **IPC Statistics & Monitoring (Issue #22)**
   - Track sent/received counts
   - Monitor queue depth
   - Detect deadlock-like conditions
   - Health check for IPC layer

7. **Deadlock Detection (Issue #23)**
   - Detect circular wait patterns
   - Timeout-based detection
   - Automatic recovery (break deadlock, log incident)
   - Safety-critical: prevent system hang

**Why it matters:**
- RBC involves many tasks (track manager, train controller, dispatcher, logger)
- Each needs different communication patterns
- Request-reply for queries, pub-sub for events
- Priority for safety-critical signals
- Flow control prevents queue overflow

**Tradeoffs:**
- Adds complexity to IPC layer
- Requires careful deadlock analysis
- Memory overhead for priority queues

**API Sketch:**
```c
// Request-Reply
rte_status_t rte_ipc_send_request(handle, request, reply, timeout_ms);

// Pub-Sub
rte_status_t rte_ipc_subscribe(topic, subscriber_queue, filter);
rte_status_t rte_ipc_publish(topic, message);

// Priority
rte_status_t rte_ipc_send_priority(handle, message, priority, timeout_ms);

// Statistics
rte_status_t rte_ipc_get_stats(handle, stats);
```

**MISRA Considerations:**
- All dynamic behavior (routing) validated at registration time
- Deadlock detection must be deterministic
- No unbounded allocation for subscribers

**Related:**
- Message Queue (#2 — complementary, but IPC is more powerful)
- Watchdog (#3 — monitors IPC health)
- Diagnostics (#5 — logs IPC events)

---

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
typedef struct rte_hsm_state {
    const char *name;
    rte_status_t (*on_entry)(void *context);
    rte_status_t (*on_exit)(void *context);
    rte_status_t (*on_event)(void *context, rte_hsm_event_t event);
    // Orthogonal regions, parent state, transitions...
} rte_hsm_state_t;

rte_status_t rte_hsm_dispatch(rte_hsm_t *hsm, rte_hsm_event_t event);
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
#define RTE_MSGQUEUE_CAPACITY 128

typedef struct {
    uint32_t msg_type;
    uint8_t payload[64];
} rte_msg_t;

rte_status_t rte_msgqueue_send(rte_msgqueue_t *q, const rte_msg_t *msg);
rte_status_t rte_msgqueue_recv(rte_msgqueue_t *q, rte_msg_t *msg);
rte_status_t rte_msgqueue_is_empty(const rte_msgqueue_t *q, bool *empty);
```

**MISRA Considerations:**
- Validate queue capacity at compile time (static assertion)
- Overflow behavior must be deterministic and documented

**Related:** Hierarchical State Machine (consumes events), Watchdog (monitors queue depth)

---

### Watchdog Mechanism (System / Task / Channel / Checkpoint)

**Motivation:** Safety-critical systems must detect and recover from system hangs, task starvation, deadlocks, and channel timeouts without human intervention. EN 50128 SIL 4 requires active liveness monitoring.

**What it does:**
- **System Watchdog:** Detects if entire RBC system is hung (no task making progress)
- **Task Watchdog:** Monitors individual task/thread liveness (heartbeat checking)
- **Channel Watchdog:** Detects stuck IPC/redundancy channels (no messages flowing)
- **Checkpoint Watchdog:** Integrated with barrier sync; detects nodes not reaching checkpoint
- **Recovery Actions:** Configurable responses (log, safe-state, reboot, failover)

**Why it matters:**
- Unattended operation requires automatic failure recovery
- Mandatory for SIL 4 certification (EN 50128 liveness requirement)
- Integrates with redundancy framework (detect + failover)
- Critical for real-time systems: detect deadline violations immediately
- Bridges system monitoring, IPC, redundancy, and reboot layers

**Tradeoffs:**
- Requires timer support (hardware or software) for periodic ticks
- False positives possible if application is slower than expected (requires tuning)
- Watchdog itself must be simple and reliable (no deadlocks in watchdog)

**Features:**
1. **Per-Component Monitoring** — System, task, channel, checkpoint
2. **Configurable Timeouts** — Typical: 100ms–1000ms for task, 200ms for checkpoint
3. **Recovery Actions** — Log, safe-state, reboot, or custom callback
4. **Health Status API** — Non-blocking query of kicks, fires, time-remaining
5. **Integration with Checkpoints** — Auto-detect slow nodes
6. **Integration with Redundancy** — Failover on watchdog timeout
7. **Audit Trail** — All watchdog fires logged with timestamps

**API Overview:**
```c
// Create and manage watchdog
rte_watchdog_t wd;
rte_watchdog_config_t config = {
    .type = RTE_WATCHDOG_SYSTEM,      // or TASK, CHANNEL, CHECKPOINT
    .name = "rbc_main_wd",
    .timeout_ms = 1000,                // Deadline
    .action = RTE_WATCHDOG_ACTION_SAFESTATE  // Recovery action
};
rte_watchdog_create(&wd, &config);
rte_watchdog_start(wd);

// Kick watchdog (reset countdown)
rte_watchdog_kick(wd);

// Query status (non-blocking)
rte_watchdog_status_t status;
rte_watchdog_get_status(wd, &status);
// status.time_until_fire, status.kicks, status.fires, etc.

// Stop and destroy
rte_watchdog_stop(wd);
rte_watchdog_destroy(wd);
```

**Usage Pattern (Main Loop):**
```c
while (running) {
    process_signals();
    process_trains();
    update_speed_limits();
    
    // Prove we're alive (resets 1-second timeout)
    rte_watchdog_kick(wd);
    
    sleep_ms(100);
}
```

**Usage Pattern (Task-Specific):**
```c
// Each task monitors its own progress
while (running) {
    // Checkpoint 1
    rte_channel_checkpoint(vital_ch, &ckpt1);
    
    // Process (must complete within timeout)
    process_signals(&signals);
    
    // Checkpoint 2
    rte_channel_checkpoint(vital_ch, &ckpt2);
    
    // Prove this task made progress
    rte_watchdog_kick(task_wd);
}
```

**MISRA Considerations:**
- All state transitions must be atomic (no dynamic lock primitives)
- Deterministic O(1) operations (no allocation, no loops)
- Timeout handler reentrant (safe from ISR context)
- No unbounded recursion or call chains

**SIL 4 Safety Properties:**
- **Detection Latency:** < timeout_ms (e.g., detect hang within 1 second)
- **False Positives:** None (only fired if no kick received)
- **Recovery Determinism:** Specified action always taken
- **Audit Trail:** All fires logged with timestamp
- **Failsafe:** Default action is safe-state (never silent failure)

**Related:**
- Timer (provides periodic ticks)
- Redundancy/Checkpoints (integrated watchdog on barriers)
- Reboot (triggered on recovery)
- Task (monitors scheduler liveness)
- Logging (audit trail)

**Design Doc:** [WATCHDOG_DESIGN.md](WATCHDOG_DESIGN.md)

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
} rte_protected_u32_t;

rte_status_t rte_protected_read(const rte_protected_u32_t *p, uint32_t *out);
rte_status_t rte_protected_write(rte_protected_u32_t *p, uint32_t value);

// For lock-free patterns:
rte_status_t rte_atomic_cas(volatile uint32_t *addr, uint32_t expected, 
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

### Mock OSAdapter Harness

**Purpose:** Build-time selectable mock OSAdapters for all OAL services, enabling fast CI/unit testing on the host.

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
- Graceful degradation (disabled features return `RTE_STATUS_NOT_AVAILABLE`)
- Useful for multi-variant firmware

**Cons:**
- Adds runtime overhead
- Harder to guarantee behavior (disabled feature still in binary)

**API:**
```c
typedef enum {
    RTE_FEATURE_HSM,
    RTE_FEATURE_MSGQUEUE,
    RTE_FEATURE_WATCHDOG,
    // ...
} rte_feature_id_t;

rte_status_t rte_enable_feature(rte_feature_id_t id, bool enabled);
rte_status_t rte_is_feature_available(rte_feature_id_t id, bool *available);
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
| **Mock OSAdapter Harness** | 2 | Good | Low | **Invest for testing** |
| Tier 3 features | 3 | Moderate | Varies | Defer unless specified |

### Implementation Sequence

**v0.2.0 (Next Release, Q3 2026):**
1. Configure system (CMake Feature Flags — Approach 1)
2. Hierarchical State Machine
3. Event/Message Queue
4. Watchdog & Health Monitor
5. Diagnostic Ring Buffer
6. Checksum & CRC Utilities
7. Mock OSAdapter Harness

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

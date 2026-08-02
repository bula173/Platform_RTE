# Hardware Redundancy & Voting Framework

⚠️ **STATUS: DESIGN PHASE** — This document describes proposed SAPI APIs for redundancy and voting that have not yet been implemented. Code examples are illustrative of the design intent and do not correspond to existing APIs. This specification will become implemented in safeAPIFramework v0.4.0 or later.

See [ROADMAP.md](ROADMAP.md) for implementation timeline and priority.

---

## Overview

Safety-critical systems (SIL 3/4) require hardware redundancy to detect and recover from single-point failures. This document proposes an abstraction layer for **redundant communication channels** with **voting/arbitration logic** that will be added to the safeAPIFramework.

**Key Concepts:**
- **Vital Channels:** Safety-critical, must be redundant (2oo2, 2oo3)
- **Non-Vital Channels:** Non-critical, single channel acceptable
- **Voting Strategies:** 2oo2 (both agree), 2oo3 (majority agree), NMR (N-modular)
- **Channel Monitoring:** Detect disagreement, track faults, trigger safe states

---

## Architecture

### High-Level Design

```
┌────────────────────────────────────────────────────────┐
│          Application (RBC)                            │
│  (uses unified API, unaware of redundancy)            │
└──────────────┬───────────────────────────────────────┘
               │
        ┌──────▼──────────────────┐
        │ Redundancy Manager      │
        │ (voting & arbitration)  │
        ├────────────────────────┤
        │ • Channel monitoring    │
        │ • Voting logic          │
        │ • Fault detection       │
        │ • Safe-state triggers   │
        └──┬────────┬────────┬───┘
           │        │        │
    ┌──────▼──┐ ┌──▼──┐ ┌──▼────┐
    │Channel 1│ │Ch. 2│ │Ch. 3  │
    │(VITAL)  │ │(VIT)│ │(NV)   │
    └─────────┘ └─────┘ └────────┘
       │         │         │
    ┌──▼──────────▼─────────▼──┐
    │  Hardware (Processors)    │
    │  - Dual-core CPU (2oo2)  │
    │  - Triple-core CPU (2oo3)│
    │  - Heterogeneous (mixed) │
    └──────────────────────────┘
```

### Channel Types

#### Vital Channel (Safety-Critical)

```c
// 2oo2 Vital Channel (must be redundant)
typedef struct {
    sapi_redundancy_channel_t channel_a;
    sapi_redundancy_channel_t channel_b;
    sapi_redundancy_voting_t voting;  // 2oo2
} sapi_vital_channel_2oo2_t;

// Properties:
// - Always requires redundancy
// - Voting: BOTH channels must agree
// - Disagreement → Safe-state transition
// - No single-channel fallback
// - Health monitoring mandatory
```

#### Non-Vital Channel (Best-Effort)

```c
// Non-Vital Single Channel
typedef struct {
    sapi_redundancy_channel_t channel;
} sapi_nonvital_channel_t;

// Properties:
// - Single channel acceptable
// - No voting required
// - Graceful degradation on fault
// - Health monitoring optional
```

---

## Voting Strategies

### 2oo2 (Two-out-of-Two)

**Rule:** Both channels must agree; disagreement triggers safe-state.

```
Input A ────┐
            ├──→ Voting Logic ──→ Output
Input B ────┤
            └─ EQUAL? YES → output
                          NO  → FAULT
```

**Use Case:** Dual-redundant systems (dual-core processors)

**Safety:** Can detect single-point failures via disagreement

**Example:**
```c
sapi_redundancy_config_t config = {
    .strategy = SAPI_VOTING_2OO2,
    .vital = true,
    .channel_a = &channel_a,
    .channel_b = &channel_b,
    .on_disagreement = safe_state_transition
};
```

### 2oo3 (Two-out-of-Three)

**Rule:** Majority vote (≥2 channels agree). Tolerates 1 channel failure.

```
Input A ────┐
            ├──→ Voting Logic ──→ Output
Input B ────┤
            │  (majority vote)
Input C ────┘
            └─ ≥2 EQUAL? YES → output
                            NO  → FAULT
```

**Use Case:** Triple-redundant systems (triple-core processors)

**Safety:** Can tolerate and isolate single-channel fault

**Example:**
```c
sapi_redundancy_config_t config = {
    .strategy = SAPI_VOTING_2OO3,
    .vital = true,
    .channel_a = &channel_a,
    .channel_b = &channel_b,
    .channel_c = &channel_c,
    .on_disagreement = fault_isolation_and_recovery
};
```

### NMR (N-Modular Redundancy)

**Rule:** N channels vote by majority; tolerates N/2 failures.

```
Channels {1..N}
    ↓
Voting Matrix
    ↓
Majority Vote
    ↓
Output or FAULT
```

**Use Case:** Systems with 4+ redundant processors

**Safety:** Scalable to high-redundancy systems

---

## API Design

### Core Abstractions

```c
/* ============================================================================
 * Vital Channel (Redundant, Safety-Critical)
 * ========================================================================== */

/**
 * @brief Vital channel handle (abstracts redundancy)
 */
typedef struct sapi_vital_channel_s *sapi_vital_channel_t;

/**
 * @brief Voting strategy for vital channels
 */
typedef enum {
    SAPI_VOTING_2OO2,    // Dual-channel: both must agree
    SAPI_VOTING_2OO3,    // Triple-channel: majority vote
    SAPI_VOTING_NMR      // N-modular: arbitrary N
} sapi_voting_strategy_t;

/**
 * @brief Configuration for vital channel
 */
typedef struct {
    const char *name;
    sapi_voting_strategy_t strategy;
    
    // Channel references (varies by strategy)
    sapi_ipc_handle_t *channels;     // Array of underlying channels
    size_t num_channels;              // 2 for 2oo2, 3 for 2oo3, N for NMR
    
    // Fault handling
    void (*on_disagreement)(void *context);
    void *fault_context;
    
    // Health monitoring
    sapi_duration_ms_t timeout_ms;
} sapi_vital_channel_config_t;

/**
 * @brief Create a vital (redundant) channel
 *
 * @param handle_out Receives vital channel handle
 * @param config Channel configuration
 * @return SAPI_STATUS_OK on success
 *
 * Example (2oo2):
 * @code
 * sapi_ipc_handle_t channels[2] = {channel_a, channel_b};
 * sapi_vital_channel_config_t config = {
 *     .name = "vital_signal_channel",
 *     .strategy = SAPI_VOTING_2OO2,
 *     .channels = channels,
 *     .num_channels = 2,
 *     .on_disagreement = safe_state_handler,
 *     .timeout_ms = 100
 * };
 * sapi_vital_channel_create(&vital_ch, &config);
 * @endcode
 */
sapi_status_t sapi_vital_channel_create(sapi_vital_channel_t *handle_out,
                                         const sapi_vital_channel_config_t *config);

/**
 * @brief Send on vital channel (broadcasts to all redundant channels)
 *
 * Sends message to all underlying channels. All must succeed for operation
 * to succeed.
 *
 * @param channel Vital channel handle
 * @param message Message to send
 * @param size Message size
 * @param timeout_ms Timeout for all channels to complete
 * @return SAPI_STATUS_OK if all channels sent successfully
 *         SAPI_STATUS_ERROR if any channel failed
 *         SAPI_STATUS_TIMEOUT if any channel timed out
 */
sapi_status_t sapi_vital_send(sapi_vital_channel_t channel,
                               const void *message,
                               size_t size,
                               sapi_duration_ms_t timeout_ms);

/**
 * @brief Receive on vital channel (voting-based reception)
 *
 * Receives from all channels and applies voting logic:
 * - 2oo2: Both must agree, return disagreement as error
 * - 2oo3: Majority vote, tolerate one disagreement
 * - NMR: Majority vote, tolerate N/2 failures
 *
 * @param channel Vital channel handle
 * @param message_out Receives voted message
 * @param size Max message size
 * @param timeout_ms Timeout for all channels to respond
 * @return SAPI_STATUS_OK if voting succeeded
 *         SAPI_STATUS_ERROR if voting failed (disagreement)
 *         SAPI_STATUS_TIMEOUT if channels didn't respond
 */
sapi_status_t sapi_vital_receive(sapi_vital_channel_t channel,
                                  void *message_out,
                                  size_t size,
                                  sapi_duration_ms_t timeout_ms);

/**
 * @brief Get channel health status
 *
 * @param channel Vital channel handle
 * @param health_out Receives health information
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_vital_get_health(sapi_vital_channel_t channel,
                                     sapi_channel_health_t *health_out);

/**
 * @brief Destroy vital channel
 */
sapi_status_t sapi_vital_channel_destroy(sapi_vital_channel_t channel);

/* ============================================================================
 * Non-Vital Channel (Single, Best-Effort)
 * ========================================================================== */

/**
 * @brief Non-vital channel handle (single underlying channel)
 */
typedef struct sapi_nonvital_channel_s *sapi_nonvital_channel_t;

/**
 * @brief Configuration for non-vital channel
 */
typedef struct {
    const char *name;
    sapi_ipc_handle_t underlying_channel;
    sapi_duration_ms_t timeout_ms;
} sapi_nonvital_channel_config_t;

/**
 * @brief Create a non-vital (single) channel
 *
 * Non-vital channels don't require redundancy. On fault, graceful
 * degradation or retry logic may be applied by application.
 *
 * @param handle_out Receives channel handle
 * @param config Channel configuration
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_nonvital_channel_create(sapi_nonvital_channel_t *handle_out,
                                            const sapi_nonvital_channel_config_t *config);

/**
 * @brief Send on non-vital channel
 */
sapi_status_t sapi_nonvital_send(sapi_nonvital_channel_t channel,
                                  const void *message,
                                  size_t size,
                                  sapi_duration_ms_t timeout_ms);

/**
 * @brief Receive on non-vital channel
 */
sapi_status_t sapi_nonvital_receive(sapi_nonvital_channel_t channel,
                                     void *message_out,
                                     size_t size,
                                     sapi_duration_ms_t timeout_ms);

/**
 * @brief Destroy non-vital channel
 */
sapi_status_t sapi_nonvital_channel_destroy(sapi_nonvital_channel_t channel);

/* ============================================================================
 * Channel Health Monitoring
 * ========================================================================== */

/**
 * @brief Channel health information
 */
typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t errors;
    uint32_t timeouts;
    uint32_t disagreements;    // For vital channels only
    
    // Fault detection
    uint8_t channel_ok[4];     // Per-channel health (up to 4 channels)
    uint8_t majority_vote_ok;  // Voting succeeded
} sapi_channel_health_t;

/**
 * @brief Get unified channel health (abstracts redundancy)
 *
 * Application doesn't need to know about redundancy; just queries health.
 *
 * @param channel Vital or non-vital channel
 * @param health_out Receives health status
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_channel_get_health(void *channel,
                                       sapi_channel_health_t *health_out);
```

---

## Example: 2oo2 Vital Channel (Dual-Redundant CPU)

### Architecture

```
Dual-Core Processor (2 CPUs)
├─ CPU A (Core 1)
│  └─ Channel A (IPC to CPU B)
└─ CPU B (Core 2)
   └─ Channel B (IPC to CPU A)

They communicate via 2oo2 voting:
- Both must agree on messages
- Disagreement triggers safe-state
```

### Application Code

```c
// Create two underlying IPC channels (one per CPU)
sapi_ipc_handle_t channel_a, channel_b;
sapi_ipc_create(&storage_a, &config_a, &channel_a);
sapi_ipc_create(&storage_b, &config_b, &channel_b);

// Create vital channel that wraps both with 2oo2 voting
sapi_ipc_handle_t channels[2] = {channel_a, channel_b};
sapi_vital_channel_config_t vital_config = {
    .name = "dual_redundant_signal",
    .strategy = SAPI_VOTING_2OO2,
    .channels = channels,
    .num_channels = 2,
    .on_disagreement = handle_dual_disagreement,
    .timeout_ms = 100
};

sapi_vital_channel_t vital_signal;
sapi_vital_channel_create(&vital_signal, &vital_config);

// Application sends on vital channel (broadcasts to both CPUs)
signal_command_t cmd = {.signal_id = 5, .state = SIGNAL_RED};
sapi_status_t status = sapi_vital_send(vital_signal, &cmd, sizeof(cmd), 100);

if (status != SAPI_STATUS_OK) {
    SAPI_LOG_ERROR("Dual-redundant send failed: %d", status);
    sapi_safestate_trigger();  // Safe-state transition
}

// Application receives on vital channel (waits for both CPUs to agree)
signal_reply_t reply;
status = sapi_vital_receive(vital_signal, &reply, sizeof(reply), 100);

if (status == SAPI_STATUS_OK) {
    // Both CPUs agreed on reply - safe to use
    printf("Signal update confirmed by both CPUs\n");
} else if (status == SAPI_STATUS_ERROR) {
    // CPUs disagreed - likely a fault
    SAPI_LOG_ERROR("CPU disagreement detected!");
    sapi_safestate_trigger();
}
```

---

## Example: 2oo3 Vital Channel (Triple-Redundant CPU)

### Architecture

```
Triple-Core Processor (3 CPUs)
├─ CPU A (Core 1)
│  ├─ Channel A (IPC to CPUs B & C)
├─ CPU B (Core 2)
│  ├─ Channel B (IPC to CPUs A & C)
└─ CPU C (Core 3)
   └─ Channel C (IPC to CPUs A & B)

They communicate via 2oo3 voting:
- Majority (≥2) must agree
- Can tolerate 1 CPU fault
- If only 1 agrees: fault isolation
```

### Application Code

```c
// Create three underlying IPC channels
sapi_ipc_handle_t channels[3] = {channel_a, channel_b, channel_c};

sapi_vital_channel_config_t vital_config = {
    .name = "triple_redundant_signal",
    .strategy = SAPI_VOTING_2OO3,  // Majority vote
    .channels = channels,
    .num_channels = 3,
    .on_disagreement = handle_triple_disagreement,
    .timeout_ms = 100
};

sapi_vital_channel_t vital_signal;
sapi_vital_channel_create(&vital_signal, &vital_config);

// Send: broadcasts to all 3 CPUs
signal_command_t cmd = {.signal_id = 5, .state = SIGNAL_RED};
sapi_vital_send(vital_signal, &cmd, sizeof(cmd), 100);

// Receive: waits for majority vote
signal_reply_t reply;
status = sapi_vital_receive(vital_signal, &reply, sizeof(reply), 100);

switch (status) {
    case SAPI_STATUS_OK:
        // ≥2 CPUs agreed - safe to use reply
        // (may ignore disagreement of 1 faulty CPU)
        break;
    
    case SAPI_STATUS_ERROR:
        // Majority vote failed (≤1 CPU agreed)
        // Likely multiple faults - trigger safe-state
        sapi_safestate_trigger();
        break;
}

// Monitor health to detect faulty CPU
sapi_channel_health_t health;
sapi_vital_get_health(vital_signal, &health);

// Identify which CPU(s) are faulty
for (int i = 0; i < 3; i++) {
    if (!health.channel_ok[i]) {
        SAPI_LOG_ERROR("CPU %d is faulty, isolating...", i);
        isolate_faulty_cpu(i);
    }
}
```

---

## Example: Non-Vital Channel (Single CPU)

### Application Code

```c
// Create single non-vital channel (best-effort)
sapi_ipc_handle_t underlying_ch;
sapi_ipc_create(&storage, &config, &underlying_ch);

sapi_nonvital_channel_config_t nv_config = {
    .name = "diagnostic_log",
    .underlying_channel = underlying_ch,
    .timeout_ms = 1000  // Generous timeout
};

sapi_nonvital_channel_t log_channel;
sapi_nonvital_channel_create(&log_channel, &nv_config);

// Send log message (best-effort)
log_msg_t msg = {.level = LOG_INFO, .data = "System operational"};
status = sapi_nonvital_send(log_channel, &msg, sizeof(msg), 1000);

if (status != SAPI_STATUS_OK) {
    // Non-critical, so log failure but continue
    fprintf(stderr, "Log send failed (non-critical): %d\n", status);
    // No safe-state trigger needed
}

// Receive from non-vital channel
log_msg_t reply;
status = sapi_nonvital_receive(log_channel, &reply, sizeof(reply), 1000);

if (status == SAPI_STATUS_OK) {
    printf("Log acknowledged\n");
} else if (status == SAPI_STATUS_TIMEOUT) {
    // Timeout on non-vital is acceptable
    printf("Log acknowledgment timeout (non-critical)\n");
}
```

---

## Safety Properties

### 2oo2 Redundancy

| Property | Guarantee |
|----------|-----------|
| **Single-Point Failure Detection** | ✓ Yes (via disagreement) |
| **Fault Tolerance** | 0 (any fault detected) |
| **Safe Failure Mode** | Safe-state on disagreement |
| **Typical Use** | Dual-core, dual-NVM, dual-communication |

### 2oo3 Redundancy

| Property | Guarantee |
|----------|-----------|
| **Single-Point Failure Detection** | ✓ Yes (majority vote) |
| **Fault Tolerance** | 1 faulty channel |
| **Safe Failure Mode** | Majority vote; isolate minority |
| **Typical Use** | Triple-core, aerospace, critical control |

### Non-Vital Channel

| Property | Guarantee |
|----------|-----------|
| **Single-Point Failure Tolerance** | No (fails over) |
| **Graceful Degradation** | Yes (application retry logic) |
| **Safe Failure Mode** | Best-effort; application decides |
| **Typical Use** | Logging, non-critical updates, diagnostics |

---

## Integration with Framework

### In IPC Module

```c
// Vital channel = wrapper around base IPC send/receive
// - Manages multiple underlying IPC handles
// - Applies voting logic
// - Monitors disagreement
// - Triggers faults on voting failure

// Non-vital channel = thin wrapper around single IPC
// - Provides unified API
// - Timeout handling
// - Optional health tracking
```

### In Watchdog Module

```c
// Monitor channel health via sapi_channel_get_health()
// Detect disagreement patterns (indicates fault)
// Trigger safe-state on majority vote failure
```

### In Diagnostics Module

```c
// Log channel-specific faults
// Track disagreement events
// Report per-CPU health to monitoring system
```

---

## Roadmap Integration

### New GitHub Issues (Tier 1)

- **#24** Vital Channel Abstraction (2oo2 / 2oo3)
- **#25** Channel Health Monitoring & Fault Detection
- **#26** Non-Vital Channel Fallback
- **#27** CPU Fault Isolation Strategy

### v0.2.0 Additions

```
IPC Enhancements (#17-23)
  ├─ Request-Reply (#17)
  ├─ Pub-Subscribe (#18)
  └─ ...

NEW: Hardware Redundancy (#24-27)
  ├─ Vital Channel 2oo2/2oo3 (#24)
  ├─ Health Monitoring (#25)
  ├─ Non-Vital Channels (#26)
  └─ Fault Isolation (#27)
```

---

## Critical Safety Requirement: Checkpoint Synchronization & Pre-Commit

### Checkpoint Barrier Synchronization

**Before any synchronization or voting, all nodes MUST reach the same checkpoint.**

The framework enforces temporal consistency by requiring all nodes to:
1. Reach a defined checkpoint in their processing logic
2. Wait at that checkpoint for other nodes (with configurable timeout, e.g., 200ms)
3. Proceed together once all nodes are synchronized
4. THEN exchange data and perform voting

This ensures that nodes are processing the **same logical input** at the **same logical point** before any comparison occurs. Without checkpoint synchronization, nodes could be at different stages of processing, making voting meaningless or revealing inconsistent state.

```
┌──────────────────────────────────────────────────────┐
│  CHECKPOINT BARRIER SYNCHRONIZATION (Configurable)  │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Site A    │  Site B    │  Site C                   │
│            │            │                           │
│  Process   │  Process   │  Process                  │
│  input     │  input     │  input                    │
│  (at own   │  (at own   │  (at own                 │
│   speed)   │   speed)   │   speed)                 │
│    │       │    │       │    │                     │
│    ▼       │    ▼       │    ▼                     │
│  [Checkpoint]│  [Checkpoint]│  [Checkpoint]        │
│  (ready)   │  (ready)   │  (blocked)              │
│    │       │    │       │    ║                     │
│    │       │    │       │    ║ waiting for A & B  │
│    │       │    │       │    ║ (max 200ms)        │
│    │       │    │       │    ║                     │
│  <<────────┼────┼────────────>>                    │
│  All nodes synchronized at checkpoint               │
│    │       │    │       │    │                     │
│  TIMEOUT EXCEEDED? ─→ Fault detected, abort        │
│    │       │    │       │    │                     │
│    ▼       ▼    ▼       ▼    ▼                     │
│  ╔═══════════════════════════════╗                │
│  ║  All nodes at checkpoint       ║                │
│  ║  (synchronized in TIME)        ║                │
│  ║  (same LOGICAL POINT)          ║                │
│  ║  (same INPUT state)            ║                │
│  ╚═══════════════════════════════╝                │
│    │                               │                │
│    └─────────────┬─────────────────┘                │
│                  │                                  │
│          SYNCHRONIZE DATA                          │
│          EXCHANGE OUTPUTS                          │
│          PERFORM VOTING                            │
│          COMMIT & OUTPUT                           │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Complete Execution Flow: Checkpoint → Sync → Vote → Output

```
CHANNEL EXECUTION TIMELINE
══════════════════════════════════════════════════════════════════════

Phase 0: CHECKPOINT BARRIER (Configurable Timeout, e.g., 200ms)
──────────────────────────────────────────────────────────────────────
  All nodes process independently until reaching defined checkpoint.
  Faster nodes wait at barrier for slower nodes.
  Max delay configurable (typical: 50ms to 500ms).
  
  Status: Site A ✓  Site B ✓  Site C ⏳ → Site C ✓ → SYNCHRONIZED
  
  On timeout: Faulty node detected, safe-state triggered.
  Guarantee: All proceeding nodes are at same logical point.

Phase 1: PROCESS (to checkpoint)
──────────────────────────────────────────────────────────────────────
  ├─ All nodes receive same input independently
  ├─ All nodes process until checkpoint
  ├─ Generate candidate output (staged, not committed)
  └─ Node processing speed may vary, but result is at checkpoint

Phase 2: CHECKPOINT SYNCHRONIZATION
──────────────────────────────────────────────────────────────────────
  ├─ Wait at checkpoint for all nodes to reach it
  ├─ Exchange checkpoint status (heartbeat)
  ├─ If any node doesn't reach checkpoint within max_delay
  │  └─→ Fault detected → Safe-state trigger
  └─ All nodes proceed only when synchronized

Phase 3: DATA SYNCHRONIZATION
──────────────────────────────────────────────────────────────────────
  ├─ Exchange processed data across all nodes
  ├─ Verify all nodes have identical output data
  ├─ Backup confirms sync (active-passive mode)
  └─ Detect disagreements

Phase 4: VOTING / CROSS-COMPARISON
──────────────────────────────────────────────────────────────────────
  ├─ Apply voting logic (2oo2, 2oo3, etc.)
  ├─ If voting FAILS → ABORT (no output)
  └─ If voting SUCCEEDS → proceed to commit

Phase 5: COMMIT & OUTPUT
──────────────────────────────────────────────────────────────────────
  ├─ All nodes commit to decision
  ├─ All nodes acknowledge commitment
  └─ Output sent ONLY after checkpoint + sync + vote + commit

Failure Paths (all trigger safe-state):
  ├─ Checkpoint fails (timeout)  → Faulty node detected
  ├─ Sync fails (data mismatch)  → Consensus broken
  ├─ Voting fails (no consensus) → Decision not possible
  └─ Commit fails (safety fault) → Atomic commit failed
  
  ⟹ NO OUTPUT CAN ESCAPE WITHOUT FULL CONSENSUS

════════════════════════════════════════════════════════════════════════
```

### API Design: Checkpoint-Aware Channels

```c
/**
 * @brief Checkpoint configuration
 *
 * Defines a synchronization point where all nodes must wait for each other.
 */
typedef struct {
    uint32_t checkpoint_id;         /**< Unique ID for this checkpoint */
    sapi_duration_ms_t max_delay_ms;/**< Max allowed delay (e.g., 200ms) */
    uint32_t expected_node_count;   /**< How many nodes to wait for */
} sapi_checkpoint_config_t;

/**
 * @brief Register a checkpoint in channel processing
 *
 * Called by application at a specific point in processing logic.
 * Blocks until all nodes reach this checkpoint (or timeout).
 *
 * Usage Pattern:
 *   ① Process input to a known point (same on all nodes)
 *   ② Call sapi_channel_checkpoint()
 *   ③ Wait for all nodes to sync (blocking, with timeout)
 *   ④ All nodes resume together (guaranteed at same logical point)
 *   ⑤ Continue with data synchronization & voting
 *
 * Example:
 * @code
 * // All RBC nodes reach checkpoint after signal processing
 * sapi_checkpoint_config_t ckpt = {
 *     .checkpoint_id = 1,
 *     .max_delay_ms = 200,           // Allow 200ms delay for slower node
 *     .expected_node_count = 3       // Waiting for all 3 sites
 * };
 * 
 * status = sapi_channel_checkpoint(vital_signal, &ckpt);
 * if (status != SAPI_STATUS_OK) {
 *     // One node didn't reach checkpoint in time
 *     // Fault detected → safe-state
 *     sapi_safestate_trigger();
 * }
 * // All nodes guaranteed at same logical point now
 * @endcode
 *
 * @param channel Vital or non-vital channel
 * @param config Checkpoint configuration
 * @return SAPI_STATUS_OK if all nodes synchronized at checkpoint
 *         SAPI_STATUS_TIMEOUT if some nodes didn't reach checkpoint
 *         SAPI_STATUS_ERROR if channel fault detected
 */
sapi_status_t sapi_channel_checkpoint(sapi_vital_channel_t channel,
                                       const sapi_checkpoint_config_t *config);

/**
 * @brief Query checkpoint status (non-blocking diagnostics)
 *
 * Useful for logging and diagnostics. Tells which nodes have reached
 * which checkpoint without blocking.
 *
 * @param channel Vital channel
 * @param checkpoint_id ID to query
 * @param status_out Bitmap: bit N = 1 if node N reached checkpoint
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_channel_checkpoint_status(sapi_vital_channel_t channel,
                                              uint32_t checkpoint_id,
                                              uint32_t *status_out);

/**
 * @brief Stage output (prepare but don't send)
 *
 * After checkpoint synchronization and processing, stage the output locally.
 * Output is NOT sent yet — waiting for data sync + voting.
 *
 * @param channel Vital or non-vital channel
 * @param output_data Candidate output to stage
 * @param size Output data size
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_channel_stage_output(sapi_vital_channel_t channel,
                                         const void *output_data,
                                         size_t size);

/**
 * @brief Synchronize staged output across nodes
 *
 * Exchange staged outputs between all nodes (now guaranteed at same
 * checkpoint). Verify that all nodes have identical staged data.
 *
 * @param channel Vital channel
 * @param timeout_ms Max wait for all nodes to sync data
 * @return SAPI_STATUS_OK if sync succeeded
 *         SAPI_STATUS_ERROR if nodes disagree
 *         SAPI_STATUS_TIMEOUT if nodes don't respond
 */
sapi_status_t sapi_channel_sync_output(sapi_vital_channel_t channel,
                                        sapi_duration_ms_t timeout_ms);

/**
 * @brief Commit output after successful voting
 *
 * Only call this if sapi_channel_sync_output() succeeded.
 * Commits staged output and marks as ready for transmission.
 *
 * @param channel Vital channel
 * @return SAPI_STATUS_OK if commit succeeded
 *         SAPI_STATUS_ERROR if commit failed (safety fault)
 */
sapi_status_t sapi_channel_commit_output(sapi_vital_channel_t channel);

/**
 * @brief Send output (only after checkpoint + sync + commit)
 *
 * Transmit committed output to external system.
 * Should only be called after:
 *   1. sapi_channel_checkpoint() succeeded (all nodes at same point)
 *   2. sapi_channel_sync_output() succeeded (data synchronized)
 *   3. sapi_channel_commit_output() succeeded (consensus committed)
 *
 * @param channel Vital channel
 * @param output_data Committed output
 * @param size Output size
 * @return SAPI_STATUS_OK if send succeeded
 */
sapi_status_t sapi_channel_send_output(sapi_vital_channel_t channel,
                                        const void *output_data,
                                        size_t size);

/**
 * @brief Abort staged output (on voting failure)
 *
 * Called if voting/cross-comparison fails.
 * Discards staged output without sending.
 * Triggers safe-state transition.
 *
 * @param channel Vital channel
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_channel_abort_output(sapi_vital_channel_t channel);
```

### Application Flow Example (Online Mode - 2oo3)

```c
// Three RBC sites, 2oo3 voting

// ============================================================================
// PHASE 1: PROCESS (to checkpoint)
// ============================================================================
signal_result_t my_result = process_rbc_signal();

// Stage output locally (not sent yet)
sapi_channel_stage_output(vital_signal, &my_result, sizeof(my_result));
SAPI_LOG_INFO("Staged output: signal=%d", my_result.signal_state);

// ============================================================================
// PHASE 2: CHECKPOINT BARRIER (All nodes synchronized in time)
// ============================================================================
// All three sites reach checkpoint after signal processing
// Slower sites wait for faster sites (max 200ms)
sapi_checkpoint_config_t ckpt = {
    .checkpoint_id = 1,
    .max_delay_ms = 200,           // Allow 200ms delay
    .expected_node_count = 3       // Waiting for all 3 sites
};

status = sapi_channel_checkpoint(vital_signal, &ckpt);

if (status != SAPI_STATUS_OK) {
    SAPI_LOG_ERROR("Checkpoint timeout: one site didn't respond!");
    // One node is faulty or too slow → safe-state
    sapi_channel_abort_output(vital_signal);
    sapi_safestate_trigger();
    return;
}

SAPI_LOG_INFO("Checkpoint reached: all 3 sites synchronized (same logical point)");

// ============================================================================
// PHASE 3: DATA SYNCHRONIZATION (at checkpoint)
// ============================================================================
// All three sites now guaranteed at same logical point
// Exchange their staged outputs
status = sapi_channel_sync_output(vital_signal, 100);  // 100ms timeout

if (status != SAPI_STATUS_OK) {
    SAPI_LOG_ERROR("Sync failed: sites disagree on output!");
    // Data is NOT synchronized — do NOT proceed to output
    sapi_channel_abort_output(vital_signal);
    sapi_safestate_trigger();  // Trigger safe-state
    return;
}

SAPI_LOG_INFO("Data sync succeeded: all sites have identical output");

// ============================================================================
// PHASE 4: VOTING (verify consensus)
// ============================================================================
sapi_channel_health_t health;
sapi_vital_get_health(vital_signal, &health);

if (!health.majority_vote_ok) {
    SAPI_LOG_ERROR("Voting failed: no majority agreement!");
    sapi_channel_abort_output(vital_signal);
    sapi_safestate_trigger();
    return;
}

SAPI_LOG_INFO("Voting succeeded: ≥2 sites agree");

// ============================================================================
// PHASE 5: COMMIT & OUTPUT
// ============================================================================
// All nodes commit to the decision
status = sapi_channel_commit_output(vital_signal);

if (status != SAPI_STATUS_OK) {
    SAPI_LOG_ERROR("Commit failed: safety fault detected!");
    sapi_safestate_trigger();
    return;
}

SAPI_LOG_INFO("Output committed: safe to send");

// NOW send output (after checkpoint + sync + voting + commit)
status = sapi_channel_send_output(vital_signal, 
                                   &my_result, sizeof(my_result));

if (status == SAPI_STATUS_OK) {
    SAPI_LOG_INFO("Output sent successfully (all constraints satisfied)");
} else {
    SAPI_LOG_ERROR("Output send failed!");
    sapi_safestate_trigger();
}
```

### Application Flow Example (Hot Standby - Active-Passive)

```c
// Primary site (active), Backup site (standby)

// ============================================================================
// PHASE 1: PROCESS (Primary only, to checkpoint)
// ============================================================================
signal_result_t result = process_rbc_signal();  // Only primary processes

// Stage output locally
sapi_channel_stage_output(vital_signal, &result, sizeof(result));
SAPI_LOG_INFO("Primary staged output");

// ============================================================================
// PHASE 2: CHECKPOINT BARRIER (Primary waits for Backup readiness)
// ============================================================================
// Primary reaches checkpoint and waits for backup to be ready
// Backup must acknowledge it's ready to receive state
sapi_checkpoint_config_t ckpt = {
    .checkpoint_id = 1,
    .max_delay_ms = 200,           // Allow 200ms for backup to catch up
    .expected_node_count = 2       // Primary + Backup
};

status = sapi_channel_checkpoint(vital_signal, &ckpt);

if (status != SAPI_STATUS_OK) {
    SAPI_LOG_ERROR("Checkpoint timeout: backup not responding!");
    // Backup is faulty or offline → safe-state
    sapi_channel_abort_output(vital_signal);
    sapi_safestate_trigger();
    return;
}

SAPI_LOG_INFO("Checkpoint reached: backup confirmed ready (synchronized)");

// ============================================================================
// PHASE 3: DATA SYNCHRONIZATION (Primary → Backup)
// ============================================================================
// Primary now sends staged output to backup
// Backup must echo back to confirm receipt
status = sapi_channel_sync_output(vital_signal, 100);

if (status != SAPI_STATUS_OK) {
    SAPI_LOG_ERROR("Sync with backup failed!");
    // Backup didn't acknowledge or data mismatch
    sapi_channel_abort_output(vital_signal);
    sapi_safestate_trigger();  // Trigger safe-state
    return;
}

SAPI_LOG_INFO("Backup confirmed data sync (echoed back correctly)");

// ============================================================================
// PHASE 4: VOTING (Primary agrees with Backup echo)
// ============================================================================
// Backup echoes back the data it received
// Primary verifies it matches what was sent
// This is the "2oo2 consensus" in standby mode

sapi_channel_health_t health;
sapi_vital_get_health(vital_signal, &health);

if (!health.majority_vote_ok) {
    SAPI_LOG_ERROR("Backup data mismatch detected!");
    sapi_channel_abort_output(vital_signal);
    sapi_safestate_trigger();
    return;
}

SAPI_LOG_INFO("Voting succeeded: Primary and Backup agree");

// ============================================================================
// PHASE 5: COMMIT & OUTPUT
// ============================================================================
status = sapi_channel_commit_output(vital_signal);

if (status == SAPI_STATUS_OK) {
    // NOW send output (after checkpoint + backup confirms sync)
    status = sapi_channel_send_output(vital_signal, 
                                      &result, sizeof(result));
    
    if (status == SAPI_STATUS_OK) {
        SAPI_LOG_INFO("Output sent (backup in sync, consensus reached)");
    } else {
        SAPI_LOG_ERROR("Output send failed!");
        sapi_safestate_trigger();
    }
} else {
    SAPI_LOG_ERROR("Commit failed: safety fault detected!");
    sapi_safestate_trigger();
}
```

### Safety Guarantees

With checkpoint synchronization + pre-commit pattern:

✅ **Temporal Consistency (Checkpoint Barrier)**
  - All nodes reach same logical processing point
  - Faster nodes wait for slower nodes (configurable timeout, e.g., 200ms)
  - Faulty node detected if checkpoint timeout exceeded
  - Guarantee: All proceeding nodes are synchronized in TIME

✅ **Logical Consistency (Same Input State)**
  - All nodes process same input to checkpoint
  - No node at different logical stage when voting occurs
  - Voting is meaningful (comparing apples-to-apples)
  - No race conditions (checkpoint barrier enforces order)

✅ **No Contradictory Outputs**
  - Output only sent when ALL nodes agree (after checkpoint + sync)
  - If any node disagrees, output is aborted
  - No node sends output before consensus

✅ **Atomic Consistency**
  - Checkpoint synchronization (all nodes at same point)
  - Data synchronization (all nodes have identical data)
  - Voting (consensus verified)
  - Commit (all nodes commit together)
  - No partial outputs

✅ **Fail-Safe at Every Stage**
  - Checkpoint timeout → faulty node detected → safe-state
  - Data sync failure → consensus broken → abort + safe-state
  - Voting failure → no majority → abort + safe-state
  - Commit failure → atomic commit failed → abort + safe-state
  - No output escapes without full consensus

✅ **Audit Trail**
  - Log checkpoint entry (which nodes reached it, timing)
  - Log checkpoint completion or timeout
  - Log data sync status
  - Log voting results
  - Log commit status
  - Full traceability for certification

✅ **Graceful Degradation (2oo3)**
  - If Site C misses checkpoint (timeout), it's isolated
  - Only Sites A & B proceed (now 2oo2 mode)
  - Sites A & B reach checkpoint and vote
  - No contradictory outputs from any site
  - Majority decision proceeds

✅ **Configurable Fault Tolerance**
  - Checkpoint timeout configurable per application (50ms–500ms typical)
  - Allows tuning for different hardware speeds
  - Slower CPUs get longer wait times
  - Faster systems can use shorter timeouts for quicker fault detection

---

## Cluster Redundancy: Online vs. Hot Standby

### Online Mode (Active-Active)

**All nodes process simultaneously; must agree.**

```
┌──────────────────────────────────────┐
│  Redundant Cluster (Online Mode)     │
├──────────────────────────────────────┤
│                                      │
│  ┌────────────┐   ┌────────────┐   │
│  │ RBC Node 1 │   │ RBC Node 2 │   │
│  │ (Active)   │   │ (Active)   │   │
│  └──────┬─────┘   └──────┬─────┘   │
│         │                 │         │
│    Process Signal X  Process Signal X│
│         │                 │         │
│         └────────┬────────┘         │
│                  │                  │
│            Must Agree on Output     │
│            (Voter / Cross-Compare)  │
│                  │                  │
│                  ▼                  │
│             OUTPUT (if agreed)      │
│             or FAULT (disagreement) │
│                                     │
└──────────────────────────────────────┘
```

**Properties:**
- Both nodes process continuously
- Synchronization required
- Higher detection latency (both must respond)
- Single-point failure is detected immediately
- Better utilization (both working)

**Use Case:**
- Dual-site ERTMS control centers
- Both sites process RBC logic
- Voting on each decision
- Detect site failure within milliseconds

### Hot Standby Mode (Active-Passive)

**Primary node processes; backup ready to take over on failure.**

```
┌──────────────────────────────────────┐
│ Redundant Cluster (Hot Standby)      │
├──────────────────────────────────────┤
│                                      │
│  ┌────────────┐   ┌────────────┐   │
│  │ RBC Node 1 │   │ RBC Node 2 │   │
│  │ (Primary)  │   │ (Standby)  │   │
│  │ [ACTIVE]   │   │ [READY]    │   │
│  └──────┬─────┘   └──────┬─────┘   │
│         │                 │         │
│    Process Input     Replicate State │
│         │                 │         │
│         └────────┬────────┘         │
│                  │                  │
│            Heartbeat / State Sync   │
│                  │                  │
│                  ▼                  │
│             PRIMARY OUTPUT          │
│                                     │
│    [If Primary Fails]               │
│    Standby Detects Loss & Takes Over│
│                                     │
└──────────────────────────────────────┘
```

**Properties:**
- Only primary processes (lower latency)
- Standby replicates state (hot = ready)
- Failure detection via heartbeat
- Failover latency: heartbeat timeout + warmup
- More efficient (single-system work)

**Use Case:**
- Primary ERTMS control center
- Backup center in standby
- Heartbeat every 100ms
- On primary failure, backup takes over within 200ms

---

## Voting Topologies: Voter vs. Cross-Comparison

### Voter Topology (Centralized)

**Central voter collects from multiple nodes and decides.**

```
       Node A            Node B            Node C
        │                 │                 │
        ├─ Output A ──────┤                 │
        │                 ├─ Output B ──────┤
        │                 │                 ├─ Central Voter
        │                 │                 │
        └─────────────────┴─────────────────┘
                          │
                    Voting Logic
                    (2oo3 majority)
                          │
                          ▼
                     Final Output
```

**Characteristics:**
- Centralized voting authority
- Voter receives outputs from all nodes
- Clear winner (majority vote)
- Single voter is potential bottleneck
- Deterministic, easy to verify

**API:**
```c
sapi_status_t sapi_cluster_voter_create(sapi_cluster_voter_t *voter,
                                         sapi_voter_config_t *config);
// Voter collects from 2oo2 or 2oo3 nodes
// Determines majority
// Outputs unified result
```

### Cross-Comparison Topology (Distributed)

**Nodes peer-to-peer compare results; gossip to consensus.**

```
       Node A            Node B            Node C
        │                 │                 │
        ├─ Compare ────────┤                 │
        ├─ Compare ─────────────────────────┤
        │                 │                 │
        │    ┌────────────┴─────────────┐   │
        │    │                          │   │
        │    ├─ Compare ────────────────┤   │
        │    │                          │   │
        │    └─→ Gossip Protocol ←──────┘   │
        │         (Achieve Consensus)       │
        │                                   │
        ▼                  ▼                ▼
    Decision via       Decision via     Decision via
    Comparison &       Comparison &     Comparison &
    Consensus          Consensus        Consensus
```

**Characteristics:**
- Distributed, no central authority
- Nodes compare with peers (typically 2-3 neighbors)
- Consensus-based decision
- No single point of failure
- More resilient to voter failure
- Slightly higher latency (gossip rounds)

**API:**
```c
sapi_status_t sapi_cluster_peer_create(sapi_cluster_peer_t *peer,
                                        sapi_peer_config_t *config);
// Peer compares own result with neighbors
// Exchanges comparison results
// Reaches consensus
// Outputs local decision based on consensus
```

---

## Cluster Configuration Examples

### Example 1: Dual-Site Online Voting

```c
// Two ERTMS RBC sites in online (active-active) mode
sapi_cluster_config_t config = {
    .mode = SAPI_CLUSTER_ONLINE,        // Active-active
    .topology = SAPI_VOTER,             // Central voter
    .num_nodes = 2,
    .voting_strategy = SAPI_VOTING_2OO2 // Both must agree
};

// Node A (runs on Site A)
sapi_cluster_node_t node_a;
sapi_cluster_node_create(&node_a, &config);

// Node A processes signal, sends to voter
signal_result_t my_result = process_signal();
sapi_cluster_send_to_voter(node_a, &my_result, sizeof(my_result));

// Wait for voter's decision (2oo2: both sites must agree)
signal_result_t voted_result;
sapi_cluster_receive_voted(node_a, &voted_result, sizeof(voted_result), 500);
// If timeout → disagreement with Site B → FAULT
```

### Example 2: Triple-Site Hot Standby with Cross-Comparison

```c
// Three ERTMS sites: 1 primary, 2 backups (hot standby)
// Sites use peer-to-peer cross-comparison (gossip)

// Primary site
sapi_cluster_config_t primary_config = {
    .mode = SAPI_CLUSTER_HOT_STANDBY,      // Active-passive
    .role = SAPI_CLUSTER_ROLE_PRIMARY,
    .topology = SAPI_CROSS_COMPARISON,     // Peer-to-peer
    .num_nodes = 3,
    .heartbeat_ms = 100
};

sapi_cluster_node_t primary;
sapi_cluster_node_create(&primary, &primary_config);

// Primary processes and broadcasts result
signal_result_t result = process_signal();
sapi_cluster_broadcast(primary, &result, sizeof(result));

// Backups compare with each other and with primary
// Gossip consensus in background
// If primary heartbeat fails → backup takes over

// Backup site (standby)
sapi_cluster_config_t backup_config = {
    .mode = SAPI_CLUSTER_HOT_STANDBY,
    .role = SAPI_CLUSTER_ROLE_STANDBY,
    .topology = SAPI_CROSS_COMPARISON,
    .heartbeat_timeout_ms = 200  // Take over if no primary heartbeat
};

sapi_cluster_node_t backup;
sapi_cluster_node_create(&backup, &backup_config);

// Backup replicates primary's state
sapi_cluster_replicate_state(backup, primary_state, sizeof(primary_state));

// On primary failure (heartbeat timeout)
status = sapi_cluster_wait_for_heartbeat(backup, 200);
if (status == SAPI_STATUS_TIMEOUT) {
    // Primary is down, take over
    sapi_cluster_promote_to_primary(backup);
    SAPI_LOG_ERROR("Primary site down, promoted to primary");
}
```

---

## Roadmap Integration (Updated)

### New GitHub Issues (Tier 1)

- **#24** Vital Channel Abstraction (2oo2 / 2oo3)
- **#25** Channel Health Monitoring & Fault Detection
- **#26** Non-Vital Channel Fallback
- **#27** CPU Fault Isolation & Recovery
- **#28** Cluster Redundancy: Online Mode (Active-Active)
- **#29** Cluster Redundancy: Hot Standby (Active-Passive)
- **#30** Voter Topology (Centralized Voting)
- **#31** Cross-Comparison Topology (Distributed Gossip)

### v0.2.0 Scope Expansion

```
Hardware Redundancy & Clustering (#24-31)
├─ Single-System Redundancy (#24-27)
│  ├─ Vital channels (2oo2/2oo3)
│  ├─ Health monitoring
│  ├─ Non-vital channels
│  └─ Fault isolation
│
└─ Cluster Redundancy (#28-31)
   ├─ Online mode (active-active)
   ├─ Hot standby (active-passive)
   ├─ Voter topology (centralized)
   └─ Cross-comparison (distributed)

**Estimated Effort:** 4-5 weeks
**Total Tier 1 (with IPC):** 7-9 weeks
```

---

## References

- EN 50128 / EN 50129 — Safety standards for railway systems
- IEEE 1202 — Standard for Distributed Control Systems
- "Fault Tolerance in Distributed Computing" — Classic texts on voting
- IEC 61508 — Functional Safety of E/E/PE safety-related systems
- ERTMS/ETCS Specifications — Multi-site RBC redundancy requirements

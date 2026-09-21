# Watchdog Mechanism - Platform_RTE Design

📋 **STATUS:** API DESIGN COMPLETE, IMPLEMENTATION IN PROGRESS
- API specification: `include/safeapi/watchdog/rte_watchdog.h` ✅
- Implementation: `src/watchdog/rte_watchdog.c` (stub code, being developed)
- Target release: safeAPIFramework v0.3.0

## Overview

The watchdog mechanism detects system hang conditions (system-level, task-level, channel-level) and triggers configurable recovery actions (reboot, safe-state, failover).

**Key Concepts:**
- **System Watchdog:** Detects if entire system is unresponsive
- **Task Watchdog:** Detects if a specific task/thread is hung
- **Checkpoint Watchdog:** Detects if a node doesn't reach checkpoint in time
- **Channel Watchdog:** Detects if IPC/redundancy channel is stuck
- **Recovery Action:** Configurable response (reboot, safe-state, failover, log)

---

## Why Watchdogs Matter (SIL 4)

### Problem: Silent Failures
```
RBC logic hangs silently
    ↓
Application stops sending output
    ↓
External systems wait indefinitely
    ↓
Safety violation (no control signal)
```

### Solution: Active Monitoring
```
Task reaches checkpoint every 100ms (guaranteed)
    ↓
Watchdog "kicks" (resets) timer
    ↓
If no kick within timeout (e.g., 500ms)
    ↓
Watchdog fires → RECOVERY ACTION
    ↓
Safe-state / reboot / failover triggered
```

---

## Architecture

```
┌──────────────────────────────────────────────────┐
│              Application Layer                   │
│  (Tasks, Threads, Checkpoint Handlers)           │
└──────────────┬───────────────────────────────────┘
               │ (periodic kick/pet)
        ┌──────▼──────────────────┐
        │   Watchdog Manager      │
        │ (Central coordinator)   │
        ├────────────────────────┤
        │ • System watchdog      │
        │ • Task watchdog(s)     │
        │ • Channel watchdog(s)  │
        │ • Checkpoint watchdog  │
        └──┬────┬────────┬────┬──┘
           │    │        │    │
    ┌──────▼──┐ │   ┌────▼──┐ │
    │  Timer  │ │   │ Reset │ │
    │ (HW/SW) │ │   │ Logic │ │
    └─────────┘ │   └───────┘ │
                │              │
         ┌──────▼──────────────▼──┐
         │   Recovery Actions     │
         ├────────────────────────┤
         │ • Trigger safe-state   │
         │ • Initiate reboot      │
         │ • Failover to backup   │
         │ • Log watchdog event   │
         └────────────────────────┘
```

---

## API Design

### Core Types

```c
/**
 * @brief Watchdog handle (opaque)
 */
typedef struct rte_watchdog_s *rte_watchdog_t;

/**
 * @brief Watchdog type
 */
typedef enum {
    RTE_WATCHDOG_SYSTEM,      // Entire system hung detection
    RTE_WATCHDOG_TASK,        // Specific task/thread hung
    RTE_WATCHDOG_CHANNEL,     // IPC channel stuck
    RTE_WATCHDOG_CHECKPOINT   // Node didn't reach checkpoint
} rte_watchdog_type_t;

/**
 * @brief Recovery action when watchdog fires
 */
typedef enum {
    RTE_WATCHDOG_ACTION_LOG,           // Log event only
    RTE_WATCHDOG_ACTION_SAFESTATE,     // Trigger safe-state transition
    RTE_WATCHDOG_ACTION_REBOOT,        // Reboot system
    RTE_WATCHDOG_ACTION_FAILOVER,      // Failover to backup (cluster only)
    RTE_WATCHDOG_ACTION_CUSTOM         // Custom callback
} rte_watchdog_action_t;

/**
 * @brief Watchdog configuration
 */
typedef struct {
    rte_watchdog_type_t type;
    const char *name;
    rte_duration_ms_t timeout_ms;      // Kick deadline
    rte_watchdog_action_t action;
    void (*custom_action)(void *context);
    void *context;
} rte_watchdog_config_t;

/**
 * @brief Watchdog health/status
 */
typedef struct {
    uint8_t active;                 // 1 = watchdog enabled
    uint32_t kicks;                 // Total kicks/pets
    uint32_t fires;                 // Total fires
    uint32_t recoveries;            // Total recovery actions
    rte_duration_ms_t time_since_last_kick;  // ms since last kick
    rte_duration_ms_t time_until_fire;       // ms until timeout
} rte_watchdog_status_t;
```

### System Watchdog API

```c
/**
 * @brief Create system-level watchdog
 *
 * Detects if entire RBC system is hung (no task making progress).
 * Any task can "kick" the watchdog to prove system liveness.
 *
 * Usage:
 * @code
 * rte_watchdog_config_t config = {
 *     .type = RTE_WATCHDOG_SYSTEM,
 *     .name = "rbc_system_wd",
 *     .timeout_ms = 1000,            // 1 second
 *     .action = RTE_WATCHDOG_ACTION_SAFESTATE
 * };
 * rte_watchdog_t wd;
 * rte_watchdog_create(&wd, &config);
 * @endcode
 *
 * @param handle_out Receives watchdog handle
 * @param config Watchdog configuration
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_watchdog_create(rte_watchdog_t *handle_out,
                                    const rte_watchdog_config_t *config);

/**
 * @brief Kick (pet) system watchdog
 *
 * Called periodically by ANY task to prove system liveness.
 * Resets the timeout countdown.
 *
 * Usage:
 * @code
 * // Main event loop
 * while (true) {
 *     process_events();
 *     
 *     // Prove we're alive
 *     rte_watchdog_kick(system_wd);
 *     
 *     sleep_ms(100);
 * }
 * @endcode
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_watchdog_kick(rte_watchdog_t watchdog);

/**
 * @brief Start watchdog timer
 *
 * Enables watchdog monitoring. Timer begins counting.
 * Must be called after rte_watchdog_create().
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_watchdog_start(rte_watchdog_t watchdog);

/**
 * @brief Stop watchdog timer
 *
 * Disables watchdog (e.g., during shutdown or maintenance).
 * Timer stops counting; no timeout will occur.
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_watchdog_stop(rte_watchdog_t watchdog);

/**
 * @brief Get watchdog status
 *
 * Non-blocking query of watchdog state.
 *
 * @param watchdog Watchdog handle
 * @param status_out Receives watchdog status
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_watchdog_get_status(rte_watchdog_t watchdog,
                                        rte_watchdog_status_t *status_out);

/**
 * @brief Destroy watchdog
 *
 * Stops and deallocates watchdog.
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_watchdog_destroy(rte_watchdog_t watchdog);
```

### Task Watchdog API

```c
/**
 * @brief Create task-specific watchdog
 *
 * Monitors a single task/thread for liveness.
 * Only that task should kick this watchdog.
 *
 * Usage:
 * @code
 * // In task initialization
 * rte_watchdog_config_t config = {
 *     .type = RTE_WATCHDOG_TASK,
 *     .name = "signal_processor_wd",
 *     .timeout_ms = 500,             // 500ms deadline per iteration
 *     .action = RTE_WATCHDOG_ACTION_SAFESTATE
 * };
 * rte_watchdog_t task_wd;
 * rte_watchdog_create(&task_wd, &config);
 * rte_watchdog_start(task_wd);
 * 
 * // In task main loop
 * while (running) {
 *     process_signals();
 *     rte_watchdog_kick(task_wd);   // Must kick within 500ms
 * }
 * @endcode
 *
 * @param handle_out Receives watchdog handle
 * @param config Task watchdog config (type = RTE_WATCHDOG_TASK)
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_watchdog_create(rte_watchdog_t *handle_out,
                                    const rte_watchdog_config_t *config);
```

### Checkpoint Watchdog API (Integrated)

```c
/**
 * @brief Create checkpoint watchdog
 *
 * Automatically detects when a node doesn't reach checkpoint in time.
 * Integrated with rte_channel_checkpoint() function.
 *
 * Usage:
 * @code
 * rte_checkpoint_config_t ckpt = {
 *     .checkpoint_id = 1,
 *     .max_delay_ms = 200,           // Checkpoint timeout
 *     .expected_node_count = 3,
 *     
 *     // WATCHDOG INTEGRATION
 *     .watchdog_enabled = true,
 *     .watchdog_action = RTE_WATCHDOG_ACTION_FAILOVER
 * };
 * 
 * status = rte_channel_checkpoint(vital_signal, &ckpt);
 * // If timeout: watchdog fires automatically
 * // Action: failover to remaining nodes (2oo3 → 2oo2)
 * @endcode
 */
```

### Channel Watchdog API

```c
/**
 * @brief Create channel watchdog (for IPC/redundancy)
 *
 * Detects if an IPC channel or redundancy channel is stuck
 * (no messages flowing, communication timeout).
 *
 * Usage:
 * @code
 * rte_watchdog_config_t config = {
 *     .type = RTE_WATCHDOG_CHANNEL,
 *     .name = "vital_signal_channel_wd",
 *     .timeout_ms = 100,             // Expect message every 100ms
 *     .action = RTE_WATCHDOG_ACTION_SAFESTATE
 * };
 * rte_watchdog_create(&channel_wd, &config);
 * rte_watchdog_start(channel_wd);
 * 
 * // In channel receive loop
 * while (true) {
 *     status = rte_vital_receive(channel, &msg, sizeof(msg), 100);
 *     if (status == RTE_STATUS_OK) {
 *         process_message(&msg);
 *         rte_watchdog_kick(channel_wd);  // Channel alive
 *     }
 * }
 * @endcode
 */
```

---

## Usage Patterns

### Pattern 1: Simple System Liveness

```c
// Main RBC loop

rte_watchdog_config_t wd_config = {
    .type = RTE_WATCHDOG_SYSTEM,
    .name = "rbc_main_wd",
    .timeout_ms = 1000,               // 1 second deadline
    .action = RTE_WATCHDOG_ACTION_SAFESTATE
};

rte_watchdog_t wd;
rte_watchdog_create(&wd, &wd_config);
rte_watchdog_start(wd);

while (running) {
    // Process RBC logic
    process_signals();
    process_trains();
    update_speed_limits();
    
    // Prove we're alive (resets 1-second timeout)
    rte_watchdog_kick(wd);
    
    sleep_ms(100);
}

rte_watchdog_stop(wd);
rte_watchdog_destroy(wd);
```

### Pattern 2: Task-Specific Monitoring

```c
// Signal processing task (runs every 50ms)

rte_watchdog_config_t task_wd_config = {
    .type = RTE_WATCHDOG_TASK,
    .name = "signal_processor_wd",
    .timeout_ms = 500,                // 500ms per iteration max
    .action = RTE_WATCHDOG_ACTION_REBOOT
};

rte_watchdog_t task_wd;
rte_watchdog_create(&task_wd, &task_wd_config);
rte_watchdog_start(task_wd);

while (running) {
    // Checkpoint 1: Start processing
    rte_checkpoint_config_t ckpt1 = {
        .checkpoint_id = 1,
        .max_delay_ms = 100
    };
    rte_channel_checkpoint(vital_ch, &ckpt1);
    
    // Process signals (must complete within 100ms)
    signal_list_t signals = fetch_signals();
    process_signals(&signals);
    
    // Checkpoint 2: Processing done
    rte_checkpoint_config_t ckpt2 = {
        .checkpoint_id = 2,
        .max_delay_ms = 100
    };
    rte_channel_checkpoint(vital_ch, &ckpt2);
    
    // Prove task made progress
    rte_watchdog_kick(task_wd);
}

rte_watchdog_stop(task_wd);
rte_watchdog_destroy(task_wd);
```

### Pattern 3: Redundant System with Watchdog-Triggered Failover

```c
// Online mode (2oo3) with automatic failover on node hang

rte_watchdog_config_t failover_wd = {
    .type = RTE_WATCHDOG_CHECKPOINT,
    .name = "node_c_failover_wd",
    .timeout_ms = 200,               // Node C must reach checkpoint in 200ms
    .action = RTE_WATCHDOG_ACTION_FAILOVER
};

rte_checkpoint_config_t ckpt = {
    .checkpoint_id = 1,
    .max_delay_ms = 200,
    .watchdog_enabled = true,
    .watchdog_config = &failover_wd
};

// If Node C hangs and doesn't reach checkpoint:
// 1. Watchdog timeout fires (200ms)
// 2. Watchdog action = FAILOVER
// 3. Node C isolated from voting
// 4. Cluster continues as 2oo2 (A & B only)

status = rte_channel_checkpoint(vital_signal, &ckpt);

if (status == RTE_STATUS_TIMEOUT) {
    RTE_LOG_ERROR("Node C missing checkpoint - watchdog triggered failover");
    // Cluster automatically reduced to 2oo2
}
```

### Pattern 4: Health Monitoring Dashboard

```c
// Periodic health check (runs every 1 second)

rte_watchdog_status_t wd_status;
rte_watchdog_get_status(system_wd, &wd_status);

printf("Watchdog Health:\n");
printf("  Active:     %s\n", wd_status.active ? "YES" : "NO");
printf("  Kicks:      %u (total kicks)\n", wd_status.kicks);
printf("  Fires:      %u (total fires)\n", wd_status.fires);
printf("  Recoveries: %u (recovery actions)\n", wd_status.recoveries);
printf("  Time since last kick: %u ms\n", wd_status.time_since_last_kick);
printf("  Time until timeout:   %u ms\n", wd_status.time_until_fire);

// Alert if watchdog is about to fire
if (wd_status.time_until_fire < 100) {
    RTE_LOG_WARN("Watchdog about to fire in %u ms!", 
                   wd_status.time_until_fire);
}
```

---

## Integration Points

### With Checkpoints

```c
// Checkpoint already includes watchdog support
rte_checkpoint_config_t ckpt = {
    .checkpoint_id = 1,
    .max_delay_ms = 200,
    .expected_node_count = 3,
    
    // Watchdog automatically monitors checkpoint barrier
    .watchdog_enabled = true,
    .watchdog_action = RTE_WATCHDOG_ACTION_FAILOVER
};

status = rte_channel_checkpoint(vital_signal, &ckpt);
// Watchdog fires if any node doesn't reach checkpoint
```

### With Safe-State

```c
// When watchdog fires with SAFESTATE action
rte_watchdog_config_t config = {
    .action = RTE_WATCHDOG_ACTION_SAFESTATE
};

// Automatically calls: rte_safestate_trigger()
// Application transitions to safe state (signals all RED, etc.)
```

### With Logging

```c
// All watchdog events logged automatically
RTE_LOG_ERROR("Watchdog fired: system_wd (timeout 1000ms)");
RTE_LOG_ERROR("  Last kick: 1250ms ago");
RTE_LOG_ERROR("  Recovery action: SAFESTATE");

// Full audit trail for certification
```

---

## Safety Properties

| Property | Guarantee |
|----------|-----------|
| **Detection Latency** | < timeout (e.g., detect hang within 1 second) |
| **False Positives** | None (kicked every iteration = no false alarm) |
| **Recovery Determinism** | Deterministic action (reboot, safe-state, log) |
| **Audit Trail** | Every watchdog fire logged with timestamp |
| **SIL 4 Compliance** | Meets EN 50128 liveness requirement |

---

## Roadmap

### v0.3.0 Additions

```
Watchdog Framework (#32)
├─ System watchdog
├─ Task watchdog(s)
├─ Channel watchdog
├─ Checkpoint watchdog (integrated)
├─ Recovery actions (reboot, safe-state, failover)
└─ Health monitoring API

Watchdog Examples (#33)
├─ Single system watchdog
├─ Multi-task watchdogs
├─ Redundant system auto-failover
└─ Health dashboard
```

### GitHub Issues

- **#32** Watchdog Framework & API
- **#33** Watchdog Examples & Documentation
- **#34** Hardware Watchdog Integration (IWDG, WDT)
- **#35** Watchdog Statistics & Diagnostics

---

## Next: Implementation Steps

1. **Core Watchdog Manager:** Timer management, kick tracking, timeout detection
2. **Recovery Actions:** Safe-state triggers, reboot sequences, failover logic
3. **Checkpoint Integration:** Auto-watchdog on checkpoint barriers
4. **Examples:** System, task, redundant system scenarios
5. **Certification:** Audit trail, compliance documentation

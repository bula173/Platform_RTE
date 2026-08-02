# Watchdog Mechanism - SAPI Design

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
typedef struct sapi_watchdog_s *sapi_watchdog_t;

/**
 * @brief Watchdog type
 */
typedef enum {
    SAPI_WATCHDOG_SYSTEM,      // Entire system hung detection
    SAPI_WATCHDOG_TASK,        // Specific task/thread hung
    SAPI_WATCHDOG_CHANNEL,     // IPC channel stuck
    SAPI_WATCHDOG_CHECKPOINT   // Node didn't reach checkpoint
} sapi_watchdog_type_t;

/**
 * @brief Recovery action when watchdog fires
 */
typedef enum {
    SAPI_WATCHDOG_ACTION_LOG,           // Log event only
    SAPI_WATCHDOG_ACTION_SAFESTATE,     // Trigger safe-state transition
    SAPI_WATCHDOG_ACTION_REBOOT,        // Reboot system
    SAPI_WATCHDOG_ACTION_FAILOVER,      // Failover to backup (cluster only)
    SAPI_WATCHDOG_ACTION_CUSTOM         // Custom callback
} sapi_watchdog_action_t;

/**
 * @brief Watchdog configuration
 */
typedef struct {
    sapi_watchdog_type_t type;
    const char *name;
    sapi_duration_ms_t timeout_ms;      // Kick deadline
    sapi_watchdog_action_t action;
    void (*custom_action)(void *context);
    void *context;
} sapi_watchdog_config_t;

/**
 * @brief Watchdog health/status
 */
typedef struct {
    uint8_t active;                 // 1 = watchdog enabled
    uint32_t kicks;                 // Total kicks/pets
    uint32_t fires;                 // Total fires
    uint32_t recoveries;            // Total recovery actions
    sapi_duration_ms_t time_since_last_kick;  // ms since last kick
    sapi_duration_ms_t time_until_fire;       // ms until timeout
} sapi_watchdog_status_t;
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
 * sapi_watchdog_config_t config = {
 *     .type = SAPI_WATCHDOG_SYSTEM,
 *     .name = "rbc_system_wd",
 *     .timeout_ms = 1000,            // 1 second
 *     .action = SAPI_WATCHDOG_ACTION_SAFESTATE
 * };
 * sapi_watchdog_t wd;
 * sapi_watchdog_create(&wd, &config);
 * @endcode
 *
 * @param handle_out Receives watchdog handle
 * @param config Watchdog configuration
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_watchdog_create(sapi_watchdog_t *handle_out,
                                    const sapi_watchdog_config_t *config);

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
 *     sapi_watchdog_kick(system_wd);
 *     
 *     sleep_ms(100);
 * }
 * @endcode
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_watchdog_kick(sapi_watchdog_t watchdog);

/**
 * @brief Start watchdog timer
 *
 * Enables watchdog monitoring. Timer begins counting.
 * Must be called after sapi_watchdog_create().
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_watchdog_start(sapi_watchdog_t watchdog);

/**
 * @brief Stop watchdog timer
 *
 * Disables watchdog (e.g., during shutdown or maintenance).
 * Timer stops counting; no timeout will occur.
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_watchdog_stop(sapi_watchdog_t watchdog);

/**
 * @brief Get watchdog status
 *
 * Non-blocking query of watchdog state.
 *
 * @param watchdog Watchdog handle
 * @param status_out Receives watchdog status
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_watchdog_get_status(sapi_watchdog_t watchdog,
                                        sapi_watchdog_status_t *status_out);

/**
 * @brief Destroy watchdog
 *
 * Stops and deallocates watchdog.
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_watchdog_destroy(sapi_watchdog_t watchdog);
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
 * sapi_watchdog_config_t config = {
 *     .type = SAPI_WATCHDOG_TASK,
 *     .name = "signal_processor_wd",
 *     .timeout_ms = 500,             // 500ms deadline per iteration
 *     .action = SAPI_WATCHDOG_ACTION_SAFESTATE
 * };
 * sapi_watchdog_t task_wd;
 * sapi_watchdog_create(&task_wd, &config);
 * sapi_watchdog_start(task_wd);
 * 
 * // In task main loop
 * while (running) {
 *     process_signals();
 *     sapi_watchdog_kick(task_wd);   // Must kick within 500ms
 * }
 * @endcode
 *
 * @param handle_out Receives watchdog handle
 * @param config Task watchdog config (type = SAPI_WATCHDOG_TASK)
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_watchdog_create(sapi_watchdog_t *handle_out,
                                    const sapi_watchdog_config_t *config);
```

### Checkpoint Watchdog API (Integrated)

```c
/**
 * @brief Create checkpoint watchdog
 *
 * Automatically detects when a node doesn't reach checkpoint in time.
 * Integrated with sapi_channel_checkpoint() function.
 *
 * Usage:
 * @code
 * sapi_checkpoint_config_t ckpt = {
 *     .checkpoint_id = 1,
 *     .max_delay_ms = 200,           // Checkpoint timeout
 *     .expected_node_count = 3,
 *     
 *     // WATCHDOG INTEGRATION
 *     .watchdog_enabled = true,
 *     .watchdog_action = SAPI_WATCHDOG_ACTION_FAILOVER
 * };
 * 
 * status = sapi_channel_checkpoint(vital_signal, &ckpt);
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
 * sapi_watchdog_config_t config = {
 *     .type = SAPI_WATCHDOG_CHANNEL,
 *     .name = "vital_signal_channel_wd",
 *     .timeout_ms = 100,             // Expect message every 100ms
 *     .action = SAPI_WATCHDOG_ACTION_SAFESTATE
 * };
 * sapi_watchdog_create(&channel_wd, &config);
 * sapi_watchdog_start(channel_wd);
 * 
 * // In channel receive loop
 * while (true) {
 *     status = sapi_vital_receive(channel, &msg, sizeof(msg), 100);
 *     if (status == SAPI_STATUS_OK) {
 *         process_message(&msg);
 *         sapi_watchdog_kick(channel_wd);  // Channel alive
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

sapi_watchdog_config_t wd_config = {
    .type = SAPI_WATCHDOG_SYSTEM,
    .name = "rbc_main_wd",
    .timeout_ms = 1000,               // 1 second deadline
    .action = SAPI_WATCHDOG_ACTION_SAFESTATE
};

sapi_watchdog_t wd;
sapi_watchdog_create(&wd, &wd_config);
sapi_watchdog_start(wd);

while (running) {
    // Process RBC logic
    process_signals();
    process_trains();
    update_speed_limits();
    
    // Prove we're alive (resets 1-second timeout)
    sapi_watchdog_kick(wd);
    
    sleep_ms(100);
}

sapi_watchdog_stop(wd);
sapi_watchdog_destroy(wd);
```

### Pattern 2: Task-Specific Monitoring

```c
// Signal processing task (runs every 50ms)

sapi_watchdog_config_t task_wd_config = {
    .type = SAPI_WATCHDOG_TASK,
    .name = "signal_processor_wd",
    .timeout_ms = 500,                // 500ms per iteration max
    .action = SAPI_WATCHDOG_ACTION_REBOOT
};

sapi_watchdog_t task_wd;
sapi_watchdog_create(&task_wd, &task_wd_config);
sapi_watchdog_start(task_wd);

while (running) {
    // Checkpoint 1: Start processing
    sapi_checkpoint_config_t ckpt1 = {
        .checkpoint_id = 1,
        .max_delay_ms = 100
    };
    sapi_channel_checkpoint(vital_ch, &ckpt1);
    
    // Process signals (must complete within 100ms)
    signal_list_t signals = fetch_signals();
    process_signals(&signals);
    
    // Checkpoint 2: Processing done
    sapi_checkpoint_config_t ckpt2 = {
        .checkpoint_id = 2,
        .max_delay_ms = 100
    };
    sapi_channel_checkpoint(vital_ch, &ckpt2);
    
    // Prove task made progress
    sapi_watchdog_kick(task_wd);
}

sapi_watchdog_stop(task_wd);
sapi_watchdog_destroy(task_wd);
```

### Pattern 3: Redundant System with Watchdog-Triggered Failover

```c
// Online mode (2oo3) with automatic failover on node hang

sapi_watchdog_config_t failover_wd = {
    .type = SAPI_WATCHDOG_CHECKPOINT,
    .name = "node_c_failover_wd",
    .timeout_ms = 200,               // Node C must reach checkpoint in 200ms
    .action = SAPI_WATCHDOG_ACTION_FAILOVER
};

sapi_checkpoint_config_t ckpt = {
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

status = sapi_channel_checkpoint(vital_signal, &ckpt);

if (status == SAPI_STATUS_TIMEOUT) {
    SAPI_LOG_ERROR("Node C missing checkpoint - watchdog triggered failover");
    // Cluster automatically reduced to 2oo2
}
```

### Pattern 4: Health Monitoring Dashboard

```c
// Periodic health check (runs every 1 second)

sapi_watchdog_status_t wd_status;
sapi_watchdog_get_status(system_wd, &wd_status);

printf("Watchdog Health:\n");
printf("  Active:     %s\n", wd_status.active ? "YES" : "NO");
printf("  Kicks:      %u (total kicks)\n", wd_status.kicks);
printf("  Fires:      %u (total fires)\n", wd_status.fires);
printf("  Recoveries: %u (recovery actions)\n", wd_status.recoveries);
printf("  Time since last kick: %u ms\n", wd_status.time_since_last_kick);
printf("  Time until timeout:   %u ms\n", wd_status.time_until_fire);

// Alert if watchdog is about to fire
if (wd_status.time_until_fire < 100) {
    SAPI_LOG_WARN("Watchdog about to fire in %u ms!", 
                   wd_status.time_until_fire);
}
```

---

## Integration Points

### With Checkpoints

```c
// Checkpoint already includes watchdog support
sapi_checkpoint_config_t ckpt = {
    .checkpoint_id = 1,
    .max_delay_ms = 200,
    .expected_node_count = 3,
    
    // Watchdog automatically monitors checkpoint barrier
    .watchdog_enabled = true,
    .watchdog_action = SAPI_WATCHDOG_ACTION_FAILOVER
};

status = sapi_channel_checkpoint(vital_signal, &ckpt);
// Watchdog fires if any node doesn't reach checkpoint
```

### With Safe-State

```c
// When watchdog fires with SAFESTATE action
sapi_watchdog_config_t config = {
    .action = SAPI_WATCHDOG_ACTION_SAFESTATE
};

// Automatically calls: sapi_safestate_trigger()
// Application transitions to safe state (signals all RED, etc.)
```

### With Logging

```c
// All watchdog events logged automatically
SAPI_LOG_ERROR("Watchdog fired: system_wd (timeout 1000ms)");
SAPI_LOG_ERROR("  Last kick: 1250ms ago");
SAPI_LOG_ERROR("  Recovery action: SAFESTATE");

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

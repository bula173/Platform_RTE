# SafeAPIFramework Integration in 2oo2 Examples

## Overview

This document shows how to integrate **safeAPIFramework (Platform_RTE)** modules into the 2oo2 voting examples for production railway systems.

## Current State vs. Platform_RTE Integration

### Current Examples (Standalone)
- ✗ Manual error handling
- ✗ No standardized status codes
- ✗ Printf for logging (non-deterministic)
- ✗ Manual timing management
- ✗ Direct memory access (no IPC abstraction)

### Platform_RTE-Integrated Examples (Production-Ready)
- ✓ Consistent error handling via `rte_status_t`
- ✓ Deterministic logging via `rte_log`
- ✓ Fixed-width types via `rte_types.h`
- ✓ Lifecycle management via `rte_appmanager`
- ✓ Timer scheduling via `rte_timer`
- ✓ IPC abstraction via `rte_ipc`
- ✓ Safe casting via `rte_cast`

## Integration Points

### 1. Status Codes (rte_status_t)

**Before (Manual):**
```c
int perform_voting(cmd_a, cmd_b, result_out) {
    if (timeout) return -1;      /* What does -1 mean? */
    if (mismatch) return 1;      /* What does 1 mean? */
    return 0;                     /* Success */
}
```

**After (Platform_RTE):**
```c
#include "safeapi/status/rte_status.h"

rte_status_t perform_voting(const train_command_t *cmd_a,
                              const train_command_t *cmd_b,
                              train_command_t *result_out)
{
    if (cmd_a == NULL || cmd_b == NULL) {
        return RTE_STATUS_INVALID_PARAM;
    }

    if (timeout_expired()) {
        return RTE_STATUS_TIMEOUT;       /* Standard SIL 4 code */
    }

    if (cmd_a->signal_state != cmd_b->signal_state) {
        *result_out = safe_state();
        return RTE_STATUS_OK;            /* Decided, though mismatch */
    }

    *result_out = *cmd_a;
    return RTE_STATUS_OK;
}
```

### 2. Fixed-Width Types (rte_types.h)

**Before (Unsafe):**
```c
typedef struct {
    unsigned int train_id;        /* How wide? 16-bit? 32-bit? */
    unsigned long position;       /* Platform-dependent! */
} train_command_t;
```

**After (Platform_RTE - SIL 4 Safe):**
```c
#include "safeapi/types/rte_types.h"

typedef struct {
    uint32_t train_id;            /* Always 32-bit, everywhere */
    uint32_t position;            /* Explicit width */
    uint8_t signal_state;         /* 8-bit enum */
    uint16_t speed_limit;         /* 16-bit km/h */
    rte_duration_ms_t deadline;  /* Milliseconds (uint32_t) */
    rte_checksum_t checksum;     /* CRC32 */
} train_command_t;
```

### 3. Logging (rte_log)

**Before (Non-Deterministic):**
```c
printf("[WEST:A] Cycle %u: result=%d\n", cycle, result);  /* Blocks I/O */
fprintf(stderr, "MISMATCH detected\n");                     /* Non-blocking? */
```

**After (SIL 4 Safe):**
```c
#include "safeapi/log/rte_log.h"

/* Safe, non-blocking logging */
char log_buf[128];
snprintf(log_buf, sizeof(log_buf),
         "[WEST:A] Cycle %u: signal=%u, speed=%u",
         cycle, cmd.signal_state, cmd.speed_limit);
rte_log_write(RTE_LOG_LEVEL_INFO, "CHANNEL_A", log_buf);

/* On mismatch: */
rte_log_write(RTE_LOG_LEVEL_ERROR, "VOTER", 
               "2oo2 mismatch: safe state activated");
```

### 4. Deterministic Timing (rte_timer)

**Before (OS-Dependent):**
```c
usleep(25000);  /* How accurate? Platform? */
```

**After (SIL 4 Safe):**
```c
#include "safeapi/timer/rte_timer.h"

/* Register deterministic timer backend at startup */
rte_timer_backend_t backend = {
    .set_alarm = qnx_set_alarm,   /* Platform-specific */
    .get_elapsed = qnx_get_elapsed,
};
rte_timer_register_backend(&backend);

/* In voting loop */
rte_status_t rc = rte_timer_sleep(25);  /* 25ms, deterministic */
if (rc != RTE_STATUS_OK) {
    /* Timer backend not registered or failed */
    return rc;
}
```

### 5. Safe Casting (rte_cast)

**Before (Silent Overflow):**
```c
uint8_t signal = (uint8_t)cmd->signal_state;  /* Truncates! */
if (signal > 2) { /* ... */ }                  /* Always false */
```

**After (Checked Casting):**
```c
#include "safeapi/cast/rte_cast.h"

uint8_t signal;
rte_status_t rc = rte_cast_uint32_to_uint8(cmd->signal_state, &signal);
if (rc != RTE_STATUS_OK) {
    /* Value out of range - safe to reject */
    return RTE_STATUS_VALUE_OUT_OF_RANGE;
}
```

### 6. Application Lifecycle (rte_appmanager)

**Before (Manual):**
```c
int main() {
    /* Init phase */
    if (init_process_a() < 0) return 1;
    if (init_process_b() < 0) return 1;
    if (init_process_c() < 0) return 1;

    /* Execution phase */
    while (running) {
        if (process_a_step() < 0) goto error;
        if (process_b_step() < 0) goto error;
        if (process_c_step() < 0) goto error;
    }

    /* Shutdown phase */
error:
    process_a_shutdown();
    process_b_shutdown();
    process_c_shutdown();
    return error_count;
}
```

**After (Platform_RTE - SIL 4 Safe):**
```c
#include "safeapi/appmanager/rte_appmanager.h"

rte_status_t voting_process_init(void *context) {
    /* One-time initialization */
    return RTE_STATUS_OK;
}

rte_status_t voting_process_execute(void *context) {
    /* Called repeatedly in main loop */
    /* Returns status - manager handles errors */
    return RTE_STATUS_OK;
}

rte_status_t voting_process_shutdown(void *context) {
    /* Cleanup - guaranteed to run even on error */
    return RTE_STATUS_OK;
}

int main() {
    rte_appmanager_operations_t ops = {
        .init = voting_process_init,
        .execute = voting_process_execute,
        .shutdown = voting_process_shutdown,
        .get_name = get_name,
        .get_version = get_version,
    };

    rte_appmanager_config_t config = {
        .ops = &ops,
        .context = NULL,
        .max_iterations = 0,           /* Infinite */
        .error_threshold = 10,         /* Shutdown on 10 errors */
    };

    /* Manager handles: init → loop → shutdown */
    return rte_appmanager_run(&config);
}
```

### 7. IPC Abstraction (rte_ipc)

**Before (Direct Shared Memory):**
```c
/* Process A writes directly */
shared_mem.cmd_a = my_command;
__sync_synchronize();

/* Process B reads directly */
train_command_t received = shared_mem.cmd_b;
```

**After (Platform_RTE - Abstracted):**
```c
#include "safeapi/ipc/rte_ipc_request_reply.h"

/* Backend handles: shared memory, QNX msgpass, TCP, etc. */
rte_ipc_backend_t backend = {
    .send = backend_send,
    .receive = backend_receive,
};
rte_ipc_register_backend(&backend);

/* Process A sends */
rte_status_t rc = rte_ipc_send(other_process_id,
                                  &my_command,
                                  sizeof(my_command));

/* Process B receives */
train_command_t received;
rc = rte_ipc_receive(queue_id, &received, sizeof(received), 25);
```

## Complete Platform_RTE-Integrated Example

### Header File with Platform_RTE

```c
/* voting.h */
#include <stdint.h>

#include "safeapi/types/rte_types.h"
#include "safeapi/status/rte_status.h"
#include "safeapi/timer/rte_timer.h"
#include "safeapi/log/rte_log.h"
#include "safeapi/cast/rte_cast.h"

typedef struct {
    uint32_t train_id;
    uint32_t position;
    uint8_t signal_state;
    uint8_t emergency_stop;
    uint16_t speed_limit;
    rte_duration_ms_t timestamp_ms;
    rte_checksum_t checksum;
    uint32_t sequence_number;
} train_command_t;

rte_status_t train_command_validate(const train_command_t *cmd);
rte_status_t voting_compare_and_decide(const train_command_t *cmd_a,
                                         const train_command_t *cmd_b,
                                         train_command_t *result_out);
```

### Implementation with Platform_RTE

```c
/* voting.c */
#include "voting.h"

rte_status_t train_command_validate(const train_command_t *cmd)
{
    if (cmd == NULL) {
        return RTE_STATUS_INVALID_PARAM;
    }

    /* Validate signal state: must be 0-2 */
    uint8_t sig;
    rte_status_t rc = rte_cast_uint32_to_uint8(cmd->signal_state, &sig);
    if (rc != RTE_STATUS_OK) {
        rte_log_write(RTE_LOG_LEVEL_ERROR, "VALIDATE",
                       "Signal state out of range");
        return RTE_STATUS_VALUE_OUT_OF_RANGE;
    }

    if (sig > 2) {
        return RTE_STATUS_INVALID_PARAM;
    }

    /* Verify checksum */
    rte_checksum_t computed = rte_checksum_compute(
        (const uint8_t *)cmd,
        sizeof(*cmd) - sizeof(cmd->checksum)
    );

    if (computed != cmd->checksum) {
        rte_log_write(RTE_LOG_LEVEL_ERROR, "VALIDATE",
                       "Checksum mismatch (corruption detected)");
        return RTE_STATUS_DATA_CORRUPTION;
    }

    return RTE_STATUS_OK;
}

rte_status_t voting_compare_and_decide(const train_command_t *cmd_a,
                                         const train_command_t *cmd_b,
                                         train_command_t *result_out)
{
    if (cmd_a == NULL || cmd_b == NULL || result_out == NULL) {
        return RTE_STATUS_INVALID_PARAM;
    }

    /* Validate both inputs */
    rte_status_t rc = train_command_validate(cmd_a);
    if (rc != RTE_STATUS_OK) {
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg),
                 "Channel A validation failed: %s",
                 rte_status_to_string(rc));
        rte_log_write(RTE_LOG_LEVEL_ERROR, "VOTER", log_msg);
        return rc;
    }

    rc = train_command_validate(cmd_b);
    if (rc != RTE_STATUS_OK) {
        rte_log_write(RTE_LOG_LEVEL_ERROR, "VOTER",
                       "Channel B validation failed");
        return rc;
    }

    /* Cross-compare */
    if (cmd_a->train_id == cmd_b->train_id &&
        cmd_a->signal_state == cmd_b->signal_state &&
        cmd_a->speed_limit == cmd_b->speed_limit) {

        /* MATCH */
        *result_out = *cmd_a;
        rte_log_write(RTE_LOG_LEVEL_DEBUG, "VOTER", "2oo2 MATCH");
        return RTE_STATUS_OK;

    } else {

        /* MISMATCH - Safe state */
        *result_out = (train_command_t){
            .train_id = cmd_a->train_id,
            .signal_state = 0,      /* Red */
            .speed_limit = 0,       /* Stop */
            .emergency_stop = 1,
        };
        rte_log_write(RTE_LOG_LEVEL_WARN, "VOTER",
                       "2oo2 MISMATCH - safe state activated");
        return RTE_STATUS_OK;
    }
}
```

## Platform_RTE Module Mapping

| Platform_RTE Module | Voting Use Case |
|---|---|
| `rte_status` | Error codes (INVALID_PARAM, TIMEOUT, etc.) |
| `rte_types` | Fixed-width types (uint32_t, rte_duration_ms_t) |
| `rte_log` | Non-blocking logging per process |
| `rte_timer` | Deterministic cycle timing (25ms) |
| `rte_cast` | Safe type conversions (signal state 0-2) |
| `rte_checksum` | Data integrity (CRC32 on commands) |
| `rte_ipc` | Process communication (shared memory, msgpass, TCP) |
| `rte_appmanager` | Lifecycle management (init → execute → shutdown) |
| `rte_safestate` | Safe assertions (`RTE_ASSERT`, `RTE_SAFESTATE`) |

## Migration Path

### Step 1: Add Platform_RTE Status Codes
```c
#include "safeapi/status/rte_status.h"
/* Replace: int rc → rte_status_t rc */
```

### Step 2: Add Platform_RTE Types
```c
#include "safeapi/types/rte_types.h"
/* Replace: uint32_t → fixed-width types from Platform_RTE */
```

### Step 3: Add Platform_RTE Logging
```c
#include "safeapi/log/rte_log.h"
/* Replace: printf/fprintf → rte_log_write */
```

### Step 4: Add Deterministic Timing
```c
#include "safeapi/timer/rte_timer.h"
/* Replace: usleep → rte_timer_sleep (with backend) */
```

### Step 5: Add Safe Casting
```c
#include "safeapi/cast/rte_cast.h"
/* Replace: (uint8_t)x → rte_cast_uint32_to_uint8(x, &result) */
```

### Step 6: Add Checksum Validation
```c
#include "safeapi/checksum/rte_checksum.h"
/* Add: integrity checks on train commands */
```

### Step 7: Add Lifecycle Management
```c
#include "safeapi/appmanager/rte_appmanager.h"
/* Replace: main loop → rte_appmanager_run */
```

## Benefits of Platform_RTE Integration

✓ **SIL 4 Compliance**: Proven patterns, no security issues  
✓ **Portability**: Switch backends (shared mem → TCP → QNX msgpass)  
✓ **Type Safety**: Fixed-width types, no platform surprises  
✓ **Observability**: Structured logging across all processes  
✓ **Error Handling**: Consistent status codes everywhere  
✓ **Determinism**: RTOS-aware timing and synchronization  
✓ **Testing**: Mock backends for unit tests  

## Example: Build with Platform_RTE

```bash
# Cross-compile for QNX with Platform_RTE
qcc -std=c99 \
    -I../../../include \
    -L../../../build/src/status \
    -L../../../build/src/log \
    -L../../../build/src/timer \
    voting.c process_a.c \
    -o west_process_a \
    -lsafeapi_status \
    -lsafeapi_log \
    -lsafeapi_timer
```

---

**Next Step**: Create `voting-sapi.c` showing full Platform_RTE integration in cross-comparison example.

# Application Manager Architecture

This document describes the **Application Manager** pattern used in safeAPIFramework examples.

---

## Motivation

Safety-critical applications require disciplined lifecycle management:

1. **Initialization** — One-time setup, error detection
2. **Execution** — Main work loop, error handling
3. **Shutdown** — Graceful cleanup, resource release

Each phase has different characteristics and requirements.

---

## Architecture

### Single Entry Point Pattern

```
┌─────────────────────────────────┐
│         main()                  │
│  (Single entry point)           │
└────────────┬────────────────────┘
             │
             ▼
┌─────────────────────────────────┐
│   app_manager_run()             │
│  (Lifecycle management)         │
│                                 │
│  1. Initialize                  │
│  2. Execute (loop)              │
│  3. Shutdown                    │
│                                 │
│  Error handling                 │
│  State tracking                 │
│  Statistics collection          │
└────────────┬────────────────────┘
             │
    ┌────────┴────────┐
    │                 │
    ▼                 ▼
┌──────────────┐  ┌─────────────────┐
│ Application  │  │ Logging,        │
│  Operations  │  │ State, Stats    │
└──────────────┘  └─────────────────┘
```

### Lifecycle States

```
┌──────────────────────────────────────────────────────────────┐
│                  APPLICATION LIFECYCLE                       │
└──────────────────────────────────────────────────────────────┘

  UNINITIALIZED
       │
       ▼
  INITIALIZING ──(error)──┐
       │                  │
       ▼                  │
   RUNNING ──(error)──┐   │
       │              │   │
       ▼              │   │
  SHUTTING_DOWN <─────┘   │
       │                  │
       ▼                  │
   SHUTDOWN <─────────────┘
       │
       ▼
    EXIT (0 or 1)

  Always: shutdown() is called regardless of path taken
```

### Error Handling Strategy

```
Application Error → Log error
                 ↓
            Increment error_count
                 ↓
      Check error_threshold
                 ↓
         ┌───────┴───────┐
         │               │
    (threshold       (threshold
     exceeded)      not reached)
         │               │
         ▼               ▼
     SHUTDOWN       Continue loop
```

---

## Implementation Pattern

### 1. Define Application Context

```c
typedef struct {
    app_state_t state;
    uint32_t message_count;
    uint32_t heartbeat_count;
    uint32_t error_count;
    volatile int running;
} app_context_t;

static app_context_t g_app = {0};
```

### 2. Implement Required Operations

```c
/* Initialization */
static rte_status_t app_init(void *context)
{
    app_context_t *app = (app_context_t *)context;
    
    RTE_LOG_INFO("Initializing application");
    
    // One-time setup: open files, allocate buffers, etc.
    // Return error if setup fails
    
    app->state = APP_STATE_RUNNING;
    return RTE_STATUS_OK;
}

/* Main execution loop (one iteration) */
static rte_status_t app_execute(void *context)
{
    app_context_t *app = (app_context_t *)context;
    
    // Do one iteration of work:
    // - Process one message
    // - Check one timer
    // - Update one state
    // - Return immediately
    
    if (error_condition) {
        return RTE_STATUS_ERROR;  // Signal error to manager
    }
    
    return RTE_STATUS_OK;
}

/* Graceful shutdown */
static rte_status_t app_shutdown(void *context)
{
    app_context_t *app = (app_context_t *)context;
    
    RTE_LOG_INFO("Shutting down application");
    
    // Release resources: close files, free buffers, etc.
    // MUST NOT FAIL - log errors but continue
    
    return RTE_STATUS_OK;
}

static const char *app_get_name(void) { return "my-app"; }
static const char *app_get_version(void) { return "1.0.0"; }
```

### 3. Configure Application Manager

```c
int main(int argc, char *argv[])
{
    const app_manager_operations_t ops = {
        .init = app_init,
        .execute = app_execute,
        .shutdown = app_shutdown,
        .get_name = app_get_name,
        .get_version = app_get_version
    };

    app_manager_config_t config = {
        .ops = &ops,
        .context = &g_app,
        .max_iterations = 0,        // Run forever
        .error_threshold = 10       // Stop after 10 errors
    };

    return app_manager_run(&config);
}
```

---

## Benefits

### Safety & Reliability

- **Structured lifecycle** — Clear init/run/shutdown phases
- **Error isolation** — Errors in execute don't prevent shutdown
- **Resource cleanup** — Shutdown guaranteed to run
- **Error threshold** — Auto-stop on excessive errors

### Code Clarity

- **Single entry point** — `main()` is just a stub
- **Operations interface** — Clear contract for each phase
- **No global state** — App state passed via context pointer
- **Testability** — Each operation can be tested independently

### Observability

- **State tracking** — Query current app state
- **Statistics** — Iteration count, error count
- **Logging** — Manager logs all transitions
- **Exit codes** — Meaningful return values (0 vs 1)

---

## Comparison: With vs Without Manager

### Without Manager (Original Pattern)

```c
int main() {
    // Mixed initialization & execution
    if (init() != OK) goto error;
    
    while (!shutdown_requested) {
        execute();  // No error checking
    }
    
    shutdown();     // May be skipped on error
    return 0;
    
error:
    cleanup();      // Duplicated shutdown logic
    return 1;
}
```

**Problems:**
- Shutdown may be skipped
- Mixed concerns in main()
- Duplicated error paths
- No state tracking

### With Manager (Refactored)

```c
static rte_status_t app_init(void *ctx)  { /* ... */ }
static rte_status_t app_execute(void *ctx) { /* ... */ }
static rte_status_t app_shutdown(void *ctx) { /* ... */ }

int main() {
    return app_manager_run(&(app_manager_config_t){
        .ops = &(app_manager_operations_t){
            .init = app_init,
            .execute = app_execute,
            .shutdown = app_shutdown,
            // ...
        },
        .context = &g_app,
        .max_iterations = 0,
        .error_threshold = 10
    });
}
```

**Benefits:**
- Clear, distinct phases
- Shutdown always runs
- Error threshold enforced
- State tracked automatically
- Logging built-in

---

## Execution Guarantees

### Initialization

- Called exactly **once** at startup
- If fails → shutdown() is called, application exits with code 1
- If succeeds → application enters execution phase

### Execution

- Called repeatedly until:
  - `max_iterations` is reached, OR
  - `shutdown_requested` is true, OR
  - Error threshold exceeded
- Each call is independent (no state between iterations)
- Errors are logged but execution continues (unless threshold reached)

### Shutdown

- Called exactly **once** at end
- Called **regardless of initialization success**
- Must not fail (errors logged but ignored)
- All resources released before exit

---

## Real-World Example

### Railway Message Processor (Refactored)

**Before (no manager):**
```c
int main() {
    if (init()) goto fail;
    while (running) {
        process_message();
    }
    shutdown();
    return 0;
fail:
    shutdown();  // Duplicated
    return 1;
}
```

**After (with manager):**
```c
static rte_status_t app_init(void *ctx) {
    rte_log_initialize();
    g_app.state = RUNNING;
    return RTE_STATUS_OK;
}

static rte_status_t app_execute(void *ctx) {
    rte_status_t status = simulate_train_message();
    if (status != RTE_STATUS_OK) g_app.error_count++;
    return status;
}

static rte_status_t app_shutdown(void *ctx) {
    RTE_LOG_INFO("Shutting down");
    rte_log_shutdown();
    return RTE_STATUS_OK;
}

int main() {
    return app_manager_run(&config);
}
```

**Improvements:**
- No error path duplication
- Shutdown guaranteed
- Clear phase separation
- Automatic error tracking

---

## Testing with Application Manager

### Unit Testing

Test each operation independently:

```c
void test_app_init() {
    app_context_t ctx = {0};
    assert(app_init(&ctx) == RTE_STATUS_OK);
}

void test_app_execute() {
    app_context_t ctx = {.state = APP_STATE_RUNNING};
    assert(app_execute(&ctx) == RTE_STATUS_OK);
}

void test_app_shutdown() {
    app_context_t ctx = {.state = APP_STATE_SHUTTING_DOWN};
    assert(app_shutdown(&ctx) == RTE_STATUS_OK);
}
```

### Integration Testing

Test full lifecycle:

```c
void test_full_lifecycle() {
    app_manager_config_t config = {
        .ops = &ops,
        .context = &ctx,
        .max_iterations = 100,
        .error_threshold = 5
    };
    
    int result = app_manager_run(&config);
    assert(result == EXIT_SUCCESS);
    
    app_manager_state_t stats;
    app_manager_get_stats(&stats);
    assert(stats.iteration_count == 100);
}
```

---

## Adapting Existing Applications

### Step 1: Identify Initialization Code

Move all setup logic into `app_init()`:
- File I/O initialization
- Memory allocation
- Service registration
- Event loop setup

### Step 2: Extract Work Unit

Make `app_execute()` do one unit of work:
- Process one message
- Check one timer tick
- Update one state
- Handle one event

### Step 3: Create Shutdown Handler

Implement `app_shutdown()` with cleanup:
- Close files
- Release memory
- Deregister services
- Log statistics

### Step 4: Replace main()

Replace old main loop with single call to `app_manager_run()`.

---

## Related Documentation

- `app_manager.h` — Complete API documentation
- `examples/linux-posix-app/` — Example (will be refactored to use manager)
- `examples/qnx-rtos-app/` — Example (will be refactored to use manager)

---

## References

- [Safety-Critical Software Development](https://en.wikipedia.org/wiki/Safety-critical_system)
- [MISRA C:2012 - Lifecycle Management](https://www.misra.org.uk/)
- [EN 50128 - Software Development Processes](https://en.wikipedia.org/wiki/EN_50128)

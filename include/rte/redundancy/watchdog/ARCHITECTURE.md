@page watchdog_architecture Watchdog Module - Architecture

@section watchdog_architecture_overview Design Overview

Watchdog tracks liveness via timeout mechanism. Application periodically kicks
to reset countdown. On timeout, registered recovery action fires (log, safe-state,
reboot, custom callback, or failover). Framework manages global watchdog manager
that coordinates all watchdog instances.

@section watchdog_architecture_api Core API

@subsection watchdog_architecture_api_create Creation

@code
rte_status_t rte_watchdog_create(
    rte_watchdog_storage_t *storage,
    const rte_watchdog_config_t *config,
    rte_watchdog_t *out_handle
);
@endcode

Caller provides storage; framework fills it with state. Config specifies type,
timeout, and recovery action.

@subsection watchdog_architecture_api_lifecycle Lifecycle

@code
rte_watchdog_start(handle)     // Begin countdown
rte_watchdog_kick(handle)      // Reset countdown
rte_watchdog_stop(handle)      // Stop countdown
rte_watchdog_destroy(handle)   // Release resources
@endcode

@subsection watchdog_architecture_api_query Query

@code
rte_watchdog_get_status(handle, &status);
@endcode

Returns: kicks (total), fires (total timeouts), time_until_fire, active flag.

@section watchdog_architecture_manager Watchdog Manager

Global singleton managing all watchdogs:

@code
rte_watchdog_manager_initialize()   // Call at startup
rte_watchdog_manager_shutdown()     // Call at shutdown
@endcode

Manager coordinates timeout callbacks and recovery actions.

@section watchdog_architecture_state State Per Watchdog

@verbatim
Countdown timer      - Current time until timeout
Last kick time      - When last kicked
Total kicks         - Diagnostic counter
Total fires         - Diagnostic counter
Is active           - Enabled/disabled state
Type/action/config  - Static configuration
@endverbatim

@section watchdog_architecture_timeout Timeout Mechanics

On every tick (typically 1ms from system timer):
1. Decrement countdown for each active watchdog
2. If countdown reaches 0:
   a. Mark watchdog as fired
   b. Invoke recovery action
   c. Stop the watchdog

On kick:
1. If not fired: reset countdown to timeout_ms
2. If fired: return error (cannot kick)

@section watchdog_architecture_recovery Recovery Actions

Action on timeout depends on configuration:

@verbatim
LOG          - Log the event via rte_log
SAFESTATE    - Invoke RTE_SAFESTATE(SAFE, APP_REASON_WATCHDOG)
REBOOT       - Invoke RTE_REBOOT(APP_REASON_WATCHDOG)
FAILOVER     - Application-defined failover (distributed systems)
CUSTOM       - Call custom callback (application-provided)
@endverbatim

@section watchdog_architecture_performance Performance

| Operation | Time | Notes |
|-----------|------|-------|
| create() | O(1) | Initialize state |
| start() | O(1) | Set countdown |
| kick() | O(1) | Reset countdown |
| get_status() | O(1) | Read state |
| destroy() | O(1) | Clean up |
| Manager tick | O(n) | n = active watchdogs |

System tick is the only O(n) operation; called once per millisecond.

@section watchdog_architecture_limits Constraints

@verbatim
Max watchdog count      - No limit; all static
Min timeout            - Depends on timer resolution (typically 1ms)
Max timeout            - 2^32 - 1 milliseconds (~49 days)
Watchdog size          - 64 bytes (RTE_DECLARE_STORAGE)
@endverbatim

@section watchdog_architecture_integration Integration with Safe-State

Watchdog recovery action can trigger safe-state:

@code
.action = RTE_WATCHDOG_ACTION_SAFESTATE
  │
  └─ On timeout:
      └─ RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE, reason)
          └─ Enters safe-state as programmed
@endcode

@section watchdog_architecture_testing Unit Tests

**Test 1: Timeout fires recovery action**

@code
rte_watchdog_t wd;
rte_watchdog_create(&storage, &config, &wd);
rte_watchdog_start(wd);
// Wait for timeout without kicking
sleep_ms(config.timeout_ms + 10);
// Verify recovery action was triggered
ASSERT(action_was_fired == 1);
@endcode

**Test 2: Kick resets countdown**

@code
rte_watchdog_start(wd);
sleep_ms(50);  // Wait halfway to timeout
rte_watchdog_kick(wd);
sleep_ms(50);  // Half timeout again
// Should not have fired yet (total 100ms < timeout 200ms)
ASSERT(status.fires == 0);
@endcode

@section watchdog_architecture_see_also See Also

- @ref watchdog_user_guide for usage patterns
- @ref safestate_architecture for safe-state integration

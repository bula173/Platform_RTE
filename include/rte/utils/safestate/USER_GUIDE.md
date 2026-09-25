@page safestate_user_guide Safe-State Module - User Guide

@section safestate_user_guide_overview What is Safe-State?

Safe-State provides fail-safe transitions for safety-critical systems.
When any component detects a critical condition, it can transition the
system to one of three levels:
- DEGRADED: Reduced functionality but still operating
- SAFE: Critical functions disabled, fail-safe state active
- REBOOT: Controlled restart required

Key principle: If correctness cannot be guaranteed, enter fail-safe mode.

@section safestate_user_guide_levels Safe-State Levels

The module defines three severity levels (in order of escalation):

@subsection safestate_user_guide_level_degraded DEGRADED (Level 0)

Reduced capability but system can continue operating.
Example: Redundant channel failed, but primary still working.
Handler CAN return to caller (system continues execution).

@subsection safestate_user_guide_level_safe SAFE (Level 1)

Fail-safe state: critical functions disabled, system in safe configuration.
Example: Both channels failed, both must be considered unavailable.
Handler MUST NOT return; if it does, framework enters infinite loop.

@subsection safestate_user_guide_level_reboot REBOOT (Level 2)

Emergency restart required.
Example: Critical error detected, recovery only via fresh start.
Handler MUST NOT return; if it does, framework enters infinite loop.

@section safestate_user_guide_quick_start Quick Start

@subsection safestate_user_guide_qs_register 1. Register Handler at Startup

@code
void my_safe_state_handler(rte_safestate_level_t level,
                           rte_safestate_reason_t reason,
                           const char *file,
                           int32_t line,
                           const char *message) {
    // Log the event
    printf("Safe-state [%d]: reason=%u at %s:%ld msg=%s\n",
           level, reason, file, line, message);

    if (level == RTE_SAFESTATE_LEVEL_DEGRADED) {
        // Can return - system continues
        activate_fallback_mode();
        return;
    }

    // For SAFE and REBOOT: do not return
    // Perform final shutdown, then let framework handle it
    close_all_connections();
    flush_logs();
    // Handler does not return
}

// At application startup:
rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_DEGRADED,
                                my_safe_state_handler);
rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_SAFE,
                                my_safe_state_handler);
rte_safestate_register_handler(RTE_SAFESTATE_LEVEL_REBOOT,
                                my_safe_state_handler);
@endcode

@subsection safestate_user_guide_qs_assert 2. Use RTE_ASSERT for Checked Assertions

Replace standard assert() with RTE_ASSERT() - always active, even in production.

@code
RTE_ASSERT(buffer != NULL);           // Always checked
RTE_ASSERT(count > 0 && count < 256); // Condition must be true
@endcode

On failure: enters SAFE state with RTE_SAFESTATE_REASON_ASSERT_FAILED.

@subsection safestate_user_guide_qs_trigger 3. Trigger Safe-State Explicitly

@code
// Critical channel failed
if (vital_channel_status == CHANNEL_ERROR) {
    RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE,
                  CUSTOM_REASON_VITAL_CHANNEL_FAILED);
}

// Need restart
if (recovery_failed) {
    RTE_REBOOT(CUSTOM_REASON_RECOVERY_IMPOSSIBLE);
    // Does not return
}
@endcode

@section safestate_user_guide_examples Practical Examples

@subsection safestate_user_guide_example_voting Example 1: Redundant Channel Voting Failure

@code
rte_status_t process_vital_command(const vital_msg_t *msg) {
    // Both channels must deliver identical message
    // If either fails or values differ -> critical failure

    if (msg->checksum_mismatch) {
        log_error("Vital message checksum failed");
        RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE,
                      CUSTOM_REASON_CHECKSUM_MISMATCH);
        return RTE_STATUS_INVALID_PARAM;
    }

    if (!msg->channel_a_valid || !msg->channel_b_valid) {
        log_error("Channel unavailable: a=%d b=%d",
                 msg->channel_a_valid, msg->channel_b_valid);
        RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE,
                      CUSTOM_REASON_CHANNEL_UNAVAILABLE);
        return RTE_STATUS_INVALID_PARAM;
    }

    // Both channels valid and checksums match - safe to proceed
    execute_command(msg);
    return RTE_STATUS_OK;
}
@endcode

@subsection safestate_user_guide_example_degraded Example 2: Graceful Degradation

@code
void check_system_health(void) {
    rte_system_health_t health;
    get_system_health(&health);

    uint32_t failures = 0;
    if (!health.primary_ok) failures++;
    if (!health.backup_ok) failures++;
    if (!health.tertiary_ok) failures++;

    if (failures == 0) {
        // All systems OK - normal operation
        RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_DEGRADED,
                      CUSTOM_REASON_RECOVERED);
    } else if (failures == 1) {
        // One system down - degraded but operational
        log_warning("System degraded: %u failures", failures);
        RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_DEGRADED,
                      CUSTOM_REASON_REDUCED_REDUNDANCY);
    } else if (failures >= 2) {
        // Multiple failures - must enter safe state
        log_critical("System failed: %u failures", failures);
        RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE,
                      CUSTOM_REASON_INSUFFICIENT_REDUNDANCY);
    }
}
@endcode

@subsection safestate_user_guide_example_assertion Example 3: Assertion-Based Fault Detection

@code
void process_train_command(const train_cmd_t *cmd) {
    // These assertions are ALWAYS active, even in production builds
    RTE_ASSERT(cmd != NULL);
    RTE_ASSERT(cmd->speed <= MAX_TRAIN_SPEED);
    RTE_ASSERT(cmd->brake_level <= MAX_BRAKE);

    // If any assertion fails -> SAFE state with REASON_ASSERT_FAILED
    // No exception, no recovery - just fail-safe

    execute_train_command(cmd);
}
@endcode

@section safestate_user_guide_patterns Common Patterns

@subsection safestate_user_guide_pattern_health Pattern 1: Health Monitoring Loop

Periodically check system health and transition accordingly:

@code
void health_check_loop(void) {
    while (1) {
        rte_system_status_t status = get_system_status();

        if (status.all_healthy) {
            // Try to recover to normal if previously degraded
            if (current_level != RTE_SAFESTATE_LEVEL_DEGRADED) {
                RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_DEGRADED, 0);
            }
        } else if (status.critical_fault) {
            // Critical system fault - enter SAFE
            RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE,
                          CUSTOM_REASON_CRITICAL_FAULT);
        }

        sleep_ms(100);  // Check every 100ms
    }
}
@endcode

@subsection safestate_user_guide_pattern_failsafe Pattern 2: Fail-Safe-by-Default

Assume unsafe unless proven otherwise:

@code
// BAD: Assumes safe unless proven unsafe
rte_status_t rc = critical_operation();
if (rc == RTE_STATUS_OK) {
    proceed_with_operation();
}
// What if rc indicates error? Continues anyway!

// GOOD: Assumes unsafe, must prove safe
rte_status_t rc = critical_operation();
if (rc != RTE_STATUS_OK) {
    RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE, REASON);
    return;
}
proceed_with_operation();  // Proven safe
@endcode

@section safestate_user_guide_app_reason_codes Application-Specific Reason Codes

Framework reserves reason codes 0-4095. Applications use 4096 and above:

@code
#define CUSTOM_REASON_VITAL_CHANNEL_FAILED 4096
#define CUSTOM_REASON_CHECKSUM_MISMATCH 4097
#define CUSTOM_REASON_REDUCED_REDUNDANCY 4098
#define CUSTOM_REASON_INSUFFICIENT_REDUNDANCY 4099
#define CUSTOM_REASON_CRITICAL_FAULT 4100
#define CUSTOM_REASON_RECOVERY_IMPOSSIBLE 4101
#define CUSTOM_REASON_CHANNEL_UNAVAILABLE 4102
#define CUSTOM_REASON_RECOVERED 4103
@endcode

@section safestate_user_guide_guidelines Best Practices

1. Register all three handlers at startup
   - Even if not all levels used, register handlers for each
   - Prevents undefined behavior if level is entered without handler

2. Handlers should not block
   - Keep handler execution time minimal
   - Log events, close resources quickly, then exit
   - Framework guarantees no further code execution after SAFE/REBOOT

3. Use meaningful reason codes
   - Not just UNSPECIFIED
   - Helps with diagnostics and post-incident analysis
   - Reserve ranges: framework 0-4095, application 4096+

4. RTE_ASSERT for programmer errors
   - Use for assumptions that should always be true
   - Example: pointer not NULL, array index in range
   - Failure = bug in code, must stop immediately

5. RTE_SAFESTATE for runtime faults
   - Use for conditions that can legitimately fail at runtime
   - Example: communication failure, sensor error
   - Failure = system state invalid, must enter safe mode

@section safestate_user_guide_see_also See Also

- @ref safestate_architecture for internal design
- @ref watchdog_user_guide for watchdog integration
- @ref reboot_user_guide for reboot handling

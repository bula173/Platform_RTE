/**
 * @file rte_watchdog.h
 * @brief Watchdog mechanism for detecting system/task hang conditions
 *
 * Provides system-level, task-level, and channel-level watchdog timers
 * that detect hung components and trigger recovery actions (log, safe-state,
 * reboot, failover, or a custom callback). Implemented as a fixed-size
 * static pool, timed via rte_timer_now() - see src/watchdog/rte_watchdog.c
 * and rte_watchdog_timer_tick()'s own doc for how expiry is detected
 * (poll-driven, not an OS-specific interrupt of its own).
 *
 * @defgroup WATCHDOG Watchdog Mechanism
 * @brief Detect hung systems/tasks and trigger recovery
 * @{
 */

#ifndef RTE_WATCHDOG_H
#define RTE_WATCHDOG_H

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types & Constants
 * ========================================================================== */

/**
 * @brief Watchdog handle (opaque)
 */
typedef struct rte_watchdog_s *rte_watchdog_t;

/**
 * @brief Watchdog type
 *
 * Defines what the watchdog is monitoring.
 */
typedef enum {
    RTE_WATCHDOG_SYSTEM,      /**< Entire system liveness */
    RTE_WATCHDOG_TASK,        /**< Specific task/thread */
    RTE_WATCHDOG_CHANNEL,     /**< IPC/redundancy channel */
    RTE_WATCHDOG_CHECKPOINT   /**< Checkpoint barrier */
} rte_watchdog_type_t;

/**
 * @brief Recovery action when watchdog fires
 *
 * Defines what happens when watchdog timeout occurs.
 */
typedef enum {
    RTE_WATCHDOG_ACTION_LOG,           /**< Log event only */
    RTE_WATCHDOG_ACTION_SAFESTATE,     /**< Trigger safe-state */
    RTE_WATCHDOG_ACTION_REBOOT,        /**< System reboot */
    RTE_WATCHDOG_ACTION_FAILOVER,      /**< Loss of a redundant peer/channel detected - invokes
                                          *   config->custom_action (same dispatch as
                                          *   RTE_WATCHDOG_ACTION_CUSTOM; custom_action is required
                                          *   for this action too - see rte_watchdog_create()). Use
                                          *   this over CUSTOM when the *reason* a watchdog exists is
                                          *   specifically "my redundant partner stopped
                                          *   responding" (e.g. a dual-channel cross-compare link) -
                                          *   the distinct name documents intent at the call site,
                                          *   even though the mechanism is identical to CUSTOM.
                                          *   REQ-WATCHDOG-002 */
    RTE_WATCHDOG_ACTION_CUSTOM         /**< Custom callback */
} rte_watchdog_action_t;

/**
 * @brief Watchdog configuration
 *
 * Defines watchdog behavior: type, timeout, recovery action.
 */
typedef struct {
    rte_watchdog_type_t type;          /**< Watchdog type (system/task/channel) */
    const char *name;                   /**< Watchdog name (for logging) */
    rte_duration_ms_t timeout_ms;      /**< Timeout deadline (ms) */
    rte_watchdog_action_t action;      /**< Recovery action on timeout */
    void (*custom_action)(void *ctx);   /**< Handler callback - required (rte_watchdog_create()
                                          *   returns RTE_STATUS_INVALID_PARAM otherwise) when
                                          *   action is RTE_WATCHDOG_ACTION_CUSTOM or
                                          *   RTE_WATCHDOG_ACTION_FAILOVER; unused for the other
                                          *   actions. REQ-WATCHDOG-001 */
    void *context;                      /**< Context for callback */
} rte_watchdog_config_t;

/**
 * @brief Watchdog health/status information
 *
 * Retrieved via rte_watchdog_get_status() for monitoring.
 */
typedef struct {
    uint8_t active;                     /**< 1 = watchdog enabled */
    uint32_t kicks;                     /**< Total number of kicks */
    uint32_t fires;                     /**< Total number of timeouts */
    uint32_t recoveries;                /**< Total recovery actions */
    rte_duration_ms_t time_since_last_kick; /**< ms since last kick */
    rte_duration_ms_t time_until_fire;      /**< ms until timeout */
} rte_watchdog_status_t;

/* ============================================================================
 * API: Core Watchdog Operations
 * ========================================================================== */

/**
 * @brief Create a watchdog
 *
 * Initializes a watchdog with specified configuration. Watchdog starts
 * in disabled state; call rte_watchdog_start() to enable.
 *
 * @param handle_out Receives watchdog handle
 * @param config Watchdog configuration
 * @return RTE_STATUS_OK on success
 *         RTE_STATUS_ERROR on failure
 *
 * @pre config != NULL
 * @post watchdog is created but not started
 *
 * @safety No dynamic memory allocation; all storage pre-allocated.
 *
 * Example (system watchdog):
 * @code
 * rte_watchdog_config_t config = {
 *     .type = RTE_WATCHDOG_SYSTEM,
 *     .name = "rbc_main_wd",
 *     .timeout_ms = 1000,
 *     .action = RTE_WATCHDOG_ACTION_SAFESTATE
 * };
 * rte_watchdog_t wd;
 * rte_watchdog_create(&wd, &config);
 * @endcode
 */
rte_status_t rte_watchdog_create(rte_watchdog_t *handle_out,
                                    const rte_watchdog_config_t *config);

/**
 * @brief Start watchdog timer
 *
 * Enables watchdog monitoring. Countdown begins from timeout_ms.
 * Must be called after rte_watchdog_create().
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success
 *
 * @pre watchdog != NULL
 * @post watchdog is counting down; will fire if not kicked
 *
 * @safety Deterministic: no blocking, no dynamic allocation
 */
rte_status_t rte_watchdog_start(rte_watchdog_t watchdog);

/**
 * @brief Stop watchdog timer
 *
 * Disables watchdog monitoring. No timeout will occur until restarted.
 * Used during shutdown or maintenance.
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success
 *
 * @pre watchdog != NULL
 * @post watchdog is stopped; no timeout possible until start() called
 *
 * @safety Deterministic; disarms watchdog
 */
rte_status_t rte_watchdog_stop(rte_watchdog_t watchdog);

/**
 * @brief Kick (pet) watchdog - prove liveness
 *
 * Resets timeout countdown. Must be called periodically (before timeout
 * expires) to prevent watchdog from firing.
 *
 * Typical usage: called in main event loop, task loop, or after checkpoint.
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success (countdown reset)
 *         RTE_STATUS_ERROR if watchdog already fired (recovery in progress)
 *
 * @pre watchdog != NULL
 * @post timeout countdown reset to timeout_ms
 *
 * @safety Deterministic: no blocking, O(1) time
 *
 * Example:
 * @code
 * while (running) {
 *     process_events();
 *     rte_watchdog_kick(wd);  // Prove we're alive
 * }
 * @endcode
 */
rte_status_t rte_watchdog_kick(rte_watchdog_t watchdog);

/**
 * @brief Get watchdog status
 *
 * Non-blocking query of watchdog state (kicks, fires, time remaining).
 * Useful for health monitoring and diagnostics.
 *
 * @param watchdog Watchdog handle
 * @param status_out Receives watchdog status
 * @return RTE_STATUS_OK on success
 *
 * @pre watchdog != NULL, status_out != NULL
 * @post status_out populated with current watchdog state
 *
 * @safety Non-blocking, read-only, no side effects
 *
 * Example:
 * @code
 * rte_watchdog_status_t status;
 * rte_watchdog_get_status(wd, &status);
 * printf("Watchdog: %u kicks, %u fires, %u ms until timeout\n",
 *        status.kicks, status.fires, status.time_until_fire);
 * @endcode
 */
rte_status_t rte_watchdog_get_status(rte_watchdog_t watchdog,
                                        rte_watchdog_status_t *status_out);

/**
 * @brief Destroy watchdog
 *
 * Stops and deallocates watchdog. After destruction, handle is invalid.
 *
 * @param watchdog Watchdog handle
 * @return RTE_STATUS_OK on success
 *
 * @pre watchdog != NULL, watchdog was created via rte_watchdog_create()
 * @post watchdog is stopped and deallocated
 *
 * @safety Deterministic; safe to call on stopped or running watchdog
 */
rte_status_t rte_watchdog_destroy(rte_watchdog_t watchdog);

/* ============================================================================
 * API: Watchdog Manager (Global)
 * ========================================================================== */

/**
 * @brief Initialize watchdog manager (call once at startup)
 *
 * Sets up the central watchdog manager that coordinates all watchdog timers.
 * Must be called before creating any watchdogs.
 *
 * @return RTE_STATUS_OK on success
 *
 * @post watchdog manager is ready to create watchdogs
 *
 * @safety Called once at startup
 */
rte_status_t rte_watchdog_manager_initialize(void);

/**
 * @brief Shutdown watchdog manager (call once at shutdown)
 *
 * Stops all running watchdogs and shuts down manager.
 * After shutdown, cannot create new watchdogs until re-initialized.
 *
 * @return RTE_STATUS_OK on success
 *
 * @post all watchdogs stopped; manager deallocated
 *
 * @safety Safe to call multiple times; idempotent
 */
rte_status_t rte_watchdog_manager_shutdown(void);

/* ============================================================================
 * Watchdog Timeout Callback (Invoked by Framework)
 * ========================================================================== */

/**
 * @brief Watchdog timeout handler (INTERNAL - called by framework)
 *
 * Invoked by watchdog timer when timeout occurs. Applies recovery action
 * (log, safe-state, reboot, failover, or custom callback).
 *
 * This is a framework-internal function; applications don't call it directly.
 *
 * @param watchdog_id ID of watchdog that fired
 *
 * @note Called from timer interrupt context (may be ISR)
 * @note Recovery actions are async/deferred (logged, queued)
 *
 * @internal
 */
void rte_watchdog_timeout_handler(uint32_t watchdog_id);

/**
 * @brief Poll all active watchdogs for expiry (call periodically)
 *
 * This implementation has no OS-specific interrupt/thread of its own
 * (consistent with ADR-005: OS-specific timing belongs in an integrator
 * OSAdapter, not in this module). Instead, the integrating application is
 * responsible for calling this function regularly - e.g. from a
 * rte_timer periodic callback, or once per iteration of a
 * rte_appmanager execute() cycle - so that any watchdog whose deadline
 * has passed is detected and its configured recovery action
 * (rte_watchdog_timeout_handler()) is dispatched.
 *
 * @note Non-blocking; O(N) over the fixed watchdog pool per call.
 * @note No-op if the watchdog manager has not been initialized.
 */
void rte_watchdog_timer_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_WATCHDOG_H */

/** @} */ /* WATCHDOG */

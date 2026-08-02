/**
 * @file sapi_watchdog.h
 * @brief Watchdog mechanism for detecting system/task hang conditions
 *
 * ⚠️ **STATUS:** API DESIGN COMPLETE, IMPLEMENTATION IN PROGRESS
 * Target availability: safeAPIFramework v0.3.0
 *
 * Provides system-level, task-level, and channel-level watchdog timers
 * that detect hung components and trigger recovery actions (reboot, safe-state,
 * failover). Full API is defined below; stub implementations are being replaced
 * with production code.
 *
 * @defgroup WATCHDOG Watchdog Mechanism
 * @brief Detect hung systems/tasks and trigger recovery
 * @{
 */

#ifndef SAPI_WATCHDOG_H
#define SAPI_WATCHDOG_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types & Constants
 * ========================================================================== */

/**
 * @brief Watchdog handle (opaque)
 */
typedef struct sapi_watchdog_s *sapi_watchdog_t;

/**
 * @brief Watchdog type
 *
 * Defines what the watchdog is monitoring.
 */
typedef enum {
    SAPI_WATCHDOG_SYSTEM,      /**< Entire system liveness */
    SAPI_WATCHDOG_TASK,        /**< Specific task/thread */
    SAPI_WATCHDOG_CHANNEL,     /**< IPC/redundancy channel */
    SAPI_WATCHDOG_CHECKPOINT   /**< Checkpoint barrier */
} sapi_watchdog_type_t;

/**
 * @brief Recovery action when watchdog fires
 *
 * Defines what happens when watchdog timeout occurs.
 */
typedef enum {
    SAPI_WATCHDOG_ACTION_LOG,           /**< Log event only */
    SAPI_WATCHDOG_ACTION_SAFESTATE,     /**< Trigger safe-state */
    SAPI_WATCHDOG_ACTION_REBOOT,        /**< System reboot */
    SAPI_WATCHDOG_ACTION_FAILOVER,      /**< Failover to backup */
    SAPI_WATCHDOG_ACTION_CUSTOM         /**< Custom callback */
} sapi_watchdog_action_t;

/**
 * @brief Watchdog configuration
 *
 * Defines watchdog behavior: type, timeout, recovery action.
 */
typedef struct {
    sapi_watchdog_type_t type;          /**< Watchdog type (system/task/channel) */
    const char *name;                   /**< Watchdog name (for logging) */
    sapi_duration_ms_t timeout_ms;      /**< Timeout deadline (ms) */
    sapi_watchdog_action_t action;      /**< Recovery action on timeout */
    void (*custom_action)(void *ctx);   /**< Custom action callback (if action=CUSTOM) */
    void *context;                      /**< Context for callback */
} sapi_watchdog_config_t;

/**
 * @brief Watchdog health/status information
 *
 * Retrieved via sapi_watchdog_get_status() for monitoring.
 */
typedef struct {
    uint8_t active;                     /**< 1 = watchdog enabled */
    uint32_t kicks;                     /**< Total number of kicks */
    uint32_t fires;                     /**< Total number of timeouts */
    uint32_t recoveries;                /**< Total recovery actions */
    sapi_duration_ms_t time_since_last_kick; /**< ms since last kick */
    sapi_duration_ms_t time_until_fire;      /**< ms until timeout */
} sapi_watchdog_status_t;

/* ============================================================================
 * API: Core Watchdog Operations
 * ========================================================================== */

/**
 * @brief Create a watchdog
 *
 * Initializes a watchdog with specified configuration. Watchdog starts
 * in disabled state; call sapi_watchdog_start() to enable.
 *
 * @param handle_out Receives watchdog handle
 * @param config Watchdog configuration
 * @return SAPI_STATUS_OK on success
 *         SAPI_STATUS_ERROR on failure
 *
 * @pre config != NULL
 * @post watchdog is created but not started
 *
 * @safety No dynamic memory allocation; all storage pre-allocated.
 *
 * Example (system watchdog):
 * @code
 * sapi_watchdog_config_t config = {
 *     .type = SAPI_WATCHDOG_SYSTEM,
 *     .name = "rbc_main_wd",
 *     .timeout_ms = 1000,
 *     .action = SAPI_WATCHDOG_ACTION_SAFESTATE
 * };
 * sapi_watchdog_t wd;
 * sapi_watchdog_create(&wd, &config);
 * @endcode
 */
sapi_status_t sapi_watchdog_create(sapi_watchdog_t *handle_out,
                                    const sapi_watchdog_config_t *config);

/**
 * @brief Start watchdog timer
 *
 * Enables watchdog monitoring. Countdown begins from timeout_ms.
 * Must be called after sapi_watchdog_create().
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success
 *
 * @pre watchdog != NULL
 * @post watchdog is counting down; will fire if not kicked
 *
 * @safety Deterministic: no blocking, no dynamic allocation
 */
sapi_status_t sapi_watchdog_start(sapi_watchdog_t watchdog);

/**
 * @brief Stop watchdog timer
 *
 * Disables watchdog monitoring. No timeout will occur until restarted.
 * Used during shutdown or maintenance.
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success
 *
 * @pre watchdog != NULL
 * @post watchdog is stopped; no timeout possible until start() called
 *
 * @safety Deterministic; disarms watchdog
 */
sapi_status_t sapi_watchdog_stop(sapi_watchdog_t watchdog);

/**
 * @brief Kick (pet) watchdog - prove liveness
 *
 * Resets timeout countdown. Must be called periodically (before timeout
 * expires) to prevent watchdog from firing.
 *
 * Typical usage: called in main event loop, task loop, or after checkpoint.
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success (countdown reset)
 *         SAPI_STATUS_ERROR if watchdog already fired (recovery in progress)
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
 *     sapi_watchdog_kick(wd);  // Prove we're alive
 * }
 * @endcode
 */
sapi_status_t sapi_watchdog_kick(sapi_watchdog_t watchdog);

/**
 * @brief Get watchdog status
 *
 * Non-blocking query of watchdog state (kicks, fires, time remaining).
 * Useful for health monitoring and diagnostics.
 *
 * @param watchdog Watchdog handle
 * @param status_out Receives watchdog status
 * @return SAPI_STATUS_OK on success
 *
 * @pre watchdog != NULL, status_out != NULL
 * @post status_out populated with current watchdog state
 *
 * @safety Non-blocking, read-only, no side effects
 *
 * Example:
 * @code
 * sapi_watchdog_status_t status;
 * sapi_watchdog_get_status(wd, &status);
 * printf("Watchdog: %u kicks, %u fires, %u ms until timeout\n",
 *        status.kicks, status.fires, status.time_until_fire);
 * @endcode
 */
sapi_status_t sapi_watchdog_get_status(sapi_watchdog_t watchdog,
                                        sapi_watchdog_status_t *status_out);

/**
 * @brief Destroy watchdog
 *
 * Stops and deallocates watchdog. After destruction, handle is invalid.
 *
 * @param watchdog Watchdog handle
 * @return SAPI_STATUS_OK on success
 *
 * @pre watchdog != NULL, watchdog was created via sapi_watchdog_create()
 * @post watchdog is stopped and deallocated
 *
 * @safety Deterministic; safe to call on stopped or running watchdog
 */
sapi_status_t sapi_watchdog_destroy(sapi_watchdog_t watchdog);

/* ============================================================================
 * API: Watchdog Manager (Global)
 * ========================================================================== */

/**
 * @brief Initialize watchdog manager (call once at startup)
 *
 * Sets up the central watchdog manager that coordinates all watchdog timers.
 * Must be called before creating any watchdogs.
 *
 * @return SAPI_STATUS_OK on success
 *
 * @post watchdog manager is ready to create watchdogs
 *
 * @safety Called once at startup
 */
sapi_status_t sapi_watchdog_manager_initialize(void);

/**
 * @brief Shutdown watchdog manager (call once at shutdown)
 *
 * Stops all running watchdogs and shuts down manager.
 * After shutdown, cannot create new watchdogs until re-initialized.
 *
 * @return SAPI_STATUS_OK on success
 *
 * @post all watchdogs stopped; manager deallocated
 *
 * @safety Safe to call multiple times; idempotent
 */
sapi_status_t sapi_watchdog_manager_shutdown(void);

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
void sapi_watchdog_timeout_handler(uint32_t watchdog_id);

#ifdef __cplusplus
}
#endif

#endif /* SAPI_WATCHDOG_H */

/** @} */ /* WATCHDOG */

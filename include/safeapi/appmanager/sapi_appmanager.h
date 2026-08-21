/**
 * @file sapi_appmanager.h
 * @brief Application Manager abstraction for safeAPIFramework applications
 *
 * Provides a single entry point pattern for safety-critical applications with
 * well-defined lifecycle: initialize → execute → shutdown.
 *
 * Each application implements the sapi_appmanager_operations_t interface:
 *   - init() — One-time initialization
 *   - pre_execute() — Optional, per-cycle input/prepare stage (may be NULL)
 *   - execute() — Main application loop (mandatory)
 *   - post_execute() — Optional, per-cycle output/cleanup stage (may be NULL)
 *   - shutdown() — Graceful cleanup
 *
 * The sapi_appmanager_run() function manages lifecycle and error handling.
 * Per ADR-019, it can also be configured (sapi_appmanager_config_t::checkpoint)
 * to perform a bounded sapi_channel_checkpoint() rendezvous (ADR-017) at the
 * start of every cycle, ahead of pre_execute/execute/post_execute, so a
 * dual/multi-channel application does not have to hand-roll that call
 * itself; this is opt-in and defaults to disabled (NULL).
 *
 * REQ-APPMANAGER-001: Applications shall use the Application Manager for
 * controlled initialization, execution, and shutdown lifecycle.
 *
 * ADR-026: the moment init() returns SAPI_STATUS_OK, sapi_appmanager_run()
 * locks the application's setup phase (sapi_lifecycle_lock() - see
 * sapi_lifecycle.h) - every setup-only constructor this framework ships
 * (sapi_timer_create(), sapi_channel_init(), sapi_voter_init()/
 * _register_channel(), sapi_cross_comparator_init()/_register_channel(),
 * sapi_watchdog_create()) then rejects with SAPI_STATUS_INVALID_STATE for
 * the remainder of this run: a timer/channel/voter/cross-comparator/
 * watchdog an application's own init() did not already create is not one
 * its execute()/pre_execute()/post_execute() may create either.
 * sapi_netlink_open()/sapi_dual_channel_init()/sapi_dual_negotiator_init()
 * are deliberately NOT gated by this lock - see sapi_lifecycle.h's own
 * doc for why a link an application already owns being re-established
 * after a drop is not the same thing this lock exists to prevent.
 *
 * @note Unlike the seven OAL services (sapi_timer, sapi_nvm, sapi_memory,
 * sapi_task, sapi_ipc, sapi_log, sapi_reboot), this module is not
 * backend-dispatched (ADR-005) - it is a direct, OS-agnostic
 * implementation, same as sapi_safestate. sapi_appmanager_install_default_signal_handlers()
 * below is a **deliberate, narrow exception** to that OS-agnosticism: it
 * is a POSIX-only convenience, compiled out (returns
 * SAPI_STATUS_NOT_SUPPORTED) on any non-POSIX target. It exists because
 * "let something external ask a long-running sapi_appmanager_run() loop
 * to stop" is such a common integration need on POSIX hosts that most
 * integrators would otherwise reimplement the same few lines of
 * sigaction() themselves - see that function's own doc for the exact
 * scope of the exception.
 *
 * @defgroup APPMANAGER Application Manager
 * @brief init/execute/shutdown lifecycle entry point for applications
 * @{
 */

#ifndef SAFEAPI_APPMANAGER_H
#define SAFEAPI_APPMANAGER_H

#include <stdint.h>
#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"
#include "safeapi/voter/sapi_voter.h"
#include "safeapi/watchdog/sapi_watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Application Manager Interface
 * ========================================================================== */

/**
 * @brief Application state enumeration
 *
 * Tracks the lifecycle state of an application managed by the app manager.
 */
typedef enum {
    SAPI_APP_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    SAPI_APP_STATE_INITIALIZING = 1,   /**< Initialization in progress */
    SAPI_APP_STATE_RUNNING = 2,        /**< Normal execution */
    SAPI_APP_STATE_SHUTTING_DOWN = 3,  /**< Shutdown in progress */
    SAPI_APP_STATE_SHUTDOWN = 4,       /**< Shutdown complete */
    SAPI_APP_STATE_ERROR = 5           /**< Error state */
} sapi_app_state_t;

/**
 * @brief Application lifecycle operations
 *
 * Each application must implement these operations to be managed by the
 * application manager.
 *
 * REQ-APPMANAGER-002: Applications shall implement all mandatory operations
 * in the sapi_appmanager_operations_t interface (init, execute, shutdown,
 * get_name, get_version). pre_execute and post_execute are optional
 * (ADR-019, REQ-APPMANAGER-006) and may be left NULL.
 */
typedef struct {
    /**
     * @brief Initialize application
     *
     * Called once at startup. Should initialize all services, resources,
     * and state. If initialization fails, the application is not started.
     *
     * @param context Application-specific context pointer
     * @return SAPI_STATUS_OK on success, error code on failure
     */
    sapi_status_t (*init)(void *context);

    /**
     * @brief Optional per-cycle input/prepare stage (ADR-019)
     *
     * If non-NULL, called once per iteration immediately before execute()
     * (and, if configured, immediately after that cycle's checkpoint
     * rendezvous - see sapi_appmanager_config_t::checkpoint). Intended for
     * "read/prepare this cycle's inputs" work that a dual-channel
     * application wants to keep separate from its decision logic. May be
     * NULL, in which case this stage is skipped - existing applications
     * that only implement execute() are unaffected.
     *
     * A non-OK return is handled exactly like execute() returning
     * non-OK: logged, counted against error_count, and checked against
     * error_threshold; execute() and post_execute() are still skipped
     * for that iteration in that case (see sapi_appmanager_run()).
     *
     * @param context Application-specific context pointer
     * @return SAPI_STATUS_OK on normal execution
     *         Other codes for error conditions
     */
    sapi_status_t (*pre_execute)(void *context);

    /**
     * @brief Execute main application logic
     *
     * Called in a loop after initialization. The execute function should
     * process one iteration of work and return. Returning non-OK status
     * may trigger shutdown depending on error severity.
     *
     * @param context Application-specific context pointer
     * @return SAPI_STATUS_OK on normal execution
     *         Other codes for error conditions
     */
    sapi_status_t (*execute)(void *context);

    /**
     * @brief Optional per-cycle output/cleanup stage (ADR-019)
     *
     * If non-NULL, called once per iteration immediately after execute()
     * returns SAPI_STATUS_OK. Intended for "send this cycle's outputs"
     * work that a dual-channel application wants to keep separate from
     * its decision logic. May be NULL, in which case this stage is
     * skipped - existing applications that only implement execute() are
     * unaffected.
     *
     * A non-OK return is handled exactly like execute() returning
     * non-OK: logged, counted against error_count, and checked against
     * error_threshold.
     *
     * @param context Application-specific context pointer
     * @return SAPI_STATUS_OK on normal execution
     *         Other codes for error conditions
     */
    sapi_status_t (*post_execute)(void *context);

    /**
     * @brief Shutdown application
     *
     * Called once at shutdown. Should release resources and perform
     * graceful cleanup. This is always called, even if init/execute failed.
     *
     * @param context Application-specific context pointer
     * @return SAPI_STATUS_OK on success
     *
     * Note: This function must not fail. It should handle all cleanup
     * robustly, logging errors but continuing cleanup.
     */
    sapi_status_t (*shutdown)(void *context);

    /**
     * @brief Get human-readable application name
     *
     * Used for logging and diagnostics.
     *
     * @return Pointer to null-terminated application name
     */
    const char *(*get_name)(void);

    /**
     * @brief Get application version string
     *
     * Used for logging and version tracking.
     *
     * @return Pointer to null-terminated version string (e.g., "1.0.0")
     */
    const char *(*get_version)(void);
} sapi_appmanager_operations_t;

/**
 * @brief Optional built-in checkpoint rendezvous configuration (ADR-019).
 *
 * When attached to sapi_appmanager_config_t::checkpoint, sapi_appmanager_run()
 * calls sapi_channel_checkpoint() once at the start of every cycle - before
 * pre_execute()/execute()/post_execute() - using the running
 * sapi_appmanager_state_t::iteration_count as the checkpoint's checkpoint_id,
 * so no separate per-cycle counter is needed. Leave
 * sapi_appmanager_config_t::checkpoint NULL to disable this entirely (the
 * default); this is opt-in because sapi_channel_checkpoint() blocks for up
 * to max_delay_ms, which is only wanted by applications that are actually
 * part of a synchronized multi-channel group.
 *
 * @safety A checkpoint timeout does not introduce a second safety reaction:
 *         sapi_channel_checkpoint() has already called sapi_safestate_enter()
 *         at SAPI_SAFESTATE_LEVEL_SAFE (REQ-CHECKPOINT-003) before returning
 *         SAPI_STATUS_TIMEOUT to sapi_appmanager_run(), which then folds
 *         that status into its ordinary error_count/error_threshold
 *         handling like any other failed stage (REQ-APPMANAGER-007). Note
 *         that with this framework's shipped sapi_safestate.c, entering
 *         SAPI_SAFESTATE_LEVEL_SAFE is an unconditional, permanent halt
 *         (REQ-COMMON-SAFESTATE-002), so this error_count path is a
 *         defensive fallback - correct if ever reached - rather than the
 *         expected outcome of a real timeout.
 */
typedef struct {
    sapi_voter_t *voter;                   /**< Checkpoint target: a voter with its channels already registered
                                             *   (ADR-025 - previously a single sapi_channel_t; that type is
                                             *   now one link, registered N-per-voter). May still be NULL when
                                             *   sapi_appmanager_run() is first called, e.g. if an integrator's
                                             *   own init() is what populates it. May also be set back to NULL
                                             *   at runtime (e.g. from a background reconnect task) to pause
                                             *   checkpointing without that being treated as an error - see
                                             *   sapi_appmanager_run()'s own doc. */
    sapi_duration_ms_t max_delay_ms;       /**< Forwarded to sapi_checkpoint_config_t::max_delay_ms. */
    uint32_t expected_node_count;          /**< Forwarded to sapi_checkpoint_config_t::expected_node_count. */
    sapi_watchdog_t watchdog;              /**< Optional liveness watchdog kicked on success; may be NULL. */
} sapi_appmanager_checkpoint_config_t;

/**
 * @brief Application manager configuration
 */
typedef struct {
    const sapi_appmanager_operations_t *ops;  /**< Application operations */
    void *context;                            /**< Application context */
    uint32_t max_iterations;                  /**< Max execute() calls (0 = infinite) */
    uint32_t error_threshold;                 /**< Errors before shutdown (0 = no limit) */
    const sapi_appmanager_checkpoint_config_t *checkpoint; /**< Optional built-in cycle checkpoint (ADR-019); NULL = disabled (default). */
} sapi_appmanager_config_t;

/**
 * @brief Application manager runtime state
 */
typedef struct {
    sapi_app_state_t state;         /**< Current lifecycle state (init/running/etc.). */
    uint32_t iteration_count;       /**< Number of completed execute() cycles. */
    uint32_t error_count;           /**< Number of execute() cycles that returned an error. */
    sapi_status_t last_error;       /**< Most recent non-OK status observed, if any. */
} sapi_appmanager_state_t;

/* ============================================================================
 * Application Manager API
 * ========================================================================== */

/**
 * @brief Run application with lifecycle management
 *
 * This is the single entry point for all applications. It manages the
 * complete lifecycle:
 *   1. Initialize (init)
 *   2. Per-cycle loop, in order (ADR-019):
 *      a. Checkpoint rendezvous, if config->checkpoint is non-NULL
 *      b. pre_execute(), if ops->pre_execute is non-NULL
 *      c. execute()
 *      d. post_execute(), if ops->post_execute is non-NULL
 *   3. Shutdown (shutdown)
 *
 * Error handling:
 * - REQ-APPMANAGER-009 (ADR-026): if a PREVIOUS call to this function is
 *   still mid-lifecycle (its own INITIALIZING/RUNNING/SHUTTING_DOWN) when
 *   this one is entered, it is refused immediately with EXIT_FAILURE and
 *   none of that previous call's state is touched - this is the single
 *   entry point and cannot be concurrently re-entered. A NEW call made
 *   only after a previous one has fully returned (state SHUTDOWN/ERROR)
 *   is unaffected - this framework's own test suite relies on exactly
 *   that sequential-call pattern.
 * - If init() fails, shutdown() is still called and EXIT_FAILURE is returned
 * - If the checkpoint, pre_execute(), execute(), or post_execute() stage of
 *   a cycle returns non-OK, that is logged and counted against error_count
 *   exactly the same way regardless of which stage failed; the remaining
 *   stages of that same cycle are skipped, and the loop continues to the
 *   next cycle (unless error_threshold is reached)
 * - If error_threshold is reached, application shuts down
 * - shutdown() is always called, even on error
 *
 * @param config Application manager configuration
 * @return 0 (EXIT_SUCCESS) if application completed normally
 *         1 (EXIT_FAILURE) if initialization failed or errors exceeded threshold
 *
 * Example (single-stage, unchanged from before ADR-019):
 * @code
 * const sapi_appmanager_operations_t ops = {
 *     .init = my_app_init,
 *     .execute = my_app_execute,
 *     .shutdown = my_app_shutdown,
 *     .get_name = my_app_get_name,
 *     .get_version = my_app_get_version
 * };
 *
 * sapi_appmanager_config_t config = {
 *     .ops = &ops,
 *     .context = &my_app_context,
 *     .max_iterations = 0,      // Run forever
 *     .error_threshold = 10      // Stop after 10 errors
 * };
 *
 * return sapi_appmanager_run(&config);
 * @endcode
 *
 * Example (pre/post hooks plus a built-in cross-channel checkpoint, ADR-019):
 * @code
 * const sapi_appmanager_operations_t ops = {
 *     .init = my_app_init,
 *     .pre_execute = my_app_read_inputs,
 *     .execute = my_app_decide,
 *     .post_execute = my_app_send_outputs,
 *     .shutdown = my_app_shutdown,
 *     .get_name = my_app_get_name,
 *     .get_version = my_app_get_version
 * };
 *
 * static const sapi_appmanager_checkpoint_config_t checkpoint_cfg = {
 *     .voter = &my_voter,   // already has its channels sapi_voter_register_channel()-ed
 *     .max_delay_ms = 200,
 *     .expected_node_count = 1,
 *     .watchdog = NULL
 * };
 *
 * sapi_appmanager_config_t config = {
 *     .ops = &ops,
 *     .context = &my_app_context,
 *     .max_iterations = 0,
 *     .error_threshold = 10,
 *     .checkpoint = &checkpoint_cfg
 * };
 *
 * return sapi_appmanager_run(&config);
 * @endcode
 */
int sapi_appmanager_run(const sapi_appmanager_config_t *config);

/**
 * @brief Get current application state
 *
 * Returns the current state of a running application manager.
 * Note: This is intended for monitoring, not for control.
 *
 * @return Current application state
 */
sapi_app_state_t sapi_appmanager_get_state(void);

/**
 * @brief Get application statistics
 *
 * Returns runtime statistics (iterations, errors, etc.).
 *
 * @param state Pointer to sapi_appmanager_state_t to populate
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_appmanager_get_stats(sapi_appmanager_state_t *state);

/**
 * @brief Request application shutdown
 *
 * Signals the application to begin shutdown. The execute loop will
 * terminate after the current iteration.
 *
 * Note: This function requests shutdown but does not guarantee immediate
 * termination. The shutdown may take one iteration to complete.
 */
void sapi_appmanager_request_shutdown(void);

/**
 * @brief POSIX-only convenience: installs SIGINT and SIGTERM handlers
 *        that call sapi_appmanager_request_shutdown(), so an operator
 *        (Ctrl+C) or process manager (SIGTERM) can stop a
 *        sapi_appmanager_run() loop gracefully - shutdown() still runs,
 *        this is not a hard kill.
 *
 * This is the one function in this module with an OS dependency - see
 * this header's own file-level @note for why that is an intentional,
 * scoped exception rather than a general pattern for this framework.
 * Safe to call unconditionally on any target: on a non-POSIX build this
 * compiles to a no-op that returns SAPI_STATUS_NOT_SUPPORTED, so
 * portable integration code does not need its own \#ifdef around the
 * call site.
 *
 * Call this before sapi_appmanager_run() (or from init()); calling it
 * more than once re-installs the same handlers (idempotent, matches
 * sapi_safestate_register_handler()'s "registering again replaces the
 * previous" convention).
 *
 * @return SAPI_STATUS_OK if both handlers were installed;
 *         SAPI_STATUS_NOT_SUPPORTED on a non-POSIX build;
 *         SAPI_STATUS_INTERNAL_ERROR if the underlying sigaction() call
 *         itself failed (see errno at the call site for detail - not
 *         surfaced here, consistent with this framework not using errno
 *         in its own public API, CLAUDE.md's "no use of errno in
 *         production paths").
 */
sapi_status_t sapi_appmanager_install_default_signal_handlers(void);

/**
 * @brief Forcibly resets the application manager's own bookkeeping
 *        (lifecycle state, iteration/error counters, shutdown-request
 *        flag, and the ADR-026 setup-phase lock) back to its initial,
 *        pre-run condition.
 *
 * Normal use of sapi_appmanager_run() never requires this: every one of
 * its own entry/exit paths already resets this same state on its own
 * (see REQ-APPMANAGER-009). This function exists for the one case that
 * bypasses those paths entirely: an application-level fault handler that
 * itself performs a non-local jump (e.g. `longjmp()`) out of a
 * `SAPI_SAFESTATE_LEVEL_SAFE`/`_REBOOT` reaction instead of the
 * framework's own documented never-returns contract
 * (REQ-COMMON-SAFESTATE-002). After such a jump, sapi_appmanager_run()
 * never reaches its own SHUTDOWN phase, and this module's internal state
 * would otherwise stay permanently stuck mid-lifecycle - causing every
 * subsequent sapi_appmanager_run() call to be rejected by the
 * single-entry-point guard. Call this once, immediately after regaining
 * control via such a non-local jump, before calling
 * sapi_appmanager_run() again.
 *
 * @safety Calling this while a sapi_appmanager_run() call is genuinely
 *         still executing (not abandoned via a non-local jump)
 *         corrupts that call's own state. This function exists
 *         specifically for the abandoned-via-non-local-jump case, not
 *         general use - MISRA C:2012 Rule 21.4 already prohibits
 *         `<setjmp.h>` in production code, so this situation should
 *         never arise there; it exists because this framework's own
 *         test suite has no other way to exercise a
 *         `SAPI_SAFESTATE_LEVEL_SAFE` reaction (which never returns)
 *         without one.
 */
void sapi_appmanager_reset_state(void);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_APPMANAGER_H */

/** @} */ /* APPMANAGER */

/**
 * @file rte_appmanager.h
 * @brief Application Manager abstraction for RteFramework applications
 *
 * Provides a single entry point pattern for safety-critical applications with
 * well-defined lifecycle: initialize → execute → shutdown.
 *
 * Each application implements the rte_appmanager_operations_t interface:
 *   - init() — One-time initialization
 *   - pre_execute() — Optional, per-cycle input/prepare stage (may be NULL)
 *   - execute() — Main application loop (mandatory)
 *   - post_execute() — Optional, per-cycle output/cleanup stage (may be NULL)
 *   - shutdown() — Graceful cleanup
 *
 * The rte_appmanager_run() function manages lifecycle and error handling.
 * Per ADR-019/ADR-034, it can also be configured (rte_appmanager_config_t::checkpoint)
 * to perform a bounded rte_channel_checkpoint() rendezvous (ADR-017) as the
 * LAST stage of every cycle, after pre_execute/execute/post_execute, so a
 * dual/multi-channel application does not have to hand-roll that call
 * itself; this is opt-in and defaults to disabled (NULL).
 *
 * ADR-034: the checkpoint's own checkpoint_id is no longer a bare cycle
 * counter (that broke across an independent reboot of just one channel -
 * see rte_appmanager_checkpoint_config_t's own doc). Instead, application
 * code calls RTE_CHECKPOINT_MARK() (or the labelled variant,
 * RTE_CHECKPOINT_MARK_LABEL()) at meaningful decision/preparation points
 * during pre_execute()/execute()/post_execute(); each call folds a CRC64
 * hash of its call site (by default __FILE__:__LINE__, or a caller-supplied
 * label) into a single running per-cycle signature. The checkpoint stage
 * then rendezvous-compares THIS cycle's own folded signature with the
 * peer's - so a match proves both channels reached the exact same sequence
 * of marks this cycle (the same program path), not merely "both channels
 * are alive." If ops->on_checkpoint_result is non-NULL, it is called
 * exactly once per cycle with the outcome, letting an application stage
 * (queue but not transmit) its outputs during the cycle and only commit
 * (actually send) them once the checkpoint confirms both channels agree -
 * see that field's own doc.
 *
 * REQ-APPMANAGER-001: Applications shall use the Application Manager for
 * controlled initialization, execution, and shutdown lifecycle.
 *
 * ADR-026: the moment init() returns RTE_STATUS_OK, rte_appmanager_run()
 * locks the application's setup phase (rte_lifecycle_lock() - see
 * rte_lifecycle.h) - every setup-only constructor this framework ships
 * (rte_timer_create(), rte_channel_init(), rte_voter_init()/
 * _register_channel(), rte_cross_comparator_init()/_register_channel(),
 * rte_watchdog_create()) then rejects with RTE_STATUS_INVALID_STATE for
 * the remainder of this run: a timer/channel/voter/cross-comparator/
 * watchdog an application's own init() did not already create is not one
 * its execute()/pre_execute()/post_execute() may create either.
 * rte_netlink_open()/rte_dual_channel_init()/rte_dual_negotiator_init()
 * are deliberately NOT gated by this lock - see rte_lifecycle.h's own
 * doc for why a link an application already owns being re-established
 * after a drop is not the same thing this lock exists to prevent.
 *
 * @note Unlike the seven OAL services (rte_timer, rte_nvm, rte_memory,
 * rte_task, rte_ipc, rte_log, rte_reboot), this module is not
 * osadapter-dispatched (ADR-005) - it is a direct, OS-agnostic
 * implementation, same as rte_safestate. rte_appmanager_install_default_signal_handlers()
 * below is a **deliberate, narrow exception** to that OS-agnosticism: it
 * is a POSIX-only convenience, compiled out (returns
 * RTE_STATUS_NOT_SUPPORTED) on any non-POSIX target. It exists because
 * "let something external ask a long-running rte_appmanager_run() loop
 * to stop" is such a common integration need on POSIX hosts that most
 * integrators would otherwise reimplement the same few lines of
 * sigaction() themselves - see that function's own doc for the exact
 * scope of the exception.
 *
 * @defgroup APPMANAGER Application Manager
 * @brief init/execute/shutdown lifecycle entry point for applications
 * @{
 */

#ifndef RTE_APPMANAGER_H
#define RTE_APPMANAGER_H

#include <stdint.h>
#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"
#include "rte/redundancy/voter/rte_voter.h"
#include "rte/redundancy/watchdog/rte_watchdog.h"

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
    RTE_APP_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    RTE_APP_STATE_INITIALIZING = 1,   /**< Initialization in progress */
    RTE_APP_STATE_RUNNING = 2,        /**< Normal execution */
    RTE_APP_STATE_SHUTTING_DOWN = 3,  /**< Shutdown in progress */
    RTE_APP_STATE_SHUTDOWN = 4,       /**< Shutdown complete */
    RTE_APP_STATE_ERROR = 5           /**< Error state */
} rte_app_state_t;

/**
 * @brief Application lifecycle operations
 *
 * Each application must implement these operations to be managed by the
 * application manager.
 *
 * REQ-APPMANAGER-002: Applications shall implement all mandatory operations
 * in the rte_appmanager_operations_t interface (init, execute, shutdown,
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
     * @return RTE_STATUS_OK on success, error code on failure
     */
    rte_status_t (*init)(void *context);

    /**
     * @brief Optional per-cycle input/prepare stage (ADR-019)
     *
     * If non-NULL, called once per iteration immediately before execute()
     * (and, if configured, immediately after that cycle's checkpoint
     * rendezvous - see rte_appmanager_config_t::checkpoint). Intended for
     * "read/prepare this cycle's inputs" work that a dual-channel
     * application wants to keep separate from its decision logic. May be
     * NULL, in which case this stage is skipped - existing applications
     * that only implement execute() are unaffected.
     *
     * A non-OK return is handled exactly like execute() returning
     * non-OK: logged, counted against error_count, and checked against
     * error_threshold; execute() and post_execute() are still skipped
     * for that iteration in that case (see rte_appmanager_run()).
     *
     * @param context Application-specific context pointer
     * @return RTE_STATUS_OK on normal execution
     *         Other codes for error conditions
     */
    rte_status_t (*pre_execute)(void *context);

    /**
     * @brief Execute main application logic
     *
     * Called in a loop after initialization. The execute function should
     * process one iteration of work and return. Returning non-OK status
     * may trigger shutdown depending on error severity.
     *
     * @param context Application-specific context pointer
     * @return RTE_STATUS_OK on normal execution
     *         Other codes for error conditions
     */
    rte_status_t (*execute)(void *context);

    /**
     * @brief Optional per-cycle output/cleanup stage (ADR-019)
     *
     * If non-NULL, called once per iteration immediately after execute()
     * returns RTE_STATUS_OK. Intended for "send this cycle's outputs"
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
     * @return RTE_STATUS_OK on normal execution
     *         Other codes for error conditions
     */
    rte_status_t (*post_execute)(void *context);

    /**
     * @brief Optional per-cycle checkpoint outcome hook (ADR-034)
     *
     * If non-NULL and config->checkpoint is configured, called exactly
     * once per cycle immediately after the checkpoint stage (the last
     * stage of the cycle - see this header's own file-level doc and
     * rte_appmanager_run()'s doc for the stage order). Intended for a
     * "stage output during the cycle, only actually transmit it here"
     * pattern: `committed` is true when this cycle's checkpoint signature
     * matched the peer's (or checkpoint is disabled/paused - see below),
     * meaning any output staged during pre_execute()/execute()/
     * post_execute() may now be safely sent; false only when
     * rte_channel_checkpoint() itself returned non-OK (a genuine
     * validation failure or timeout - REQ-CHECKPOINT-003 has already
     * driven this process to RTE_SAFESTATE_LEVEL_SAFE by the time this
     * is called), meaning staged output for this cycle should be
     * discarded, never transmitted.
     *
     * Called with committed=true (never false) when config->checkpoint is
     * NULL, or when config->checkpoint->voter is NULL (checkpointing
     * paused) - an application using the stage-then-commit pattern does
     * not need to special-case "was checkpoint even active this cycle";
     * disabled/paused checkpointing simply never withholds a commit. May
     * be NULL, in which case this hook is skipped entirely - existing
     * applications that do not stage output are unaffected.
     *
     * A non-OK return is handled exactly like the other optional stages:
     * logged and counted against error_count/error_threshold.
     *
     * @param context   Application-specific context pointer
     * @param committed true if this cycle's output may be sent; false if
     *                  it must be discarded
     * @return RTE_STATUS_OK on normal handling; other codes for error
     *         conditions (e.g. the staged-output queue itself failed to
     *         flush)
     */
    rte_status_t (*on_checkpoint_result)(void *context, bool committed);

    /**
     * @brief Shutdown application
     *
     * Called once at shutdown. Should release resources and perform
     * graceful cleanup. This is always called, even if init/execute failed.
     *
     * @param context Application-specific context pointer
     * @return RTE_STATUS_OK on success
     *
     * Note: This function must not fail. It should handle all cleanup
     * robustly, logging errors but continuing cleanup.
     */
    rte_status_t (*shutdown)(void *context);

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
} rte_appmanager_operations_t;

/**
 * @brief Optional built-in checkpoint rendezvous configuration (ADR-019/ADR-034).
 *
 * When attached to rte_appmanager_config_t::checkpoint, rte_appmanager_run()
 * calls rte_channel_checkpoint() once at the END of every cycle - after
 * pre_execute()/execute()/post_execute() have all run - using this cycle's
 * own folded checkpoint-mark signature (see this header's file-level ADR-034
 * doc and RTE_CHECKPOINT_MARK()) as the checkpoint's checkpoint_id.
 *
 * ADR-034 history: this used to run FIRST, before pre_execute(), using the
 * running rte_appmanager_state_t::iteration_count as checkpoint_id, on the
 * theory that "no separate per-cycle counter is needed." That broke in
 * practice: iteration_count is a process-local counter that resets to 0 on
 * every reboot, and two independently-rebooting channels' counters have no
 * reason to ever coincide again after either one reboots alone - REQ-
 * CHECKPOINT-002 would then correctly (but uselessly) keep rejecting every
 * reply as "wrong checkpoint_id" until, by chance, both channels next
 * rebooted together. The mark-signature scheme replaces the counter with a
 * value that is naturally equal on both sides whenever they actually took
 * the same program path this cycle, regardless of either side's reboot
 * history.
 *
 * Leave rte_appmanager_config_t::checkpoint NULL to disable this entirely
 * (the default); this is opt-in because rte_channel_checkpoint() blocks
 * for up to max_delay_ms, which is only wanted by applications that are
 * actually part of a synchronized multi-channel group.
 *
 * @safety A checkpoint timeout does not introduce a second safety reaction:
 *         rte_channel_checkpoint() has already called rte_safestate_enter()
 *         at RTE_SAFESTATE_LEVEL_SAFE (REQ-CHECKPOINT-003) before returning
 *         RTE_STATUS_TIMEOUT to rte_appmanager_run(), which then folds
 *         that status into its ordinary error_count/error_threshold
 *         handling like any other failed stage (REQ-APPMANAGER-007). Note
 *         that with this framework's shipped rte_safestate.c, entering
 *         RTE_SAFESTATE_LEVEL_SAFE is an unconditional, permanent halt
 *         (REQ-COMMON-SAFESTATE-002), so this error_count path is a
 *         defensive fallback - correct if ever reached - rather than the
 *         expected outcome of a real timeout.
 */
typedef struct {
    rte_voter_t *voter;                   /**< Checkpoint target: a voter with its channels already registered
                                             *   (ADR-025 - previously a single rte_channel_t; that type is
                                             *   now one link, registered N-per-voter). May still be NULL when
                                             *   rte_appmanager_run() is first called, e.g. if an integrator's
                                             *   own init() is what populates it. May also be set back to NULL
                                             *   at runtime (e.g. from a background reconnect task) to pause
                                             *   checkpointing without that being treated as an error - see
                                             *   rte_appmanager_run()'s own doc. */
    rte_duration_ms_t max_delay_ms;       /**< Forwarded to rte_checkpoint_config_t::max_delay_ms. */
    uint32_t expected_node_count;          /**< Forwarded to rte_checkpoint_config_t::expected_node_count. */
    rte_watchdog_t watchdog;              /**< Optional liveness watchdog kicked on success; may be NULL. */
} rte_appmanager_checkpoint_config_t;

/**
 * @brief Application manager configuration
 */
typedef struct {
    const rte_appmanager_operations_t *ops;  /**< Application operations */
    void *context;                            /**< Application context */
    uint32_t max_iterations;                  /**< Max execute() calls (0 = infinite) */
    uint32_t error_threshold;                 /**< Errors before shutdown (0 = no limit) */
    const rte_appmanager_checkpoint_config_t *checkpoint; /**< Optional built-in cycle checkpoint (ADR-019); NULL = disabled (default). */
} rte_appmanager_config_t;

/**
 * @brief Application manager runtime state
 */
typedef struct {
    rte_app_state_t state;         /**< Current lifecycle state (init/running/etc.). */
    uint32_t iteration_count;       /**< Number of completed execute() cycles. */
    uint32_t error_count;           /**< Number of execute() cycles that returned an error. */
    rte_status_t last_error;       /**< Most recent non-OK status observed, if any. */
} rte_appmanager_state_t;

/* ============================================================================
 * Checkpoint marks (ADR-034)
 * ========================================================================== */

/**
 * @brief Reset value of the per-cycle checkpoint signature before any
 *        marks are folded into it (ADR-034) - see
 *        rte_appmanager_checkpoint_fold_signature()'s own doc. A cycle in
 *        which no RTE_CHECKPOINT_MARK() call is ever made therefore
 *        always computes checkpoint_id as
 *        `rte_appmanager_checkpoint_fold_signature(RTE_APPMANAGER_CHECKPOINT_SIGNATURE_SEED)`
 *        - a fixed, deterministic value, not zero by coincidence.
 */
#define RTE_APPMANAGER_CHECKPOINT_SIGNATURE_SEED ((uint64_t)0xFFFFFFFFFFFFFFFFULL)

/**
 * @brief Pure helper: folds a 64-bit checkpoint signature down to the
 *        uint32_t rte_checkpoint_config_t::checkpoint_id expects (ADR-034).
 *
 * An explicit, checked fold - XOR of the signature's two 32-bit halves
 * (CLAUDE.md: no implicit truncating cast) - not a bare narrowing
 * assignment. Exposed as a pure, stateless function (no reference to any
 * in-progress cycle) so a test, or an integrator's own diagnostic logging,
 * can independently compute the expected checkpoint_id for a known
 * signature value without needing to be inside an active
 * rte_appmanager_run() cycle - e.g.
 * `rte_appmanager_checkpoint_fold_signature(RTE_APPMANAGER_CHECKPOINT_SIGNATURE_SEED)`
 * gives the checkpoint_id for a cycle that made no marks at all.
 *
 * @param signature The 64-bit signature to fold (e.g. the seed, for a
 *                   no-marks cycle).
 * @return The folded 32-bit checkpoint_id.
 */
uint32_t rte_appmanager_checkpoint_fold_signature(uint64_t signature);

/**
 * @brief Folds a hash of this call site into the current cycle's checkpoint
 *        signature (ADR-034).
 *
 * Intended to be called via RTE_CHECKPOINT_MARK() / RTE_CHECKPOINT_MARK_LABEL()
 * below, not directly, so file/line are captured at the real call site. Each
 * call computes a CRC64 hash of either `label` (if non-NULL) or `file:line`,
 * then chains it into the running per-cycle signature:
 * `signature = crc64(encode_le(signature) || encode_le(mark_hash))` - the
 * same explicit-little-endian-byte-pack convention rte_checkpoint.c's own
 * build_arrival_message() already uses, so the fold is well-defined
 * regardless of the two channels' CPU architectures.
 *
 * A no-op (does not touch the running signature) if both `file` and `label`
 * are NULL, and outside a rte_appmanager_run() cycle (before the loop
 * starts, or after it ends) - safe to call unconditionally from shared code
 * paths that might run during init()/shutdown() too.
 *
 * @param file  Source file of the call site (normally __FILE__); ignored
 *              if `label` is non-NULL.
 * @param line  Source line of the call site (normally __LINE__); ignored
 *              if `label` is non-NULL.
 * @param label Optional caller-supplied identity for this mark instead of
 *              file:line (e.g. when several call sites should be treated
 *              as the same logical mark, or a single call site's mark
 *              identity should depend on which branch was taken). May be
 *              NULL to use file:line.
 */
void rte_appmanager_checkpoint_mark(const char *file, int32_t line, const char *label);

/**
 * @def RTE_CHECKPOINT_MARK
 * @brief Records a checkpoint mark for this call site (ADR-034) - see
 *        rte_appmanager_checkpoint_mark()'s own doc.
 */
#define RTE_CHECKPOINT_MARK() rte_appmanager_checkpoint_mark(__FILE__, (int32_t)__LINE__, NULL)

/**
 * @def RTE_CHECKPOINT_MARK_LABEL
 * @brief Records a checkpoint mark identified by a caller-supplied label
 *        instead of this call site's file:line (ADR-034) - see
 *        rte_appmanager_checkpoint_mark()'s own doc.
 */
#define RTE_CHECKPOINT_MARK_LABEL(label_str) rte_appmanager_checkpoint_mark(__FILE__, (int32_t)__LINE__, (label_str))

/* ============================================================================
 * Application Manager API
 * ========================================================================== */

/**
 * @brief Run application with lifecycle management
 *
 * This is the single entry point for all applications. It manages the
 * complete lifecycle:
 *   1. Initialize (init)
 *   2. Per-cycle loop, in order (ADR-034 - checkpoint moved to the end,
 *      see rte_appmanager_checkpoint_config_t's own doc for why):
 *      a. Reset this cycle's checkpoint-mark signature to a fixed seed
 *      b. pre_execute(), if ops->pre_execute is non-NULL
 *      c. execute()
 *      d. post_execute(), if ops->post_execute is non-NULL
 *      e. Checkpoint rendezvous, if config->checkpoint is non-NULL, using
 *         this cycle's own folded mark signature as checkpoint_id
 *      f. ops->on_checkpoint_result(), if non-NULL
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
 * - If the pre_execute(), execute(), post_execute(), or checkpoint stage of
 *   a cycle returns non-OK, that is logged and counted against error_count
 *   exactly the same way regardless of which stage failed; any remaining
 *   stage of that same cycle before the checkpoint stage is skipped, and
 *   the loop continues to the next cycle (unless error_threshold is
 *   reached) - EXCEPT ops->on_checkpoint_result(), which (per ADR-034) is
 *   still called even when the checkpoint stage itself failed, precisely
 *   so a staging application always learns the outcome (committed=false)
 *   and can discard what it staged this cycle; see that field's own doc
 * - If error_threshold is reached, application shuts down
 * - shutdown() is always called, even on error
 *
 * @param config Application manager configuration
 * @return 0 (EXIT_SUCCESS) if application completed normally
 *         1 (EXIT_FAILURE) if initialization failed or errors exceeded threshold
 *
 * Example (single-stage, unchanged from before ADR-019):
 * @code
 * const rte_appmanager_operations_t ops = {
 *     .init = my_app_init,
 *     .execute = my_app_execute,
 *     .shutdown = my_app_shutdown,
 *     .get_name = my_app_get_name,
 *     .get_version = my_app_get_version
 * };
 *
 * rte_appmanager_config_t config = {
 *     .ops = &ops,
 *     .context = &my_app_context,
 *     .max_iterations = 0,      // Run forever
 *     .error_threshold = 10      // Stop after 10 errors
 * };
 *
 * return rte_appmanager_run(&config);
 * @endcode
 *
 * Example (pre/post hooks plus a built-in cross-channel checkpoint,
 * ADR-019/ADR-034 - my_app_prepare_outputs() marks and stages, never sends
 * directly; my_app_commit_outputs() only runs after the checkpoint
 * confirms both channels took the same path this cycle):
 * @code
 * const rte_appmanager_operations_t ops = {
 *     .init = my_app_init,
 *     .pre_execute = my_app_read_inputs,
 *     .execute = my_app_decide,
 *     .post_execute = my_app_prepare_outputs,   // calls RTE_CHECKPOINT_MARK(), stages, does not transmit
 *     .on_checkpoint_result = my_app_commit_outputs, // committed=true -> flush staged output; false -> discard it
 *     .shutdown = my_app_shutdown,
 *     .get_name = my_app_get_name,
 *     .get_version = my_app_get_version
 * };
 *
 * static const rte_appmanager_checkpoint_config_t checkpoint_cfg = {
 *     .voter = &my_voter,   // already has its channels rte_voter_register_channel()-ed
 *     .max_delay_ms = 200,
 *     .expected_node_count = 1,
 *     .watchdog = NULL
 * };
 *
 * rte_appmanager_config_t config = {
 *     .ops = &ops,
 *     .context = &my_app_context,
 *     .max_iterations = 0,
 *     .error_threshold = 10,
 *     .checkpoint = &checkpoint_cfg
 * };
 *
 * return rte_appmanager_run(&config);
 * @endcode
 */
int rte_appmanager_run(const rte_appmanager_config_t *config);

/**
 * @brief Get current application state
 *
 * Returns the current state of a running application manager.
 * Note: This is intended for monitoring, not for control.
 *
 * @return Current application state
 */
rte_app_state_t rte_appmanager_get_state(void);

/**
 * @brief Get application statistics
 *
 * Returns runtime statistics (iterations, errors, etc.).
 *
 * @param state Pointer to rte_appmanager_state_t to populate
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_appmanager_get_stats(rte_appmanager_state_t *state);

/**
 * @brief Request application shutdown
 *
 * Signals the application to begin shutdown. The execute loop will
 * terminate after the current iteration.
 *
 * Note: This function requests shutdown but does not guarantee immediate
 * termination. The shutdown may take one iteration to complete.
 */
void rte_appmanager_request_shutdown(void);

/**
 * @brief POSIX-only convenience: installs SIGINT and SIGTERM handlers
 *        that call rte_appmanager_request_shutdown(), so an operator
 *        (Ctrl+C) or process manager (SIGTERM) can stop a
 *        rte_appmanager_run() loop gracefully - shutdown() still runs,
 *        this is not a hard kill.
 *
 * This is the one function in this module with an OS dependency - see
 * this header's own file-level @note for why that is an intentional,
 * scoped exception rather than a general pattern for this framework.
 * Safe to call unconditionally on any target: on a non-POSIX build this
 * compiles to a no-op that returns RTE_STATUS_NOT_SUPPORTED, so
 * portable integration code does not need its own \#ifdef around the
 * call site.
 *
 * Call this before rte_appmanager_run() (or from init()); calling it
 * more than once re-installs the same handlers (idempotent, matches
 * rte_safestate_register_handler()'s "registering again replaces the
 * previous" convention).
 *
 * @return RTE_STATUS_OK if both handlers were installed;
 *         RTE_STATUS_NOT_SUPPORTED on a non-POSIX build;
 *         RTE_STATUS_INTERNAL_ERROR if the underlying sigaction() call
 *         itself failed (see errno at the call site for detail - not
 *         surfaced here, consistent with this framework not using errno
 *         in its own public API, CLAUDE.md's "no use of errno in
 *         production paths").
 */
rte_status_t rte_appmanager_install_default_signal_handlers(void);

/**
 * @brief Forcibly resets the application manager's own bookkeeping
 *        (lifecycle state, iteration/error counters, shutdown-request
 *        flag, and the ADR-026 setup-phase lock) back to its initial,
 *        pre-run condition.
 *
 * Normal use of rte_appmanager_run() never requires this: every one of
 * its own entry/exit paths already resets this same state on its own
 * (see REQ-APPMANAGER-009). This function exists for the one case that
 * bypasses those paths entirely: an application-level fault handler that
 * itself performs a non-local jump (e.g. `longjmp()`) out of a
 * `RTE_SAFESTATE_LEVEL_SAFE`/`_REBOOT` reaction instead of the
 * framework's own documented never-returns contract
 * (REQ-COMMON-SAFESTATE-002). After such a jump, rte_appmanager_run()
 * never reaches its own SHUTDOWN phase, and this module's internal state
 * would otherwise stay permanently stuck mid-lifecycle - causing every
 * subsequent rte_appmanager_run() call to be rejected by the
 * single-entry-point guard. Call this once, immediately after regaining
 * control via such a non-local jump, before calling
 * rte_appmanager_run() again.
 *
 * @safety Calling this while a rte_appmanager_run() call is genuinely
 *         still executing (not abandoned via a non-local jump)
 *         corrupts that call's own state. This function exists
 *         specifically for the abandoned-via-non-local-jump case, not
 *         general use - MISRA C:2012 Rule 21.4 already prohibits
 *         `<setjmp.h>` in production code, so this situation should
 *         never arise there; it exists because this framework's own
 *         test suite has no other way to exercise a
 *         `RTE_SAFESTATE_LEVEL_SAFE` reaction (which never returns)
 *         without one.
 */
void rte_appmanager_reset_state(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_APPMANAGER_H */

/** @} */ /* APPMANAGER */

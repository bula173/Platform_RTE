/**
 * @def _POSIX_C_SOURCE
 * @brief struct sigaction/sigaction()/sigemptyset() below are POSIX.1-2001,
 *        not base ISO C99 - must be defined before ANY header is included
 *        (same rule/pattern as safeAPIRBC2oo2's channel_ab.c/site.c/
 *        monitor_c.c). Without this, glibc's strict-C99 mode hides these
 *        declarations entirely: this exact omission passed on macOS
 *        (Apple's libc does not gate them behind the same feature-test
 *        macro) but failed Linux CI with "storage size of 'sa' isn't
 *        known" / implicit-declaration errors under -Werror - caught via
 *        a real GitHub Actions failure, not local testing.
 */
#define _POSIX_C_SOURCE 200809L

/**
 * @file sapi_appmanager.c
 * @brief Application Manager implementation
 * @ingroup APPMANAGER
 *
 * Provides lifecycle management for safety-critical applications.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "safeapi/appmanager/sapi_appmanager.h"
#include "safeapi/checkpoint/sapi_checkpoint.h"
#include "safeapi/lifecycle/sapi_lifecycle.h"
#include "safeapi/timer/sapi_timer.h"

/* sapi_appmanager_install_default_signal_handlers()'s POSIX detection -
 * see this file's own implementation below and the function's doc in
 * sapi_appmanager.h for why this is a scoped, documented exception to
 * this module (and this framework)'s usual OS-agnosticism. */
/**
 * @def SAPI_APPMANAGER_HAVE_POSIX_SIGNALS
 * @brief 1 when compiled on a POSIX-ish target (signal.h available) so
 *        sapi_appmanager_install_default_signal_handlers() can install a
 *        real SIGINT/SIGTERM handler; 0 otherwise, in which case that
 *        function returns SAPI_STATUS_NOT_SUPPORTED.
 */
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#define SAPI_APPMANAGER_HAVE_POSIX_SIGNALS 1
#include <signal.h>
#include <string.h>
#else
#define SAPI_APPMANAGER_HAVE_POSIX_SIGNALS 0
#endif

/** @brief Global application state (single instance; no dynamic allocation). */
static sapi_appmanager_state_t g_app_state = {
    .state = SAPI_APP_STATE_UNINITIALIZED,
    .iteration_count = 0,
    .error_count = 0,
    .last_error = SAPI_STATUS_OK
};

/** @brief Set by the installed signal handler (or sapi_appmanager_request_shutdown())
 *         to request that sapi_appmanager_run()'s loop exit cleanly. */
static volatile int g_shutdown_requested = 0;

/**
 * @brief Records the outcome of one per-cycle stage (checkpoint, pre_execute,
 *        execute, or post_execute) against the shared error accounting, and
 *        decides whether the remaining stages of this cycle should run.
 *
 * Centralizes the "log, count against error_count, check error_threshold"
 * behavior that ADR-019's checkpoint/pre_execute/post_execute stages share
 * with execute()'s pre-existing error handling, so all four stages react to
 * a non-OK status identically (REQ-APPMANAGER-007).
 *
 * @param status          Status returned by the stage that just ran.
 * @param stage_name      Short, human-readable stage name for the log line
 *                        on failure (e.g. "checkpoint", "pre_execute").
 *                        Must not be NULL.
 * @param error_threshold Errors before shutdown (0 = no limit), forwarded
 *                        from sapi_appmanager_config_t::error_threshold.
 * @return true if status is SAPI_STATUS_OK (caller should proceed to the
 *         next stage of this cycle); false otherwise (caller should skip
 *         the remaining stages for this cycle - g_app_state.error_count and,
 *         if error_threshold was reached, g_shutdown_requested have already
 *         been updated).
 */
static bool sapi_appmanager_handle_stage_result(sapi_status_t status,
                                                  const char *stage_name,
                                                  uint32_t error_threshold)
{
    bool ok = (status == SAPI_STATUS_OK);

    if (!ok) {
        g_app_state.error_count++;
        g_app_state.last_error = status;

        fprintf(stderr, "[APPMANAGER ERROR] %s failed: %d\n", stage_name, (int)status);

        if (error_threshold > 0 && g_app_state.error_count >= error_threshold) {
            fprintf(stderr,
                    "[APPMANAGER ERROR] Error threshold exceeded (%u/%u), shutting down\n",
                    g_app_state.error_count,
                    error_threshold);
            g_shutdown_requested = 1;
        }
    }

    return ok;
}

/**
 * @brief Floors the retry interval for a failed checkpoint-stage attempt
 *        at max_delay_ms, measured from checkpoint_start_ms.
 *
 * The checkpoint stage runs before pre_execute() every cycle (ADR-019) so
 * a desynced peer is caught before either channel acts on that cycle's
 * data - but that ordering means pre_execute() is never reached on a
 * failed-checkpoint cycle, and pre_execute() is the ONLY place any of
 * this framework's consumers own their cycle pacing (this module
 * deliberately owns no timer of its own - ADR-001 section 4). A
 * checkpoint that fails *validation* (e.g. `voter == NULL` - a normal,
 * documented way to pause checkpointing while its own underlying
 * transport is known down, not a rare condition - see
 * sapi_appmanager_run()'s own doc above) returns immediately, at zero
 * cost, every single call - without this floor, sapi_appmanager_run()'s
 * own loop would retry it as fast as the CPU allows, one failed attempt
 * (and one error log line) at a time, forever.
 *
 * Reuses max_delay_ms (already a caller-configured, meaningful budget)
 * as the floor rather than inventing a new config field: a failed
 * checkpoint now never retries faster than a checkpoint that blocked for
 * its full timeout and then failed would have.
 *
 * REQ-APPMANAGER-008: a failed checkpoint-stage attempt shall not be
 * retried faster than max_delay_ms (when a timer backend is registered).
 *
 * @param checkpoint_start_ms  Timestamp captured immediately before the
 *                             failed sapi_channel_checkpoint() call.
 * @param max_delay_ms         The checkpoint's own configured budget.
 *
 * @safety No-op if no timer backend is registered (checkpoint_start_ms
 *         degrades to 0 - see sapi_timer_now()'s own doc, same convention
 *         used elsewhere in this framework, e.g. sapi_dual_negotiator's
 *         own startup timestamp) or max_delay_ms is 0: neither can be
 *         paced without fabricating a wait time nobody configured - a
 *         known, accepted limitation, not a silent behavior change for
 *         an integrator who never registered a timer backend at all.
 *         Implemented as a bounded sapi_timer_now() poll: no portable
 *         blocking-sleep primitive exists at this layer by design (this
 *         framework never owns an OS-specific blocking call itself);
 *         this matches the same polling pattern this framework's own
 *         consumers already use for their bounded waits.
 */
static void sapi_appmanager_pace_failed_checkpoint(sapi_timestamp_ms_t checkpoint_start_ms,
                                                     sapi_duration_ms_t max_delay_ms)
{
    sapi_timestamp_ms_t now_ms = 0U;

    if ((checkpoint_start_ms == 0U) || (max_delay_ms == 0U)) {
        return;
    }
    (void)sapi_timer_now(&now_ms);
    while ((now_ms >= checkpoint_start_ms) &&
           ((now_ms - checkpoint_start_ms) < (sapi_timestamp_ms_t)max_delay_ms)) {
        (void)sapi_timer_now(&now_ms);
    }
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

int sapi_appmanager_run(const sapi_appmanager_config_t *config)
{
    sapi_status_t status;

    /* Validate configuration */
    if (config == NULL || config->ops == NULL) {
        fprintf(stderr, "ERROR: Invalid sapi_appmanager_config_t\n");
        return EXIT_FAILURE;
    }

    if (config->ops->init == NULL ||
        config->ops->execute == NULL ||
        config->ops->shutdown == NULL ||
        config->ops->get_name == NULL ||
        config->ops->get_version == NULL) {
        fprintf(stderr, "ERROR: Incomplete sapi_appmanager_operations_t\n");
        return EXIT_FAILURE;
    }

    /* pre_execute/post_execute are optional (ADR-019); no NULL check here
     * is an error - a NULL value simply means that stage is skipped below. */

    /* REQ-APPMANAGER-009 (ADR-026): single-entry-point enforcement - this
     * is THE one function that runs an application's lifecycle (see this
     * function's own doc: "the single entry point for all applications"),
     * so a call arriving while a PREVIOUS call is still mid-lifecycle
     * (its own INITIALIZING/RUNNING/SHUTTING_DOWN) is refused outright,
     * without touching any of g_app_state - unlike every other rejection
     * path in this function, this one must NOT reset state out from under
     * whichever call is already using it. Checked before the reset below
     * runs, deliberately: this framework's own test suite (and any
     * integrator) calling sapi_appmanager_run() again only AFTER a
     * previous call has fully returned (state SHUTDOWN/ERROR by then) is
     * unaffected - see sapi_lifecycle.h's own doc on why this remains a
     * single-process-wide-instance model, not multi-instance. */
    if ((g_app_state.state == SAPI_APP_STATE_INITIALIZING) ||
        (g_app_state.state == SAPI_APP_STATE_RUNNING) ||
        (g_app_state.state == SAPI_APP_STATE_SHUTTING_DOWN)) {
        fprintf(stderr, "ERROR: sapi_appmanager_run() called while an application is already "
                         "running (state=%d) - this is the single entry point, it cannot be "
                         "re-entered concurrently\n", (int)g_app_state.state);
        return EXIT_FAILURE;
    }

    /* Deliberately NOT validated here: config->checkpoint->voter being
     * NULL at this point. An integrator may legitimately populate that
     * target inside their own init() (e.g. a checkpoint transport whose
     * channels are only registered as part of application startup, not
     * before sapi_appmanager_run() is even called) - the per-cycle call
     * below already handles a NULL voter safely (sapi_channel_checkpoint()
     * returns SAPI_STATUS_INVALID_PARAM, handled identically to any other
     * failed stage, never a crash), so an integrator can also toggle it
     * to NULL transiently at runtime to pause checkpointing (e.g. while
     * its own underlying transport is known down) without that ever
     * being treated as a startup error. */

    /* Initialize application manager state */
    g_app_state.state = SAPI_APP_STATE_INITIALIZING;
    g_app_state.iteration_count = 0;
    g_app_state.error_count = 0;
    g_app_state.last_error = SAPI_STATUS_OK;
    g_shutdown_requested = 0;
    /* REQ-APPMANAGER-010 (ADR-026): setup is allowed again for this (new) call's own INIT
     * phase - see sapi_lifecycle.h's own doc on why this reset lives here
     * rather than only at the end of the previous call. */
    sapi_lifecycle_unlock();

    /* Print banner */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("%s v%s\n", config->ops->get_name(), config->ops->get_version());
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    /* ========================================================================
     * INITIALIZATION PHASE
     * ======================================================================== */

    status = config->ops->init(config->context);

    if (status != SAPI_STATUS_OK) {
        fprintf(stderr, "[APPMANAGER ERROR] Initialization failed: %d\n", status);
        g_app_state.state = SAPI_APP_STATE_ERROR;
        g_app_state.last_error = status;
        g_app_state.error_count++;

        /* Always attempt shutdown even on init failure */
        config->ops->shutdown(config->context);

        return EXIT_FAILURE;
    }

    g_app_state.state = SAPI_APP_STATE_RUNNING;
    /* REQ-APPMANAGER-010 (ADR-026): setup phase locked - every setup-only constructor
     * (sapi_timer_create(), sapi_channel_init(), sapi_voter_init()/
     * _register_channel(), sapi_cross_comparator_init()/
     * _register_channel(), sapi_watchdog_create()) now rejects with
     * SAPI_STATUS_INVALID_STATE for the rest of this run - see
     * sapi_lifecycle.h's own doc for the deliberately-excluded
     * netlink/dual reconnect exceptions. */
    sapi_lifecycle_lock();

    /* ========================================================================
     * EXECUTION PHASE
     * ======================================================================== */

    while (!g_shutdown_requested &&
           (config->max_iterations == 0 ||
            g_app_state.iteration_count < config->max_iterations)) {

        g_app_state.iteration_count++;

        /* Stage 1 (ADR-019, optional): bounded cross-channel checkpoint
         * rendezvous, run before any of this cycle's own work so a desynced
         * peer is caught before either channel acts on this cycle's data.
         * Reuses iteration_count as the checkpoint_id - see
         * sapi_appmanager_checkpoint_config_t's own doc for why.
         *
         * REQ-APPMANAGER-011: config->checkpoint->voter == NULL is
         * deliberately treated the SAME as config->checkpoint == NULL -
         * skip straight to stage 2 - not as a failed checkpoint attempt.
         * An integrator may legitimately
         * toggle voter to NULL at runtime to pause checkpointing while its
         * own underlying transport is known down (see
         * sapi_appmanager_config_t's own doc); sapi_channel_checkpoint()
         * itself already documents returning SAPI_STATUS_INVALID_PARAM
         * for this case "never a crash" - but until this check existed,
         * that INVALID_PARAM was still fed through
         * sapi_appmanager_handle_stage_result() as a FAILED stage, which
         * paces and continue's, skipping pre_execute()/execute()/
         * post_execute() entirely for as long as voter stayed NULL. Found
         * live: this starves every one of a consumer's own per-cycle
         * safety checks - including sapi_watchdog_timer_tick(), so a
         * watchdog-driven REBOOT-on-link-loss reaction could never fire
         * during exactly the sustained-outage scenario it exists for,
         * because the cyclic executive never reached the code that ticks
         * it. Skipping the stage outright (no sapi_channel_checkpoint()
         * call, no pacing, no continue) keeps every later stage running
         * every cycle regardless of how long checkpoint stays paused -
         * consistent with this framework's single-threaded, no-async-
         * timer design (REQ-APPMANAGER-*): a watchdog's own expiry is
         * still just "compare now against its own saved start timestamp",
         * which now actually gets asked every cycle again, no thread
         * needed on top of the one already driving this loop. */
        if ((config->checkpoint != NULL) && (config->checkpoint->voter != NULL)) {
            sapi_checkpoint_config_t checkpoint_cfg;
            sapi_timestamp_ms_t checkpoint_start_ms = 0U;

            checkpoint_cfg.checkpoint_id = g_app_state.iteration_count;
            checkpoint_cfg.max_delay_ms = config->checkpoint->max_delay_ms;
            checkpoint_cfg.expected_node_count = config->checkpoint->expected_node_count;
            checkpoint_cfg.watchdog = config->checkpoint->watchdog;

            (void)sapi_timer_now(&checkpoint_start_ms);
            status = sapi_channel_checkpoint(config->checkpoint->voter, &checkpoint_cfg);
            if (!sapi_appmanager_handle_stage_result(status, "checkpoint", config->error_threshold)) {
                /* See sapi_appmanager_pace_failed_checkpoint()'s own doc:
                 * without this, a checkpoint that fails validation
                 * immediately spins this loop as fast as the CPU allows -
                 * pre_execute(), the only place any consumer's own cycle
                 * pacing lives, is never reached on this path. This is
                 * now only reached for a GENUINE checkpoint failure
                 * (voter non-NULL, rendezvous itself failed/timed out) -
                 * the voter == NULL "paused" case is handled above and
                 * never reaches this branch at all. */
                sapi_appmanager_pace_failed_checkpoint(checkpoint_start_ms, checkpoint_cfg.max_delay_ms);
                continue;
            }
        }

        /* Stage 2 (ADR-019, optional): per-cycle input/prepare stage. */
        if (config->ops->pre_execute != NULL) {
            status = config->ops->pre_execute(config->context);
            if (!sapi_appmanager_handle_stage_result(status, "pre_execute", config->error_threshold)) {
                continue;
            }
        }

        /* Stage 3 (mandatory): main application logic, unchanged. */
        status = config->ops->execute(config->context);
        if (!sapi_appmanager_handle_stage_result(status, "execute", config->error_threshold)) {
            continue;
        }

        /* Stage 4 (ADR-019, optional): per-cycle output/cleanup stage. Only
         * reached once execute() itself succeeded. */
        if (config->ops->post_execute != NULL) {
            status = config->ops->post_execute(config->context);
            (void)sapi_appmanager_handle_stage_result(status, "post_execute", config->error_threshold);
        }
    }

    /* ========================================================================
     * SHUTDOWN PHASE
     * ======================================================================== */

    g_app_state.state = SAPI_APP_STATE_SHUTTING_DOWN;
    /* REQ-APPMANAGER-010 (ADR-026): setup is allowed again from here on - not just at the next
     * call's own top-of-run reset above. Without this, setup code that
     * legitimately runs BETWEEN two sapi_appmanager_run() calls (e.g. a
     * test fixture - or integrator code - that builds the NEXT run's own
     * voter/channels/timers before calling sapi_appmanager_run() again)
     * would be incorrectly rejected: it runs after this run's own
     * sapi_lifecycle_lock() but before the next run's own unlock. */
    sapi_lifecycle_unlock();

    status = config->ops->shutdown(config->context);

    if (status != SAPI_STATUS_OK) {
        fprintf(stderr, "[APPMANAGER WARN] Shutdown returned non-OK status: %d\n", status);
        /* Continue anyway; shutdown must complete */
    }

    g_app_state.state = SAPI_APP_STATE_SHUTDOWN;

    /* Print final summary */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("%s Summary\n", config->ops->get_name());
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Iterations executed: %u\n", g_app_state.iteration_count);
    printf("Errors encountered:  %u\n", g_app_state.error_count);
    printf("Final state:         %s\n",
           (g_app_state.error_count > 0 ? "ERROR" : "OK"));
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    /* Determine exit code */
    if (g_app_state.error_count > 0 && config->error_threshold > 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

sapi_app_state_t sapi_appmanager_get_state(void)
{
    return g_app_state.state;
}

sapi_status_t sapi_appmanager_get_stats(sapi_appmanager_state_t *state)
{
    if (state == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    *state = g_app_state;
    return SAPI_STATUS_OK;
}

void sapi_appmanager_request_shutdown(void)
{
    g_shutdown_requested = 1;
}

void sapi_appmanager_reset_state(void)
{
    g_app_state.state = SAPI_APP_STATE_UNINITIALIZED;
    g_app_state.iteration_count = 0;
    g_app_state.error_count = 0;
    g_app_state.last_error = SAPI_STATUS_OK;
    g_shutdown_requested = 0;
    sapi_lifecycle_unlock();
}

#if SAPI_APPMANAGER_HAVE_POSIX_SIGNALS

/* Async-signal-safe: writes one volatile int, nothing else (no I/O, no
 * allocation) - see sapi_appmanager_request_shutdown()'s own definition
 * above. */
static void sapi_appmanager_signal_handler(int signum)
{
    (void)signum;
    sapi_appmanager_request_shutdown();
}

sapi_status_t sapi_appmanager_install_default_signal_handlers(void)
{
    struct sigaction sa;

    /* sigemptyset()/sigaction() failure paths below (each GCOVR_EXCL_LINE)
     * are checked defensively per this project's error-handling
     * convention, but neither call has a documented failure mode for the
     * fixed, always-valid arguments used here (a stack-local sigset_t;
     * SIGINT/SIGTERM, both always-valid, unblockable-by-definition signal
     * numbers) - forcing a real failure would need OS-level fault
     * injection (e.g. LD_PRELOAD interposition), not something a portable
     * unit test can do. */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sapi_appmanager_signal_handler;
    if (sigemptyset(&sa.sa_mask) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR; /* GCOVR_EXCL_LINE */
    }
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR; /* GCOVR_EXCL_LINE */
    }
    if (sigaction(SIGTERM, &sa, NULL) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR; /* GCOVR_EXCL_LINE */
    }
    return SAPI_STATUS_OK;
}

#else

sapi_status_t sapi_appmanager_install_default_signal_handlers(void)
{
    return SAPI_STATUS_NOT_SUPPORTED;
}

#endif /* SAPI_APPMANAGER_HAVE_POSIX_SIGNALS */

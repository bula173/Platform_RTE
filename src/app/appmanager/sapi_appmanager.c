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
#include <string.h>
#include "safeapi/app/appmanager/sapi_appmanager.h"
#include "safeapi/redundancy/checksum/sapi_checksum.h"
#include "safeapi/redundancy/checkpoint/sapi_checkpoint.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi/oal/log/sapi_log.h"
#include "safeapi/oal/timer/sapi_timer.h"

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
        char msg[96];

        g_app_state.error_count++;
        g_app_state.last_error = status;

        (void)snprintf(msg, sizeof(msg), "%s failed: %d", stage_name, (int)status);
        sapi_log_write(SAPI_LOG_LEVEL_ERROR, "APPMANAGER", msg);

        if (error_threshold > 0 && g_app_state.error_count >= error_threshold) {
            (void)snprintf(msg, sizeof(msg), "Error threshold exceeded (%u/%u), shutting down",
                            g_app_state.error_count, error_threshold);
            sapi_log_write(SAPI_LOG_LEVEL_ERROR, "APPMANAGER", msg);
            g_shutdown_requested = 1;
        }
    }

    return ok;
}

/** @brief Running per-cycle checkpoint-mark signature (ADR-034) - see
 *         sapi_appmanager_checkpoint_mark()'s own doc in the header. Reset
 *         once per cycle by sapi_appmanager_checkpoint_signature_reset();
 *         folded by every sapi_appmanager_checkpoint_mark() call in
 *         between. Deliberately NOT part of the public sapi_appmanager_state_t
 *         (no consumer needs to read it directly - only the folded
 *         checkpoint_id, computed on demand by
 *         sapi_appmanager_checkpoint_fold_signature(), leaves this file). */
static uint64_t g_checkpoint_signature;

/** @brief True once g_checkpoint_signature has been reset for the CURRENT
 *         sapi_appmanager_run() cycle - see sapi_appmanager_checkpoint_mark()'s
 *         doc: a mark call outside an active cycle (before the loop starts,
 *         or after it ends) is a documented no-op rather than silently
 *         folding into whatever a NEXT, unrelated run's first cycle
 *         computes. */
static bool g_checkpoint_signature_active;

/**
 * @brief Resets g_checkpoint_signature to a fixed seed and marks it active
 *        for the current cycle - called once per sapi_appmanager_run()
 *        loop iteration, before pre_execute() (ADR-034).
 */
static void sapi_appmanager_checkpoint_signature_reset(void)
{
    g_checkpoint_signature = SAPI_APPMANAGER_CHECKPOINT_SIGNATURE_SEED;
    g_checkpoint_signature_active = true;
}

/**
 * @brief Packs a uint64_t into an 8-byte buffer, explicit little-endian -
 *        same convention sapi_checkpoint.c's own build_arrival_message()
 *        already uses, so a fold computed on one CPU architecture and
 *        compared against a peer's own fold on a different architecture
 *        still agrees (no raw struct/memcpy of a multi-byte integer
 *        across a network).
 */
static void sapi_appmanager_encode_u64_le(uint8_t out[8], uint64_t value)
{
    uint32_t i;

    for (i = 0U; i < 8U; i++) {
        out[i] = (uint8_t)((value >> (8U * i)) & 0xFFU);
    }
}

uint32_t sapi_appmanager_checkpoint_fold_signature(uint64_t signature)
{
    uint32_t low = (uint32_t)(signature & 0xFFFFFFFFULL);
    uint32_t high = (uint32_t)((signature >> 32) & 0xFFFFFFFFULL);

    return low ^ high;
}

/* ============================================================================
 * Checkpoint marks (ADR-034)
 * ========================================================================== */

void sapi_appmanager_checkpoint_mark(const char *file, int32_t line, const char *label)
{
    char text[128];
    uint8_t fold_buf[16];
    sapi_crc64_t mark_hash;

    if (!g_checkpoint_signature_active) {
        /* Documented no-op outside an active sapi_appmanager_run() cycle -
         * see this function's own header doc. */
        return;
    }

    if (label != NULL) {
        (void)snprintf(text, sizeof(text), "%s", label);
    } else if (file != NULL) {
        (void)snprintf(text, sizeof(text), "%s:%d", file, (int)line);
    } else {
        return;
    }

    mark_hash = sapi_checksum_crc64((const uint8_t *)text, strnlen(text, sizeof(text)));

    sapi_appmanager_encode_u64_le(&fold_buf[0], g_checkpoint_signature);
    sapi_appmanager_encode_u64_le(&fold_buf[8], (uint64_t)mark_hash);

    g_checkpoint_signature = (uint64_t)sapi_checksum_crc64(fold_buf, sizeof(fold_buf));
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

int sapi_appmanager_run(const sapi_appmanager_config_t *config)
{
    sapi_status_t status;

    /* Validate configuration */
    if (config == NULL || config->ops == NULL) {
        sapi_log_write(SAPI_LOG_LEVEL_ERROR, "APPMANAGER", "Invalid sapi_appmanager_config_t");
        return EXIT_FAILURE;
    }

    if (config->ops->init == NULL ||
        config->ops->execute == NULL ||
        config->ops->shutdown == NULL ||
        config->ops->get_name == NULL ||
        config->ops->get_version == NULL) {
        sapi_log_write(SAPI_LOG_LEVEL_ERROR, "APPMANAGER", "Incomplete sapi_appmanager_operations_t");
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
        char msg[128];

        (void)snprintf(msg, sizeof(msg),
                        "sapi_appmanager_run() called while an application is already running "
                        "(state=%d) - this is the single entry point, it cannot be re-entered concurrently",
                        (int)g_app_state.state);
        sapi_log_write(SAPI_LOG_LEVEL_ERROR, "APPMANAGER", msg);
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
    /* ADR-034: no cycle is active yet - any SAPI_CHECKPOINT_MARK() called
     * from init() is a documented no-op (see sapi_appmanager_checkpoint_mark()'s
     * own doc), not a fold into whatever this run's first real cycle
     * computes. */
    g_checkpoint_signature_active = false;
    /* REQ-APPMANAGER-010 (ADR-026): setup is allowed again for this (new) call's own INIT
     * phase - see sapi_lifecycle.h's own doc on why this reset lives here
     * rather than only at the end of the previous call. */
    sapi_lifecycle_unlock();

    /* Startup banner */
    {
        char msg[96];

        (void)snprintf(msg, sizeof(msg), "%s v%s starting", config->ops->get_name(), config->ops->get_version());
        sapi_log_write(SAPI_LOG_LEVEL_INFO, "APPMANAGER", msg);
    }

    /* ========================================================================
     * INITIALIZATION PHASE
     * ======================================================================== */

    status = config->ops->init(config->context);

    if (status != SAPI_STATUS_OK) {
        char msg[64];

        (void)snprintf(msg, sizeof(msg), "Initialization failed: %d", (int)status);
        sapi_log_write(SAPI_LOG_LEVEL_ERROR, "APPMANAGER", msg);
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

        /* ADR-034: reset this cycle's checkpoint-mark signature before any
         * of this cycle's own work runs, so SAPI_CHECKPOINT_MARK() calls
         * made during pre_execute()/execute()/post_execute() below fold
         * into a signature that represents ONLY this cycle's own path -
         * see sapi_appmanager_checkpoint_mark()'s own doc. */
        sapi_appmanager_checkpoint_signature_reset();

        /* Stage 1 (ADR-019, optional): per-cycle input/prepare stage. */
        if (config->ops->pre_execute != NULL) {
            status = config->ops->pre_execute(config->context);
            if (!sapi_appmanager_handle_stage_result(status, "pre_execute", config->error_threshold)) {
                continue;
            }
        }

        /* Stage 2 (mandatory): main application logic, unchanged. */
        status = config->ops->execute(config->context);
        if (!sapi_appmanager_handle_stage_result(status, "execute", config->error_threshold)) {
            continue;
        }

        /* Stage 3 (ADR-019, optional): per-cycle output/cleanup stage. Only
         * reached once execute() itself succeeded. */
        if (config->ops->post_execute != NULL) {
            status = config->ops->post_execute(config->context);
            (void)sapi_appmanager_handle_stage_result(status, "post_execute", config->error_threshold);
        }

        /* Stage 4 (ADR-019/ADR-034, optional): bounded cross-channel
         * checkpoint rendezvous, run LAST - after pre_execute()/execute()/
         * post_execute() have all had a chance to fold their own marks
         * into this cycle's signature (SAPI_CHECKPOINT_MARK()) - using
         * that folded signature as the checkpoint_id. See
         * sapi_appmanager_checkpoint_config_t's own doc for why this
         * replaced the old iteration_count-based scheme and the old
         * before-everything stage position.
         *
         * REQ-APPMANAGER-011: config->checkpoint->voter == NULL is
         * deliberately treated the SAME as config->checkpoint == NULL -
         * skip the rendezvous itself - not as a failed checkpoint attempt.
         * An integrator may legitimately toggle voter to NULL at runtime
         * to pause checkpointing while its own underlying transport is
         * known down (see sapi_appmanager_config_t's own doc).
         *
         * committed starts true: with no checkpoint configured (or paused),
         * ops->on_checkpoint_result() - if registered - is still called,
         * with committed=true (see that field's own doc: an application
         * using stage-then-commit should not have to special-case whether
         * checkpointing happened to be active this cycle). */
        {
            bool committed = true;

            if ((config->checkpoint != NULL) && (config->checkpoint->voter != NULL)) {
                sapi_checkpoint_config_t checkpoint_cfg;

                checkpoint_cfg.checkpoint_id = sapi_appmanager_checkpoint_fold_signature(g_checkpoint_signature);
                checkpoint_cfg.max_delay_ms = config->checkpoint->max_delay_ms;
                checkpoint_cfg.expected_node_count = config->checkpoint->expected_node_count;
                checkpoint_cfg.watchdog = config->checkpoint->watchdog;

                status = sapi_channel_checkpoint(config->checkpoint->voter, &checkpoint_cfg);
                committed = sapi_appmanager_handle_stage_result(status, "checkpoint", config->error_threshold);
                /* Deliberately no pacing/continue on failure here (unlike
                 * every other stage above): pre_execute() already ran this
                 * cycle - now that checkpoint runs LAST, a failed
                 * checkpoint no longer starves it, so the pacing workaround
                 * this stage used to need (see git history) is gone. Falls
                 * through to on_checkpoint_result below regardless, then
                 * the loop naturally continues to its own top. */
            }

            if (config->ops->on_checkpoint_result != NULL) {
                status = config->ops->on_checkpoint_result(config->context, committed);
                (void)sapi_appmanager_handle_stage_result(status, "on_checkpoint_result", config->error_threshold);
            }
        }
    }

    /* ========================================================================
     * SHUTDOWN PHASE
     * ======================================================================== */

    g_app_state.state = SAPI_APP_STATE_SHUTTING_DOWN;
    /* ADR-034: the cycle loop has exited - any SAPI_CHECKPOINT_MARK() called
     * from shutdown() is a documented no-op (see
     * sapi_appmanager_checkpoint_mark()'s own doc). */
    g_checkpoint_signature_active = false;
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
        char msg[64];

        (void)snprintf(msg, sizeof(msg), "Shutdown returned non-OK status: %d", (int)status);
        sapi_log_write(SAPI_LOG_LEVEL_WARNING, "APPMANAGER", msg);
        /* Continue anyway; shutdown must complete */
    }

    g_app_state.state = SAPI_APP_STATE_SHUTDOWN;

    /* Final summary - the iteration count is this event's natural Cycle
     * field, so this uses sapi_log_write_event() (structured), not
     * sapi_log_write() like this function's other, cycle-less messages
     * above. */
    {
        char extra[64];

        (void)snprintf(extra, sizeof(extra), "Errors=%u FinalState=%s", g_app_state.error_count,
                        (g_app_state.error_count > 0 ? "ERROR" : "OK"));
        sapi_log_write_event(SAPI_LOG_LEVEL_INFO, "", g_app_state.iteration_count, "APPMANAGER", "-", "SUMMARY",
                              config->ops->get_name(), extra);
    }

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
    g_checkpoint_signature_active = false;
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

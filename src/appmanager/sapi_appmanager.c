/**
 * @def _POSIX_C_SOURCE
 * @brief struct sigaction/sigaction()/sigemptyset() below are POSIX.1-2001,
 *        not base ISO C99 - must be defined before ANY header is included
 *        (same rule/pattern as safeAPIExample's channel_ab.c/site.c/
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

    /* Deliberately NOT validated here: config->checkpoint->vital_channel
     * being NULL at this point. An integrator may legitimately populate
     * that target inside their own init() (e.g. a checkpoint transport
     * that is only opened/vital_channel_init()-ed as part of application
     * startup, not before sapi_appmanager_run() is even called) - the
     * per-cycle call below already handles a NULL vital_channel safely
     * (sapi_channel_checkpoint() returns SAPI_STATUS_INVALID_PARAM,
     * handled identically to any other failed stage, never a crash), so
     * an integrator can also toggle it to NULL transiently at runtime to
     * pause checkpointing (e.g. while its own underlying transport is
     * known down) without that ever being treated as a startup error. */

    /* Initialize application manager state */
    g_app_state.state = SAPI_APP_STATE_INITIALIZING;
    g_app_state.iteration_count = 0;
    g_app_state.error_count = 0;
    g_app_state.last_error = SAPI_STATUS_OK;
    g_shutdown_requested = 0;

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
         * sapi_appmanager_checkpoint_config_t's own doc for why. */
        if (config->checkpoint != NULL) {
            sapi_checkpoint_config_t checkpoint_cfg;

            checkpoint_cfg.checkpoint_id = g_app_state.iteration_count;
            checkpoint_cfg.max_delay_ms = config->checkpoint->max_delay_ms;
            checkpoint_cfg.expected_node_count = config->checkpoint->expected_node_count;
            checkpoint_cfg.watchdog = config->checkpoint->watchdog;

            status = sapi_channel_checkpoint(config->checkpoint->vital_channel, &checkpoint_cfg);
            if (!sapi_appmanager_handle_stage_result(status, "checkpoint", config->error_threshold)) {
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

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sapi_appmanager_signal_handler;
    if (sigemptyset(&sa.sa_mask) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    if (sigaction(SIGTERM, &sa, NULL) != 0)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    return SAPI_STATUS_OK;
}

#else

sapi_status_t sapi_appmanager_install_default_signal_handlers(void)
{
    return SAPI_STATUS_NOT_SUPPORTED;
}

#endif /* SAPI_APPMANAGER_HAVE_POSIX_SIGNALS */

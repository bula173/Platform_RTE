/**
 * @file sapi_appmanager.c
 * @brief Application Manager implementation
 *
 * Provides lifecycle management for safety-critical applications.
 */

#include <stdio.h>
#include <stdlib.h>
#include "safeapi/appmanager/sapi_appmanager.h"

/* Global application state (static) */
static sapi_appmanager_state_t g_app_state = {
    .state = SAPI_APP_STATE_UNINITIALIZED,
    .iteration_count = 0,
    .error_count = 0,
    .last_error = SAPI_STATUS_OK
};

static volatile int g_shutdown_requested = 0;

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

        /* Execute one iteration of application work */
        status = config->ops->execute(config->context);

        /* Handle errors */
        if (status != SAPI_STATUS_OK) {
            g_app_state.error_count++;
            g_app_state.last_error = status;

            /* Check error threshold */
            if (config->error_threshold > 0 &&
                g_app_state.error_count >= config->error_threshold) {
                fprintf(stderr,
                        "[APPMANAGER ERROR] Error threshold exceeded (%u/%u), shutting down\n",
                        g_app_state.error_count,
                        config->error_threshold);
                g_shutdown_requested = 1;
            }
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

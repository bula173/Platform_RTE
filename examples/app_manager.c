/**
 * @file app_manager.c
 * @brief Application Manager implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include "safeapi/log.h"
#include "app_manager.h"

/* Global application state (static) */
static app_manager_state_t g_app_state = {
    .state = APP_STATE_UNINITIALIZED,
    .iteration_count = 0,
    .error_count = 0,
    .last_error = SAPI_STATUS_OK
};

static volatile int g_shutdown_requested = 0;

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

int app_manager_run(const app_manager_config_t *config)
{
    sapi_status_t status;

    /* Validate configuration */
    if (config == NULL || config->ops == NULL) {
        fprintf(stderr, "ERROR: Invalid app_manager_config_t\n");
        return EXIT_FAILURE;
    }

    if (config->ops->init == NULL ||
        config->ops->execute == NULL ||
        config->ops->shutdown == NULL ||
        config->ops->get_name == NULL ||
        config->ops->get_version == NULL) {
        fprintf(stderr, "ERROR: Incomplete app_manager_operations_t\n");
        return EXIT_FAILURE;
    }

    /* Initialize application manager state */
    g_app_state.state = APP_STATE_INITIALIZING;
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

    SAPI_LOG_INFO("=== %s v%s starting ===",
                  config->ops->get_name(),
                  config->ops->get_version());

    /* ========================================================================
     * INITIALIZATION PHASE
     * ======================================================================== */

    SAPI_LOG_INFO("Initialization phase");
    status = config->ops->init(config->context);

    if (status != SAPI_STATUS_OK) {
        SAPI_LOG_ERROR("Initialization failed: %d", status);
        g_app_state.state = APP_STATE_ERROR;
        g_app_state.last_error = status;
        g_app_state.error_count++;

        /* Always attempt shutdown even on init failure */
        SAPI_LOG_INFO("Shutdown phase (after init failure)");
        config->ops->shutdown(config->context);

        return EXIT_FAILURE;
    }

    g_app_state.state = APP_STATE_RUNNING;
    SAPI_LOG_INFO("Initialization complete");

    /* ========================================================================
     * EXECUTION PHASE
     * ======================================================================== */

    SAPI_LOG_INFO("Execution phase starting (max_iterations=%u, error_threshold=%u)",
                  config->max_iterations,
                  config->error_threshold);

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

            SAPI_LOG_ERROR("Execution error at iteration %u: %d",
                          g_app_state.iteration_count,
                          status);

            /* Check error threshold */
            if (config->error_threshold > 0 &&
                g_app_state.error_count >= config->error_threshold) {
                SAPI_LOG_ERROR("Error threshold exceeded (%u/%u), shutting down",
                              g_app_state.error_count,
                              config->error_threshold);
                g_shutdown_requested = 1;
            }
        }
    }

    SAPI_LOG_INFO("Execution phase complete (iterations=%u, errors=%u)",
                  g_app_state.iteration_count,
                  g_app_state.error_count);

    /* ========================================================================
     * SHUTDOWN PHASE
     * ======================================================================== */

    g_app_state.state = APP_STATE_SHUTTING_DOWN;
    SAPI_LOG_INFO("Shutdown phase");

    status = config->ops->shutdown(config->context);

    if (status != SAPI_STATUS_OK) {
        SAPI_LOG_WARN("Shutdown returned non-OK status: %d", status);
        /* Continue anyway; shutdown must complete */
    }

    g_app_state.state = APP_STATE_SHUTDOWN;

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

    SAPI_LOG_INFO("=== %s shutdown complete ===\n",
                  config->ops->get_name());

    /* Determine exit code */
    if (g_app_state.error_count > 0 && config->error_threshold > 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

app_state_t app_manager_get_state(void)
{
    return g_app_state.state;
}

sapi_status_t app_manager_get_stats(app_manager_state_t *state)
{
    if (state == NULL) {
        return SAPI_STATUS_ERROR;
    }

    *state = g_app_state;
    return SAPI_STATUS_OK;
}

void app_manager_request_shutdown(void)
{
    SAPI_LOG_INFO("Shutdown requested via app_manager_request_shutdown()");
    g_shutdown_requested = 1;
}

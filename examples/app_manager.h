/**
 * @file app_manager.h
 * @brief Application Manager abstraction for safeAPIFramework applications
 *
 * Provides a single entry point pattern for safety-critical applications with
 * well-defined lifecycle: initialize → execute → shutdown.
 *
 * Each application implements the app_manager_operations_t interface:
 *   - init() — One-time initialization
 *   - execute() — Main application loop
 *   - shutdown() — Graceful cleanup
 *
 * The app_manager_run() function manages lifecycle and error handling.
 */

#ifndef EXAMPLES_APP_MANAGER_H
#define EXAMPLES_APP_MANAGER_H

#include <stdint.h>
#include "safeapi/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Application Manager Interface
 * ========================================================================== */

/**
 * @brief Application state enumeration
 */
typedef enum {
    APP_STATE_UNINITIALIZED = 0,
    APP_STATE_INITIALIZING = 1,
    APP_STATE_RUNNING = 2,
    APP_STATE_SHUTTING_DOWN = 3,
    APP_STATE_SHUTDOWN = 4,
    APP_STATE_ERROR = 5
} app_state_t;

/**
 * @brief Application lifecycle operations
 *
 * Each application must implement these operations to be managed by the
 * application manager.
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
     * @brief Execute main application logic
     *
     * Called in a loop after initialization. The execute function should
     * process one iteration of work and return. Returning non-OK status
     * may trigger shutdown depending on error severity.
     *
     * @param context Application-specific context pointer
     * @return SAPI_STATUS_OK on normal execution
     *         SAPI_STATUS_ERROR to trigger shutdown
     *         Other codes for specific error conditions
     */
    sapi_status_t (*execute)(void *context);

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
} app_manager_operations_t;

/**
 * @brief Application manager configuration
 */
typedef struct {
    const app_manager_operations_t *ops;  /**< Application operations */
    void *context;                        /**< Application context */
    uint32_t max_iterations;              /**< Max execute() calls (0 = infinite) */
    uint32_t error_threshold;             /**< Errors before shutdown (0 = no limit) */
} app_manager_config_t;

/**
 * @brief Application manager runtime state
 */
typedef struct {
    app_state_t state;
    uint32_t iteration_count;
    uint32_t error_count;
    sapi_status_t last_error;
} app_manager_state_t;

/* ============================================================================
 * Application Manager API
 * ========================================================================== */

/**
 * @brief Run application with lifecycle management
 *
 * This is the single entry point for all applications. It manages the
 * complete lifecycle:
 *   1. Initialize (init)
 *   2. Execute loop (execute)
 *   3. Shutdown (shutdown)
 *
 * Error handling:
 * - If init() fails, shutdown() is still called and EXIT_FAILURE is returned
 * - If execute() fails, error is logged and loop continues (unless threshold reached)
 * - If error_threshold is reached, application shuts down
 * - shutdown() is always called, even on error
 *
 * @param config Application manager configuration
 * @return 0 (EXIT_SUCCESS) if application completed normally
 *         1 (EXIT_FAILURE) if initialization failed or errors exceeded threshold
 *
 * Example:
 * @code
 * const app_manager_operations_t ops = {
 *     .init = my_app_init,
 *     .execute = my_app_execute,
 *     .shutdown = my_app_shutdown,
 *     .get_name = my_app_get_name,
 *     .get_version = my_app_get_version
 * };
 *
 * app_manager_config_t config = {
 *     .ops = &ops,
 *     .context = &my_app_context,
 *     .max_iterations = 0,      // Run forever
 *     .error_threshold = 10      // Stop after 10 errors
 * };
 *
 * return app_manager_run(&config);
 * @endcode
 */
int app_manager_run(const app_manager_config_t *config);

/**
 * @brief Get current application state
 *
 * Returns the current state of a running application manager.
 * Note: This is intended for monitoring, not for control.
 *
 * @return Current application state
 */
app_state_t app_manager_get_state(void);

/**
 * @brief Get application statistics
 *
 * Returns runtime statistics (iterations, errors, etc.).
 *
 * @param state Pointer to app_manager_state_t to populate
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t app_manager_get_stats(app_manager_state_t *state);

/**
 * @brief Request application shutdown
 *
 * Signals the application to begin shutdown. The execute loop will
 * terminate after the current iteration.
 *
 * Note: This function requests shutdown but does not guarantee immediate
 * termination. The shutdown may take one iteration to complete.
 */
void app_manager_request_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* EXAMPLES_APP_MANAGER_H */

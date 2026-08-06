/**
 * @page status_user_guide Status Module - User Guide
 *
 * @section status_user_guide_overview What is the Status Module?
 *
 * The Status module provides a unified error/status code system for the entire
 * framework. Every operation returns a `sapi_status_t` code indicating success
 * or failure reason.
 *
 * **Key idea:** All functions return the same status codes, making error handling
 * consistent across the entire framework.
 *
 * @section status_user_guide_quick_start Quick Start
 *
 * ### 1. Check Operation Success
 *
 * ```c
 * #include "safeapi/status/sapi_status.h"
 *
 * sapi_status_t rc = some_operation();
 * if (rc != SAPI_STATUS_OK) {
 *     printf("Operation failed: %s\n", sapi_status_to_string(rc));
 *     return rc;
 * }
 * ```
 *
 * ### 2. Common Status Codes
 *
 * ```c
 * SAPI_STATUS_OK                  // Operation succeeded
 * SAPI_STATUS_INVALID_PARAM       // Bad parameter
 * SAPI_STATUS_TIMEOUT             // Operation timed out
 * SAPI_STATUS_HARDWARE_FAULT      // Hardware error
 * SAPI_STATUS_RESOURCE_EXHAUSTED  // Out of memory/capacity
 * SAPI_STATUS_NOT_INITIALIZED     // Module not initialized
 * SAPI_STATUS_NOT_SUPPORTED       // Operation not supported
 * ```
 *
 * ### 3. Convert to String for Logging
 *
 * ```c
 * sapi_status_t rc = some_operation();
 * log_error("Operation failed: %s (code=%d)",
 *           sapi_status_to_string(rc), rc);
 * ```
 *
 * @section status_user_guide_error_handling Error Handling Patterns
 *
 * ### Pattern 1: Early Return
 *
 * ```c
 * sapi_status_t initialize_system(void) {
 *     sapi_status_t rc;
 *
 *     rc = init_module_a();
 *     if (rc != SAPI_STATUS_OK) return rc;  // Exit early on failure
 *
 *     rc = init_module_b();
 *     if (rc != SAPI_STATUS_OK) return rc;  // Exit early on failure
 *
 *     rc = init_module_c();
 *     if (rc != SAPI_STATUS_OK) return rc;  // Exit early on failure
 *
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * ### Pattern 2: Log and Continue
 *
 * ```c
 * void process_data(void) {
 *     // Non-critical operation - log but continue
 *     sapi_status_t rc = non_critical_operation();
 *     if (rc != SAPI_STATUS_OK) {
 *         log_warning("Non-critical operation failed: %s",
 *                    sapi_status_to_string(rc));
 *         // Continue despite failure
 *     }
 * }
 * ```
 *
 * ### Pattern 3: Trigger Safe-State on Critical Error
 *
 * ```c
 * void vital_operation(void) {
 *     sapi_status_t rc = critical_operation();
 *     if (rc != SAPI_STATUS_OK) {
 *         // Safety-critical failure - enter safe-state
 *         log_error("Critical failure: %s", sapi_status_to_string(rc));
 *         SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_SAFE,
 *                       SAPI_SAFESTATE_REASON_UNSPECIFIED);
 *     }
 * }
 * ```
 *
 * @section status_user_guide_status_codes Complete Status Code Reference
 *
 * | Code | Meaning | Action |
 * |------|---------|--------|
 * | `SAPI_STATUS_OK` | Success | Continue |
 * | `SAPI_STATUS_INVALID_PARAM` | Bad input | Fix parameters, retry or fail |
 * | `SAPI_STATUS_TIMEOUT` | Operation timed out | Retry or fail |
 * | `SAPI_STATUS_HARDWARE_FAULT` | Hardware error | Log, possibly trigger safe-state |
 * | `SAPI_STATUS_RESOURCE_EXHAUSTED` | No capacity | Free resources, retry or fail |
 * | `SAPI_STATUS_NOT_INITIALIZED` | Module not ready | Call init first |
 * | `SAPI_STATUS_NOT_SUPPORTED` | Unavailable feature | Use alternative or fail |
 *
 * @section status_user_guide_examples Practical Examples
 *
 * ### Example 1: Buffer Operation
 *
 * ```c
 * uint8_t buffer[256];
 * size_t written = 0;
 *
 * sapi_status_t rc = sapi_buffer_write(&buffer_handle, data, size, &written);
 * switch (rc) {
 * case SAPI_STATUS_OK:
 *     printf("Wrote %zu bytes\n", written);
 *     break;
 * case SAPI_STATUS_RESOURCE_EXHAUSTED:
 *     printf("Buffer is full\n");
 *     break;
 * case SAPI_STATUS_INVALID_PARAM:
 *     printf("Invalid parameter\n");
 *     break;
 * default:
 *     printf("Unknown error: %s\n", sapi_status_to_string(rc));
 * }
 * ```
 *
 * ### Example 2: Timer Operation
 *
 * ```c
 * sapi_timer_t timer;
 *
 * sapi_status_t rc = sapi_timer_create(&timer, "heartbeat", 1000);
 * if (rc != SAPI_STATUS_OK) {
 *     log_error("Failed to create timer: %s", sapi_status_to_string(rc));
 *     return rc;
 * }
 *
 * rc = sapi_timer_start(&timer);
 * if (rc != SAPI_STATUS_OK) {
 *     log_error("Failed to start timer: %s", sapi_status_to_string(rc));
 *     sapi_timer_destroy(&timer);
 *     return rc;
 * }
 * ```
 *
 * @section status_user_guide_mapping Status Codes to Safety Actions
 *
 * **For Safety-Critical Code:**
 *
 * ```c
 * // Vital operations MUST succeed or trigger safe-state
 * sapi_status_t rc = vital_operation();
 * if (rc != SAPI_STATUS_OK) {
 *     // ANY failure in vital code is a safety event
 *     trigger_safe_state(REASON_VITAL_FAILURE);
 *     return rc;
 * }
 * ```
 *
 * **For Non-Vital Code:**
 *
 * ```c
 * // Non-vital operations can fail gracefully
 * sapi_status_t rc = diagnostics_operation();
 * if (rc != SAPI_STATUS_OK) {
 *     // Log but don't trigger safe-state
 *     log_debug("Diagnostics failed: %s", sapi_status_to_string(rc));
 *     // Continue normal operation
 * }
 * ```
 *
 * @section status_user_guide_guidelines Best Practices
 *
 * 1. **Always check return codes** - Don't ignore status codes
 *
 *    ```c
 *    // ❌ BAD: Ignoring return code
 *    some_operation();
 *
 *    // ✓ GOOD: Checking return code
 *    sapi_status_t rc = some_operation();
 *    if (rc != SAPI_STATUS_OK) { /* handle error */ }
 *    ```
 *
 * 2. **Use early returns** - Exit on first error
 *
 *    ```c
 *    // ✓ GOOD: Early exit
 *    sapi_status_t rc = operation_a();
 *    if (rc != SAPI_STATUS_OK) return rc;
 *
 *    rc = operation_b();
 *    if (rc != SAPI_STATUS_OK) return rc;
 *    ```
 *
 * 3. **Log meaningful messages** - Include the status code
 *
 *    ```c
 *    log_error("Channel read failed: %s (code=%d)",
 *              sapi_status_to_string(rc), rc);
 *    ```
 *
 * 4. **Distinguish critical from non-critical**
 *
 *    ```c
 *    // Critical: trigger safe-state on ANY failure
 *    if (vital_rc != SAPI_STATUS_OK) trigger_safe_state();
 *
 *    // Non-critical: log and continue
 *    if (diag_rc != SAPI_STATUS_OK) log_debug("...");
 *    ```
 *
 * @section status_user_guide_see_also See Also
 *
 * - @ref status_architecture for internal design
 * - @ref safestate_user_guide for safe-state triggered on critical failures
 * - @ref log_user_guide for logging status codes
 *
 */

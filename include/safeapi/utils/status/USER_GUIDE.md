/**
 * @page status_user_guide Status Module - User Guide
 *
 * @section status_user_guide_overview What is the Status Module?
 *
 * The Status module provides a unified error/status code system for the entire
 * framework. Every operation returns a `rte_status_t` code indicating success
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
 * #include "safeapi/status/rte_status.h"
 *
 * rte_status_t rc = some_operation();
 * if (rc != RTE_STATUS_OK) {
 *     printf("Operation failed: %s\n", rte_status_to_string(rc));
 *     return rc;
 * }
 * ```
 *
 * ### 2. Common Status Codes
 *
 * ```c
 * RTE_STATUS_OK                  // Operation succeeded
 * RTE_STATUS_INVALID_PARAM       // Bad parameter
 * RTE_STATUS_TIMEOUT             // Operation timed out
 * RTE_STATUS_HARDWARE_FAULT      // Hardware error
 * RTE_STATUS_RESOURCE_EXHAUSTED  // Out of memory/capacity
 * RTE_STATUS_NOT_INITIALIZED     // Module not initialized
 * RTE_STATUS_NOT_SUPPORTED       // Operation not supported
 * ```
 *
 * ### 3. Convert to String for Logging
 *
 * ```c
 * rte_status_t rc = some_operation();
 * log_error("Operation failed: %s (code=%d)",
 *           rte_status_to_string(rc), rc);
 * ```
 *
 * @section status_user_guide_error_handling Error Handling Patterns
 *
 * ### Pattern 1: Early Return
 *
 * ```c
 * rte_status_t initialize_system(void) {
 *     rte_status_t rc;
 *
 *     rc = init_module_a();
 *     if (rc != RTE_STATUS_OK) return rc;  // Exit early on failure
 *
 *     rc = init_module_b();
 *     if (rc != RTE_STATUS_OK) return rc;  // Exit early on failure
 *
 *     rc = init_module_c();
 *     if (rc != RTE_STATUS_OK) return rc;  // Exit early on failure
 *
 *     return RTE_STATUS_OK;
 * }
 * ```
 *
 * ### Pattern 2: Log and Continue
 *
 * ```c
 * void process_data(void) {
 *     // Non-critical operation - log but continue
 *     rte_status_t rc = non_critical_operation();
 *     if (rc != RTE_STATUS_OK) {
 *         log_warning("Non-critical operation failed: %s",
 *                    rte_status_to_string(rc));
 *         // Continue despite failure
 *     }
 * }
 * ```
 *
 * ### Pattern 3: Trigger Safe-State on Critical Error
 *
 * ```c
 * void vital_operation(void) {
 *     rte_status_t rc = critical_operation();
 *     if (rc != RTE_STATUS_OK) {
 *         // Safety-critical failure - enter safe-state
 *         log_error("Critical failure: %s", rte_status_to_string(rc));
 *         RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE,
 *                       RTE_SAFESTATE_REASON_UNSPECIFIED);
 *     }
 * }
 * ```
 *
 * @section status_user_guide_status_codes Complete Status Code Reference
 *
 * | Code | Meaning | Action |
 * |------|---------|--------|
 * | `RTE_STATUS_OK` | Success | Continue |
 * | `RTE_STATUS_INVALID_PARAM` | Bad input | Fix parameters, retry or fail |
 * | `RTE_STATUS_TIMEOUT` | Operation timed out | Retry or fail |
 * | `RTE_STATUS_HARDWARE_FAULT` | Hardware error | Log, possibly trigger safe-state |
 * | `RTE_STATUS_RESOURCE_EXHAUSTED` | No capacity | Free resources, retry or fail |
 * | `RTE_STATUS_NOT_INITIALIZED` | Module not ready | Call init first |
 * | `RTE_STATUS_NOT_SUPPORTED` | Unavailable feature | Use alternative or fail |
 *
 * @section status_user_guide_examples Practical Examples
 *
 * ### Example 1: Buffer Operation
 *
 * ```c
 * uint8_t buffer[256];
 * size_t written = 0;
 *
 * rte_status_t rc = rte_buffer_write(&buffer_handle, data, size, &written);
 * switch (rc) {
 * case RTE_STATUS_OK:
 *     printf("Wrote %zu bytes\n", written);
 *     break;
 * case RTE_STATUS_RESOURCE_EXHAUSTED:
 *     printf("Buffer is full\n");
 *     break;
 * case RTE_STATUS_INVALID_PARAM:
 *     printf("Invalid parameter\n");
 *     break;
 * default:
 *     printf("Unknown error: %s\n", rte_status_to_string(rc));
 * }
 * ```
 *
 * ### Example 2: Timer Operation
 *
 * ```c
 * rte_timer_t timer;
 *
 * rte_status_t rc = rte_timer_create(&timer, "heartbeat", 1000);
 * if (rc != RTE_STATUS_OK) {
 *     log_error("Failed to create timer: %s", rte_status_to_string(rc));
 *     return rc;
 * }
 *
 * rc = rte_timer_start(&timer);
 * if (rc != RTE_STATUS_OK) {
 *     log_error("Failed to start timer: %s", rte_status_to_string(rc));
 *     rte_timer_destroy(&timer);
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
 * rte_status_t rc = vital_operation();
 * if (rc != RTE_STATUS_OK) {
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
 * rte_status_t rc = diagnostics_operation();
 * if (rc != RTE_STATUS_OK) {
 *     // Log but don't trigger safe-state
 *     log_debug("Diagnostics failed: %s", rte_status_to_string(rc));
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
 *    rte_status_t rc = some_operation();
 *    if (rc != RTE_STATUS_OK) { /* handle error */ }
 *    ```
 *
 * 2. **Use early returns** - Exit on first error
 *
 *    ```c
 *    // ✓ GOOD: Early exit
 *    rte_status_t rc = operation_a();
 *    if (rc != RTE_STATUS_OK) return rc;
 *
 *    rc = operation_b();
 *    if (rc != RTE_STATUS_OK) return rc;
 *    ```
 *
 * 3. **Log meaningful messages** - Include the status code
 *
 *    ```c
 *    log_error("Channel read failed: %s (code=%d)",
 *              rte_status_to_string(rc), rc);
 *    ```
 *
 * 4. **Distinguish critical from non-critical**
 *
 *    ```c
 *    // Critical: trigger safe-state on ANY failure
 *    if (vital_rc != RTE_STATUS_OK) trigger_safe_state();
 *
 *    // Non-critical: log and continue
 *    if (diag_rc != RTE_STATUS_OK) log_debug("...");
 *    ```
 *
 * @section status_user_guide_see_also See Also
 *
 * - @ref status_architecture for internal design
 * - @ref safestate_user_guide for safe-state triggered on critical failures
 * - @ref log_user_guide for logging status codes
 *
 */

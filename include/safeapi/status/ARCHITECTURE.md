/**
 * @page status_architecture Status Module - Architecture
 *
 * @section overview Design Overview
 *
 * The Status module defines a unified error handling system across the entire
 * framework. Every operation returns a `sapi_status_t` code that indicates
 * success or the specific failure reason.
 *
 * **Design principle:** Consistency. All modules use the same status codes,
 * making error handling predictable and consistent.
 *
 * @section status_codes Status Code Definitions
 *
 * ```c
 * typedef enum {
 *     SAPI_STATUS_OK = 0,                 // Success
 *     SAPI_STATUS_INVALID_PARAM = 1,      // Invalid parameter
 *     SAPI_STATUS_TIMEOUT = 2,            // Operation timed out
 *     SAPI_STATUS_HARDWARE_FAULT = 3,     // Hardware error
 *     SAPI_STATUS_RESOURCE_EXHAUSTED = 4, // Out of capacity
 *     SAPI_STATUS_NOT_INITIALIZED = 5,    // Not initialized
 *     SAPI_STATUS_NOT_SUPPORTED = 6,      // Not supported
 * } sapi_status_t;
 * ```
 *
 * @section code_allocation Code Allocation Strategy
 *
 * ```
 * 0: SAPI_STATUS_OK (success - no error)
 * 1-6: Framework core status codes (parameter, timeout, hardware, etc.)
 * 7-999: Reserved for framework expansion
 * 1000+: Application-specific error codes (user can extend)
 * ```
 *
 * @section rationale Design Rationale
 *
 * ### Why Unified Status Codes?
 *
 * **Without framework status codes:**
 * ```c
 * int timer_result = sapi_timer_create(...);  // What does -1 mean?
 * bool buffer_result = sapi_buffer_write(...); // What does false mean?
 * errno = ...; // Different modules use different mechanisms
 * ```
 * Problem: Inconsistent error handling across modules.
 *
 * **With framework status codes:**
 * ```c
 * sapi_status_t timer_rc = sapi_timer_create(...);
 * sapi_status_t buffer_rc = sapi_buffer_write(...);
 * // Both return the SAME code type
 * // Same error handling everywhere
 * ```
 *
 * ### Why Not Use errno?
 *
 * - errno is thread-unsafe (global state)
 * - errno is platform-specific (POSIX assumption)
 * - errno values vary by system
 * - MISRA-compliant systems avoid global state
 *
 * @section extension Application-Specific Codes
 *
 * Applications can define their own status codes:
 *
 * ```c
 * // In application header
 * typedef enum {
 *     SAPI_STATUS_OK = 0,
 *     // ... framework codes ...
 *     SAPI_STATUS_NOT_SUPPORTED = 6,
 *
 *     // Application-specific (starting from 1000)
 *     APP_STATUS_TRAIN_SPEED_LIMIT_EXCEEDED = 1000,
 *     APP_STATUS_TRACK_INTEGRITY_CHECK_FAILED = 1001,
 *     APP_STATUS_SIGNAL_ASPECT_UNKNOWN = 1002,
 * } app_status_t;
 * ```
 *
 * @section conversion String Conversion
 *
 * The module provides `sapi_status_to_string()` for diagnostics:
 *
 * ```c
 * sapi_status_t rc = some_operation();
 * if (rc != SAPI_STATUS_OK) {
 *     printf("Error: %s\n", sapi_status_to_string(rc));
 *     // Output: "Error: SAPI_STATUS_TIMEOUT"
 * }
 * ```
 *
 * **Implementation:**
 * ```c
 * const char *sapi_status_to_string(sapi_status_t status) {
 *     switch (status) {
 *     case SAPI_STATUS_OK:
 *         return "SAPI_STATUS_OK";
 *     case SAPI_STATUS_INVALID_PARAM:
 *         return "SAPI_STATUS_INVALID_PARAM";
 *     case SAPI_STATUS_TIMEOUT:
 *         return "SAPI_STATUS_TIMEOUT";
 *     // ... etc ...
 *     default:
 *         return "SAPI_STATUS_UNKNOWN";
 *     }
 * }
 * ```
 *
 * @section usage_patterns Usage Patterns in Framework
 *
 * ### Pattern 1: Initialization Check
 *
 * Every module follows this pattern:
 * ```c
 * sapi_status_t sapi_module_init(config_t *config) {
 *     // Validate input
 *     if (config == NULL) {
 *         return SAPI_STATUS_INVALID_PARAM;
 *     }
 *
 *     // Check if already initialized
 *     if (is_initialized) {
 *         return SAPI_STATUS_NOT_INITIALIZED;
 *     }
 *
 *     // Perform initialization
 *     // ...
 *
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * ### Pattern 2: Operation with Timeout
 *
 * Operations that can timeout:
 * ```c
 * sapi_status_t sapi_channel_recv(handle, buffer, size, timeout) {
 *     // Try to receive data within timeout
 *     // ...
 *     if (no_data_within_timeout) {
 *         return SAPI_STATUS_TIMEOUT;  // Caller knows why
 *     }
 *     // ...
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * ### Pattern 3: Resource Exhaustion
 *
 * Operations that can fail due to capacity:
 * ```c
 * sapi_status_t sapi_buffer_write(handle, data, size, written) {
 *     // Check buffer capacity
 *     if (buffer_remaining < size) {
 *         return SAPI_STATUS_RESOURCE_EXHAUSTED;
 *     }
 *     // ...
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * @section safestate Integration with Safe-State
 *
 * Status codes inform safe-state decisions:
 *
 * **Critical failure:**
 * ```c
 * sapi_status_t rc = vital_operation();
 * if (rc != SAPI_STATUS_OK) {
 *     // ANY failure in vital code triggers safe-state
 *     SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_SAFE,
 *                   SAPI_SAFESTATE_REASON_COMMUNICATION_FAILURE);
 * }
 * ```
 *
 * **Non-critical operation:**
 * ```c
 * sapi_status_t rc = diagnostics_operation();
 * if (rc != SAPI_STATUS_OK) {
 *     // Log but DON'T trigger safe-state
 *     log_debug("Diagnostics error: %s", sapi_status_to_string(rc));
 * }
 * ```
 *
 * @section misra MISRA Compliance
 *
 * The status module supports MISRA C:2012:
 *
 * - ✓ Fixed return type (sapi_status_t enum)
 * - ✓ No implicit conversions (explicit enum values)
 * - ✓ No global state (errno-free)
 * - ✓ Deterministic (same code always means same error)
 * - ✓ Thread-safe (no shared state)
 *
 * @section implementation Implementation Details
 *
 * ### Files
 *
 * - `include/safeapi/status/sapi_status.h` — Public API
 * - `src/status/sapi_status.c` — String conversion table
 *
 * ### String Conversion Table
 *
 * To keep binaries small, string conversion uses a lookup table:
 *
 * ```c
 * static const char *status_strings[] = {
 *     [SAPI_STATUS_OK] = "SAPI_STATUS_OK",
 *     [SAPI_STATUS_INVALID_PARAM] = "SAPI_STATUS_INVALID_PARAM",
 *     // ...
 * };
 * ```
 *
 * Pros: O(1) lookup, small code size
 * Cons: Must maintain both enum and table (kept in sync via static checks)
 *
 * @section testing Testing Strategy
 *
 * ### Unit Tests
 *
 * ```c
 * void test_status_to_string(void) {
 *     // Test all known codes
 *     assert(strcmp(sapi_status_to_string(SAPI_STATUS_OK), "SAPI_STATUS_OK") == 0);
 *     assert(strcmp(sapi_status_to_string(SAPI_STATUS_TIMEOUT), "SAPI_STATUS_TIMEOUT") == 0);
 *     // ...
 *
 *     // Test unknown code
 *     assert(strcmp(sapi_status_to_string(9999), "SAPI_STATUS_UNKNOWN") == 0);
 * }
 * ```
 *
 * @section future Future Extensions
 *
 * Possible extensions (reserved code space):
 * - Additional timeout types (READ_TIMEOUT vs WRITE_TIMEOUT)
 * - Buffer-specific codes (BUFFER_EMPTY, BUFFER_FULL)
 * - Network codes (CONNECTION_REFUSED, CONNECTION_RESET)
 * - All would fit within 1000+ application-specific range
 *
 */

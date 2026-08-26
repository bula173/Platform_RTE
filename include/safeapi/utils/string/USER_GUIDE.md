/**
 * @page string_user_guide String Module - User Guide
 *
 * @section string_user_guide_overview What is the String Module?
 *
 * The String module provides **bounded string operations** to prevent buffer overflows.
 * All operations specify maximum lengths and return status codes instead of silently
 * truncating or crashing.
 *
 * **Key idea:** Strings with known boundaries. No overflows, no truncation surprises.
 *
 * @section string_user_guide_quick_start Quick Start
 *
 * ### 1. Include Header
 *
 * ```c
 * #include "safeapi/string/sapi_string.h"
 * ```
 *
 * ### 2. Safe Copy
 *
 * ```c
 * // ✗ BAD: Unbounded
 * strcpy(dest, src);  // What if src is longer than dest?
 *
 * // ✓ GOOD: Bounded with status code
 * sapi_status_t rc = sapi_strncpy(dest, src, 32);
 * if (rc != SAPI_STATUS_OK) {
 *     log_error("String copy failed: %s", sapi_status_to_string(rc));
 * }
 * ```
 *
 * ### 3. Safe Concatenation
 *
 * ```c
 * char path[256];
 * strcpy(path, "/home/");
 *
 * sapi_status_t rc = sapi_strncat(path, username, 256);
 * if (rc == SAPI_STATUS_RESOURCE_EXHAUSTED) {
 *     log_error("Path too long");
 *     return rc;
 * }
 * ```
 *
 * ### 4. Safe String Length
 *
 * ```c
 * size_t len = sapi_strnlen(str, 256);
 * if (len == 256) {
 *     log_error("String not null-terminated within 256 bytes");
 * }
 * ```
 *
 * @section string_user_guide_operations Operations
 *
 * ```c
 * // Copy: Bounded strcpy
 * sapi_status_t sapi_strncpy(char *dest, const char *src, size_t max_len);
 *
 * // Concatenate: Bounded strcat
 * sapi_status_t sapi_strncat(char *dest, const char *src, size_t max_len);
 *
 * // Length: With maximum limit
 * size_t sapi_strnlen(const char *str, size_t max_len);
 *
 * // Comparison: Bounded strcmp
 * int sapi_strncmp(const char *a, const char *b, size_t max_len);
 *
 * // Format: Bounded sprintf
 * sapi_status_t sapi_snprintf(char *buf, size_t max_len, const char *fmt, ...);
 * ```
 *
 * @section string_user_guide_examples Practical Examples
 *
 * ### Example 1: Command Parsing
 *
 * ```c
 * void parse_command(const char *input) {
 *     char cmd[32];
 *     char arg[64];
 *
 *     // Parse safely
 *     if (sscanf(input, "%31s %63s", cmd, arg) != 2) {
 *         log_error("Invalid command format");
 *         return;
 *     }
 *
 *     // Process command
 *     if (sapi_strncmp(cmd, "status", 32) == 0) {
 *         handle_status(arg);
 *     } else if (sapi_strncmp(cmd, "reset", 32) == 0) {
 *         handle_reset(arg);
 *     }
 * }
 * ```
 *
 * ### Example 2: Log Message Formatting
 *
 * ```c
 * void log_event(const char *event, int code, const char *detail) {
 *     char buffer[256];
 *     sapi_status_t rc = sapi_snprintf(buffer, sizeof(buffer),
 *         "[%u] EVENT: %s (code=%d) - %s",
 *         get_timestamp(), event, code, detail);
 *
 *     if (rc != SAPI_STATUS_OK) {
 *         // Message too long, truncated or failed
 *         log_error("Log message formatting failed");
 *         return;
 *     }
 *
 *     write_to_log(buffer);
 * }
 * ```
 *
 * @section string_user_guide_guidelines Best Practices
 *
 * 1. **Always Specify Maximum Length**
 *    - No open-ended operations
 *    - Always know buffer size
 *    - Use sizeof(buffer) when possible
 *
 * 2. **Check Return Codes**
 *    - SAPI_STATUS_OK: Success, string complete
 *    - SAPI_STATUS_RESOURCE_EXHAUSTED: String truncated/overflow
 *    - SAPI_STATUS_INVALID_PARAM: Bad input
 *
 * 3. **Use Fixed-Size Buffers**
 *    - Never dynamically sized strings
 *    - Allocate statically
 *    - Know max length at compile-time
 *
 * 4. **Prefer snprintf Over sprintf**
 *    - snprintf has length limit
 *    - sprintf doesn't (buffer overflow risk)
 *    - Always use sapi_snprintf()
 *
 * @section string_user_guide_see_also See Also
 *
 * - @ref string_architecture for internal design
 * - @ref buffer_user_guide for buffer operations
 * - @ref log_user_guide for logging
 *
 */

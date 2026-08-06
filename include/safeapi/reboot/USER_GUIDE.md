/**
 * @page reboot_user_guide Reboot Module - User Guide
 *
 * @section reboot_user_guide_overview What is Reboot?
 *
 * The Reboot module requests a controlled system restart. It's the mechanism
 * you call when unrecoverable errors require a fresh start. The integrator
 * provides a backend that actually performs the reset (watchdog, CPU reset,
 * supervisory processor command, etc.).
 *
 * Key idea: When recovery is impossible, restart cleanly rather than hang.
 *
 * @section reboot_user_guide_quick_start Quick Start
 *
 * @subsection reboot_user_guide_qs_backend 1. Register Backend at Startup
 *
 * Integrator supplies platform-specific reboot implementation:
 *
 * @code
 * sapi_status_t platform_reboot(uint16_t reason_code) {
 *     // Backend-specific: trigger CPU reset, watchdog, etc.
 *     // This function must NOT return on success
 *
 *     // Example: use hardware watchdog
 *     watchdog_enable_without_pet(1ms);  // Timeout in 1ms
 *     while (1) {}  // Wait for watchdog to reset
 *
 *     return SAPI_STATUS_NOT_SUPPORTED;  // Only if watchdog unavailable
 * }
 *
 * sapi_reboot_backend_t backend = {
 *     .request = platform_reboot
 * };
 *
 * sapi_reboot_register_backend(&backend);
 * @endcode
 *
 * @subsection reboot_user_guide_qs_request 2. Request Reboot on Critical Error
 *
 * @code
 * if (unrecoverable_error_detected) {
 *     sapi_status_t rc = sapi_reboot_request(APP_REASON_CRITICAL_ERROR);
 *     // This function typically does not return (CPU resets)
 *     // If it does return, error code indicates why reboot failed
 *     if (rc != SAPI_STATUS_OK) {
 *         log_critical("Reboot failed: %s", sapi_status_to_string(rc));
 *     }
 * }
 * @endcode
 *
 * @section reboot_user_guide_integration Integration with Safe-State
 *
 * Typically called from SAPI_REBOOT macro or safe-state handler:
 *
 * @code
 * void my_safestate_handler(sapi_safestate_level_t level, ...) {
 *     if (level == SAPI_SAFESTATE_LEVEL_REBOOT) {
 *         log_critical("Initiating controlled restart");
 *         flush_logs();
 *         close_resources();
 *
 *         // Request reboot - does not return
 *         sapi_reboot_request(APP_REASON_SAFESTATE_REBOOT);
 *
 *         // If execution reaches here, reboot failed
 *         // Enter infinite loop as fallback
 *         while (1) {}
 *     }
 * }
 * @endcode
 *
 * @section reboot_user_guide_reason_codes Reason Codes
 *
 * Pass reason code (16-bit) to identify why reboot was requested:
 *
 * @code
 * #define APP_REASON_CRITICAL_ERROR 1
 * #define APP_REASON_WATCHDOG_TIMEOUT 2
 * #define APP_REASON_MEMORY_CORRUPTION 3
 * #define APP_REASON_UNRECOVERABLE_FAULT 4
 * #define APP_REASON_FIRMWARE_UPDATE 5
 * @endcode
 *
 * Backend can persist reason code to NVM for post-reboot diagnostics.
 *
 * @section reboot_user_guide_examples Practical Examples
 *
 * @subsection reboot_user_guide_example_watchdog Example 1: Watchdog-Triggered Reboot
 *
 * @code
 * void watchdog_handler(void *context) {
 *     app_t *app = (app_t *)context;
 *     log_critical("Watchdog timeout - system hung");
 *     log_critical("  Cycle count: %u", app->cycle_count);
 *     log_critical("  Initiating controlled reboot");
 *
 *     sapi_reboot_request(APP_REASON_WATCHDOG_TIMEOUT);
 *     // Does not return (CPU resets)
 * }
 * @endcode
 *
 * @subsection reboot_user_guide_example_memory Example 2: Memory Corruption Detection
 *
 * @code
 * void check_memory_integrity(void) {
 *     uint32_t checksum = compute_memory_checksum(&heap_start, heap_size);
 *     if (checksum != expected_checksum) {
 *         log_critical("Memory corruption detected!");
 *         log_critical("  Computed: 0x%x", checksum);
 *         log_critical("  Expected: 0x%x", expected_checksum);
 *         log_critical("  Rebooting...");
 *
 *         sapi_reboot_request(APP_REASON_MEMORY_CORRUPTION);
 *         // Does not return
 *     }
 * }
 * @endcode
 *
 * @subsection reboot_user_guide_example_fallback Example 3: Reboot with Fallback
 *
 * @code
 * void request_reboot_with_timeout(uint32_t timeout_ms) {
 *     sapi_watchdog_t fallback_wd = setup_timeout_watchdog(timeout_ms);
 *     sapi_watchdog_start(fallback_wd);
 *
 *     sapi_status_t rc = sapi_reboot_request(APP_REASON_SHUTDOWN);
 *
 *     // If reboot failed, watchdog will force reset
 *     if (rc != SAPI_STATUS_OK) {
 *         log_error("Reboot request failed: %s", sapi_status_to_string(rc));
 *         // Don't kick watchdog - let it timeout and force reset
 *         while (1) {}
 *     }
 * }
 * @endcode
 *
 * @section reboot_user_guide_guidelines Best Practices
 *
 * 1. Register backend at startup
 *    - Before any code can request reboot
 *    - Platform-specific implementation
 *    - Must handle lack of hardware support gracefully
 *
 * 2. Use specific reason codes
 *    - Not just 0
 *    - Aids post-reboot diagnostics
 *    - Backend can log/persist for analysis
 *
 * 3. Prepare for non-return
 *    - sapi_reboot_request() typically doesn't return
 *    - Don't rely on execution continuing
 *    - Any final cleanup should happen before calling
 *
 * 4. Have fallback mechanism
 *    - Use watchdog timeout as fallback
 *    - Ensures reset happens even if backend fails
 *    - Better than hanging indefinitely
 *
 * 5. Only on unrecoverable errors
 *    - Not for transient failures
 *    - Not for user-initiated shutdown (use graceful exit)
 *    - Only when system state is unsafe to continue
 *
 * @section reboot_user_guide_see_also See Also
 *
 * - @ref reboot_architecture for internal design
 * - @ref safestate_user_guide for SAPI_REBOOT macro
 * - @ref watchdog_user_guide for watchdog-triggered reset
 *
 */

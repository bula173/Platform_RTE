/**
 * @page log_user_guide Logging Module - User Guide
 *
 * @section log_user_guide_overview What is Logging?
 *
 * Logging provides bounded ring-buffer logging for diagnostics. All entries
 * go to fixed buffer; oldest entries overwritten when full.
 *
 * @section log_user_guide_levels Log Levels
 *
 * DEBUG - Development diagnostics (disabled in production)
 * INFO - Informational events
 * WARNING - Potential issues, continuing
 * ERROR - Failures, recovery attempted
 * CRITICAL - Imminent shutdown
 *
 * @section log_user_guide_quick_start Quick Start
 *
 * @code
 * sapi_log_debug("value=%d", x);
 * sapi_log_info("System initialized");
 * sapi_log_warning("Low battery");
 * sapi_log_error("Read failed: %s", reason);
 * sapi_log_critical("Entering safe-state");
 * @endcode
 *
 * @section log_user_guide_guidelines Best Practices
 *
 * 1. Use appropriate level
 * 2. Include context (what, why, state)
 * 3. Never log secrets (passwords, keys)
 * 4. Flush before shutdown/reboot
 *
 * @section log_user_guide_see_also See Also
 *
 * - @ref log_architecture for design
 *
 */


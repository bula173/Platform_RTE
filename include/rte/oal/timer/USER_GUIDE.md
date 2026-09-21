/**
 * @page timer_user_guide Timer Module - User Guide
 *
 * @section timer_user_guide_overview What is a Timer?
 *
 * Timer provides one-shot or periodic timeouts with millisecond resolution.
 * You create a timer with callback, start it, and the framework invokes your
 * callback when the deadline expires. Caller provides storage; no allocation.
 *
 * @section timer_user_guide_quick_start Quick Start
 *
 * @subsection timer_user_guide_qs_create 1. Create Timer
 *
 * @code
 * rte_timer_storage_t storage;  // Caller-owned storage
 *
 * rte_timer_config_t config = {
 *     .mode = RTE_TIMER_MODE_PERIODIC,
 *     .period_ms = 100,              // Fire every 100ms
 *     .callback = my_timer_callback,
 *     .user_ctx = my_context
 * };
 *
 * rte_timer_handle_t timer;
 * rte_timer_create(&storage, &config, &timer);
 * @endcode
 *
 * @subsection timer_user_guide_qs_start 2. Start Timer
 *
 * @code
 * rte_timer_start(timer);
 * // Timer now running, will fire every 100ms
 * @endcode
 *
 * @subsection timer_user_guide_qs_callback 3. Implement Callback
 *
 * @code
 * void my_timer_callback(rte_timer_handle_t handle, void *user_ctx) {
 *     app_t *app = (app_t *)user_ctx;
 *     app->timer_fired_count++;
 *     // Do work - keep it short, runs in callback context
 * }
 * @endcode
 *
 * @section timer_user_guide_modes Timer Modes
 *
 * @verbatim
 * ONE_SHOT - Fire once after delay, then stop
 * PERIODIC - Fire repeatedly at interval
 * @endverbatim
 *
 * @section timer_user_guide_examples Practical Examples
 *
 * @subsection timer_user_guide_example_periodic Example 1: Periodic Heartbeat
 *
 * @code
 * void heartbeat_callback(rte_timer_handle_t handle, void *ctx) {
 *     app_t *app = (app_t *)ctx;
 *     send_heartbeat_message();
 *     app->heartbeat_count++;
 * }
 *
 * void app_init(app_t *app) {
 *     rte_timer_config_t config = {
 *         .mode = RTE_TIMER_MODE_PERIODIC,
 *         .period_ms = 1000,          // Every second
 *         .callback = heartbeat_callback,
 *         .user_ctx = app
 *     };
 *     rte_timer_create(&app->heartbeat_storage, &config, &app->heartbeat_timer);
 *     rte_timer_start(app->heartbeat_timer);
 * }
 * @endcode
 *
 * @subsection timer_user_guide_example_oneshot Example 2: One-Shot Timeout
 *
 * @code
 * void timeout_callback(rte_timer_handle_t handle, void *ctx) {
 *     log_warning("Operation timeout!");
 *     // Timer fired once and stopped
 * }
 *
 * void operation_with_timeout(void) {
 *     rte_timer_config_t config = {
 *         .mode = RTE_TIMER_MODE_ONE_SHOT,
 *         .period_ms = 5000,          // 5 second timeout
 *         .callback = timeout_callback,
 *         .user_ctx = NULL
 *     };
 *
 *     rte_timer_create(&timeout_storage, &config, &timer);
 *     rte_timer_start(timer);
 *
 *     // Do operation...
 *     if (operation_completed) {
 *         rte_timer_stop(timer);  // Cancel timeout
 *     }
 *     // If operation takes >5s, timeout fires
 * }
 * @endcode
 *
 * @section timer_user_guide_operations Timer Operations
 *
 * @code
 * rte_timer_create(storage, config, out_handle)  // Create
 * rte_timer_start(handle)                         // Start countdown
 * rte_timer_stop(handle)                          // Stop (idempotent)
 * rte_timer_destroy(handle)                       // Release resources
 * rte_timer_now_ms(out_now_ms)                    // Get current time
 * @endcode
 *
 * @section timer_user_guide_guidelines Best Practices
 *
 * 1. Create at startup, before use
 *    - Call rte_timer_create() once
 *    - Can start/stop as needed
 *    - Destroy at shutdown
 *
 * 2. Keep callbacks short
 *    - Runs in callback context (may be ISR)
 *    - Minimize time in callback
 *    - Do real work in main loop
 *
 * 3. Use appropriate period for mode
 *    - PERIODIC: main loop cycle time (10-100ms typical)
 *    - ONE_SHOT: operation deadline
 *
 * 4. Stop before destroying
 *    - rte_timer_stop() then rte_timer_destroy()
 *    - Ensure timer not firing during cleanup
 *
 * @section timer_user_guide_see_also See Also
 *
 * - @ref timer_architecture for internal design
 * - @ref watchdog_user_guide for watchdog timing
 *
 */


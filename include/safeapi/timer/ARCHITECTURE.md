/**
 * @page timer_architecture Timer Module - Architecture
 *
 * @section timer_architecture_overview Design Overview
 *
 * Timer provides one-shot or periodic callbacks with millisecond resolution.
 * Caller provides storage; framework manages state. Callbacks fired by system
 * tick (typically 1ms from monotonic clock).
 *
 * @section timer_architecture_api Core API
 *
 * @code
 * sapi_timer_create(storage, config, out_handle)   // Create timer
 * sapi_timer_start(handle)                          // Start countdown
 * sapi_timer_stop(handle)                           // Stop countdown
 * sapi_timer_destroy(handle)                        // Release
 * sapi_timer_now_ms(out_now_ms)                     // Get current time
 * @endcode
 *
 * @section timer_architecture_config Configuration
 *
 * @code
 * typedef struct {
 *     sapi_timer_mode_t mode;        // ONE_SHOT or PERIODIC
 *     sapi_duration_ms_t period_ms;  // Period/delay in ms
 *     sapi_timer_callback_t callback;// Called on expiry
 *     void *user_ctx;                // Passed to callback
 * } sapi_timer_config_t;
 * @endcode
 *
 * @section timer_architecture_storage Storage Model
 *
 * Caller provides 64-byte storage via sapi_timer_storage_t.
 * Framework uses for internal state. Caller keeps valid for timer lifetime.
 *
 * @section timer_architecture_callback Callback Signature
 *
 * @code
 * typedef void (*sapi_timer_callback_t)(
 *     sapi_timer_handle_t handle,
 *     void *user_ctx
 * );
 * @endcode
 *
 * Called when timer expires. Runs in callback context (may be ISR on some backends).
 * Keep execution time minimal.
 *
 * @section timer_architecture_timing Timer Tick System
 *
 * Framework needs monotonic clock with ~1ms resolution. Backend provides.
 * Every tick, all active timers decrement countdown. On zero, callback fires.
 *
 * @section timer_architecture_performance O(1) Per Timer
 *
 * | Operation | Time |
 * |-----------|------|
 * | create() | O(1) |
 * | start() | O(1) |
 * | stop() | O(1) |
 * | destroy() | O(1) |
 * | tick() | O(n) [n = active timers] |
 *
 * Tick is only O(n) operation; typically called once per millisecond.
 *
 * @section timer_architecture_limits Constraints
 *
 * @verbatim
 * Min period      - 1ms (timer resolution)
 * Max period      - 2^32 - 1 ms (~49 days)
 * Storage size    - 64 bytes
 * Callback time   - Caller's responsibility to bound
 * @endverbatim
 *
 * @section timer_architecture_see_also See Also
 *
 * - @ref timer_user_guide for usage patterns
 * - @ref watchdog_architecture for watchdog timing
 *
 */


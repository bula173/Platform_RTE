/**
 * @page appmanager_integration App Manager Integration with Channels
 *
 * @section overview Overview
 *
 * The Application Manager provides lifecycle management (init → execute → shutdown).
 * The developer implements the `execute()` function which runs the main loop.
 *
 * **Key Point:** App Manager does NOT provide:
 * - Channel registry
 * - Channel reading with callbacks
 * - Timer service
 * - Task queue
 * - Main loop structure
 *
 * **These are YOUR responsibility** in the `execute()` function.
 *
 * @section architecture Integration Architecture
 *
 * ```
 * sapi_appmanager_run(&config)
 *     │
 *     └─ Calls ops->init(&context)  ◄─ You implement this
 *     │
 *     └─ Loop: Calls ops->execute(&context) ◄─ You implement this
 *     │   ├─ Read channels with callbacks
 *     │   ├─ Handle timers
 *     │   ├─ Handle tasks
 *     │   ├─ Prepare data
 *     │   └─ Send output
 *     │
 *     └─ Calls ops->shutdown(&context)  ◄─ You implement this
 * ```
 *
 * @section lifecycle Application Lifecycle
 *
 * ### 1. Initialization (init)
 *
 * Called once at startup. Your responsibility:
 * - Create and register named channels
 * - Initialize channel registry
 * - Initialize timer service
 * - Initialize task queue
 * - Allocate application state
 *
 * ```c
 * sapi_status_t my_app_init(void *context) {
 *     my_app_t *app = (my_app_t *)context;
 *
 *     // Create channel registry
 *     app->channel_registry = create_channel_registry();
 *
 *     // Configure and create TCP channel
 *     tcp_config.name = "online_to_standby_vital";
 *     tcp_config.remote_ip = "192.168.1.100";
 *     tcp_config.remote_port = 5000;
 *     tcp_config.on_data_available = on_vital_command;
 *     tcp_config.callback_context = app;
 *
 *     sapi_ipc_create_tcp(&app->vital_channel, &tcp_config);
 *
 *     // Register in registry
 *     channel_registry_register(
 *         app->channel_registry,
 *         "online_to_standby_vital",
 *         on_vital_command,
 *         app
 *     );
 *
 *     // Initialize timer service
 *     app->timers = timer_service_init(MAX_TIMERS);
 *
 *     // Initialize task queue
 *     app->tasks = task_queue_init(MAX_TASKS);
 *
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * ### 2. Main Loop (execute)
 *
 * Called repeatedly. YOUR MAIN LOOP - typical 10ms cycle:
 *
 * ```c
 * sapi_status_t my_app_execute(void *context) {
 *     my_app_t *app = (my_app_t *)context;
 *     uint64_t cycle_start = get_time_ms();
 *
 *     // ===== STEP 1: READ CHANNELS =====
 *     // App Manager decides WHEN to read (deterministic)
 *     read_channels_with_callbacks(app->channel_registry);
 *     // Callbacks invoked for channels with data:
 *     //   on_vital_command("online_to_standby_vital", ...)
 *     //   on_feedback_received("standby_to_online_feedback", ...)
 *     //   on_telemetry_received("online_to_diag_telemetry", ...)
 *
 *     // ===== STEP 2: PROCESS DATA =====
 *     // Already done in callbacks above
 *
 *     // ===== STEP 3: HANDLE TIMERS =====
 *     timer_event_t expired[10];
 *     int count = timer_service_get_expired(app->timers, expired, 10);
 *     for (int i = 0; i < count; i++) {
 *         handle_timer(&expired[i], app);
 *     }
 *
 *     // ===== STEP 4: HANDLE TASKS =====
 *     task_t pending[10];
 *     int task_count = task_queue_get_pending(app->tasks, pending, 10);
 *     for (int i = 0; i < task_count; i++) {
 *         execute_task(&pending[i], app);
 *     }
 *
 *     // ===== STEP 5: PREPARE OUTPUT =====
 *     // Cross-compare, apply safety rules, format for transmission
 *     train_command_t output;
 *     prepare_dual_output(&app->state, &output);
 *
 *     // ===== STEP 6: SEND VITAL =====
 *     // Via vital_channel (automatic 2oo2 voting)
 *     sapi_vital_channel_send(&app->vital_channel, &output);
 *
 *     // ===== STEP 7: SEND NON-VITAL =====
 *     // Fire-and-forget diagnostics (non-blocking)
 *     telemetry_t telemetry;
 *     prepare_telemetry(&app->state, &telemetry);
 *     sapi_ipc_send(&app->diag_channel, &telemetry, 0);  // timeout=0
 *
 *     // ===== STEP 8: MAINTAIN CYCLE TIME =====
 *     uint64_t elapsed = get_time_ms() - cycle_start;
 *     if (elapsed < 10) {
 *         sleep_ms(10 - elapsed);  // Maintain 10ms cycle
 *     } else if (elapsed > 15) {
 *         log_warning("Cycle overrun: %llu ms", elapsed);
 *     }
 *
 *     return SAPI_STATUS_OK;  // Continue loop
 * }
 * ```
 *
 * **What App Manager Does:**
 * - Calls execute() repeatedly
 * - Catches/logs any errors
 * - Tracks iteration count
 * - Stops if error threshold exceeded
 *
 * **What YOU Do:**
 * - Everything inside execute():
 *   - Read channels (when you decide)
 *   - Invoke callbacks
 *   - Process timers and tasks
 *   - Prepare and send data
 *   - Maintain timing
 *
 * ### 3. Shutdown (shutdown)
 *
 * Called once at termination (always, even on error):
 *
 * ```c
 * sapi_status_t my_app_shutdown(void *context) {
 *     my_app_t *app = (my_app_t *)context;
 *
 *     // Close all channels
 *     sapi_ipc_destroy(&app->vital_channel);
 *     sapi_ipc_destroy(&app->diag_channel);
 *
 *     // Free timer service
 *     timer_service_destroy(app->timers);
 *
 *     // Free task queue
 *     task_queue_destroy(app->tasks);
 *
 *     // Free channel registry
 *     channel_registry_destroy(app->channel_registry);
 *
 *     log_info("Application shutdown complete");
 *     return SAPI_STATUS_OK;  // Must not fail
 * }
 * ```
 *
 * @section complete_example Complete Example
 *
 * ### Application Context
 *
 * ```c
 * typedef struct {
 *     // Channels
 *     channel_registry_t *channel_registry;
 *     sapi_vital_channel_t vital_channel;
 *     sapi_ipc_handle_t diag_channel;
 *
 *     // Services
 *     timer_service_t *timers;
 *     task_queue_t *tasks;
 *
 *     // State
 *     train_state_t state;
 *     uint32_t iteration_count;
 *
 *     // Statistics
 *     uint32_t vital_messages_received;
 *     uint32_t feedback_messages_received;
 * } my_app_t;
 * ```
 *
 * ### Main Function
 *
 * ```c
 * int main(void) {
 *     my_app_t app = {0};
 *
 *     const sapi_appmanager_operations_t ops = {
 *         .init = my_app_init,
 *         .execute = my_app_execute,
 *         .shutdown = my_app_shutdown,
 *         .get_name = my_app_get_name,
 *         .get_version = my_app_get_version,
 *     };
 *
 *     sapi_appmanager_config_t config = {
 *         .ops = &ops,
 *         .context = &app,
 *         .max_iterations = 0,        // Run forever
 *         .error_threshold = 10,      // Stop after 10 errors
 *     };
 *
 *     return sapi_appmanager_run(&config);
 * }
 * ```
 *
 * **App Manager Behavior:**
 * 1. Calls my_app_init(&app)
 *    - Creates channels, timers, tasks
 * 2. Loop: Calls my_app_execute(&app) repeatedly
 *    - Your main loop runs here
 *    - Reads channels with callbacks
 *    - Processes timers and tasks
 *    - Sends output
 * 3. Calls my_app_shutdown(&app)
 *    - Closes resources
 * 4. Returns 0 (success) or 1 (failure)
 *
 * @section responsibility Responsibility Summary
 *
 * **App Manager Provides:**
 * - ✓ Lifecycle management (init/execute/shutdown)
 * - ✓ Error tracking (error_count, error_threshold)
 * - ✓ Iteration counting
 * - ✓ State management (INITIALIZING, RUNNING, SHUTTING_DOWN, etc.)
 * - ✓ Graceful shutdown coordination
 *
 * **YOU Implement in execute():**
 * - ✓ Channel registry (create, register, read)
 * - ✓ Channel reading with callbacks (read_channels_with_callbacks)
 * - ✓ Timer service (create, get_expired, handle)
 * - ✓ Task queue (create, get_pending, execute)
 * - ✓ Main loop structure and timing
 * - ✓ Data processing and preparation
 * - ✓ Sending vital and non-vital outputs
 *
 * @section you_implement What You Need to Implement
 *
 * These are NOT provided by the framework - you implement them:
 *
 * ```c
 * // Channel registry (managing named channels)
 * typedef struct {
 *     channel_entry_t channels[MAX_CHANNELS];
 *     uint32_t count;
 * } channel_registry_t;
 *
 * channel_registry_t *create_channel_registry(void);
 * void channel_registry_register(channel_registry_t *reg,
 *                                const char *name,
 *                                sapi_channel_callback_t callback,
 *                                void *context);
 * void read_channels_with_callbacks(channel_registry_t *reg);
 *
 * // Timer service (managing periodic/one-shot timers)
 * timer_service_t *timer_service_init(uint32_t max_timers);
 * int timer_service_get_expired(timer_service_t *ts,
 *                               timer_event_t *expired,
 *                               int max_count);
 * void timer_service_destroy(timer_service_t *ts);
 *
 * // Task queue (managing pending work)
 * task_queue_t *task_queue_init(uint32_t max_tasks);
 * int task_queue_get_pending(task_queue_t *tq,
 *                            task_t *pending,
 *                            int max_count);
 * void task_queue_destroy(task_queue_t *tq);
 * ```
 *
 * These are project-specific - the framework gives you the patterns,
 * you implement the concrete services.
 *
 * @section guidelines Design Guidelines
 *
 * 1. **Deterministic Timing**
 *    - Decide fixed cycle time (e.g., 10ms)
 *    - App Manager calls execute() repeatedly
 *    - YOU maintain the cycle time inside execute()
 *
 * 2. **Channel Reading**
 *    - App Manager does NOT read channels automatically
 *    - YOU explicitly call read_channels_with_callbacks()
 *    - YOU decide when in the cycle to read
 *
 * 3. **Error Handling**
 *    - execute() errors are caught by App Manager
 *    - Count errors, trigger shutdown at threshold
 *    - Graceful degradation (continue if possible)
 *
 * 4. **Shutdown**
 *    - shutdown() is always called
 *    - Must be robust (even if init/execute failed)
 *    - Close resources, log final state
 *
 * @section next_steps Next Steps
 *
 * 1. Understand App Manager lifecycle (init/execute/shutdown)
 * 2. Implement channel registry for named channels
 * 3. Implement timer service
 * 4. Implement task queue
 * 5. Build main loop in execute() function
 * 6. Test with your actual transports (TCP, FIFO, etc.)
 *
 */

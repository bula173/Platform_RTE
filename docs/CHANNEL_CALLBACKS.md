/**
 * @page channel_callbacks Channel Naming and Callback Registration
 *
 * @section overview Channel Identification with Names and Callbacks
 *
 * Each channel has a unique name and optional callback function. The App Manager
 * controls WHEN to read channels. When the App Manager decides to read, callbacks
 * are invoked for channels that have data available.
 *
 * **Key Design:** Not direct event-driven, but controlled by App Manager.
 * App Manager decides when to poll channels, and callbacks are invoked only
 * when the App Manager explicitly reads.
 *
 * @section channel_naming Channel Naming Convention
 *
 * Channel names serve as unique identifiers and should follow a consistent pattern:
 *
 * ```
 * "<source>_<dest>_<purpose>"
 *
 * Examples:
 *   "online_to_standby_vital"      - Online sends commands to standby
 *   "standby_to_online_feedback"   - Standby replies with status
 *   "online_to_diag_telemetry"     - Online sends telemetry to diagnostics service
 *   "config_update_channel"        - Configuration updates from external source
 * ```
 *
 * **Benefits of Named Channels:**
 * - ✓ Easy to identify which channel has data in logs
 * - ✓ Debugging: "Data from online_to_standby_vital"
 * - ✓ Monitoring: Track metrics per channel name
 * - ✓ Routing: Dispatch based on channel name
 *
 * @section callback_registration Callback Registration Pattern
 *
 * ### Callback Function Signature
 *
 * ```c
 * typedef void (*sapi_channel_callback_t)(
 *     const char *channel_name,        // Identifier: "online_to_standby_vital"
 *     const void *data,                // Received data
 *     size_t data_size,                // Size of data
 *     void *context                    // User-provided context
 * );
 * ```
 *
 * ### Extended Channel Configuration
 *
 * ```c
 * typedef struct {
 *     const char *name;                // Channel identifier
 *     const char *remote_ip;           // TCP/IP destination
 *     uint16_t remote_port;
 *     size_t message_size;
 *     uint32_t timeout_ms;
 *     bool server_mode;
 *
 *     // NEW: Callback on data available
 *     sapi_channel_callback_t on_data_available;  // Optional callback
 *     void *callback_context;          // User context for callback
 * } sapi_ipc_config_tcp_t;
 * ```
 *
 * ### Example: Registering Callbacks
 *
 * ```c
 * // Define callback
 * void on_command_received(const char *channel_name,
 *                         const void *data, size_t size,
 *                         void *context)
 * {
 *     printf("Data received on channel: %s (%zu bytes)\n", channel_name, size);
 *
 *     train_command_t *cmd = (train_command_t *)data;
 *     process_command(cmd);
 * }
 *
 * // Configure channel with callback
 * sapi_ipc_config_tcp_t tcp_config = {
 *     .name = "online_to_standby_vital",
 *     .remote_ip = "192.168.1.100",
 *     .remote_port = 5000,
 *     .message_size = 256,
 *     .timeout_ms = 1000,
 *     .server_mode = false,
 *
 *     // Register callback
 *     .on_data_available = on_command_received,
 *     .callback_context = &app_context,  // Pass any context data
 * };
 *
 * // Create channel
 * sapi_ipc_handle_t tcp_channel;
 * sapi_ipc_create_tcp(&tcp_channel, &tcp_config);
 * // OS backend will call on_command_received() when data arrives
 * ```
 *
 * @section event_driven_loop Event-Driven Application Loop
 *
 * Instead of polling channels, the framework calls your callbacks:
 *
 * ```c
 * // Old way (polling - inefficient)
 * while (running) {
 *     // Poll each channel
 *     if (vital_channel_recv(&vital, &cmd, timeout=0) == OK) {
 *         process_command(&cmd);
 *     }
 *     if (feedback_channel_recv(&feedback, &status, timeout=0) == OK) {
 *         update_status(&status);
 *     }
 *     if (config_channel_recv(&config, &cfg, timeout=0) == OK) {
 *         apply_config(&cfg);
 *     }
 *     sleep(10ms);  // Wasteful if no data
 * }
 *
 * // New way (event-driven - efficient)
 * // Register callbacks at startup
 * vital_config.on_data_available = on_command_received;
 * feedback_config.on_data_available = on_feedback_received;
 * config_config.on_data_available = on_config_update;
 *
 * // Main loop just waits for events
 * while (running) {
 *     wait_for_event(channels[], timeout=1000);
 *     // Framework calls registered callbacks automatically
 * }
 * ```
 *
 * **Benefits:**
 * - ✓ Responsive: Data triggers callback immediately
 * - ✓ CPU-efficient: No busy-waiting or unnecessary polling
 * - ✓ Clean: Callbacks handle their own data
 * - ✓ Scalable: Works with many channels
 *
 * @section callback_examples Callback Examples by Channel Type
 *
 * ### Example 1: Vital Command Channel
 *
 * ```c
 * void on_vital_command(const char *channel_name,
 *                       const void *data, size_t size,
 *                       void *context)
 * {
 *     app_context_t *app = (app_context_t *)context;
 *
 *     // Channel name helps identify which channel (for monitoring)
 *     log_info("Vital command received on %s", channel_name);
 *     // e.g., "Vital command received on online_to_standby_vital"
 *
 *     // Process the command
 *     train_command_t *cmd = (train_command_t *)data;
 *     if (size != sizeof(*cmd)) {
 *         log_error("Invalid command size: expected %zu, got %zu",
 *                  sizeof(*cmd), size);
 *         return;
 *     }
 *
 *     // Execute command
 *     execute_train_command(cmd);
 *
 *     // Update metrics
 *     app->vital_commands_received++;
 * }
 *
 * // Register
 * sapi_ipc_config_tcp_t vital_config = {
 *     .name = "online_to_standby_vital",
 *     // ... other config ...
 *     .on_data_available = on_vital_command,
 *     .callback_context = &app_context,
 * };
 * ```
 *
 * ### Example 2: Feedback/Health Channel
 *
 * ```c
 * void on_standby_feedback(const char *channel_name,
 *                         const void *data, size_t size,
 *                         void *context)
 * {
 *     app_context_t *app = (app_context_t *)context;
 *
 *     log_debug("Feedback from %s", channel_name);
 *     // e.g., "Feedback from standby_to_online_feedback"
 *
 *     standby_status_t *status = (standby_status_t *)data;
 *     app->standby_healthy = status->is_healthy;
 *     app->standby_last_heartbeat = get_time_ms();
 *
 *     // Check for anomalies
 *     if (status->disagreement_count > THRESHOLD) {
 *         log_warning("Standby disagreement rate high on %s", channel_name);
 *         trigger_watchdog_check();
 *     }
 * }
 *
 * // Register
 * feedback_config.on_data_available = on_standby_feedback;
 * feedback_config.callback_context = &app_context;
 * ```
 *
 * ### Example 3: Non-Vital Diagnostics Channel
 *
 * ```c
 * void on_telemetry(const char *channel_name,
 *                   const void *data, size_t size,
 *                   void *context)
 * {
 *     app_context_t *app = (app_context_t *)context;
 *
 *     log_debug("Telemetry on %s", channel_name);
 *     // e.g., "Telemetry on online_to_diag_telemetry"
 *
 *     telemetry_t *telem = (telemetry_t *)data;
 *     // Send to remote monitoring system
 *     send_to_influxdb(channel_name, telem);
 * }
 *
 * // Register
 * telemetry_config.on_data_available = on_telemetry;
 * ```
 *
 * ### Example 4: Configuration Update Channel
 *
 * ```c
 * void on_config_update(const char *channel_name,
 *                       const void *data, size_t size,
 *                       void *context)
 * {
 *     app_context_t *app = (app_context_t *)context;
 *
 *     log_info("Configuration update on %s", channel_name);
 *     // e.g., "Configuration update on config_update_channel"
 *
 *     config_t *new_config = (config_t *)data;
 *
 *     // Validate before applying
 *     if (!validate_config(new_config)) {
 *         log_error("Invalid config on %s", channel_name);
 *         return;
 *     }
 *
 *     // Apply configuration
 *     apply_configuration(new_config);
 *     log_info("Config applied from %s", channel_name);
 * }
 *
 * // Register
 * config_config.on_data_available = on_config_update;
 * ```
 *
 * @section channel_registry Channel Registry Pattern
 *
 * For systems with many channels, maintain a registry:
 *
 * ```c
 * typedef struct {
 *     const char *name;
 *     sapi_ipc_handle_t handle;
 *     sapi_channel_callback_t callback;
 *     void *context;
 * } channel_registry_entry_t;
 *
 * typedef struct {
 *     channel_registry_entry_t channels[MAX_CHANNELS];
 *     uint32_t count;
 * } channel_registry_t;
 *
 * // Create registry
 * channel_registry_t registry = {0};
 *
 * // Register channels
 * void register_channel(channel_registry_t *reg,
 *                       const char *name,
 *                       sapi_ipc_handle_t handle,
 *                       sapi_channel_callback_t callback,
 *                       void *context)
 * {
 *     if (reg->count >= MAX_CHANNELS) {
 *         return SAPI_STATUS_RESOURCE_EXHAUSTED;
 *     }
 *
 *     reg->channels[reg->count] = {
 *         .name = name,
 *         .handle = handle,
 *         .callback = callback,
 *         .context = context,
 *     };
 *     reg->count++;
 *     return SAPI_STATUS_OK;
 * }
 *
 * // Look up by name
 * channel_registry_entry_t *find_channel(channel_registry_t *reg,
 *                                        const char *name)
 * {
 *     for (uint32_t i = 0; i < reg->count; i++) {
 *         if (strcmp(reg->channels[i].name, name) == 0) {
 *             return &reg->channels[i];
 *         }
 *     }
 *     return NULL;
 * }
 *
 * // Usage
 * channel_registry_entry_t *vital_ch = find_channel(&registry,
 *                                                    "online_to_standby_vital");
 * if (vital_ch) {
 *     // Found the channel by name
 * }
 * ```
 *
 * @section logging_and_monitoring Logging and Monitoring via Channel Names
 *
 * Channel names enable structured logging and monitoring:
 *
 * ```c
 * // Log all channel activity
 * void log_channel_activity(const char *channel_name, const char *event) {
 *     uint64_t timestamp = get_time_ms();
 *     printf("[%llu] CHANNEL[%s] %s\n", timestamp, channel_name, event);
 *     // Output: [123456] CHANNEL[online_to_standby_vital] data_received
 * }
 *
 * // Monitor health by channel name
 * typedef struct {
 *     const char *channel_name;
 *     uint32_t messages_received;
 *     uint32_t errors;
 *     uint32_t bytes_transferred;
 *     uint64_t last_activity_time;
 * } channel_metrics_t;
 *
 * // Track per channel
 * channel_metrics_t *find_or_create_metrics(const char *channel_name) {
 *     // Look up by name, create if missing
 * }
 *
 * // Update metrics in callback
 * void on_data_with_metrics(const char *channel_name,
 *                          const void *data, size_t size,
 *                          void *context)
 * {
 *     channel_metrics_t *metrics = find_or_create_metrics(channel_name);
 *     metrics->messages_received++;
 *     metrics->bytes_transferred += size;
 *     metrics->last_activity_time = get_time_ms();
 *
 *     // Actual processing
 *     process_data(channel_name, data, size);
 * }
 * ```
 *
 * @section callback_best_practices Best Practices
 *
 * 1. **Keep Callbacks Short**
 *    - Do minimal work in callback
 *    - Queue data for later processing if needed
 *    - Avoid blocking operations
 *
 *    ```c
 *    void on_data_short(const char *channel_name,
 *                       const void *data, size_t size,
 *                       void *context)
 *    {
 *        app_context_t *app = (app_context_t *)context;
 *        // Just enqueue
 *        queue_push(&app->work_queue, data, size);
 *        // Actual processing happens in main loop
 *    }
 *    ```
 *
 * 2. **Use Channel Name for Routing**
 *    - Identify which channel has data
 *    - Route to appropriate handler
 *    - Easy debugging
 *
 *    ```c
 *    if (strcmp(channel_name, "vital_command") == 0) {
 *        handle_vital_command(data);
 *    } else if (strcmp(channel_name, "feedback") == 0) {
 *        handle_feedback(data);
 *    }
 *    ```
 *
 * 3. **Log Channel Names in Errors**
 *    - Always include channel name in error logs
 *    - Helps with debugging and monitoring
 *
 *    ```c
 *    if (size != expected_size) {
 *        log_error("Channel %s: invalid size %zu (expected %zu)",
 *                 channel_name, size, expected_size);
 *    }
 *    ```
 *
 * 4. **Register Callbacks at Startup**
 *    - Don't change callbacks at runtime (if possible)
 *    - Register all channels before main loop
 *    - Makes startup sequence clear
 *
 * @section implementation_requirement OS Integrator Requirements
 *
 * **OS backend must:**
 * - ✓ Store channel name from configuration
 * - ✓ Call registered callback when data arrives
 * - ✓ Pass channel name to callback
 * - ✓ Pass data buffer to callback
 * - ✓ Pass data size to callback
 * - ✓ Pass callback_context to callback
 *
 * **Example Implementation (POSIX TCP):**
 * ```c
 * void tcp_receive_thread(tcp_channel_t *ch) {
 *     while (ch->running) {
 *         uint8_t buffer[256];
 *         ssize_t bytes = recv(ch->socket, buffer, sizeof(buffer), 0);
 *         if (bytes > 0 && ch->on_data_available) {
 *             // Call user's callback with channel name and data
 *             ch->on_data_available(
 *                 ch->config.name,        // Channel name
 *                 buffer,                 // Data
 *                 bytes,                  // Size
 *                 ch->config.callback_context  // User context
 *             );
 *         }
 *     }
 * }
 * ```
 *
 */

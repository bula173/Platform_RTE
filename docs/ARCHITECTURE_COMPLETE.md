/**
 * @page architecture_complete Complete Architecture: Named Channels with Callbacks
 *
 * @section overview System Overview
 *
 * The safeAPIFramework implements a complete layered architecture for
 * safety-critical redundant communication.
 *
 * ```
 * ┌──────────────────────────────────────────────────────┐
 * │ APPLICATION LAYER                                    │
 * │ App Manager Main Loop (10ms cycle)                   │
 * │ - Reads named channels with callbacks                │
 * │ - Processes timers and tasks                         │
 * │ - Sends vital outputs (voting)                       │
 * └──────────────────────────────────────────────────────┘
 *            │
 *            ▼
 * ┌──────────────────────────────────────────────────────┐
 * │ CHANNEL MANAGEMENT LAYER                             │
 * │                                                      │
 * │ ┌─────────────────────────────────────────────────┐  │
 * │ │ Named Channel Registry                          │  │
 * │ │ ├─ "online_to_standby_vital" → callback_fn1    │  │
 * │ │ ├─ "standby_to_online_feedback" → callback_fn2 │  │
 * │ │ ├─ "online_to_diag_telemetry" → callback_fn3   │  │
 * │ │ └─ "config_update_channel" → callback_fn4      │  │
 * │ └─────────────────────────────────────────────────┘  │
 * │                                                      │
 * │ When App Manager reads channels:                    │
 * │ For each channel in registry:                       │
 * │   if (has data) → callback(name, data, size, ctx)   │
 * └──────────────────────────────────────────────────────┘
 *            │
 *            ▼
 * ┌──────────────────────────────────────────────────────┐
 * │ VITAL CHANNEL LAYER (Voting & Redundancy)            │
 * │                                                      │
 * │ 2oo2 Voting:                                         │
 * │ ├─ Channel A: "online_to_standby_vital"            │
 * │ ├─ Channel B: "standby_to_online_vital"            │
 * │ └─ If disagree → SAFE_STATE (automatic)            │
 * └──────────────────────────────────────────────────────┘
 *            │
 *            ▼
 * ┌──────────────────────────────────────────────────────┐
 * │ TRANSPORT LAYER (Multi-Transport Abstraction)        │
 * │                                                      │
 * │ Dispatcher routes by transport type:                │
 * │ ├─ Shared Memory (ultra-low latency, <1µs)         │
 * │ ├─ FIFO (local, 1-10ms)                            │
 * │ ├─ TCP/IP (network, 50-100ms, reliable)            │
 * │ └─ UDP (network, <5ms, best-effort)                │
 * │                                                      │
 * │ User specifies: IP addresses, ports, paths         │
 * └──────────────────────────────────────────────────────┘
 *            │
 *            ▼
 * ┌──────────────────────────────────────────────────────┐
 * │ OS/RTOS LAYER (Platform Specific)                    │
 * │                                                      │
 * │ POSIX/Linux:                                         │
 * │ ├─ Shared Memory: mmap() + pthread_mutex           │
 * │ ├─ FIFO: mkfifo() + read()/write()                 │
 * │ ├─ TCP: socket() + connect()/listen()              │
 * │ └─ UDP: socket() + sendto()/recvfrom()             │
 * │                                                      │
 * │ RTOS (QNX, VxWorks):                                │
 * │ ├─ Native MsgPass (deadlock-free, deterministic)   │
 * │ ├─ Shared Memory (with real-time guarantees)       │
 * │ └─ Network protocols (TCP/UDP)                      │
 * └──────────────────────────────────────────────────────┘
 * ```
 *
 * @section channel_flow Data Flow with Named Channels
 *
 * ### Example: Online/Standby Communication (Your Use Case)
 *
 * **Configuration at Startup:**
 *
 * ```c
 * // User specifies IP address and port
 * sapi_ipc_config_tcp_t tcp_config = {
 *     .name = "online_to_standby_vital",  // Unique identifier
 *     .remote_ip = "192.168.1.100",       // User provides
 *     .remote_port = 5000,                // User chooses
 *     .message_size = 256,
 *     .timeout_ms = 1000,
 *     .on_data_available = on_command_received,  // User's callback
 *     .callback_context = &app_context,
 * };
 *
 * // Create channel
 * sapi_ipc_handle_t tcp_channel;
 * sapi_ipc_create_tcp(&tcp_channel, &tcp_config);
 *
 * // Register in registry
 * channel_registry_register("online_to_standby_vital",
 *                          on_command_received,
 *                          &app_context);
 * ```
 *
 * **App Manager Main Loop (Cyclic):**
 *
 * ```c
 * while (running) {
 *     uint64_t cycle_start = get_time_ms();
 *
 *     // STEP 1: Read channels (when App Manager decides)
 *     // App Manager explicitly asks: do any channels have data?
 *     read_channels_with_callbacks(&channel_registry);
 *     // If "online_to_standby_vital" has data:
 *     //   → on_command_received("online_to_standby_vital", data, size, ctx)
 *     // If "standby_to_online_feedback" has data:
 *     //   → on_feedback_received("standby_to_online_feedback", data, size, ctx)
 *
 *     // STEP 2: Process data from callbacks
 *     // (Already handled in callbacks above)
 *
 *     // STEP 3: Handle timers and tasks
 *     timer_service_process();
 *     task_queue_process();
 *
 *     // STEP 4: Prepare output
 *     train_command_t output;
 *     prepare_dual_output(&output);
 *
 *     // STEP 5: Send vital (with voting)
 *     vital_channel_send(&vital, &output);
 *
 *     // STEP 6: Maintain cycle time
 *     uint64_t elapsed = get_time_ms() - cycle_start;
 *     if (elapsed < 10) {
 *         sleep_ms(10 - elapsed);
 *     }
 * }
 * ```
 *
 * **Callback Implementation (User writes this):**
 *
 * ```c
 * void on_command_received(const char *channel_name,
 *                         const void *data, size_t size,
 *                         void *context)
 * {
 *     printf("Data from %s (%zu bytes)\\n", channel_name, size);
 *     // channel_name = "online_to_standby_vital"
 *
 *     train_command_t *cmd = (train_command_t *)data;
 *     process_command(cmd);
 * }
 * ```
 *
 * @section key_benefits Key Benefits of Named Channels with Callbacks
 *
 * ### 1. Identification
 * - ✓ Know which channel data came from by name
 * - ✓ Easy logging: "Data from online_to_standby_vital"
 * - ✓ Channel-specific monitoring and metrics
 *
 * ### 2. Determinism (MISRA-Critical)
 * - ✓ App Manager controls WHEN channels are read
 * - ✓ No async interrupts (predictable timing)
 * - ✓ Bounded latency (known by design)
 * - ✓ Single-threaded (no race conditions)
 * - ✓ Suitable for safety-critical systems (SIL 3/4)
 *
 * ### 3. Simplicity
 * - ✓ Easy to add/remove channels
 * - ✓ Central registry manages all channels
 * - ✓ Callbacks handle their own data
 * - ✓ Clear data flow (channel name → callback)
 *
 * ### 4. Scalability
 * - ✓ Works with many channels (same pattern)
 * - ✓ Easy to monitor health per channel
 * - ✓ Flexible callback implementations
 *
 * @section user_responsibilities User and Integrator Responsibilities
 *
 * ### Framework Provides
 * - ✓ Configuration structures (what to configure)
 * - ✓ Vital channel voting layer
 * - ✓ Channel registry pattern
 * - ✓ Callback infrastructure
 * - ✓ Architecture documentation
 *
 * ### User Provides
 * - ✓ Channel names (unique identifiers)
 * - ✓ Configuration values (IPs, ports, paths)
 * - ✓ Callback implementations (process received data)
 * - ✓ Application main loop (when to read channels)
 *
 * ### OS Integrator Provides
 * - ✓ Channel creation functions (sapi_ipc_create_tcp, etc.)
 * - ✓ Send/receive implementations
 * - ✓ Transport-specific mechanisms
 * - ✓ Mutex/semaphore management
 * - ✓ Platform-specific optimizations
 *
 * @section complete_example Complete Example: Railway Train Control
 *
 * **System Components:**
 * - Online Process (makes decisions)
 * - Standby Process (verifies decisions)
 * - Diagnostics Service (non-vital, logs data)
 *
 * **Configuration:**
 * ```c
 * // Channel 1: Online sends commands to Standby
 * tcp_config ch1 = {
 *     .name = "online_to_standby_vital",
 *     .remote_ip = "192.168.1.100",
 *     .remote_port = 5000,
 *     .on_data_available = on_command_received,
 * };
 *
 * // Channel 2: Standby sends feedback to Online
 * tcp_config ch2 = {
 *     .name = "standby_to_online_feedback",
 *     .remote_ip = "192.168.1.50",
 *     .remote_port = 5001,
 *     .on_data_available = on_feedback_received,
 * };
 *
 * // Channel 3: Diagnostics (non-blocking)
 * tcp_config ch3 = {
 *     .name = "online_to_diag_telemetry",
 *     .remote_ip = "192.168.1.200",
 *     .remote_port = 8080,
 *     .on_data_available = on_telemetry_received,
 * };
 * ```
 *
 * **Channel Registry:**
 * ```c
 * channel_registry_register("online_to_standby_vital",
 *                          on_command_received, app);
 * channel_registry_register("standby_to_online_feedback",
 *                          on_feedback_received, app);
 * channel_registry_register("online_to_diag_telemetry",
 *                          on_telemetry_received, app);
 * ```
 *
 * **App Manager Loop:**
 * ```c
 * while (running) {
 *     // Read all registered channels
 *     // Callbacks fire for any with data
 *     read_channels_with_callbacks(&registry);
 *
 *     // Process
 *     execute_pending_tasks();
 *     handle_expired_timers();
 *
 *     // Send
 *     vital_channel_send(&vital, &command);
 *
 *     // Sleep to maintain 10ms cycle
 *     maintain_cycle_time();
 * }
 * ```
 *
 * **Callback Implementations:**
 * ```c
 * void on_command_received(const char *ch_name, ...) {
 *     if (strcmp(ch_name, "online_to_standby_vital") == 0) {
 *         process_vital_command(data);
 *     }
 * }
 *
 * void on_feedback_received(const char *ch_name, ...) {
 *     if (strcmp(ch_name, "standby_to_online_feedback") == 0) {
 *         update_standby_health(data);
 *     }
 * }
 *
 * void on_telemetry_received(const char *ch_name, ...) {
 *     if (strcmp(ch_name, "online_to_diag_telemetry") == 0) {
 *         send_to_monitoring_system(data);
 *     }
 * }
 * ```
 *
 * @section next_steps Next Steps for Implementation
 *
 * 1. **Understand Architecture**
 *    - Read: docs/SYSTEM_ARCHITECTURE.md
 *    - Read: include/safeapi/vital_channel/ARCHITECTURE.md
 *
 * 2. **Choose Transports**
 *    - Read: include/safeapi/ipc/IPC_TRANSPORT_SELECTION.md
 *    - Decide: TCP/IP for online→standby? Shared memory for local?
 *
 * 3. **Configure Channels**
 *    - Read: include/safeapi/ipc/CHANNEL_CONFIGURATION.md
 *    - Define channel names: "online_to_standby_vital", etc.
 *    - Specify IPs, ports, paths (user provides)
 *
 * 4. **Implement Callbacks**
 *    - Read: docs/CHANNEL_CALLBACKS.md
 *    - Write callback for each channel
 *    - Channel name identifies which channel
 *
 * 5. **Build App Manager**
 *    - Read: docs/SYSTEM_ARCHITECTURE.md (App Manager section)
 *    - Implement main loop (10ms cycle)
 *    - Call read_channels_with_callbacks()
 *
 * 6. **Implement OS Backends**
 *    - Implement: sapi_ipc_create_tcp(), sapi_ipc_create_fifo(), etc.
 *    - Call registered callbacks when data arrives
 *    - Use channel name from configuration
 *
 * 7. **Test**
 *    - Verify callbacks invoked with correct channel names
 *    - Verify voting triggers safe-state on disagreement
 *    - Verify health tracking works
 *    - Profile cycle timing
 *
 * @section reading_order Recommended Reading Order
 *
 * For **System Architects:**
 * 1. This document (architecture_complete.md)
 * 2. docs/SYSTEM_ARCHITECTURE.md
 * 3. include/safeapi/vital_channel/ARCHITECTURE.md
 * 4. include/safeapi/vital_channel/CHANNEL_TOPOLOGIES.md
 *
 * For **Application Developers:**
 * 1. This document (architecture_complete.md)
 * 2. include/safeapi/vital_channel/CHANNEL_TOPOLOGIES.md
 * 3. docs/CHANNEL_CALLBACKS.md
 * 4. include/safeapi/ipc/CHANNEL_CONFIGURATION.md
 *
 * For **OS Integrators:**
 * 1. include/safeapi/ipc/IPC_GUIDE.md
 * 2. include/safeapi/ipc/CHANNEL_CONFIGURATION.md
 * 3. include/safeapi/ipc/IPC_TRANSPORT_SELECTION.md
 * 4. docs/CHANNEL_CALLBACKS.md (callback invocation)
 *
 */

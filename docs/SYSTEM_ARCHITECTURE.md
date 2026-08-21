/**
 * @page system_architecture System Architecture: Channels, App Manager, and Scheduling
 *
 * @section overview Complete System Architecture
 *
 * The safeAPIFramework implements a **layered communication and scheduling architecture**
 * supporting mixed POSIX/Linux and RTOS environments with redundant safety-critical
 * communication patterns.
 *
 * @section layers System Layers (Bottom to Top)
 *
 * ```
 * ┌────────────────────────────────────────────────────────────┐
 * │ APPLICATION LAYER                                          │
 * │ ┌────────────────────────────────────────────────────────┐ │
 * │ │ App Manager (coordinator)                              │ │
 * │ │ - Reads from channels                                  │ │
 * │ │ - Processes data (cross-compare, voting)              │ │
 * │ │ - Handles timers & tasks                              │ │
 * │ │ - Prepares dual/redundant transfers                   │ │
 * │ │ - Writes to output channels                           │ │
 * │ └────────────────────────────────────────────────────────┘ │
 * └───────────────────┬──────────────────────────────────────┘
 *                     │
 * ┌───────────────────▼──────────────────────────────────────┐
 * │ VITAL CHANNEL LAYER (Voting & Redundancy)               │
 * │ ┌────────────────────────────────────────────────────────┐ │
 * │ │ Vital Channel Unit A           Vital Channel Unit B   │ │
 * │ │ (Online/Primary)               (Standby/Secondary)   │ │
 * │ │                                                        │ │
 * │ │ ┌──────────────────┐   ┌──────────────────┐          │ │
 * │ │ │ 2oo2 Voting     │───│ 2oo2 Voting     │ (redundant)
 * │ │ │ [Disagreement   │   │ [Disagreement   │          │ │
 * │ │ │  → Safe-State]  │   │  → Safe-State]  │          │ │
 * │ │ └────────┬─────────┘   └────────┬─────────┘          │ │
 * │ └─────────────────────────────────────────────────────┘ │
 * │                    │
 * ┌───────────────────▼──────────────────────────────────────┐
 * │ IPC TRANSPORT LAYER (Multi-Transport Abstraction)       │
 * │                                                          │
 * │ ┌──────────────────┬──────────────────┬──────────────┐  │
 * │ │ Shared Memory    │ FIFO/Pipes       │ TCP/UDP      │  │
 * │ │ (local, <1µs)    │ (local, 1-10ms)  │ (network)    │  │
 * │ └──────────────────┴──────────────────┴──────────────┘  │
 * │                                                          │
 * │ Backend Vtable Pattern:                                │
 * │ - sapi_ipc_backend_t (send/recv/create/destroy)       │
 * │ - Pluggable: POSIX mqueue, RTOS msgpass, etc.        │
 * └──────────────────────────────────────────────────────┘
 *
 * OS/RTOS LAYER:
 * ├─ POSIX/Linux: pthreads, semaphores, pipes, sockets
 * └─ RTOS (QNX, etc): native messaging, real-time scheduling
 * ```
 *
 * @section channel_taxonomy Channel Taxonomy
 *
 * Channels are classified by **scope**, **redundancy role**, and **blocking semantics**:
 *
 * ### By Scope
 *
 * ```
 * INTERNAL CHANNELS (Within Single Unit/CPU)
 *   ├─ App Manager ←→ Task Handler
 *   ├─ App Manager ←→ Timer Service
 *   ├─ App Manager ←→ NVM (config, logs)
 *   └─ Priority: Throughput (non-blocking queues)
 *      Transport: Shared Memory (fastest)
 *
 * INTER-UNIT CHANNELS (Between Units on Same System)
 *   ├─ Vital Unit A ←→ Vital Unit B (voting/redundancy)
 *   ├─ Vital Unit ←→ Non-Vital Unit
 *   └─ Priority: Safety (blocking, ordered, timeouts)
 *      Transport: FIFO or Shared Memory
 *
 * INTER-SYSTEM CHANNELS (Between Systems/Datacenters)
 *   ├─ Online (Host A) ←→ Standby (Host B)
 *   ├─ Distributed voting with network latency
 *   └─ Priority: Reliability over latency
 *      Transport: TCP/IP (ordered, connection-aware)
 *
 * INTRA-REDUNDANCY CHANNELS (Active ←→ Redundant)
 *   ├─ Online → Standby 1 & Standby 2 (broadcast)
 *   ├─ Standby → Online (feedback, health)
 *   └─ Priority: Determinism + Low latency
 *      Transport: Depends on scope (SHM local, TCP remote)
 * ```
 *
 * ### By Redundancy Role
 *
 * **Vital Channels** (Safety-Critical, 2oo2/2oo3 Voting)
 * - All data goes through voting layer
 * - Disagreement → automatic safe-state
 * - Examples: Train command, safety-critical state
 *
 * **Non-Vital Channels** (Best-Effort, No Voting)
 * - Used for diagnostics, telemetry, configuration
 * - Failures don't trigger safe-state
 * - Examples: Logs, statistics, UI updates
 *
 * **Feedback Channels** (Health & Status)
 * - Standby → Online: "I'm healthy", disagreement counts
 * - Online → Watchdog: "Processes running", timer state
 * - Priority: Monitoring, not control
 *
 * ### By Blocking Semantics
 *
 * **Blocking Channels**
 * ```
 * Thread A                    Thread B
 *    │                           │
 *    v                           │
 * send(data)─────────────────>│recv(data)
 * [waits if queue full]       [waits if queue empty]
 * [timeout prevents deadlock]
 * ```
 * Use: Safety-critical RPC patterns (request-reply with timeout)
 *
 * **Non-Blocking Channels**
 * ```
 * Thread A                    Thread B
 *    │                           │
 *    v                           │
 * send(data) → queue           │poll/recv (data available?)
 * [returns immediately]        [check, don't wait]
 * [fails if queue full]
 * ```
 * Use: High-throughput, low-latency paths (app manager dispatch)
 *
 * **Async Channels** (FIFO with notifications)
 * ```
 * Thread A            Kernel           Thread B
 *    │                  │                │
 *    v                  │                │
 * send(data)→[queue]→signal()─────────>wakeup()
 * [returns after enqueue, before recv]
 * [receiver notified on arrival]
 * ```
 * Use: Event-driven patterns (watchdog notifications)
 *
 * @section app_manager App Manager (Orchestrator)
 *
 * The App Manager is the central coordinator that ties together channels,
 * scheduling, and data processing.
 *
 * ### App Manager Responsibilities
 *
 * ```c
 * void app_manager_main_loop(void) {
 *     while (running) {
 *         // 1. READ: Collect data from all input channels
 *         status = vital_channel_recv(&vital_cmd, timeout=100ms);
 *         status = feedback_channel_recv(&health, timeout=0);  // non-blocking
 *         status = config_channel_recv(&config, timeout=0);    // non-blocking
 *
 *         // 2. PROCESS: Update internal state
 *         if (vital_cmd.valid) {
 *             process_train_command(&vital_cmd);
 *             update_state(&system_state);
 *         }
 *
 *         if (health.disagreement_count > THRESHOLD) {
 *             log_warning("Channel health degraded");
 *         }
 *
 *         // 3. TIMERS: Check expired timers from scheduler
 *         timer_event_t expired_timers[10];
 *         int count = timer_get_expired(&expired_timers, 10);
 *         for (int i = 0; i < count; i++) {
 *             handle_timeout(&expired_timers[i]);
 *         }
 *
 *         // 4. TASKS: Execute pending task queue
 *         task_t pending_tasks[10];
 *         int task_count = task_get_pending(&pending_tasks, 10);
 *         for (int i = 0; i < task_count; i++) {
 *             execute_task(&pending_tasks[i]);
 *         }
 *
 *         // 5. PREPARE: Cross-compare and prepare redundant output
 *         train_state_t output;
 *         prepare_dual_output(&system_state, &output);
 *         // - Sanity checks (cross-compare with previous state)
 *         // - Apply safety rules (speed limits, etc)
 *         // - Format for transmission
 *
 *         // 6. SEND: Broadcast to all redundant channels (atomic)
 *         status = vital_channel_send(&vital_output, &output);
 *         if (status != OK) {
 *             trigger_safe_state(REASON_SEND_FAILURE);
 *         }
 *
 *         // 7. SEND NON-VITAL: Send diagnostics (non-blocking)
 *         telemetry_channel_send(&telemetry, timeout=0);  // Fire-and-forget
 *         log_channel_send(&log_event, timeout=0);
 *     }
 * }
 * ```
 *
 * ### App Manager Execution Model
 *
 * **Periodic Execution** (Recommended for Safety-Critical)
 * ```
 * ┌─────────┬──────────┬──────────┬──────────┬──────────┐
 * │ Cycle 0 │ Cycle 1  │ Cycle 2  │ Cycle 3  │ Cycle 4  │...
 * │ (10ms)  │ (10ms)   │ (10ms)   │ (10ms)   │ (10ms)   │
 * ├─────────┼──────────┼──────────┼──────────┼──────────┤
 * │ Read    │ Read     │ Read     │ Read     │ Read     │
 * │Process  │Process   │Process   │Process   │Process   │
 * │Send     │Send      │Send      │Send      │Send      │
 * └─────────┴──────────┴──────────┴──────────┴──────────┘
 *
 * ✓ Deterministic timing
 * ✓ Bounded worst-case latency
 * ✓ Easy to verify (fixed schedule)
 * ✗ Wastes CPU if no data ready (can optimize with blocking recv with timeout)
 * ```
 *
 * **Event-Driven Execution** (For High-Throughput, Best-Effort)
 * ```
 * Channel 0: Data available ──┐
 *                              ├──> App Manager wakes
 * Channel 1: Data available ──┤    (event loop notified)
 *                              │
 * Timer: Expired ─────────────┴
 *
 * while (running) {
 *     // Block until ANY event
 *     event = wait_for_event(channels[], timers[], timeout);
 *     // Handle event (read channel, expire timer, etc)
 * }
 *
 * ✓ Responsive (wakes immediately on data)
 * ✓ CPU-efficient (no busy waiting)
 * ✗ Harder to verify timing (depends on event arrival)
 * ✗ Not recommended for strict safety-critical systems
 * ```
 *
 * **Hybrid Execution** (Recommended for Mixed Systems)
 * ```
 * Periodic timer (10ms):
 *   - Read vital channels (blocking recv with 100ms timeout)
 *   - Process safety-critical data
 *   - Send vital outputs
 *
 * Event thread (optional):
 *   - Non-blocking poll of non-vital channels
 *   - Handle diagnostics, logging
 *   - Health monitoring
 *
 * Both share same system_state (with locking)
 * ```
 *
 * @section internal_scheduler Internal Timer Scheduler
 *
 * The framework includes a timer service for managing:
 * - Safety timeouts (watchdog, inter-process heartbeats)
 * - Application timers (speed ramp, hold times)
 * - Periodic tasks (diagnostics collection)
 *
 * ### Timer Service Architecture
 *
 * ```c
 * typedef enum {
 *     TIMER_TYPE_ONE_SHOT,        // Fire once, then stop
 *     TIMER_TYPE_PERIODIC,        // Fire repeatedly at interval
 *     TIMER_TYPE_COUNTDOWN,       // Decrement, trigger when zero
 * } timer_type_t;
 *
 * typedef struct {
 *     uint32_t timer_id;
 *     uint32_t interval_ms;       // Reload interval (periodic)
 *     uint32_t remaining_ms;      // Time until expiration
 *     timer_type_t type;
 *     void (*callback)(uint32_t timer_id, void *context);
 *     void *context;
 *     bool enabled;
 * } timer_t;
 *
 * // Create timer for watchdog heartbeat (periodic, 100ms)
 * timer_create(&watchdog_timer, "watchdog_heartbeat");
 * timer_set_periodic(&watchdog_timer, 100, on_heartbeat, NULL);
 * timer_start(&watchdog_timer);
 *
 * // Create timer for safety timeout (one-shot, 5s)
 * timer_create(&safety_timer, "speed_ramp_timeout");
 * timer_set_one_shot(&safety_timer, 5000, on_ramp_timeout, NULL);
 * timer_start(&safety_timer);
 * ```
 *
 * ### Integration with App Manager
 *
 * ```c
 * // In main loop
 * timer_event_t expired[10];
 * int count = timer_service_get_expired(&expired, 10);
 *
 * for (int i = 0; i < count; i++) {
 *     // Timer callback was already called by scheduler
 *     // But we can also handle the expiration event
 *     log_info("Timer %u expired", expired[i].timer_id);
 * }
 * ```
 *
 * @section blocking_semantics Blocking vs Non-Blocking Channel Semantics
 *
 * ### Blocking Channels (with timeout)
 *
 * **Use Case:** Vital safety-critical communication
 *
 * ```c
 * // Blocking receive (waits up to 1000ms for data)
 * status = sapi_vital_channel_recv(&vital_channel, &cmd, sizeof(cmd),
 *                                   timeout_ms = 1000,
 *                                   &vote_result);
 *
 * if (status == SAPI_STATUS_OK) {
 *     // Data arrived and voting passed
 *     process_command(&cmd);
 * } else if (status == SAPI_STATUS_TIMEOUT) {
 *     // No data within 1000ms
 *     // → Trigger safe-state (no vital input)
 *     trigger_safe_state(REASON_COMMUNICATION_TIMEOUT);
 * } else if (status == SAPI_STATUS_HARDWARE_FAULT) {
 *     // Voting failed (disagreement or channel error)
 *     // → Safe-state already triggered by vital_channel
 *     log_error("Voting failure detected");
 * }
 * ```
 *
 * **Blocking Properties:**
 * - Caller sleeps until data available OR timeout
 * - Predictable latency (timeout = max wait time)
 * - CPU-efficient (no busy-loop)
 * - ✓ Safe: Cannot silently miss data
 * - ✗ Requires reasonable timeout (too short = false alarms, too long = slow recovery)
 *
 * ### Non-Blocking Channels
 *
 * **Use Case:** Best-effort diagnostics, configuration updates
 *
 * ```c
 * // Non-blocking receive (returns immediately)
 * status = sapi_ipc_recv(&diagnostic_channel, &stats, sizeof(stats),
 *                        timeout_ms = 0);  // Zero timeout = non-blocking
 *
 * if (status == SAPI_STATUS_OK) {
 *     // Data was available in queue
 *     process_diagnostics(&stats);
 * } else if (status == SAPI_STATUS_TIMEOUT) {
 *     // No data available right now
 *     // → That's OK for diagnostics, just skip this cycle
 *     log_debug("No diagnostic data this cycle");
 * }
 * ```
 *
 * **Non-Blocking Properties:**
 * - Caller doesn't wait
 * - Returns immediately with data OR timeout status
 * - ✓ High throughput (never blocks app manager)
 * - ✓ Responsive (low latency)
 * - ✗ Data can be lost (if queue full, send fails)
 * - ✗ Must handle TIMEOUT status (no data available)
 *
 * ### Hybrid Approach (Recommended)
 *
 * ```c
 * while (running) {
 *     // VITAL: Blocking with timeout
 *     // If no data within timeout, that's a safety violation
 *     status = vital_channel_recv(&cmd, timeout_ms=1000);
 *     if (status != OK) {
 *         trigger_safe_state(REASON_VITAL_TIMEOUT);
 *     }
 *
 *     // NON-VITAL: Non-blocking
 *     // If no data, that's fine, continue normally
 *     status = config_channel_recv(&cfg, timeout_ms=0);
 *     if (status == OK) {
 *         apply_config(&cfg);
 *     }
 *
 *     // NON-VITAL: Non-blocking
 *     status = health_channel_recv(&health, timeout_ms=0);
 *     if (status == OK) {
 *         update_health_dashboard(&health);
 *     }
 *
 *     process_and_send();
 * }
 * ```
 *
 * @section posix_vs_rtos POSIX/Linux vs RTOS Implementation Differences
 *
 * ### POSIX/Linux Backend
 *
 * **Timing Precision:** Millisecond-level (10ms jitter typical)
 *
 * **Transport Methods:**
 * - Shared Memory: `mmap()` + synchronization (`pthread_mutex`, `sem_t`)
 * - FIFO: Named pipes (`mkfifo()`, `open()`, `read()`, `write()`)
 * - Sockets: TCP (`socket()`, `connect()`, `send()`), UDP (`sendto()`, `recvfrom()`)
 * - POSIX MQueue: `mq_open()`, `mq_send()`, `mq_receive()`
 *
 * **Synchronization:**
 * ```c
 * // Shared memory with mutex
 * sapi_ipc_backend_t posix_shm_backend = {
 *     .create = posix_shm_create,    // mmap() + pthread_mutex_init()
 *     .send = posix_shm_send,        // pthread_mutex_lock() + memcpy()
 *     .receive = posix_shm_recv,     // pthread_mutex_lock() + memcpy()
 *     .destroy = posix_shm_destroy,  // munmap() + pthread_mutex_destroy()
 * };
 * ```
 *
 * **Blocking Implementation:**
 * ```c
 * sapi_status_t posix_shm_recv(handle, buf, size, timeout_ms) {
 *     struct timespec deadline = current_time() + timeout_ms;
 *
 *     pthread_mutex_lock(&q->lock);
 *     while (q->count == 0) {
 *         // Wait on condition variable with timeout
 *         int rc = pthread_cond_timedwait(&q->not_empty, &q->lock, &deadline);
 *         if (rc == ETIMEDOUT) {
 *             pthread_mutex_unlock(&q->lock);
 *             return SAPI_STATUS_TIMEOUT;
 *         }
 *     }
 *     // Copy data from queue
 *     memcpy(buf, q->data[q->read_idx], size);
 *     q->read_idx = (q->read_idx + 1) % QUEUE_DEPTH;
 *     q->count--;
 *     pthread_cond_signal(&q->not_full);
 *     pthread_mutex_unlock(&q->lock);
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * **Strengths:**
 * - ✓ Portable across Linux distributions
 * - ✓ Rich networking support (TCP/IP, UDP multicast)
 * - ✓ Large ecosystem (debugging tools, profilers)
 * - ✓ Low cost (open source)
 *
 * **Weaknesses:**
 * - ✗ Non-deterministic timing (jitter from OS scheduler)
 * - ✗ Not suitable for hard real-time (ms-level, not µs-level)
 * - ✗ Requires careful tuning for consistency
 *
 * ### RTOS Backend (QNX Neutrino Example)
 *
 * **Timing Precision:** Microsecond-level (deterministic within CPU clock cycles)
 *
 * **Transport Methods:**
 * - Native MsgPass: QNX's lightweight IPC (`MsgSend()`, `MsgReceive()`)
 * - Shared Memory: Same as POSIX but with real-time scheduling
 * - Network: TCP/IP with prioritized scheduling
 *
 * **Synchronization:**
 * ```c
 * // QNX native messaging
 * typedef struct {
 *     int server_coid;      // Connection to server
 *     int server_chid;      // Channel for receiving
 *     uint8_t buffer[256];  // Message buffer
 * } qnx_msgpass_channel_t;
 *
 * sapi_status_t qnx_msgpass_send(handle, buf, size, timeout_ms) {
 *     qnx_msgpass_channel_t *ch = (qnx_msgpass_channel_t *)handle;
 *
 *     // Send is atomic and deadlock-free in QNX
 *     int rc = MsgSend(ch->server_coid, buf, size, NULL, 0);
 *     if (rc == -1 && errno == ETIMEDOUT) {
 *         return SAPI_STATUS_TIMEOUT;
 *     }
 *     return (rc >= 0) ? SAPI_STATUS_OK : SAPI_STATUS_HARDWARE_FAULT;
 * }
 *
 * sapi_status_t qnx_msgpass_recv(handle, buf, size, timeout_ms) {
 *     qnx_msgpass_channel_t *ch = (qnx_msgpass_channel_t *)handle;
 *
 *     // Receive with kernel-enforced timeout
 *     int bytes = MsgReceive(ch->server_chid, buf, size, NULL);
 *     if (bytes == -1 && errno == ETIMEDOUT) {
 *         return SAPI_STATUS_TIMEOUT;
 *     }
 *     return (bytes > 0) ? SAPI_STATUS_OK : SAPI_STATUS_HARDWARE_FAULT;
 * }
 * ```
 *
 * **Strengths:**
 * - ✓ Deterministic messaging (deadlock-free by design)
 * - ✓ Kernel-enforced timeouts (cannot miss)
 * - ✓ Priority-based scheduling (important tasks run first)
 * - ✓ Suitable for hard real-time (safety-critical systems)
 * - ✓ Built-in synchronization (no mutex needed)
 *
 * **Weaknesses:**
 * - ✗ Proprietary (QNX, VxWorks, etc.)
 * - ✗ Higher licensing costs
 * - ✗ More complex (more features = steeper learning curve)
 * - ✗ Limited ecosystem (fewer debugging tools)
 *
 * ### Recommended Approach
 *
 * **For Development & Testing:**
 * Use POSIX/Linux backend for rapid iteration. Code against the backend vtable,
 * so switching is trivial later.
 *
 * **For Production Safety-Critical:**
 * Use RTOS backend (QNX, VxWorks) for deterministic timing and fault guarantees.
 * Implement same backend vtable, so app code is identical.
 *
 * **For Hybrid Deployments:**
 * Some units on POSIX, some on RTOS. Vital channels can span both:
 * - POSIX unit (Host A) ←→ (TCP/IP) ←→ RTOS unit (embedded board)
 * - Each uses its native backend internally
 * - Communicate via network transport (TCP/IP works on both)
 *
 * @section data_flow_patterns Data Flow Patterns
 *
 * ### Pattern 1: Request-Reply (RPC Style)
 *
 * ```
 * Client                          Server
 *   │                               │
 *   │ 1. send(request) ─────────> │
 *   │    [blocks, waits]          │
 *   │                              │ 2. recv(request)
 *   │                              │
 *   │                              │ 3. process()
 *   │                              │
 *   │ 4. send(reply) <────────── │
 *   │    [continues]              │
 *   │
 *   │ 5. recv(reply)
 *   │    [gets response]
 *   │
 *   └─> process(reply)
 *
 * Use: Train controller queries signal database
 * Timeout: 500ms (reject reply if no response within 500ms)
 * ```
 *
 * ### Pattern 2: Publisher-Subscriber (Broadcast)
 *
 * ```
 * Publisher           Topic: "track_status"         Subscribers
 *    │                                                   │
 *    │ 1. publish("track_status", data) ────────>│─────┤
 *    │    [fires immediately]                    │     │
 *    │                                            │     │
 *    │ 2. multiple subscribers read independently │
 *    │    (non-blocking poll or callback)         │     │
 *    │                                            ◇     ◇
 *    │                                       Queue  Queue  Queue
 *    │                                         │      │      │
 *    │                                    Subscriber Subscriber Subscriber
 *    │                                    A          B          C
 *
 * Use: Track database broadcasts state changes
 * Timeout: Per-subscriber (each queue has own recv timeout)
 * ```
 *
 * ### Pattern 3: Vital Channel Voting (2oo2 Dual)
 *
 * ```
 * Online (Active)
 *    │
 *    ├──────────[Vital Channel: Channel 0] ────────────────┐
 *    │                                                      │
 *    ├──────────[Vital Channel: Channel 1] ────────────────┤
 *    │                                                      │
 *    └──> [Vital Channel Layer]                            │
 *         - Broadcast to both channels                      │
 *         - Receive from both channels                      │
 *         - Compare results (voting)                        │
 *         - On disagreement → SAFE-STATE                   │
 *                                                      ┌────▼────┐
 *                                                      │Standby 1│
 *                                                      │ (Local) │
 *                                                      └─────────┘
 *                                                      ┌────────┐
 *                                                      │Standby 2│
 *                                                      │(Remote)│
 *                                                      └────────┘
 * ```
 *
 * ### Pattern 4: Cross-Compare (Dual Transfer)
 *
 * ```
 * Online Processing             Standby Processing
 *        │                             │
 *        v                             v
 *   [Compute A]                   [Compute B]
 *        │                             │
 *        v                             v
 *   [Result A]                    [Result B]
 *        │                             │
 *        └─────────────┬───────────────┘
 *                      │
 *              [Cross-Compare Logic]
 *                      │
 *         if (A == B) {
 *             output = A;           ✓ Agreed
 *         } else {
 *             SAFE_STATE();         ✗ Disagreed
 *         }
 *
 * Use: Critical computations (train position, safety decisions)
 * Tolerance: Exact match only (no approximation)
 * ```
 *
 * @section configuration_example Complete Configuration Example
 *
 * ### System Setup: Railway Train Control
 *
 * ```c
 * typedef struct {
 *     // Vital channel (2oo2 voting)
 *     sapi_channel_t vital_command;      // Online ←→ Standbies
 *     sapi_channel_t vital_feedback;     // Standby ←→ Online (health)
 *
 *     // Non-vital channels
 *     sapi_ipc_handle_t internal_events;       // App Mgr ←→ Tasks (internal)
 *     sapi_ipc_handle_t telemetry;             // App Mgr → Telemetry service
 *     sapi_ipc_handle_t config_updates;        // Config service → App Mgr
 *
 *     // Timer service
 *     sapi_timer_t heartbeat_timer;            // 100ms periodic
 *     sapi_timer_t watchdog_timer;             // 5s one-shot
 *
 *     // Task queue
 *     sapi_task_queue_t tasks;
 *
 *     // Shared state
 *     train_state_t current_state;             // Protected by lock
 *     pthread_mutex_t state_lock;
 * } system_context_t;
 * ```
 *
 * ### Main Loop (App Manager)
 *
 * ```c
 * void app_manager_main(system_context_t *sys) {
 *     while (sys->running) {
 *         // 10ms cycle time
 *         uint64_t cycle_start = get_time_ms();
 *
 *         // === READ ===
 *         train_command_t cmd;
 *         sapi_vital_channel_recv(&sys->vital_command, &cmd, sizeof(cmd),
 *                                  timeout_ms=100, NULL);
 *
 *         // === PROCESS ===
 *         process_train_command(&cmd, &sys->current_state);
 *
 *         // === TIMERS ===
 *         timer_event_t expired[10];
 *         int count = sapi_timer_get_expired(&expired, 10);
 *         for (int i = 0; i < count; i++) {
 *             handle_timer_event(&expired[i], sys);
 *         }
 *
 *         // === TASKS ===
 *         sapi_task_t pending[10];
 *         count = sapi_task_get_pending(&pending, 10);
 *         for (int i = 0; i < count; i++) {
 *             execute_task(&pending[i], sys);
 *         }
 *
 *         // === PREPARE ===
 *         train_output_t output;
 *         prepare_dual_output(&sys->current_state, &output);
 *
 *         // === SEND ===
 *         sapi_channel_send(&sys->vital_command, &output);
 *
 *         // === MONITOR ===
 *         check_health(&sys->vital_feedback);
 *
 *         // === CYCLE TIMING ===
 *         uint64_t cycle_elapsed = get_time_ms() - cycle_start;
 *         if (cycle_elapsed < 10) {
 *             sleep_ms(10 - cycle_elapsed);  // Maintain 10ms cycle
 *         } else {
 *             log_warning("Cycle overrun: %llu ms", cycle_elapsed);
 *         }
 *     }
 * }
 * ```
 *
 * @section summary Summary & Key Takeaways
 *
 * 1. **Channels are multi-dimensional:**
 *    - By scope: Internal, Inter-unit, Inter-system
 *    - By role: Vital (voting), Non-vital (best-effort), Feedback (health)
 *    - By blocking: Blocking (safe), Non-blocking (fast), Async (event-driven)
 *
 * 2. **App Manager is the coordinator:**
 *    - Reads all input channels (blocking for vital, non-blocking for non-vital)
 *    - Processes data and applies safety rules
 *    - Handles timers and tasks from internal scheduler
 *    - Prepares output (cross-compare, dual transfer for redundancy)
 *    - Sends to all output channels (atomic via vital_channel)
 *
 * 3. **Blocking semantics provide safety:**
 *    - Vital channels: Blocking with timeout (fail if no response)
 *    - Non-vital: Non-blocking (skip if no data)
 *    - Timeouts prevent silent hangs
 *
 * 4. **POSIX vs RTOS tradeoff:**
 *    - POSIX: Flexible, portable, non-deterministic (ms jitter)
 *    - RTOS: Deterministic, complex, proprietary
 *    - Both support same backend vtable (pluggable)
 *
 * 5. **Multi-transport architecture:**
 *    - Same code works with shared memory, FIFO, TCP, UDP
 *    - User selects transport per channel at init
 *    - Vital channels add voting layer on top
 *    - Disagreement triggers safe-state (automatic)
 *
 */

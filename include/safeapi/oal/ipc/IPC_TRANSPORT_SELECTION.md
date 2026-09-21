/**
 * @page ipc_transport_selection IPC Transport Selection Guide
 *
 * @section ipc_transport_selection_overview Inter-Process Communication (IPC) Transport Layer
 *
 * The safeAPIFramework uses a **OSAdapter vtable pattern** for IPC transport abstraction.
 * The application integrator chooses and implements the concrete transport (shared memory,
 * FIFO, TCP/IP, UDP, etc.) without modifying the framework code.
 *
 * The framework provides:
 * - **Base IPC API**: send(), receive(), create(), destroy() (transport-agnostic)
 * - **Patterns**: Request-Reply (RPC), Pub-Sub (broadcast)
 * - **OSAdapter Interface**: rte_osadapter_ipc_t vtable for plugging in transport
 *
 * @section ipc_transport_selection_transport_options Available Transport Options
 *
 * | Transport | Use Case | Latency | Range | Reliability | Setup |
 * |-----------|----------|---------|-------|-------------|-------|
 * | **Shared Memory** | Same system, high-speed | µs | Local | FIFO queue | Simple |
 * | **FIFO (Named Pipes)** | Same system, per-process | ms | Local | FIFO queue | Simple |
 * | **TCP/IP** | Distributed, reliable | ms-100ms | Network | Ordered, connection-based | Medium |
 * | **UDP** | Distributed, low-latency | ms | Network | Best-effort, connectionless | Medium |
 * | **QNX MsgPass** | RTOS environments | µs | Local | Deadlock-free | RTOS-specific |
 * | **POSIX MQueue** | POSIX systems | ms | Local | Bounded queues | POSIX |
 *
 * @section ipc_transport_selection_osadapter_vtable OSAdapter Implementation Pattern
 *
 * Each transport implements the OSAdapter vtable:
 *
 * ```c
 * typedef struct rte_osadapter_ipc_s {
 *     rte_status_t (*create)(rte_ipc_storage_t *storage,
 *                             const rte_ipc_config_t *config,
 *                             rte_ipc_handle_t *out_handle);
 *     rte_status_t (*send)(rte_ipc_handle_t handle, const void *message,
 *                           size_t message_size, rte_duration_ms_t timeout_ms);
 *     rte_status_t (*receive)(rte_ipc_handle_t handle, void *out_message,
 *                              size_t buffer_size, rte_duration_ms_t timeout_ms);
 *     rte_status_t (*destroy)(rte_ipc_handle_t handle);
 * } rte_osadapter_ipc_t;
 * ```
 *
 * At application startup, register ONE OSAdapter:
 *
 * ```c
 * rte_osadapter_ipc_register(&my_transport_osadapter);  // Once at init
 * // All subsequent rte_ipc_* calls use this OSAdapter
 * ```
 *
 * @section ipc_transport_selection_shared_memory Shared Memory Transport
 *
 * **Best for:** Same-system redundancy (online/standby on same CPU)
 *
 * **Architecture:**
 * ```
 * ┌─────────────────────────────────────┐
 * │ Shared Memory Region (statically allocated in DDR)
 * │ ┌──────────────────────────────────┐
 * │ │ Channel 1: Message Queue (FIFO) │
 * │ └──────────────────────────────────┘
 * │ ┌──────────────────────────────────┐
 * │ │ Channel 2: Message Queue (FIFO) │
 * │ └──────────────────────────────────┘
 * │ ┌──────────────────────────────────┐
 * │ │ Channel 3: Message Queue (FIFO) │
 * │ └──────────────────────────────────┘
 * └─────────────────────────────────────┘
 *   ▲            ▲            ▲
 *   │            │            │
 * Online     Standby1    Standby2
 * ```
 *
 * **Implementation Sketch:**
 * ```c
 * typedef struct {
 *     uint32_t write_idx;
 *     uint32_t read_idx;
 *     uint8_t data[MAX_QUEUE_DEPTH][MAX_MESSAGE_SIZE];
 * } shm_queue_t;
 *
 * rte_status_t shm_send(rte_ipc_handle_t handle, const void *msg, size_t size, uint32_t timeout) {
 *     shm_queue_t *q = (shm_queue_t *)handle;
 *     // Acquire spinlock or mutex
 *     // Check if queue has space
 *     // Copy msg to q->data[q->write_idx]
 *     // Increment write_idx (wrap around)
 *     // Release spinlock
 *     // Signal semaphore/condition-var for waiters
 *     return RTE_STATUS_OK;
 * }
 * ```
 *
 * **Advantages:** Low latency, simple implementation, deterministic timing
 *
 * **Disadvantages:** Limited to same CPU/board, requires synchronization primitives
 *
 * @section ipc_transport_selection_fifo FIFO (Named Pipes) Transport
 *
 * **Best for:** Same-system inter-process communication (different processes)
 *
 * **Architecture:**
 * ```
 * Online Process                    Standby Process
 *   │                                 │
 *   v                                 v
 * /dev/fifo/online→standby    /dev/fifo/standby→online
 *   │                                 │
 *   └─────────────┬─────────────────┘
 *                 │
 *        (File system FIFO)
 * ```
 *
 * **Implementation Sketch:**
 * ```c
 * typedef struct {
 *     int fd;                    // File descriptor to FIFO
 *     char name[256];            // FIFO path
 * } fifo_channel_t;
 *
 * rte_status_t fifo_send(rte_ipc_handle_t handle, const void *msg, size_t size, uint32_t timeout) {
 *     fifo_channel_t *ch = (fifo_channel_t *)handle;
 *     // Set fd to non-blocking or use poll/select with timeout
 *     ssize_t written = write(ch->fd, msg, size);
 *     // Handle EAGAIN (queue full) with timeout retry
 *     return (written == size) ? RTE_STATUS_OK : RTE_STATUS_TIMEOUT;
 * }
 * ```
 *
 * **Advantages:** Simple, per-process isolation, OS-managed buffering
 *
 * **Disadvantages:** Slower than shared memory, file I/O overhead
 *
 * @section ipc_transport_selection_tcp_ip TCP/IP Transport
 *
 * **Best for:** Distributed online/standby across network (LAN/WAN)
 *
 * **Architecture:**
 * ```
 * Online Process (Host A)
 *      │
 *      v
 * ┌─────────────────────┐
 * │ TCP Port 5000       │
 * └─────────────────────┘
 *      │
 *      │ TCP Connection (reliable, ordered)
 *      │
 *      v
 * ┌─────────────────────┐
 * │ TCP Port 6000       │
 * └─────────────────────┘
 *      ▲
 *      │
 * Standby Process (Host B)
 * ```
 *
 * **Implementation Sketch:**
 * ```c
 * typedef struct {
 *     int socket;                // TCP socket
 *     uint32_t connect_timeout;
 * } tcp_channel_t;
 *
 * rte_status_t tcp_send(rte_ipc_handle_t handle, const void *msg, size_t size, uint32_t timeout) {
 *     tcp_channel_t *ch = (tcp_channel_t *)handle;
 *     // Send header with message size (framing)
 *     uint32_t frame_size = size;
 *     send(ch->socket, &frame_size, sizeof(frame_size), MSG_NOSIGNAL);
 *     // Send payload
 *     ssize_t sent = send(ch->socket, msg, size, MSG_NOSIGNAL);
 *     return (sent == size) ? RTE_STATUS_OK : RTE_STATUS_HARDWARE_FAULT;
 * }
 * ```
 *
 * **Advantages:** Works across network, reliable ordering, connection-based
 *
 * **Disadvantages:** Higher latency (~ms), connection management, security concerns
 *
 * @section ipc_transport_selection_udp UDP Transport
 *
 * **Best for:** Low-latency distributed communication, best-effort delivery acceptable
 *
 * **Architecture:**
 * ```
 * Online (Host A:5000)          Standby (Host B:6000)
 *      │                               │
 *      │ UDP Datagram (unordered)      │
 *      └──────────────────────────────>│
 *      │<──────────────────────────────│
 *      │ UDP Datagram (unordered)      │
 * ```
 *
 * **Implementation Sketch:**
 * ```c
 * typedef struct {
 *     int socket;                // UDP socket
 *     struct sockaddr_in peer;   // Peer address
 * } udp_channel_t;
 *
 * rte_status_t udp_send(rte_ipc_handle_t handle, const void *msg, size_t size, uint32_t timeout) {
 *     udp_channel_t *ch = (udp_channel_t *)handle;
 *     // UDP is connectionless, send directly to peer
 *     ssize_t sent = sendto(ch->socket, msg, size, MSG_DONTWAIT, 
 *                           (struct sockaddr*)&ch->peer, sizeof(ch->peer));
 *     if (sent == -1 && errno == EAGAIN) {
 *         return RTE_STATUS_TIMEOUT;  // Queue full (unordered buffer)
 *     }
 *     return (sent == size) ? RTE_STATUS_OK : RTE_STATUS_HARDWARE_FAULT;
 * }
 * ```
 *
 * **Advantages:** Low latency, simple (connectionless), multicast support
 *
 * **Disadvantages:** Best-effort (packets can be lost), no ordering guarantee, requires vital_channel for reliability
 *
 * @section ipc_transport_selection_vital_channel_integration Integration with Vital Channel
 *
 * Vital Channel provides voting/redundancy ON TOP of any transport. This enables
 * user-controlled selection and multi-transport fallback patterns.
 *
 * ### Two-Channel Redundancy (2oo2) with Transport Choice
 *
 * User decides the transports at runtime:
 *
 * ```c
 * // Configuration: Shared memory for low latency + TCP for remote fallback
 * rte_ipc_handle_t ch0, ch1;
 * rte_ipc_config_t cfg0 = {
 *     .name = "shm_channel",
 *     .message_size = sizeof(train_cmd_t),
 *     .queue_depth = 10
 * };
 * rte_ipc_config_t cfg1 = {
 *     .name = "tcp_channel",
 *     .message_size = sizeof(train_cmd_t),
 *     .queue_depth = 5
 * };
 *
 * // Select OSAdapters at startup (but only ONE global OSAdapter)
 * // If we want mixed transports, we need per-channel wrapper callbacks
 *
 * // Create channels with selected transports
 * rte_ipc_create(&storage0, &cfg0, &ch0);  // Uses registered OSAdapter
 * rte_ipc_create(&storage1, &cfg1, &ch1);  // Uses registered OSAdapter
 *
 * // Wrap in vital channel for 2oo2 voting
 * rte_channel_config_t vital_cfg = {
 *     .voting_strategy = RTE_VOTING_2OO2,
 *     .channel_timeout_ms = 1000,
 *     .log_disagreements = true,
 *     .backend_send = custom_backend_send,    // YOUR callback
 *     .backend_recv = custom_backend_recv,    // YOUR callback
 * };
 *
 * void *channels[2] = { &ch0, &ch1 };
 * rte_channel_init(&vital_channel, &vital_cfg, channels, 2);
 *
 * // Now use vital_channel for voting communication
 * rte_channel_send(&vital_channel, &cmd, sizeof(cmd));
 * ```
 *
 * ### Custom Callbacks for Multi-Transport Support
 *
 * If you need BOTH shared memory and TCP in the same application:
 *
 * ```c
 * // Channel 0 uses shared memory (fast, local)
 * // Channel 1 uses TCP (remote, reliable backup)
 *
 * rte_status_t mixed_backend_send(void *ch, const void *data, size_t size) {
 *     if (is_shm_channel(ch)) {
 *         return shm_queue_send((shm_queue_t*)ch, data, size);
 *     } else if (is_tcp_channel(ch)) {
 *         return tcp_socket_send((tcp_channel_t*)ch, data, size);
 *     }
 *     return RTE_STATUS_INVALID_PARAM;
 * }
 *
 * rte_status_t mixed_backend_recv(void *ch, void *data, size_t size, uint32_t timeout) {
 *     if (is_shm_channel(ch)) {
 *         return shm_queue_recv((shm_queue_t*)ch, data, size, timeout);
 *     } else if (is_tcp_channel(ch)) {
 *         return tcp_socket_recv((tcp_channel_t*)ch, data, size, timeout);
 *     }
 *     return RTE_STATUS_INVALID_PARAM;
 * }
 *
 * // Use this in vital_channel config
 * vital_cfg.backend_send = mixed_backend_send;
 * vital_cfg.backend_recv = mixed_backend_recv;
 * ```
 *
 * @section ipc_transport_selection_online_standby_patterns Online/Standby Redundancy Patterns
 *
 * Vital Channel supports several redundancy architectures:
 *
 * ### Pattern 1: Local Dual Channels (2oo2 Same System)
 *
 * **Scenario:** Two processes on same CPU/board (online + standby local)
 *
 * ```
 * ┌──────────────────────────────────┐
 * │ Single CPU / Single Board        │
 * │                                  │
 * │ ┌────────┐     ┌────────┐        │
 * │ │ Online │     │Standby │        │
 * │ │Process │     │Process │        │
 * │ └────┬───┘     └───┬────┘        │
 * │      │             │             │
 * │ [Shared Memory Channel 0]        │
 * │ [Shared Memory Channel 1]        │
 * │      │             │             │
 * │ ┌────▼─────────────▼────┐        │
 * │ │ Vital Channel (2oo2)  │        │
 * │ │ [Voting & Health]     │        │
 * │ └───────────────────────┘        │
 * └──────────────────────────────────┘
 * ```
 *
 * **Configuration:**
 * ```c
 * // Both channels = shared memory (low latency)
 * rte_voting_strategy = RTE_VOTING_2OO2
 * channel[0] = shared_memory_queue_0
 * channel[1] = shared_memory_queue_1
 * ```
 *
 * **Failure Handling:**
 * - If channel[0] fails (e.g., standby process crashes)
 *   → Health counters show errors on ch[1]
 *   → Watchdog detects disagreement pattern
 *   → Watchdog can trigger manual intervention (not automatic safe-state)
 * - If channel[1] fails similarly → same pattern
 * - If both channels healthy but disagree → IMMEDIATE safe-state (data corruption detected)
 *
 * ### Pattern 2: Remote Dual Channels (2oo2 Distributed)
 *
 * **Scenario:** Online + Standby on different CPUs/boards (over network)
 *
 * ```
 * Online (Host A)              Standby (Host B)
 *     │                            │
 *     v                            v
 * [Vital Compute]         [Vital Verify]
 *     │                            │
 *     ├─── TCP Port 5000 ─────────>│
 *     │<──── TCP Port 6000 ────────┤
 *     │
 * [Vital Channel (2oo2)]
 *     │
 *     ├─── TCP/IP Channel 0 ──────>│
 *     │<───────────────────────────┤
 *     │
 *     ├─── UDP Multicast Ch 1 ────>│  (backup, lower latency)
 *     │<────────────────────────────┤
 * ```
 *
 * **Configuration:**
 * ```c
 * channel[0] = tcp_channel  (high reliability, ~50ms latency)
 * channel[1] = udp_channel  (low latency ~5ms, best-effort)
 *
 * If tcp_recv times out:
 *   → Use udp result (if available)
 *   → If UDP also times out → INSUFFICIENT_QUORUM
 * If tcp and udp disagree:
 *   → VOTING_DISAGREED → SAFE-STATE
 * ```
 *
 * ### Pattern 3: Triple Redundancy (2oo3 Distributed)
 *
 * **Scenario:** Three redundant processes (tolerate 1 Byzantine fault)
 *
 * ```
 * Process A             Process B             Process C
 * (Online)             (Standby 1)           (Standby 2)
 *    │                    │                     │
 *    ├──────────────TCP───>│                     │
 *    │<───────────────────TCP─────────────────>│
 *    │                     │<────TCP────────────┤
 *    │<────────UDP─────────┤                    │
 *    │                     │<───────UDP────────>│
 *    │<──────────────UDP──────────────────────>│
 *    │
 * [Vital Channel 2oo3]
 *    ├─ Channel 0: TCP to B (reliable)
 *    ├─ Channel 1: UDP to C (fast)
 *    └─ Channel 2: TCP to C (reliable)
 *
 * Voting:
 *   - 2 or 3 channels agree → AGREED (that's the result)
 *   - 0/1 channels return (others timeout) → INSUFFICIENT_QUORUM
 *   - Each pair gives different result → DISAGREED → SAFE-STATE
 * ```
 *
 * ### Pattern 4: Hybrid Transports (Speed + Reliability)
 *
 * **Scenario:** Use fastest transport (shared memory) + fallback to reliable transport (TCP)
 *
 * ```
 * Online Process (Host A)
 *      │
 *      ├─── [Shared Memory Channel 0] ───────┐
 *      │         (0.1ms, local standby)       │
 *      │                                      v
 *      │                            ┌──────────────────┐
 *      │                            │ Standby (Host B) │
 *      │                            │ (Fast response)  │
 *      │                            └──────────────────┘
 *      │
 *      ├─── [TCP/IP Channel 1] ───────────────┐
 *      │      (50ms, remote standby)          │
 *      │                                      v
 *      │                            ┌──────────────────┐
 *      │                            │ Remote Standby   │
 *      │                            │ (Reliable backup)│
 *      │                            └──────────────────┘
 *      │
 * [Vital Channel 2oo2]
 *      │
 *      └─ Normal: Both channels agree (data replicated to both)
 *         Safe-State: Channels disagree (data corruption) OR timeout
 * ```
 *
 * @section ipc_transport_selection_decision_tree Transport Selection Decision Tree
 *
 * ```
 * Is communication local (same CPU/board)?
 *   ├─ YES → Use SHARED MEMORY
 *   │        Pros: <1µs latency, deterministic, simple locking
 *   │        Cons: Limited to local system
 *   │
 *   └─ NO → Is reliability more important than latency?
 *        ├─ YES → Use TCP/IP
 *        │        Pros: Guaranteed ordered delivery, connection-aware
 *        │        Cons: ~10-50ms latency, connection overhead
 *        │
 *        └─ NO → Use UDP
 *               Pros: <5ms latency, simple (connectionless)
 *               Cons: Best-effort, packets can be lost
 *                    → MITIGATE: Use vital_channel voting for reliability
 * ```
 *
 * @section ipc_transport_selection_recommended_configurations Recommended Configurations
 *
 * ### For Railway ERTMS (SIL 4)
 *
 * ```c
 * // 2oo2 redundancy with high reliability
 * config = {
 *     .voting_strategy = RTE_VOTING_2OO2,
 *     .channel_timeout_ms = 500,  // Strict timeout
 *     .log_disagreements = true,  // Every mismatch logged
 *     .backend_send = tcp_send,   // TCP for ordered delivery
 *     .backend_recv = tcp_recv,   // TCP connection-based
 * };
 * // Channels: TCP to hot-standby + TCP to cold-standby
 * ```
 *
 * ### For Automotive (ASIL D)
 *
 * ```c
 * // 2oo3 with mixed transports (fast + reliable)
 * config = {
 *     .voting_strategy = RTE_VOTING_2OO3,
 *     .channel_timeout_ms = 100,   // Tighter timeout for vehicles
 *     .log_disagreements = true,
 *     .backend_send = hybrid_send,  // Supports both shared-mem + TCP
 *     .backend_recv = hybrid_recv,
 * };
 * // Channels: Shared-mem (local), TCP (LAN backup), UDP (cellular fallback)
 * ```
 *
 * ### For High-Availability Web Service
 *
 * ```c
 * // 2oo2 across data centers (accept higher latency)
 * config = {
 *     .voting_strategy = RTE_VOTING_2OO2,
 *     .channel_timeout_ms = 2000,   // Allow regional latency (100ms+ RTT)
 *     .log_disagreements = false,   // Log volume too high in production
 *     .backend_send = tcp_send,
 *     .backend_recv = tcp_recv,
 * };
 * // Channels: TCP to US datacenter + TCP to EU datacenter
 * ```
 *
 * @section ipc_transport_selection_implementation_checklist Implementation Checklist
 *
 * - [ ] Choose transport for your use case (shared memory vs network)
 * - [ ] Decide redundancy pattern (2oo2 vs 2oo3 vs NMR)
 * - [ ] Implement OSAdapter callbacks (backend_send/backend_recv)
 * - [ ] Create transport-specific channels (2 or 3)
 * - [ ] Configure vital_channel with callbacks and channels
 * - [ ] Use rte_channel_send/receive for voting communication
 * - [ ] Monitor health via rte_channel_get_health()
 * - [ ] Implement watchdog for health-based channel isolation
 * - [ ] Test disagreement scenarios (simulate channel failures)
 * - [ ] Verify safe-state triggers on voting failure
 *
 */

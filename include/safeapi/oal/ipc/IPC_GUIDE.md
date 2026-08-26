/**
 * @page ipc_guide IPC (Inter-Process Communication) Guide
 *
 * @section ipc_guide_overview Overview
 *
 * The IPC module provides transport-agnostic inter-process/inter-task communication
 * with support for multiple backend implementations (Shared Memory, FIFO, TCP/IP, UDP).
 *
 * **Documents in this directory:**
 *
 * - @ref ipc_transport_selection - Complete transport selection guide
 *   - Shared Memory, FIFO, TCP/IP, UDP characteristics
 *   - POSIX vs RTOS backends
 *   - Decision tree for choosing transports
 *
 * - @ref channel_configuration - Configuration framework for users
 *   - Configuration structures for each transport type
 *   - How to specify IP addresses, ports, paths
 *   - Dispatcher callback pattern for mixed transports
 *   - Online/Standby examples with TCP/IP
 *
 * - @ref sapi_ipc_request_reply.h - Request-Reply pattern (RPC-style)
 *   - Client sends request, server replies
 *   - Deadlock-free with timeouts
 *   - Typical: Train controller queries signal database
 *
 * - @ref sapi_ipc_pubsub.h - Publish-Subscribe pattern (broadcast)
 *   - One publisher sends to multiple subscribers
 *   - Asynchronous, decoupled communication
 *   - Typical: Track database broadcasts state changes
 *
 * - @ref sapi_ipc.h - Base IPC API
 *   - Backend vtable abstraction
 *   - OS-agnostic interface
 *   - Users can register custom backends
 *
 * @section ipc_guide_layers IPC Architecture Layers
 *
 * ```
 * Application Layer (vital_channel, app manager)
 *     │
 *     v
 * IPC Pattern Layer (Request-Reply, Pub-Sub)
 *     │
 *     v
 * IPC Transport Abstraction (vtable)
 *     │
 *     v
 * OS-Specific Implementations
 *     ├─ POSIX: mmap(), mkfifo(), sockets
 *     ├─ QNX: MsgPass, shared memory
 *     ├─ RTOS: Native messaging, memory mapping
 *     └─ Custom: User-provided backends
 * ```
 *
 * @section ipc_guide_workflow Typical Workflow for Online/Standby Setup
 *
 * **Step 1: Choose Transports** (See @ref ipc_transport_selection)
 * ```
 * For online/standby over network:
 * → TCP/IP (reliable, ordered) + UDP (fast, best-effort)
 *
 * For same-board redundancy:
 * → Shared Memory (ultra-low latency) + FIFO (fallback)
 * ```
 *
 * **Step 2: Configure Channels** (See @ref channel_configuration)
 * ```c
 * sapi_ipc_config_tcp_t tcp_cfg = {
 *     .remote_ip = "192.168.1.100",    // User provides standby IP
 *     .remote_port = 5000,             // User chooses port
 *     .timeout_ms = 1000,
 * };
 *
 * sapi_ipc_handle_t tcp_channel;
 * sapi_ipc_create_tcp(&tcp_channel, &tcp_cfg);  // OS backend implements
 * ```
 *
 * **Step 3: Wrap in Vital Channel** (See @ref vital_channel_architecture)
 * ```c
 * sapi_channel_config_t vital_cfg = {
 *     .voting_strategy = SAPI_VOTING_2OO2,
 *     .backend_send = your_dispatcher_send,
 *     .backend_recv = your_dispatcher_recv,
 * };
 *
 * void *channels[2] = { &tcp_channel, &udp_channel };
 * sapi_channel_init(&vital, &vital_cfg, channels, 2);
 * ```
 *
 * **Step 4: Use in App Manager** (See @ref system_architecture)
 * ```c
 * // Main loop: read vital, process, send vital
 * while (running) {
 *     vital_channel_recv(&vital, &cmd, timeout=100ms);
 *     process_command(&cmd);
 *     vital_channel_send(&vital, &output);
 * }
 * ```
 *
 * @section ipc_guide_design_principles Design Principles
 *
 * **1. Transport Independence**
 * - Framework doesn't care what transport you use (SHM, TCP, UDP, FIFO)
 * - User chooses and configures transports
 * - OS integrator implements actual I/O
 * - Application code unchanged regardless of transport
 *
 * **2. Configuration, Not Coupling**
 * - Framework provides configuration structures
 * - User fills in IP addresses, ports, paths
 * - No hardcoding of transport mechanisms
 * - Easy to switch transports at runtime (if needed)
 *
 * **3. Extensibility**
 * - Backend vtable pattern allows new transport types
 * - Just add new configuration structure
 * - Dispatcher routes to correct backend
 * - Existing code unaffected
 *
 * **4. Safety-Critical**
 * - Vital channels require 2oo2/2oo3 voting
 * - Disagreement triggers safe-state automatically
 * - Health tracking per channel
 * - Deterministic timeouts (no indefinite waits)
 *
 * @section ipc_guide_examples Quick Examples
 *
 * **Example 1: TCP/IP Online → Standby**
 * ```c
 * // See CHANNEL_CONFIGURATION.md for complete example
 * tcp_config.remote_ip = "192.168.1.100";
 * tcp_config.remote_port = 5000;
 * ```
 *
 * **Example 2: Local Shared Memory**
 * ```c
 * // See CHANNEL_CONFIGURATION.md
 * shm_config.descriptor_path = "/dev/shm/rail_vital_ab";
 * ```
 *
 * **Example 3: Mixed Transports (Dispatcher)**
 * ```c
 * // See examples/channel_configuration_example.c
 * if (is_tcp_channel) return tcp_send(...);
 * if (is_udp_channel) return udp_send(...);
 * if (is_shm_channel) return shm_send(...);
 * ```
 *
 * @section ipc_guide_next_steps Next Steps
 *
 * 1. Read @ref ipc_transport_selection to understand transport options
 * 2. Read @ref channel_configuration to learn configuration
 * 3. Check examples/channel_configuration_example.c for working code
 * 4. Implement OS-specific backends (sapi_ipc_create_tcp, etc.)
 * 5. Integrate with vital_channel for voting/redundancy
 * 6. Review @ref system_architecture for complete system design
 *
 * @see vital_channel_architecture for voting and redundancy
 * @see system_architecture for overall system design
 *
 */

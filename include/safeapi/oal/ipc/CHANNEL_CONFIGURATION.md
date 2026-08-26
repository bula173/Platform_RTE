/**
 * @page channel_configuration Channel Configuration Guide
 *
 * @section channel_configuration_overview Channel Configuration Pattern
 *
 * The framework provides configuration structures for each channel type.
 * Users fill in these configurations with OS-specific parameters, then
 * the framework creates channels and integrates them with vital_channel.
 *
 * **Key Principle:** Framework defines the interface, user provides the parameters.
 *
 * @section channel_configuration_channel_types Supported Channel Types
 *
 * ### 1. Shared Memory Channels
 *
 * **Use:** Ultra-low latency, same CPU/board
 *
 * **OS-Specific Implementation Areas:**
 * - POSIX: `mmap()` to shared memory region
 * - QNX: `mmap()` or QNX shared memory
 * - Linux: `memfd_create()` or `/dev/shm` files
 * - RTOS: Memory mapping or inter-task memory
 *
 * **Configuration Structure:**
 * ```c
 * typedef struct {
 *     const char *name;              // Diagnostic name
 *     const char *descriptor_path;   // OS-specific path
 *                                    // POSIX: "/dev/shm/my_channel"
 *                                    // Linux: "/tmp/my_channel_shm"
 *     size_t message_size;           // Size of each message
 *     size_t queue_depth;            // How many messages to queue
 *     uint32_t timeout_ms;           // Max wait time
 * } sapi_ipc_config_shm_t;
 * ```
 *
 * **User Configuration Example:**
 * ```c
 * sapi_ipc_config_shm_t shm_config = {
 *     .name = "vital_A_to_B",
 *     .descriptor_path = "/dev/shm/rail_vital_ab",  // User provides path
 *     .message_size = sizeof(train_command_t),       // 256 bytes
 *     .queue_depth = 10,                             // Buffer 10 messages
 *     .timeout_ms = 100,                             // 100ms timeout
 * };
 *
 * sapi_ipc_handle_t shm_channel;
 * sapi_ipc_create_shm(&shm_channel, &shm_config);   // OS backend implements this
 * ```
 *
 * **Implementation by OS Integrator (NOT in framework):**
 * ```c
 * // POSIX backend implementation (example, not in framework)
 * sapi_status_t sapi_ipc_create_shm(sapi_ipc_handle_t *handle,
 *                                   const sapi_ipc_config_shm_t *config)
 * {
 *     // Create or open shared memory region
 *     int fd = shm_open(config->descriptor_path, O_CREAT | O_RDWR, 0666);
 *     if (fd < 0) {
 *         return SAPI_STATUS_HARDWARE_FAULT;
 *     }
 *
 *     // Set size and map
 *     size_t total_size = config->queue_depth * config->message_size;
 *     ftruncate(fd, total_size);
 *
 *     void *mapped = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
 *                        MAP_SHARED, fd, 0);
 *
 *     // Initialize queue structure and mutex
 *     shm_queue_t *q = (shm_queue_t *)mapped;
 *     pthread_mutexattr_t attr;
 *     pthread_mutexattr_init(&attr);
 *     pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
 *     pthread_mutex_init(&q->mutex, &attr);
 *
 *     *handle = (sapi_ipc_handle_t)q;
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * ### 2. FIFO (Named Pipes) Channels
 *
 * **Use:** Process isolation on same system, fallback transport
 *
 * **OS-Specific Implementation Areas:**
 * - POSIX: `mkfifo()` and `open()`
 * - Linux: /proc filesystem for FIFOs
 * - Windows: Named pipes (`CreateNamedPipe()`)
 * - RTOS: Message queues (if no FIFO support)
 *
 * **Configuration Structure:**
 * ```c
 * typedef struct {
 *     const char *name;              // Diagnostic name
 *     const char *fifo_path;         // Full path to FIFO
 *                                    // POSIX: "/tmp/myfifo"
 *                                    // Windows: "\\\\.\\pipe\\myfifo"
 *     size_t message_size;           // Size of each message
 *     uint32_t timeout_ms;           // Max wait time
 *     bool blocking;                 // Blocking or non-blocking mode
 * } sapi_ipc_config_fifo_t;
 * ```
 *
 * **User Configuration Example:**
 * ```c
 * sapi_ipc_config_fifo_t fifo_config = {
 *     .name = "vital_A_to_B_fallback",
 *     .fifo_path = "/tmp/rail_vital_ab_fifo",  // User provides path
 *     .message_size = sizeof(train_command_t),
 *     .timeout_ms = 500,                       // Longer timeout for FIFO
 *     .blocking = true,                        // Wait for data
 * };
 *
 * sapi_ipc_handle_t fifo_channel;
 * sapi_ipc_create_fifo(&fifo_channel, &fifo_config);  // OS backend implements
 * ```
 *
 * **Implementation by OS Integrator (example):**
 * ```c
 * // POSIX backend
 * sapi_status_t sapi_ipc_create_fifo(sapi_ipc_handle_t *handle,
 *                                    const sapi_ipc_config_fifo_t *config)
 * {
 *     // Create FIFO if it doesn't exist
 *     if (access(config->fifo_path, F_OK) != 0) {
 *         if (mkfifo(config->fifo_path, 0666) < 0) {
 *             return SAPI_STATUS_HARDWARE_FAULT;
 *         }
 *     }
 *
 *     // Open FIFO
 *     int flags = config->blocking ? 0 : O_NONBLOCK;
 *     int fd = open(config->fifo_path, O_RDWR | flags);
 *
 *     if (fd < 0) {
 *         return SAPI_STATUS_HARDWARE_FAULT;
 *     }
 *
 *     fifo_channel_t *ch = malloc(sizeof(fifo_channel_t));
 *     ch->fd = fd;
 *     ch->message_size = config->message_size;
 *     ch->timeout_ms = config->timeout_ms;
 *
 *     *handle = (sapi_ipc_handle_t)ch;
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * ### 3. TCP/IP Channels
 *
 * **Use:** Remote standby across network, reliable ordered delivery
 *
 * **OS-Specific Implementation Areas:**
 * - POSIX/Linux: BSD sockets (`socket()`, `connect()`, `send()`)
 * - Windows: Winsock2
 * - RTOS: Native TCP/IP stack or lwIP
 *
 * **Configuration Structure:**
 * ```c
 * typedef struct {
 *     const char *name;              // Diagnostic name
 *     const char *remote_ip;         // IP address of remote host
 *                                    // e.g., "192.168.1.100" or "standby.example.com"
 *     uint16_t remote_port;          // Port number
 *                                    // e.g., 5000, 8080
 *     size_t message_size;           // Max message size
 *     uint32_t timeout_ms;           // Connect/send/receive timeout
 *     bool server_mode;              // true = listen, false = connect
 * } sapi_ipc_config_tcp_t;
 * ```
 *
 * **User Configuration Example (Client/Connect):**
 * ```c
 * sapi_ipc_config_tcp_t tcp_config = {
 *     .name = "online_to_standby",
 *     .remote_ip = "192.168.1.100",          // User provides standby IP
 *     .remote_port = 5000,                   // User chooses port
 *     .message_size = sizeof(train_command_t),
 *     .timeout_ms = 1000,                    // 1s timeout for network
 *     .server_mode = false,                  // Connect to remote
 * };
 *
 * sapi_ipc_handle_t tcp_channel;
 * sapi_ipc_create_tcp(&tcp_channel, &tcp_config);  // OS backend implements
 * ```
 *
 * **User Configuration Example (Server/Listen):**
 * ```c
 * sapi_ipc_config_tcp_t tcp_server_config = {
 *     .name = "standby_listen",
 *     .remote_ip = "0.0.0.0",                // Listen on all interfaces
 *     .remote_port = 5000,                   // User chooses port
 *     .message_size = sizeof(train_command_t),
 *     .timeout_ms = 1000,
 *     .server_mode = true,                   // Listen mode
 * };
 *
 * sapi_ipc_handle_t tcp_server;
 * sapi_ipc_create_tcp(&tcp_server, &tcp_server_config);
 * ```
 *
 * **Implementation by OS Integrator (example):**
 * ```c
 * // POSIX/Linux backend
 * sapi_status_t sapi_ipc_create_tcp(sapi_ipc_handle_t *handle,
 *                                   const sapi_ipc_config_tcp_t *config)
 * {
 *     int sock = socket(AF_INET, SOCK_STREAM, 0);
 *     if (sock < 0) {
 *         return SAPI_STATUS_HARDWARE_FAULT;
 *     }
 *
 *     struct sockaddr_in addr = {
 *         .sin_family = AF_INET,
 *         .sin_port = htons(config->remote_port),
 *     };
 *
 *     if (inet_pton(AF_INET, config->remote_ip, &addr.sin_addr) <= 0) {
 *         close(sock);
 *         return SAPI_STATUS_INVALID_PARAM;
 *     }
 *
 *     if (config->server_mode) {
 *         // Server: bind and listen
 *         if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
 *             return SAPI_STATUS_HARDWARE_FAULT;
 *         }
 *         listen(sock, 1);
 *     } else {
 *         // Client: connect
 *         if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
 *             return SAPI_STATUS_HARDWARE_FAULT;
 *         }
 *     }
 *
 *     tcp_channel_t *ch = malloc(sizeof(tcp_channel_t));
 *     ch->socket = sock;
 *     ch->timeout_ms = config->timeout_ms;
 *
 *     *handle = (sapi_ipc_handle_t)ch;
 *     return SAPI_STATUS_OK;
 * }
 * ```
 *
 * ### 4. UDP Channels
 *
 * **Use:** Low-latency network communication, best-effort
 *
 * **Configuration Structure:**
 * ```c
 * typedef struct {
 *     const char *name;              // Diagnostic name
 *     const char *local_ip;          // Bind to this IP (receiver)
 *                                    // "0.0.0.0" = any interface
 *     uint16_t local_port;           // Local port to bind
 *     const char *remote_ip;         // Send to this IP
 *     uint16_t remote_port;          // Send to this port
 *     size_t message_size;           // Max datagram size
 *     uint32_t timeout_ms;           // Non-blocking typically
 * } sapi_ipc_config_udp_t;
 * ```
 *
 * **User Configuration Example:**
 * ```c
 * sapi_ipc_config_udp_t udp_config = {
 *     .name = "fast_heartbeat",
 *     .local_ip = "192.168.1.50",            // This host's IP
 *     .local_port = 5001,                    // Listen on this port
 *     .remote_ip = "192.168.1.100",          // Send to standby
 *     .remote_port = 5001,                   // Standby's port
 *     .message_size = sizeof(heartbeat_t),
 *     .timeout_ms = 0,                       // Non-blocking
 * };
 *
 * sapi_ipc_handle_t udp_channel;
 * sapi_ipc_create_udp(&udp_channel, &udp_config);
 * ```
 *
 * @section channel_configuration_channel_creation Creating Channels for Vital Communication
 *
 * ### Example: Online/Standby via TCP/IP
 *
 * **Online Host Configuration:**
 * ```c
 * // Primary channel: TCP to standby (reliable)
 * sapi_ipc_config_tcp_t tcp_config = {
 *     .name = "online_to_standby_tcp",
 *     .remote_ip = "192.168.1.100",        // Standby IP (user provides)
 *     .remote_port = 5000,                 // Standby port (user chooses)
 *     .message_size = 256,
 *     .timeout_ms = 1000,
 *     .server_mode = false,                // Connect (client)
 * };
 *
 * // Secondary channel: UDP (fast backup)
 * sapi_ipc_config_udp_t udp_config = {
 *     .name = "online_to_standby_udp",
 *     .local_ip = "192.168.1.50",          // Online IP (user provides)
 *     .local_port = 5001,
 *     .remote_ip = "192.168.1.100",        // Standby IP
 *     .remote_port = 5001,
 *     .message_size = 256,
 *     .timeout_ms = 500,
 * };
 *
 * // Create channels
 * sapi_ipc_handle_t tcp_ch, udp_ch;
 * sapi_ipc_create_tcp(&tcp_ch, &tcp_config);
 * sapi_ipc_create_udp(&udp_ch, &udp_config);
 *
 * // Wrap in vital channel for 2oo2 voting
 * sapi_channel_config_t vital_cfg = {
 *     .voting_strategy = SAPI_VOTING_2OO2,
 *     .channel_timeout_ms = 1000,
 *     .log_disagreements = true,
 *     .backend_send = tcp_udp_dispatch_send,    // Your dispatcher
 *     .backend_recv = tcp_udp_dispatch_recv,
 * };
 *
 * void *channels[2] = { &tcp_ch, &udp_ch };
 * sapi_channel_init(&vital, &vital_cfg, channels, 2);
 *
 * // Now ready for voting communication!
 * ```
 *
 * **Standby Host Configuration:**
 * ```c
 * // Listen for online connections
 * sapi_ipc_config_tcp_t tcp_server_config = {
 *     .name = "standby_listen_tcp",
 *     .remote_ip = "0.0.0.0",              // Listen on all interfaces
 *     .remote_port = 5000,                 // Must match online's config
 *     .message_size = 256,
 *     .timeout_ms = 1000,
 *     .server_mode = true,                 // Listen (server)
 * };
 *
 * // Listen for UDP
 * sapi_ipc_config_udp_t udp_server_config = {
 *     .name = "standby_listen_udp",
 *     .local_ip = "192.168.1.100",         // Standby IP (user provides)
 *     .local_port = 5001,
 *     .remote_ip = "192.168.1.50",         // Send ACK back to online
 *     .remote_port = 5001,
 *     .message_size = 256,
 *     .timeout_ms = 500,
 * };
 *
 * // Create channels (same as online)
 * sapi_ipc_handle_t tcp_ch, udp_ch;
 * sapi_ipc_create_tcp(&tcp_ch, &tcp_server_config);
 * sapi_ipc_create_udp(&udp_ch, &udp_server_config);
 *
 * // Same vital channel config (but roles reversed in logic)
 * sapi_channel_init(&vital, &vital_cfg, channels, 2);
 * ```
 *
 * @section channel_configuration_dispatcher_callback Dispatcher Callback Pattern
 *
 * Since users might mix different transports, they implement a dispatcher:
 *
 * ```c
 * // Identify channel type (add to handle or use wrapper)
 * typedef enum {
 *     CHANNEL_TYPE_TCP,
 *     CHANNEL_TYPE_UDP,
 *     CHANNEL_TYPE_SHM,
 *     CHANNEL_TYPE_FIFO,
 * } channel_type_t;
 *
 * typedef struct {
 *     channel_type_t type;
 *     union {
 *         sapi_ipc_handle_t tcp;
 *         sapi_ipc_handle_t udp;
 *         sapi_ipc_handle_t shm;
 *         sapi_ipc_handle_t fifo;
 *     } handle;
 * } channel_wrapper_t;
 *
 * // Dispatcher for send (vital_channel will call this)
 * sapi_status_t app_backend_send(void *ch, const void *data, size_t size) {
 *     channel_wrapper_t *wrapper = (channel_wrapper_t *)ch;
 *
 *     switch (wrapper->type) {
 *     case CHANNEL_TYPE_TCP:
 *         return sapi_ipc_send_tcp(wrapper->handle.tcp, data, size, 1000);
 *     case CHANNEL_TYPE_UDP:
 *         return sapi_ipc_send_udp(wrapper->handle.udp, data, size, 500);
 *     case CHANNEL_TYPE_SHM:
 *         return sapi_ipc_send_shm(wrapper->handle.shm, data, size, 100);
 *     case CHANNEL_TYPE_FIFO:
 *         return sapi_ipc_send_fifo(wrapper->handle.fifo, data, size, 500);
 *     default:
 *         return SAPI_STATUS_INVALID_PARAM;
 *     }
 * }
 *
 * // Same pattern for receive
 * sapi_status_t app_backend_recv(void *ch, void *data, size_t size, uint32_t timeout) {
 *     channel_wrapper_t *wrapper = (channel_wrapper_t *)ch;
 *
 *     switch (wrapper->type) {
 *     case CHANNEL_TYPE_TCP:
 *         return sapi_ipc_recv_tcp(wrapper->handle.tcp, data, size, timeout);
 *     case CHANNEL_TYPE_UDP:
 *         return sapi_ipc_recv_udp(wrapper->handle.udp, data, size, timeout);
 *     // ... etc
 *     }
 * }
 * ```
 *
 * @section channel_configuration_configuration_summary Configuration Parameter Summary
 *
 * **Shared Memory:**
 * - `descriptor_path`: Where to create shared memory (user specifies)
 * - `message_size`: Fixed message size (user specifies)
 * - `queue_depth`: How many messages to buffer (user specifies)
 * - `timeout_ms`: Wait timeout (user specifies)
 *
 * **FIFO:**
 * - `fifo_path`: Path to FIFO file (user specifies)
 * - `message_size`: Fixed message size (user specifies)
 * - `timeout_ms`: Wait timeout (user specifies)
 * - `blocking`: true/false (user specifies)
 *
 * **TCP/IP:**
 * - `remote_ip`: Destination IP address (user specifies)
 * - `remote_port`: Destination port (user specifies)
 * - `message_size`: Max message size (user specifies)
 * - `timeout_ms`: Connect/send/receive timeout (user specifies)
 * - `server_mode`: Listen (true) or connect (false) (user specifies)
 *
 * **UDP:**
 * - `local_ip`: Bind address (user specifies)
 * - `local_port`: Bind port (user specifies)
 * - `remote_ip`: Send-to address (user specifies)
 * - `remote_port`: Send-to port (user specifies)
 * - `message_size`: Max datagram size (user specifies)
 * - `timeout_ms`: Non-blocking if 0 (user specifies)
 *
 * @section channel_configuration_implementation_by_integrator Implementation Responsibility
 *
 * **Framework provides:**
 * - Configuration structures (what to configure)
 * - Vital channel voting layer
 * - Example dispatcher patterns
 * - Interface definitions (what to implement)
 *
 * **OS Integrator implements:**
 * - `sapi_ipc_create_shm()` - Create shared memory channel
 * - `sapi_ipc_create_fifo()` - Create FIFO channel
 * - `sapi_ipc_create_tcp()` - Create TCP channel
 * - `sapi_ipc_create_udp()` - Create UDP channel
 * - `sapi_ipc_send_*()` and `sapi_ipc_recv_*()` - I/O operations
 * - Backend dispatcher callbacks (optional, user can write)
 *
 * **User/Integrator provides:**
 * - Configuration values (IPs, ports, paths)
 * - Dispatcher implementation (which channel for which transport)
 * - Channel lifecycle (create, use, destroy)
 *
 * This separation ensures:
 * - ✓ Framework stays OS-agnostic
 * - ✓ Framework stays transport-agnostic
 * - ✓ Users have full control over configuration
 * - ✓ Easy to adapt to new OS/transports
 *
 */

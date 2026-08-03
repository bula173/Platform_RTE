/**
 * @page vital_channel_architecture Vital Channel Communication Architecture
 *
 * @section overview Overview
 *
 * **Vital Channels** provide redundancy voting for safety-critical inter-process communication.
 * A vital channel wraps multiple underlying transport channels (IPC, shared memory, TCP, etc.)
 * and applies voting logic to ensure data integrity across distributed processes.
 *
 * The key design principle: **Transport Independence**. The vital_channel module does not
 * depend on any specific IPC implementation—it uses transport-agnostic callbacks to invoke
 * send/receive operations on the underlying channels.
 *
 * @section communication_flow Communication Flow (2oo2 Example)
 *
 * In a typical railway system using 2-out-of-2 voting:
 *
 * ```
 * WEST Process A (Vital Compute)
 *     |
 *     v
 * [Vital Channel Layer]
 *     | (send via callbacks)
 *     +---> Channel 0 (IPC or shared memory to Process B)
 *     |
 *     +---> Channel 1 (IPC or shared memory to Process C)
 *
 * WEST Process B (Vital Verify)         WEST Process C (Non-Vital Service)
 *     |                                      |
 *     v                                      v
 * [Receive from Channel 0]            [Receive from Channel 1]
 *     |
 *     v
 * [Vital Channel Voting]
 *     |
 *     +-- If Ch0 == Ch1 -> AGREE, return data
 *     |
 *     +-- If Ch0 != Ch1 -> DISAGREE, trigger SAFE-STATE
 * ```
 *
 * @section transport_backends Transport Backends
 *
 * A vital channel operates on opaque `void*` channel handles. The application provides
 * two callback functions to specify HOW to communicate:
 *
 * - **backend_send()**: Invoked by vital_channel_send() for each redundant channel
 * - **backend_recv()**: Invoked by vital_channel_receive() for each redundant channel
 *
 * These callbacks abstract away the transport mechanism:
 *
 * | Transport | Backend Implementation |
 * |-----------|------------------------|
 * | IPC Request-Reply | Call sapi_ipc_rr_send() / sapi_ipc_rr_receive() |
 * | IPC Pub-Sub | Call sapi_ipc_pubsub_publish() / sapi_ipc_pubsub_receive() |
 * | Shared Memory | Direct memcpy to/from shared memory region |
 * | TCP | Call socket send()/recv() via wrapper |
 * | Custom Transport | User-defined send/recv functions |
 *
 * This design enables a single vital_channel implementation to work with ANY transport.
 *
 * @section send_semantics Send Semantics (Atomic Broadcast)
 *
 * When sapi_vital_channel_send() is called:
 *
 * 1. **Broadcast Phase**: Invoke backend_send() for each redundant channel
 *    - All channels receive the same data
 *    - Ordering is deterministic (channel 0, then 1, then 2, etc.)
 *
 * 2. **Atomicity**: ALL-or-NOTHING guarantee
 *    - If ANY channel fails or times out, return error (partial send never visible)
 *    - If ALL channels succeed, return SAPI_STATUS_OK
 *    - This prevents inconsistent system state (one receiver gets data, another doesn't)
 *
 * 3. **Health Tracking**: Update per-channel send counts and error counters
 *
 * @section receive_semantics Receive Semantics (Voting)
 *
 * When sapi_vital_channel_receive() is called:
 *
 * 1. **Collection Phase**: Invoke backend_recv() for each redundant channel
 *    - Store results in static voting buffers (one per channel)
 *    - Track timeouts and errors per-channel
 *
 * 2. **Voting Phase**: Apply quorum logic
 *    - **2oo2 (both must agree)**: Compare channels 0 and 1. If equal -> AGREED. If different -> DISAGREED.
 *    - **2oo3 (majority wins)**: Find majority among 3 channels. If tie or no majority -> INSUFFICIENT_QUORUM.
 *    - **NMR (N out of M)**: Generalized voting using quorum_size threshold.
 *
 * 3. **Disagreement Handling**
 *    - Log disagreement (if log_disagreements=true)
 *    - Call optional on_disagreement callback
 *    - **CRITICAL**: Trigger SAFE-STATE automatically via SAPI_SAFESTATE macro
 *    - Return SAPI_STATUS_HARDWARE_FAULT to caller
 *
 * 4. **Agreement Handling**
 *    - Copy agreed data to output buffer
 *    - Return SAPI_STATUS_OK
 *
 * @section quorum Quorum Requirements
 *
 * A vital channel requires sufficient healthy channels to achieve quorum:
 *
 * - **2oo2**: Need >= 2 healthy channels (both must participate)
 * - **2oo3**: Need >= 2 healthy channels (tolerate 1 fault)
 * - **NMR**: Need >= quorum_size healthy channels (tolerate M - quorum_size faults)
 *
 * If quorum is lost (e.g., both channels in 2oo2 become unhealthy), all operations
 * return SAPI_STATUS_HARDWARE_FAULT with voting result = SAPI_VOTING_INSUFFICIENT_QUORUM.
 *
 * @section health_tracking Health Monitoring
 *
 * The vital_channel module tracks per-channel metrics:
 *
 * ```c
 * typedef struct {
 *     uint32_t send_count;              // Successful sends
 *     uint32_t send_error_count;        // Failed sends
 *     uint32_t receive_count;           // Successful receives
 *     uint32_t receive_error_count;     // Failed receives (timeout/error)
 *     uint32_t disagreement_count;      // Times this channel disagreed with majority
 *     bool is_healthy;                  // Current health state (true = healthy)
 *     sapi_status_t last_error;         // Last error code
 * } sapi_vital_channel_health_t;
 * ```
 *
 * Applications can query this via sapi_vital_channel_get_health() to:
 * - Detect early signs of channel degradation (rising error counters)
 * - Isolate faulty channels (high disagreement_count)
 * - Implement predictive fault detection (before safe-state is triggered)
 *
 * @section integration Application Integration
 *
 * Typical application flow:
 *
 * ```c
 * // Step 1: Create transport-specific channels (e.g., IPC request-reply)
 * sapi_ipc_rr_server_t ch0, ch1;
 * sapi_ipc_rr_server_create(&ch0, &config);
 * sapi_ipc_rr_server_create(&ch1, &config);
 * void *channels[2] = { &ch0, &ch1 };
 *
 * // Step 2: Define backend callbacks (how to invoke transport operations)
 * sapi_status_t backend_send(void *ch, const void *data, size_t size) {
 *     return sapi_ipc_rr_send((sapi_ipc_rr_server_t*)ch, data, size);
 * }
 *
 * sapi_status_t backend_recv(void *ch, void *data, size_t size, uint32_t timeout) {
 *     return sapi_ipc_rr_receive((sapi_ipc_rr_server_t*)ch, data, size, timeout);
 * }
 *
 * // Step 3: Initialize vital channel with callbacks
 * sapi_vital_channel_config_t vital_config = {
 *     .voting_strategy = SAPI_VOTING_2OO2,
 *     .channel_timeout_ms = 1000,
 *     .log_disagreements = true,
 *     .backend_send = backend_send,      // Transport-specific callback
 *     .backend_recv = backend_recv,      // Transport-specific callback
 * };
 *
 * sapi_vital_channel_storage_t vital;
 * sapi_vital_channel_init(&vital, &vital_config, channels, 2);
 *
 * // Step 4: Use vital channel for redundant communication
 * sapi_vital_channel_send(&vital, &cmd, sizeof(cmd));  // Atomic broadcast
 * sapi_vital_channel_receive(&vital, &result, sizeof(result), NULL, NULL);  // Voting
 *
 * // Step 5: Monitor health (watchdog role)
 * sapi_vital_channel_health_t health;
 * sapi_vital_channel_get_health(&vital, 0, &health);
 * if (health.disagreement_count > THRESHOLD) {
 *     // Early warning: channel drifting, consider isolation
 * }
 * ```
 *
 * @section safety_properties Safety Properties
 *
 * The vital_channel module guarantees (for CENELEC EN 50128 SIL 3/4 contexts):
 *
 * - **Atomicity**: Sends are all-or-nothing; no partial broadcast state
 * - **Voting Integrity**: Disagreements are ALWAYS detected and escalated to safe-state
 * - **Silent Failure Prevention**: Timeouts and errors are tracked; no silent data corruption
 * - **Deterministic**: No dynamic allocation, bounded buffers, time-bounded operations
 * - **MISRA Compliant**: No recursion, no uninitialized variables, explicit casts only
 *
 * @section message_size Message Size Limits
 *
 * The vital_channel implementation uses static voting buffers:
 * ```c
 * #define SAPI_VITAL_CHANNEL_MAX_MESSAGE_SIZE 256
 * ```
 *
 * Messages larger than 256 bytes return SAPI_STATUS_RESOURCE_EXHAUSTED.
 * Applications needing larger messages should:
 * - Use chunked transfer over multiple messages
 * - Or reduce message size by refactoring data structures
 * - Do NOT use dynamic buffering (MISRA violation)
 *
 * @section examples Usage Examples
 *
 * @subsection example_2oo2 2-out-of-2 Voting (Perfect Agreement Required)
 *
 * Use case: Critical signal transmission where BOTH redundant processes must agree.
 * Example: Train speed command from central interlocking to on-board computer.
 *
 * ```c
 * config.voting_strategy = SAPI_VOTING_2OO2;
 * // Both channels must return identical data, or SAFE-STATE triggers
 * ```
 *
 * @subsection example_2oo3 2-out-of-3 Voting (Tolerate 1 Fault)
 *
 * Use case: Three redundant processes; majority vote wins. Tolerate 1 Byzantine fault.
 * Example: Odometer reading from 3 CCTV-based vision systems (different sensors may drift).
 *
 * ```c
 * config.voting_strategy = SAPI_VOTING_2OO3;
 * // If 2 channels agree, that's the result. If all 3 disagree, SAFE-STATE.
 * ```
 *
 * @subsection example_nmr N-out-of-M Voting
 *
 * Use case: Generalized voter for N processes, requires M of N to agree.
 * Example: 5 GNSS receivers; require 3 to agree within epsilon range.
 *
 * ```c
 * config.voting_strategy = SAPI_VOTING_NMR;
 * config.quorum_size = 3;  // At least 3 out of 5 must agree
 * ```
 *
 * @section limitations Known Limitations
 *
 * - **Static Message Size**: 256 byte limit (MISRA-driven, no dynamic allocation)
 * - **No Byzantine Fault Tolerance**: Designed for benign failures (timeout, corruption);
 *   does not defend against malicious/Byzantine channels
 * - **Simple Voting Only**: Exact byte-for-byte comparison; no approximate/fuzzy voting
 * - **No Automatic Channel Isolation**: Health metrics are tracked but automatic isolation
 *   is left to the application/watchdog layer
 *
 */

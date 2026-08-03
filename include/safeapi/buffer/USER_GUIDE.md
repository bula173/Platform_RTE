/**
 * @page buffer_user_guide Buffer Module - User Guide
 *
 * @section overview What is a Buffer?
 *
 * A Buffer is a lightweight, bounds-tracked view over caller-owned storage.
 * You provide the backing storage (static array, stack buffer), and the buffer
 * tracks how much data is currently valid. No allocation, no freeing - just
 * state tracking.
 *
 * Key concept: Buffer is a VIEW, not the owner of storage.
 *
 * @section quick_start Quick Start
 *
 * @subsection qs_storage 1. Allocate Backing Storage
 *
 * @code
 * // Static buffer for 256 bytes
 * static uint8_t buffer_storage[256];
 * @endcode
 *
 * @subsection qs_init 2. Bind Buffer View
 *
 * @code
 * sapi_buffer_t buf;
 *
 * sapi_status_t rc = sapi_buffer_init(&buf, buffer_storage, sizeof(buffer_storage));
 * if (rc != SAPI_STATUS_OK) {
 *     printf("Buffer init failed\n");
 *     return rc;
 * }
 *
 * // buf now has: data=buffer_storage, capacity=256, length=0
 * @endcode
 *
 * @subsection qs_use 3. Use Buffer
 *
 * @code
 * // Write data into storage (caller manages writes to data pointer)
 * memcpy(buf.data, input, input_len);
 *
 * // Mark data as valid
 * sapi_buffer_set_length(&buf, input_len);
 *
 * // Read data
 * printf("Buffer contains %zu bytes\n", buf.length);
 *
 * // Clear for reuse
 * sapi_buffer_clear(&buf);  // Sets length=0, keeps data/capacity
 * @endcode
 *
 * @section data_structures Key Structures
 *
 * @code
 * typedef struct {
 *     void   *data;      // Pointer to backing storage (caller-owned)
 *     size_t  capacity;  // Total bytes available in storage
 *     size_t  length;    // Bytes currently holding valid data
 * } sapi_buffer_t;
 *
 * typedef struct {
 *     const void *data;  // Read-only view of data
 *     size_t      length; // Bytes of valid data
 * } sapi_const_buffer_t;
 * @endcode
 *
 * Invariant: length <= capacity
 *
 * @section examples Practical Examples
 *
 * @subsection example_message Example 1: Fixed-Size Message Buffer
 *
 * @code
 * typedef struct {
 *     uint32_t command_id;
 *     uint32_t seq_num;
 *     uint8_t payload[128];
 * } message_t;
 *
 * message_t msg = {0};
 * sapi_buffer_t buf;
 *
 * // Bind buffer to message payload
 * sapi_buffer_init(&buf, msg.payload, sizeof(msg.payload));
 *
 * // Receive data into payload
 * receive_from_network(&buf.data[0], 64);
 * sapi_buffer_set_length(&buf, 64);
 *
 * // Send message
 * msg.command_id = 1;
 * msg.seq_num = seq++;
 * send_message(&msg);
 *
 * // Clear for next use
 * sapi_buffer_clear(&buf);
 * @endcode
 *
 * @subsection example_ringbuf Example 2: Read-Only View\n *\n * @code
 * sapi_buffer_t mutable_buf = {...};\n *\n * // Create read-only view\ * sapi_const_buffer_t view = {\n *     .data = mutable_buf.data,\n *     .length = mutable_buf.length\n * };\n *\n * // Pass to function that should not modify\n * process_read_only(&view);\n * @endcode\n *\n * @section operations Buffer Operations\n *\n * @subsection op_init Initialize\n *\n * @code\n * sapi_buffer_init(&buf, storage, capacity);\n * @endcode\n *\n * - Sets buf.data = storage\n * - Sets buf.capacity = capacity\n * - Sets buf.length = 0\n * - Returns INVALID_PARAM if storage==NULL or capacity==0\n *\n * @subsection op_write Write Data\n *\n * @code\n * // Caller writes to buf.data\n * memcpy(buf.data + offset, input, size);\n *\n * // Mark data as valid\n * sapi_buffer_set_length(&buf, new_length);\n * @endcode\n *\n * sapi_buffer_set_length() fails if new_length > capacity.\n *\n * @subsection op_read Read Data\n *\n * @code\n * // Caller reads from buf.data\n * size_t n = buf.length;  // How much valid data\n * memcpy(output, buf.data, n);\n * @endcode\n *\n * @subsection op_clear Clear\n *\n * @code\n * sapi_buffer_clear(&buf);  // Sets length=0, keeps capacity/data\n * @endcode\n *\n * @section guidelines Best Practices\n *\n * 1. Caller owns storage lifetime\n *    - Buffer just tracks state\n *    - Storage must outlive buffer\n *    - Static or stack allocation recommended\n *\n * 2. Check capacity before writing\n *    - Caller responsible for bounds checking\n *    - Buffer tracks length, not write protection\n *    - sapi_buffer_set_length() validates\n *\n * 3. Use sapi_buffer_set_length() for bounds checking\n *    - Always call after writing\n *    - Returns error if length > capacity\n *    - Prevents out-of-bounds bugs\n *\n * 4. Use const_buffer_t for read-only access\n *    - Pass to functions that should not modify\n *    - Type-safe read-only contract\n *\n * 5. Clear when done\n *    - Reset length to 0 for reuse\n *    - Resets valid-data range, not storage\n *\n * @section see_also See Also\n *\n * - @ref buffer_architecture for internal design\n * - @ref memory_user_guide for allocation strategies\n *\n */\n

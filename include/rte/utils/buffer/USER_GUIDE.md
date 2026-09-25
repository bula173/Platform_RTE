@page buffer_user_guide Buffer Module - User Guide

@section buffer_user_guide_overview What is a Buffer?

A Buffer is a lightweight, bounds-tracked view over caller-owned storage.
You provide the backing storage (static array, stack buffer), and the buffer
tracks how much data is currently valid. No allocation, no freeing - just
state tracking.

Key concept: Buffer is a VIEW, not the owner of storage.

@section buffer_user_guide_quick_start Quick Start

@subsection buffer_user_guide_qs_storage 1. Allocate Backing Storage

@code
// Static buffer for 256 bytes
static uint8_t buffer_storage[256];
@endcode

@subsection buffer_user_guide_qs_init 2. Bind Buffer View

@code
rte_buffer_t buf;

rte_status_t rc = rte_buffer_init(&buf, buffer_storage, sizeof(buffer_storage));
if (rc != RTE_STATUS_OK) {
    printf("Buffer init failed\n");
    return rc;
}

// buf now has: data=buffer_storage, capacity=256, length=0
@endcode

@subsection buffer_user_guide_qs_use 3. Use Buffer

@code
// Write data into storage (caller manages writes to data pointer)
memcpy(buf.data, input, input_len);

// Mark data as valid
rte_buffer_set_length(&buf, input_len);

// Read data
printf("Buffer contains %zu bytes\n", buf.length);

// Clear for reuse
rte_buffer_clear(&buf);  // Sets length=0, keeps data/capacity
@endcode

@section buffer_user_guide_data_structures Key Structures

@code
typedef struct {
    void   *data;      // Pointer to backing storage (caller-owned)
    size_t  capacity;  // Total bytes available in storage
    size_t  length;    // Bytes currently holding valid data
} rte_buffer_t;

typedef struct {
    const void *data;  // Read-only view of data
    size_t      length; // Bytes of valid data
} rte_const_buffer_t;
@endcode

Invariant: length <= capacity

@section buffer_user_guide_examples Practical Examples

@subsection buffer_user_guide_example_message Example 1: Fixed-Size Message Buffer

@code
typedef struct {
    uint32_t command_id;
    uint32_t seq_num;
    uint8_t payload[128];
} message_t;

message_t msg = {0};
rte_buffer_t buf;

// Bind buffer to message payload
rte_buffer_init(&buf, msg.payload, sizeof(msg.payload));

// Receive data into payload
receive_from_network(&buf.data[0], 64);
rte_buffer_set_length(&buf, 64);

// Send message
msg.command_id = 1;
msg.seq_num = seq++;
send_message(&msg);

// Clear for next use
rte_buffer_clear(&buf);
@endcode

@subsection buffer_user_guide_example_ringbuf Example 2: Read-Only View

@code
rte_buffer_t mutable_buf = {...};

// Create read-only view
rte_const_buffer_t view = {
    .data = mutable_buf.data,
    .length = mutable_buf.length
};

// Pass to function that should not modify
process_read_only(&view);
@endcode

@section buffer_user_guide_operations Buffer Operations

@subsection buffer_user_guide_op_init Initialize

@code
rte_buffer_init(&buf, storage, capacity);
@endcode

- Sets buf.data = storage
- Sets buf.capacity = capacity
- Sets buf.length = 0
- Returns INVALID_PARAM if storage==NULL or capacity==0

@subsection buffer_user_guide_op_write Write Data

@code
// Caller writes to buf.data
memcpy(buf.data + offset, input, size);

// Mark data as valid
rte_buffer_set_length(&buf, new_length);
@endcode

rte_buffer_set_length() fails if new_length > capacity.

@subsection buffer_user_guide_op_read Read Data

@code
// Caller reads from buf.data
size_t n = buf.length;  // How much valid data
memcpy(output, buf.data, n);
@endcode

@subsection buffer_user_guide_op_clear Clear

@code
rte_buffer_clear(&buf);  // Sets length=0, keeps capacity/data
@endcode

@section buffer_user_guide_guidelines Best Practices

1. Caller owns storage lifetime
   - Buffer just tracks state
   - Storage must outlive buffer
   - Static or stack allocation recommended

2. Check capacity before writing
   - Caller responsible for bounds checking
   - Buffer tracks length, not write protection
   - rte_buffer_set_length() validates

3. Use rte_buffer_set_length() for bounds checking
   - Always call after writing
   - Returns error if length > capacity
   - Prevents out-of-bounds bugs

4. Use const_buffer_t for read-only access
   - Pass to functions that should not modify
   - Type-safe read-only contract

5. Clear when done
   - Reset length to 0 for reuse
   - Resets valid-data range, not storage

@section buffer_user_guide_see_also See Also

- @ref buffer_architecture for internal design
- @ref memory_user_guide for allocation strategies

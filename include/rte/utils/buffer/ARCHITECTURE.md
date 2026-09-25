@page buffer_architecture Buffer Module - Architecture

@section buffer_architecture_overview View-Based Buffer Design

Buffer is a lightweight VIEW over caller-owned storage. No allocation,
no freeing - just state tracking: data pointer, capacity, current length.

@section buffer_architecture_api Core API

@code
rte_buffer_init(buf, storage, capacity);    // Bind view to storage
rte_buffer_set_length(buf, new_length);      // Mark data as valid
rte_buffer_clear(buf);                       // Reset length to 0
@endcode

@section buffer_architecture_invariants Invariants

@verbatim
buf.data != NULL           - Pointing to valid storage
buf.capacity > 0           - Has space
buf.length <= buf.capacity - Never overfilled
@endverbatim

@section buffer_architecture_ownership Ownership Model

Caller owns backing storage:
- Must allocate before rte_buffer_init()
- Must keep valid for buffer lifetime
- Caller manages writes to buf.data

Buffer manages:
- Tracking length (valid data)
- Validating length <= capacity

@section buffer_architecture_performance O(1) All Operations

| Operation | Time |
|-----------|------|
| init() | O(1) |
| set_length() | O(1) |
| clear() | O(1) |

No copying, no allocation.

@section buffer_architecture_see_also See Also

- @ref buffer_user_guide for usage patterns
- @ref memory_user_guide for storage allocation

/**
 * @page buffer_architecture Buffer Module - Architecture
 *
 * @section overview View-Based Buffer Design
 *
 * Buffer is a lightweight VIEW over caller-owned storage. No allocation,
 * no freeing - just state tracking: data pointer, capacity, current length.
 *\n * @section api Core API\n *\n * @code\n * sapi_buffer_init(buf, storage, capacity);    // Bind view to storage\n * sapi_buffer_set_length(buf, new_length);      // Mark data as valid\n * sapi_buffer_clear(buf);                       // Reset length to 0\n * @endcode\n *\n * @section invariants Invariants\n *\n * @verbatim\n * buf.data != NULL           - Pointing to valid storage\n * buf.capacity > 0           - Has space\n * buf.length <= buf.capacity - Never overfilled\n * @endverbatim\n *\n * @section ownership Ownership Model\n *\n * Caller owns backing storage:\n * - Must allocate before sapi_buffer_init()\n * - Must keep valid for buffer lifetime\n * - Caller manages writes to buf.data\n *\n * Buffer manages:\n * - Tracking length (valid data)\n * - Validating length <= capacity\n *\n * @section performance O(1) All Operations\n *\n * | Operation | Time |\n * |-----------|------|\n * | init() | O(1) |\n * | set_length() | O(1) |\n * | clear() | O(1) |\n *\n * No copying, no allocation.\n *\n * @section see_also See Also\n *\n * - @ref buffer_user_guide for usage patterns\n * - @ref memory_user_guide for storage allocation\n *\n */\n

/**
 * @page types_user_guide Types Module - User Guide
 *
 * @section overview What is Types?
 *
 * Types module defines fixed-width integer types for safety-critical systems.
 * All framework code uses `uint32_t`, `int16_t`, etc. instead of bare `int` or
 * `long`. This ensures consistent behavior across platforms (8-bit MCU, 32-bit ARM,
 * 64-bit x86).
 *
 * Key principle: Platform-independent sizing guarantees.
 *
 * @section types Available Types
 *
 * @code
 * // Unsigned integers
 * uint8_t   uint16_t   uint32_t   uint64_t
 *
 * // Signed integers  
 * int8_t    int16_t    int32_t    int64_t
 *
 * // Boolean
 * bool      (C11 standard, true/false)
 *
 * // Standard types
 * size_t    (pointer-sized)
 * @endcode
 *
 * @section quick_start Quick Start
 *
 * @subsection qs_include 1. Include Header
 *
 * @code
 * #include "safeapi/types/sapi_types.h"
 * @endcode
 *
 * @subsection qs_use 2. Use Fixed-Width Types
 *
 * @code
 * // BAD: Platform-dependent\n * int counter;          // Could be 16, 32, or 64 bits!\n * unsigned long value;  // Size varies\n *\n * // GOOD: Platform-independent\n * uint32_t counter;     // Always 32 bits\n * uint64_t value;       // Always 64 bits\n * @endcode\n *\n * @section ranges Type Ranges\n *\n * @verbatim\n * uint8_t:   0 to 255\n * uint16_t:  0 to 65,535\n * uint32_t:  0 to 4,294,967,295\n * uint64_t:  0 to (2^64 - 1)\n *\n * int8_t:    -128 to 127\n * int16_t:   -32,768 to 32,767\n * int32_t:   -2,147,483,648 to 2,147,483,647\n * int64_t:   -(2^63) to +(2^63 - 1)\n * @endverbatim\n *\n * @section examples Practical Examples\n *\n * @subsection example_struct Example 1: Type-Safe Structure\n *\n * @code\n * // For wire protocol (network or storage)\n * typedef struct {\n *     uint32_t message_id;      // Always 4 bytes\n *     uint16_t length;          // Always 2 bytes\n *     int16_t temperature;      // -50°C to +50°C\n *     uint8_t flags;            // Bit flags\n * } train_msg_t;\n *\n * // Size is predictable on ALL platforms\n * STATIC_ASSERT(sizeof(train_msg_t) == 9);\n * @endcode\n *\n * @subsection example_counter Example 2: Sized Counters\n *\n * @code\n * uint16_t packet_count;   // 0-65535 packets\n * uint32_t byte_count;     // 0-4B packets\n * uint64_t total_uptime_ms; // Milliseconds since boot\n * @endcode\n *\n * @section guidelines Best Practices\n *\n * 1. Never use bare `int`, `long`, `char`\n *    - Always explicit type: `int32_t`, `uint8_t`, etc.\n *    - Makes size guarantees visible\n *\n * 2. Match type to purpose\n *    - 16-bit counters: values 0-65535?\n *    - 32-bit time: milliseconds for ~49 days?\n *    - Don't over-allocate\n *\n * 3. Use `uint8_t` for bytes\n *    - Not `unsigned char` (confusing)\n *    - Not `int` (wrong size)\n *    - Always `uint8_t` for byte arrays\n *\n * 4. Use `bool` for single flags\n *    - `bool enabled = true;`\n *    - For multiple flags: `uint32_t flags;` with bit manipulation\n *\n * 5. Watch for overflow\n *    - `uint8_t max = 255; max++; // Wraps to 0`\n *    - `int32_t sum = MAX + 1; // Undefined behavior!`\n *    - Use next size up if needed\n *\n * @section see_also See Also\n *\n * - @ref types_architecture for implementation details\n * - @ref cast_user_guide for safe type conversions\n *\n */\n

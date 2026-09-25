@page types_user_guide Types Module - User Guide

@section types_user_guide_overview What is Types?

Types module defines fixed-width integer types for safety-critical systems.
All framework code uses `uint32_t`, `int16_t`, etc. instead of bare `int` or
`long`. This ensures consistent behavior across platforms (8-bit MCU, 32-bit ARM,
64-bit x86).

Key principle: Platform-independent sizing guarantees.

@section types_user_guide_types Available Types

@code
// Unsigned integers
uint8_t   uint16_t   uint32_t   uint64_t

// Signed integers  
int8_t    int16_t    int32_t    int64_t

// Boolean
bool      (C11 standard, true/false)

// Standard types
size_t    (pointer-sized)
@endcode

@section types_user_guide_quick_start Quick Start

@subsection types_user_guide_qs_include 1. Include Header

@code
#include "rte/types/rte_types.h"
@endcode

@subsection types_user_guide_qs_use 2. Use Fixed-Width Types

@code
// BAD: Platform-dependent
int counter;          // Could be 16, 32, or 64 bits!
unsigned long value;  // Size varies

// GOOD: Platform-independent
uint32_t counter;     // Always 32 bits
uint64_t value;       // Always 64 bits
@endcode

@section types_user_guide_ranges Type Ranges

@verbatim
uint8_t:   0 to 255
uint16_t:  0 to 65,535
uint32_t:  0 to 4,294,967,295
uint64_t:  0 to (2^64 - 1)

int8_t:    -128 to 127
int16_t:   -32,768 to 32,767
int32_t:   -2,147,483,648 to 2,147,483,647
int64_t:   -(2^63) to +(2^63 - 1)
@endverbatim

@section types_user_guide_examples Practical Examples

@subsection types_user_guide_example_struct Example 1: Type-Safe Structure

@code
// For wire protocol (network or storage)
typedef struct {
    uint32_t message_id;      // Always 4 bytes
    uint16_t length;          // Always 2 bytes
    int16_t temperature;      // -50°C to +50°C
    uint8_t flags;            // Bit flags
} train_msg_t;

// Size is predictable on ALL platforms
STATIC_ASSERT(sizeof(train_msg_t) == 9);
@endcode

@subsection types_user_guide_example_counter Example 2: Sized Counters

@code
uint16_t packet_count;   // 0-65535 packets
uint32_t byte_count;     // 0-4B packets
uint64_t total_uptime_ms; // Milliseconds since boot
@endcode

@section types_user_guide_guidelines Best Practices

1. Never use bare `int`, `long`, `char`
   - Always explicit type: `int32_t`, `uint8_t`, etc.
   - Makes size guarantees visible

2. Match type to purpose
   - 16-bit counters: values 0-65535?
   - 32-bit time: milliseconds for ~49 days?
   - Don't over-allocate

3. Use `uint8_t` for bytes
   - Not `unsigned char` (confusing)
   - Not `int` (wrong size)
   - Always `uint8_t` for byte arrays

4. Use `bool` for single flags
   - `bool enabled = true;`
   - For multiple flags: `uint32_t flags;` with bit manipulation

5. Watch for overflow
   - `uint8_t max = 255; max++; // Wraps to 0`
   - `int32_t sum = MAX + 1; // Undefined behavior!`
   - Use next size up if needed

@section types_user_guide_see_also See Also

- @ref types_architecture for implementation details
- @ref cast_user_guide for safe type conversions

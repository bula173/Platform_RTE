/**
 * @page types_architecture Types Module - Architecture
 *
 * @section overview C11 stdint.h Foundation
 *
 * Types module is a thin wrapper over C11's `<stdint.h>`. Provides fixed-width\n * integer types guaranteed by C standard: `uint8_t`, `int32_t`, etc. No\n * implementation - just type definitions and static assertions.\n *\n * @section sizes Guaranteed Sizes\n *\n * @code\n * sizeof(uint8_t)  == 1 byte\n * sizeof(uint16_t) == 2 bytes\n * sizeof(uint32_t) == 4 bytes\n * sizeof(uint64_t) == 8 bytes\n * sizeof(bool)     == 1 byte (typically)\n * @endcode\n *\n * Verified by framework at compile-time via `_Static_assert`.\n *\n * @section misra MISRA Compliance\n *\n * ✓ Rule 6.1 - Fixed-width types only\n * ✓ Rule 7.2 - Proper signedness\n * ✓ Rule 8.3 - Type compatible\n *\n * @section see_also See Also\n *\n * - @ref types_user_guide for usage patterns\n *\n */\n

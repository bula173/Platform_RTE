/**
 * @page cast_user_guide Cast Module - User Guide
 *
 * @section cast_user_guide_overview What is the Cast Module?
 *
 * The Cast module provides **checked type conversions** to prevent silent truncation
 * and overflow. Instead of C-style casts (which truncate silently), these functions
 * detect out-of-range values and return status codes.
 *
 * **Key idea:** Know when data is lost. Silent truncation is a security hole.
 *
 * @section cast_user_guide_quick_start Quick Start
 *
 * ```c
 * #include "safeapi/cast/rte_cast.h"
 *
 * // ✗ BAD: Silent truncation
 * int32_t large = 100000;
 * int16_t small = (int16_t)large;  // 100000 % 65536 = 34464 (wrong!)
 *
 * // ✓ GOOD: Checked conversion
 * int32_t large = 100000;
 * int16_t small;
 * rte_status_t rc = rte_cast_int32_to_int16(large, &small);
 * if (rc == RTE_STATUS_INVALID_PARAM) {
 *     log_error("Value %d too large for int16_t", large);
 *     return rc;
 * }
 * // small is guaranteed in range
 * ```
 *
 * @section cast_user_guide_conversions Available Conversions
 *
 * **Upcasting (safe):**
 * - int8 → int16, int32, int64
 * - uint8 → uint16, uint32, uint64
 * - int16 → int32, int64
 * - uint16 → uint32, uint64
 * - etc.
 * - Always safe (no overflow possible)

 * **Downcasting (checked):**
 * - int32 → int16, int8
 * - uint32 → uint16, uint8
 * - Returns INVALID_PARAM if out of range
 * - Value only modified if success
 *
 * **Signed to Unsigned:**
 * - int32 → uint32 (negative → error)
 * - int8 → uint8 (negative → error)
 * - Zero and positive values OK
 *
 * **Floating Point (if supported):**
 * - float → int32 (checks for infinity, NaN)
 * - double → float (checks for overflow)
 * - Non-finite values return error
 *
 * @section cast_user_guide_examples Practical Examples
 *
 * ### Example 1: ADC Reading Conversion
 *
 * ```c
 * uint16_t adc_raw = read_adc();  // 0-4095 (12-bit ADC)
 *
 * // Need to convert to temperature in 0.1°C units
 * // Safe conversion (raw fits in uint16_t)
 * int16_t temp_units;
 * rte_status_t rc = rte_cast_uint16_to_int16(adc_raw, &temp_units);
 *
 * if (rc != RTE_STATUS_OK) {
 *     log_error("ADC value out of range");
 *     RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_SAFE, REASON);
 * }
 * // temp_units is now 0-4095 safely
 * ```
 *
 * ### Example 2: CAN ID Conversion
 *
 * ```c
 * uint32_t can_id = receive_can_id();  // CAN uses 29-bit or 11-bit IDs
 *
 * // CAN extended ID uses only 29 bits, 3 bits reserved
 * if (can_id > 0x1FFFFFFF) {  // 29-bit max
 *     log_error("Invalid CAN ID: 0x%x", can_id);
 *     return RTE_STATUS_INVALID_PARAM;
 * }
 * ```
 *
 * ### Example 3: Pointer to Integer Conversion
 *
 * ```c
 * // When storing pointers as integers (e.g., for debugging)
 * void *ptr = get_pointer();
 * uint32_t addr32;
 *
 * rte_status_t rc = rte_cast_ptr_to_uint32(ptr, &addr32);
 * if (rc != RTE_STATUS_OK) {
 *     log_error("Pointer too large for uint32_t");
 *     // On 64-bit system, address might not fit in 32 bits
 * }
 * ```
 *
 * @section cast_user_guide_guidelines Best Practices
 *
 * 1. **Always Use Checked Casts for Downcasting**
 *    - Never (type)value for size reduction
 *    - Always check with rte_cast_*
 *    - Handle errors, don't ignore them
 *
 * 2. **Upcasting is Safe**
 *    - int8 → int32: safe (no loss)
 *    - Can use direct cast if confident
 *    - Or use cast functions for consistency
 *
 * 3. **Handle Errors Appropriately**
 *    - For safety-critical: trigger safe-state
 *    - For non-critical: log warning
 *    - Always have error path
 *
 * 4. **Document Assumptions**
 *    - Comment why cast is safe
 *    - Document expected ranges
 *    - Help future maintainers
 *
 * @section cast_user_guide_see_also See Also
 *
 * - @ref cast_architecture for internal design
 * - @ref types_user_guide for fixed-width types
 * - @ref status_user_guide for error handling
 *
 */

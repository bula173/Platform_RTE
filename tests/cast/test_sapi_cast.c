/* Boundary-value tests for sapi_cast (ADR-003).
 *
 * The 56 fixed-width x fixed-width cases below are mechanically generated
 * (see the ADR-003 "generation, not hand-authorship" note) to exercise the
 * exact boundary of every checked conversion: the largest/smallest value
 * that still fits (expect SAPI_STATUS_OK, correct value) and the first
 * value that doesn't (expect SAPI_STATUS_VALUE_OUT_OF_RANGE, output left
 * untouched at a sentinel).
 *
 * The 16 size_t pairs are hand-written below the generated block, since
 * size_t's width is platform-dependent and not known at generation time.
 */
#include <assert.h>
#include <stdint.h>
#include "safeapi/utils/cast/sapi_cast.h"

#define SENTINEL_BYTE 0x5A

static void test_fixed_width_pairs(void)
{
    /* sapi_cast_i8_to_i16 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int8_t in = (int8_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(INT8_MIN);
            int8_t in = (int8_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_i8_to_i32 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int8_t in = (int8_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(INT8_MIN);
            int8_t in = (int8_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_i8_to_i64 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int8_t in = (int8_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(INT8_MIN);
            int8_t in = (int8_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_i8_to_u8 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int8_t in = (int8_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int8_t in = (int8_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int8_t in = (int8_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i8_to_u16 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int8_t in = (int8_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int8_t in = (int8_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int8_t in = (int8_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i8_to_u32 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int8_t in = (int8_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int8_t in = (int8_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int8_t in = (int8_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint32_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i8_to_u64 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int8_t in = (int8_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int8_t in = (int8_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int8_t in = (int8_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i8_to_u64(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint64_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i16_to_i8 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int16_t in = (int16_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT8_MAX) + 1);
            int16_t in = (int16_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)(INT8_MIN);
            int16_t in = (int16_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT8_MIN) - 1);
            int16_t in = (int16_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i16_to_i32 */
    {
        {
            int64_t in_wide = (int64_t)(INT16_MAX);
            int16_t in = (int16_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(INT16_MIN);
            int16_t in = (int16_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_i16_to_i64 */
    {
        {
            int64_t in_wide = (int64_t)(INT16_MAX);
            int16_t in = (int16_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(INT16_MIN);
            int16_t in = (int16_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_i16_to_u8 */
    {
        {
            int64_t in_wide = (int64_t)(UINT8_MAX);
            int16_t in = (int16_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(UINT8_MAX) + 1);
            int16_t in = (int16_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)0;
            int16_t in = (int16_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int16_t in = (int16_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i16_to_u16 */
    {
        {
            int64_t in_wide = (int64_t)(INT16_MAX);
            int16_t in = (int16_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int16_t in = (int16_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int16_t in = (int16_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i16_to_u32 */
    {
        {
            int64_t in_wide = (int64_t)(INT16_MAX);
            int16_t in = (int16_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int16_t in = (int16_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int16_t in = (int16_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint32_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i16_to_u64 */
    {
        {
            int64_t in_wide = (int64_t)(INT16_MAX);
            int16_t in = (int16_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int16_t in = (int16_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int16_t in = (int16_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i16_to_u64(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint64_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i32_to_i8 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int32_t in = (int32_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT8_MAX) + 1);
            int32_t in = (int32_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)(INT8_MIN);
            int32_t in = (int32_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT8_MIN) - 1);
            int32_t in = (int32_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i32_to_i16 */
    {
        {
            int64_t in_wide = (int64_t)(INT16_MAX);
            int32_t in = (int32_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT16_MAX) + 1);
            int32_t in = (int32_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int16_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)(INT16_MIN);
            int32_t in = (int32_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT16_MIN) - 1);
            int32_t in = (int32_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int16_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i32_to_i64 */
    {
        {
            int64_t in_wide = (int64_t)(INT32_MAX);
            int32_t in = (int32_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(INT32_MIN);
            int32_t in = (int32_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_i32_to_u8 */
    {
        {
            int64_t in_wide = (int64_t)(UINT8_MAX);
            int32_t in = (int32_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(UINT8_MAX) + 1);
            int32_t in = (int32_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)0;
            int32_t in = (int32_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int32_t in = (int32_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i32_to_u16 */
    {
        {
            int64_t in_wide = (int64_t)(UINT16_MAX);
            int32_t in = (int32_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(UINT16_MAX) + 1);
            int32_t in = (int32_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)0;
            int32_t in = (int32_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int32_t in = (int32_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i32_to_u32 */
    {
        {
            int64_t in_wide = (int64_t)(INT32_MAX);
            int32_t in = (int32_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int32_t in = (int32_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int32_t in = (int32_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint32_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i32_to_u64 */
    {
        {
            int64_t in_wide = (int64_t)(INT32_MAX);
            int32_t in = (int32_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int32_t in = (int32_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int32_t in = (int32_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i32_to_u64(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint64_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i64_to_i8 */
    {
        {
            int64_t in_wide = (int64_t)(INT8_MAX);
            int64_t in = (int64_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT8_MAX) + 1);
            int64_t in = (int64_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)(INT8_MIN);
            int64_t in = (int64_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT8_MIN) - 1);
            int64_t in = (int64_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i64_to_i16 */
    {
        {
            int64_t in_wide = (int64_t)(INT16_MAX);
            int64_t in = (int64_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT16_MAX) + 1);
            int64_t in = (int64_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int16_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)(INT16_MIN);
            int64_t in = (int64_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT16_MIN) - 1);
            int64_t in = (int64_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int16_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i64_to_i32 */
    {
        {
            int64_t in_wide = (int64_t)(INT32_MAX);
            int64_t in = (int64_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT32_MAX) + 1);
            int64_t in = (int64_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int32_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)(INT32_MIN);
            int64_t in = (int64_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(INT32_MIN) - 1);
            int64_t in = (int64_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_i32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int32_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i64_to_u8 */
    {
        {
            int64_t in_wide = (int64_t)(UINT8_MAX);
            int64_t in = (int64_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(UINT8_MAX) + 1);
            int64_t in = (int64_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)0;
            int64_t in = (int64_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int64_t in = (int64_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i64_to_u16 */
    {
        {
            int64_t in_wide = (int64_t)(UINT16_MAX);
            int64_t in = (int64_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(UINT16_MAX) + 1);
            int64_t in = (int64_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)0;
            int64_t in = (int64_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int64_t in = (int64_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i64_to_u32 */
    {
        {
            int64_t in_wide = (int64_t)(UINT32_MAX);
            int64_t in = (int64_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = ((int64_t)(UINT32_MAX) + 1);
            int64_t in = (int64_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint32_t)SENTINEL_BYTE);
        }
        {
            int64_t in_wide = (int64_t)0;
            int64_t in = (int64_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int64_t in = (int64_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint32_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_i64_to_u64 */
    {
        {
            int64_t in_wide = (int64_t)(INT64_MAX);
            int64_t in = (int64_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)0;
            int64_t in = (int64_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            int64_t in_wide = (int64_t)(-1);
            int64_t in = (int64_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_i64_to_u64(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint64_t)SENTINEL_BYTE);
        }
    }

    /* sapi_cast_u8_to_i8 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT8_MAX);
            uint8_t in = (uint8_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT8_MAX) + 1U);
            uint8_t in = (uint8_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint8_t in = (uint8_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u8_to_i16 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint8_t in = (uint8_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint8_t in = (uint8_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u8_to_i32 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint8_t in = (uint8_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint8_t in = (uint8_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u8_to_i64 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint8_t in = (uint8_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint8_t in = (uint8_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u8_to_u16 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint8_t in = (uint8_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint8_t in = (uint8_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u8_to_u32 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint8_t in = (uint8_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint8_t in = (uint8_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u8_to_u64 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint8_t in = (uint8_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint8_t in = (uint8_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u8_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u16_to_i8 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT8_MAX);
            uint16_t in = (uint16_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT8_MAX) + 1U);
            uint16_t in = (uint16_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint16_t in = (uint16_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u16_to_i16 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT16_MAX);
            uint16_t in = (uint16_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT16_MAX) + 1U);
            uint16_t in = (uint16_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int16_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint16_t in = (uint16_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u16_to_i32 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT16_MAX);
            uint16_t in = (uint16_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint16_t in = (uint16_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u16_to_i64 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT16_MAX);
            uint16_t in = (uint16_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint16_t in = (uint16_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u16_to_u8 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint16_t in = (uint16_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(UINT8_MAX) + 1U);
            uint16_t in = (uint16_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint16_t in = (uint16_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u16_to_u32 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT16_MAX);
            uint16_t in = (uint16_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint16_t in = (uint16_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u16_to_u64 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT16_MAX);
            uint16_t in = (uint16_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint16_t in = (uint16_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u16_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u32_to_i8 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT8_MAX);
            uint32_t in = (uint32_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT8_MAX) + 1U);
            uint32_t in = (uint32_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint32_t in = (uint32_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u32_to_i16 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT16_MAX);
            uint32_t in = (uint32_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT16_MAX) + 1U);
            uint32_t in = (uint32_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int16_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint32_t in = (uint32_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u32_to_i32 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT32_MAX);
            uint32_t in = (uint32_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT32_MAX) + 1U);
            uint32_t in = (uint32_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int32_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint32_t in = (uint32_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u32_to_i64 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT32_MAX);
            uint32_t in = (uint32_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint32_t in = (uint32_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u32_to_u8 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint32_t in = (uint32_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(UINT8_MAX) + 1U);
            uint32_t in = (uint32_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint32_t in = (uint32_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u32_to_u16 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT16_MAX);
            uint32_t in = (uint32_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(UINT16_MAX) + 1U);
            uint32_t in = (uint32_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint32_t in = (uint32_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u32_to_u64 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT32_MAX);
            uint32_t in = (uint32_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint32_t in = (uint32_t)in_wide;
            uint64_t out = (uint64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u32_to_u64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u64_to_i8 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT8_MAX);
            uint64_t in = (uint64_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT8_MAX) + 1U);
            uint64_t in = (uint64_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int8_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint64_t in = (uint64_t)in_wide;
            int8_t out = (int8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u64_to_i16 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT16_MAX);
            uint64_t in = (uint64_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT16_MAX) + 1U);
            uint64_t in = (uint64_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int16_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint64_t in = (uint64_t)in_wide;
            int16_t out = (int16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u64_to_i32 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT32_MAX);
            uint64_t in = (uint64_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT32_MAX) + 1U);
            uint64_t in = (uint64_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int32_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint64_t in = (uint64_t)in_wide;
            int32_t out = (int32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u64_to_i64 */
    {
        {
            uint64_t in_wide = (uint64_t)(INT64_MAX);
            uint64_t in = (uint64_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(INT64_MAX) + 1U);
            uint64_t in = (uint64_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i64(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (int64_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)(0);
            uint64_t in = (uint64_t)in_wide;
            int64_t out = (int64_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_i64(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((int64_t)out == (int64_t)in_wide);
        }
    }

    /* sapi_cast_u64_to_u8 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT8_MAX);
            uint64_t in = (uint64_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(UINT8_MAX) + 1U);
            uint64_t in = (uint64_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u8(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint8_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint64_t in = (uint64_t)in_wide;
            uint8_t out = (uint8_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u8(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u64_to_u16 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT16_MAX);
            uint64_t in = (uint64_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(UINT16_MAX) + 1U);
            uint64_t in = (uint64_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u16(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint16_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint64_t in = (uint64_t)in_wide;
            uint16_t out = (uint16_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u16(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }

    /* sapi_cast_u64_to_u32 */
    {
        {
            uint64_t in_wide = (uint64_t)(UINT32_MAX);
            uint64_t in = (uint64_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
        {
            uint64_t in_wide = ((uint64_t)(UINT32_MAX) + 1U);
            uint64_t in = (uint64_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u32(in, &out);
            assert(st == SAPI_STATUS_VALUE_OUT_OF_RANGE);
            assert(out == (uint32_t)SENTINEL_BYTE);
        }
        {
            uint64_t in_wide = (uint64_t)0;
            uint64_t in = (uint64_t)in_wide;
            uint32_t out = (uint32_t)SENTINEL_BYTE;
            sapi_status_t st __attribute__((unused)) = sapi_cast_u64_to_u32(in, &out);
            assert(st == SAPI_STATUS_OK);
            assert((uint64_t)out == (uint64_t)in_wide);
        }
    }
}

static void test_size_t_pairs(void)
{
    /* size_t -> narrower unsigned: boundary at the destination's own max,
     * one past it must fail (meaningful on any host, since UINT8/16/32_MAX
     * are always representable in size_t). */
    {
        uint8_t out8 __attribute__((unused)) = (uint8_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_u8((size_t)UINT8_MAX, &out8) == SAPI_STATUS_OK);
        assert(out8 == UINT8_MAX);
        out8 = (uint8_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_u8((size_t)UINT8_MAX + 1U, &out8) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(out8 == (uint8_t)SENTINEL_BYTE);
    }
    {
        uint16_t out16 __attribute__((unused)) = (uint16_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_u16((size_t)UINT16_MAX, &out16) == SAPI_STATUS_OK);
        assert(out16 == UINT16_MAX);
        out16 = (uint16_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_u16((size_t)UINT16_MAX + 1U, &out16) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(out16 == (uint16_t)SENTINEL_BYTE);
    }
    {
        uint32_t out32 __attribute__((unused)) = (uint32_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_u32((size_t)UINT32_MAX, &out32) == SAPI_STATUS_OK);
        assert(out32 == UINT32_MAX);
        out32 = (uint32_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_u32((size_t)UINT32_MAX + 1U, &out32) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(out32 == (uint32_t)SENTINEL_BYTE);
    }
    /* size_t -> u64: always safe per ADR-003's size_t<=64bit assumption. */
    {
        uint64_t out64 __attribute__((unused)) = 0U;
        assert(sapi_cast_size_to_u64((size_t)42, &out64) == SAPI_STATUS_OK);
        assert(out64 == 42U);
    }
    /* size_t -> signed: boundary at the destination's own max; negative
     * inputs are impossible since size_t is unsigned, so no lower-bound
     * failure case exists for this direction. */
    {
        int8_t outi8 __attribute__((unused)) = (int8_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_i8((size_t)INT8_MAX, &outi8) == SAPI_STATUS_OK);
        assert(outi8 == INT8_MAX);
        outi8 = (int8_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_i8((size_t)INT8_MAX + 1U, &outi8) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outi8 == (int8_t)SENTINEL_BYTE);
    }
    {
        int16_t outi16 __attribute__((unused)) = (int16_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_i16((size_t)INT16_MAX, &outi16) == SAPI_STATUS_OK);
        assert(outi16 == INT16_MAX);
        outi16 = (int16_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_i16((size_t)INT16_MAX + 1U, &outi16) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outi16 == (int16_t)SENTINEL_BYTE);
    }
    {
        int32_t outi32 __attribute__((unused)) = (int32_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_i32((size_t)INT32_MAX, &outi32) == SAPI_STATUS_OK);
        assert(outi32 == INT32_MAX);
        outi32 = (int32_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_i32((size_t)INT32_MAX + 1U, &outi32) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outi32 == (int32_t)SENTINEL_BYTE);
    }
    {
        int64_t outi64 __attribute__((unused)) = (int64_t)SENTINEL_BYTE;
        assert(sapi_cast_size_to_i64((size_t)INT64_MAX, &outi64) == SAPI_STATUS_OK);
        assert(outi64 == INT64_MAX);
        outi64 = (int64_t)SENTINEL_BYTE;
        /* (size_t)INT64_MAX + 1 == 9223372036854775808u, representable in
         * size_t on a 64-bit host and correctly rejected. */
        assert(sapi_cast_size_to_i64((size_t)INT64_MAX + 1U, &outi64) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outi64 == (int64_t)SENTINEL_BYTE);
    }

    /* fixed -> size_t: unsigned sources always succeed (no failure case is
     * host-independent here: on a 64-bit host size_t covers the full
     * unsigned range up to u32; the u64 -> size upper-bound branch is
     * compiled out via #if SIZE_MAX < UINT64_MAX on this host, which is
     * exactly the correct behavior for a 64-bit size_t). */
    {
        size_t outsz __attribute__((unused)) = 0U;
        assert(sapi_cast_u8_to_size(UINT8_MAX, &outsz) == SAPI_STATUS_OK);
        assert(outsz == (size_t)UINT8_MAX);
        assert(sapi_cast_u16_to_size(UINT16_MAX, &outsz) == SAPI_STATUS_OK);
        assert(outsz == (size_t)UINT16_MAX);
        assert(sapi_cast_u32_to_size(UINT32_MAX, &outsz) == SAPI_STATUS_OK);
        assert(outsz == (size_t)UINT32_MAX);
        assert(sapi_cast_u64_to_size(UINT64_MAX, &outsz) == SAPI_STATUS_OK);
        assert(outsz == (size_t)UINT64_MAX);
    }
    /* signed -> size_t: negative values must always be rejected. */
    {
        size_t outsz __attribute__((unused)) = 123U;
        assert(sapi_cast_i8_to_size((int8_t)-1, &outsz) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outsz == 123U);
        assert(sapi_cast_i16_to_size((int16_t)-1, &outsz) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outsz == 123U);
        assert(sapi_cast_i32_to_size((int32_t)-1, &outsz) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outsz == 123U);
        assert(sapi_cast_i64_to_size((int64_t)-1, &outsz) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
        assert(outsz == 123U);
        assert(sapi_cast_i32_to_size(42, &outsz) == SAPI_STATUS_OK);
        assert(outsz == 42U);
        assert(sapi_cast_i8_to_size((int8_t)42, &outsz) == SAPI_STATUS_OK);
        assert(outsz == 42U);
        assert(sapi_cast_i16_to_size((int16_t)42, &outsz) == SAPI_STATUS_OK);
        assert(outsz == 42U);
        assert(sapi_cast_i64_to_size((int64_t)42, &outsz) == SAPI_STATUS_OK);
        assert(outsz == 42U);
    }
}

static void test_null_and_ok_smoke(void)
{
    uint16_t out __attribute__((unused)) = 0U;
    assert(sapi_cast_u32_to_u16(100U, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_u16(100U, &out) == SAPI_STATUS_OK);
    assert(out == 100U);
}

static void test_every_function_rejects_null_out(void)
{
    /* Every one of the 72 sapi_cast_<from>_to_<to> functions shares the
     * identical out == NULL -> SAPI_STATUS_INVALID_PARAM guard as its first
     * check (ADR-003 section 2.1, REQ-COMMON-CAST-002) - one call per
     * function is the minimal way to hit that guard for all 72, rather than
     * relying on it being incidentally exercised by the value-based tests
     * above. */
    assert(sapi_cast_i8_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i8_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i8_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i8_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i8_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i8_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i8_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i8_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i16_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i32_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_i64_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u8_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u16_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u32_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_u64_to_size(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_i8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_i16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_i32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_i64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_u8(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_u16(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_u32(0, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_cast_size_to_u64(0, NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_checked_add_sub_mul_u32(void)
{
    uint32_t out;

    assert(sapi_cast_checked_add_u32(2U, 3U, &out) == SAPI_STATUS_OK);
    assert(out == 5U);
    assert(sapi_cast_checked_add_u32(UINT32_MAX, 1U, &out) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    assert(sapi_cast_checked_add_u32(1U, 1U, NULL) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_cast_checked_sub_u32(5U, 3U, &out) == SAPI_STATUS_OK);
    assert(out == 2U);
    assert(sapi_cast_checked_sub_u32(3U, 5U, &out) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    assert(sapi_cast_checked_sub_u32(1U, 1U, NULL) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_cast_checked_mul_u32(6U, 7U, &out) == SAPI_STATUS_OK);
    assert(out == 42U);
    assert(sapi_cast_checked_mul_u32(UINT32_MAX, 2U, &out) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    assert(sapi_cast_checked_mul_u32(1U, 1U, NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_checked_add_sub_mul_size(void)
{
    size_t out;

    assert(sapi_cast_checked_add_size((size_t)2, (size_t)3, &out) == SAPI_STATUS_OK);
    assert(out == (size_t)5);
    assert(sapi_cast_checked_add_size(SIZE_MAX, (size_t)1, &out) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    assert(sapi_cast_checked_add_size((size_t)1, (size_t)1, NULL) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_cast_checked_sub_size((size_t)5, (size_t)3, &out) == SAPI_STATUS_OK);
    assert(out == (size_t)2);
    assert(sapi_cast_checked_sub_size((size_t)3, (size_t)5, &out) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    assert(sapi_cast_checked_sub_size((size_t)1, (size_t)1, NULL) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_cast_checked_mul_size((size_t)6, (size_t)7, &out) == SAPI_STATUS_OK);
    assert(out == (size_t)42);
    assert(sapi_cast_checked_mul_size(SIZE_MAX, (size_t)2, &out) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    /* a == 0 must not divide-by-zero in the overflow check itself. */
    assert(sapi_cast_checked_mul_size((size_t)0, SIZE_MAX, &out) == SAPI_STATUS_OK);
    assert(out == (size_t)0);
    assert(sapi_cast_checked_mul_size((size_t)1, (size_t)1, NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_bounds_check(void)
{
    assert(sapi_cast_bounds_check((size_t)0, (size_t)4) == SAPI_STATUS_OK);
    assert(sapi_cast_bounds_check((size_t)3, (size_t)4) == SAPI_STATUS_OK);
    assert(sapi_cast_bounds_check((size_t)4, (size_t)4) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    assert(sapi_cast_bounds_check((size_t)0, (size_t)0) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
}

int main(void)
{
    test_fixed_width_pairs();
    test_size_t_pairs();
    test_null_and_ok_smoke();
    test_every_function_rejects_null_out();
    test_checked_add_sub_mul_u32();
    test_checked_add_sub_mul_size();
    test_bounds_check();
    return 0;
}

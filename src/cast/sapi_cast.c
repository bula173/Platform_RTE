/**
 * @file sapi_cast.c
 * @ingroup CAST
 * @brief Generated from ADR-003's template; see sapi_cast.h for behavior.
 *        Every function widens its input to the int64_t/uint64_t matching
 *        the source's signedness, then range-checks against the
 *        destination's limit macros before performing the explicit cast.
 */
#include "safeapi/cast/sapi_cast.h"

sapi_status_t sapi_cast_i8_to_i16(int8_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i8_to_i32(int8_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i8_to_i64(int8_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i8_to_u8(int8_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i8_to_u16(int8_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i8_to_u32(int8_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i8_to_u64(int8_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i8_to_size(int8_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#if SIZE_MAX < UINT64_MAX
    if ((uint64_t)widened > (uint64_t)SIZE_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_i8(int16_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < (int64_t)INT8_MIN)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (widened > (int64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_i32(int16_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_i64(int16_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_u8(int16_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if ((uint64_t)widened > (uint64_t)UINT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_u16(int16_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_u32(int16_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_u64(int16_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i16_to_size(int16_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#if SIZE_MAX < UINT64_MAX
    if ((uint64_t)widened > (uint64_t)SIZE_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_i8(int32_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < (int64_t)INT8_MIN)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (widened > (int64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_i16(int32_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < (int64_t)INT16_MIN)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (widened > (int64_t)INT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_i64(int32_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_u8(int32_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if ((uint64_t)widened > (uint64_t)UINT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_u16(int32_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if ((uint64_t)widened > (uint64_t)UINT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_u32(int32_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_u64(int32_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i32_to_size(int32_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#if SIZE_MAX < UINT64_MAX
    if ((uint64_t)widened > (uint64_t)SIZE_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_i8(int64_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < (int64_t)INT8_MIN)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (widened > (int64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_i16(int64_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < (int64_t)INT16_MIN)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (widened > (int64_t)INT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_i32(int64_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < (int64_t)INT32_MIN)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (widened > (int64_t)INT32_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_u8(int64_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if ((uint64_t)widened > (uint64_t)UINT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_u16(int64_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if ((uint64_t)widened > (uint64_t)UINT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_u32(int64_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if ((uint64_t)widened > (uint64_t)UINT32_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_u64(int64_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_i64_to_size(int64_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    int64_t widened = (int64_t)in;
    if (widened < 0)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#if SIZE_MAX < UINT64_MAX
    if ((uint64_t)widened > (uint64_t)SIZE_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_i8(uint8_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_i16(uint8_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_i32(uint8_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_i64(uint8_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_u16(uint8_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_u32(uint8_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_u64(uint8_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u8_to_size(uint8_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
#if SIZE_MAX < UINT64_MAX
    {
        uint64_t widened = (uint64_t)in;
        if (widened > (uint64_t)SIZE_MAX)
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_i8(uint16_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_i16(uint16_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_i32(uint16_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_i64(uint16_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_u8(uint16_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_u32(uint16_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_u64(uint16_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u16_to_size(uint16_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
#if SIZE_MAX < UINT64_MAX
    {
        uint64_t widened = (uint64_t)in;
        if (widened > (uint64_t)SIZE_MAX)
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_i8(uint32_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_i16(uint32_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_i32(uint32_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT32_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_i64(uint32_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_u8(uint32_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_u16(uint32_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_u64(uint32_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u32_to_size(uint32_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
#if SIZE_MAX < UINT64_MAX
    {
        uint64_t widened = (uint64_t)in;
        if (widened > (uint64_t)SIZE_MAX)
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_i8(uint64_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_i16(uint64_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_i32(uint64_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT32_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_i64(uint64_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT64_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_u8(uint64_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_u16(uint64_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_u32(uint64_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT32_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_u64_to_size(uint64_t in, size_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
#if SIZE_MAX < UINT64_MAX
    {
        uint64_t widened = (uint64_t)in;
        if (widened > (uint64_t)SIZE_MAX)
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
    }
#endif
    *out = (size_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_i8(size_t in, int8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_i16(size_t in, int16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_i32(size_t in, int32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT32_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_i64(size_t in, int64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)INT64_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (int64_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_u8(size_t in, uint8_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT8_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint8_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_u16(size_t in, uint16_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT16_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint16_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_u32(size_t in, uint32_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    uint64_t widened = (uint64_t)in;
    if (widened > (uint64_t)UINT32_MAX)
    {
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out = (uint32_t)in;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cast_size_to_u64(size_t in, uint64_t *out)
{
    if (out == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* size_t is assumed to fit within uint64_t on any supported target (ADR-003). */
    *out = (uint64_t)in;
    return SAPI_STATUS_OK;
}

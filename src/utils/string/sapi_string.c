/**
 * @file sapi_string.c
 * @brief Implementation of the bounded string module (ADR-006).
 * @ingroup STRING
 */
#include "safeapi/utils/string/sapi_string.h"
#include "safeapi/utils/cast/sapi_cast.h"
#include <string.h>

sapi_status_t sapi_string_init(sapi_string_t *str, char *storage, size_t capacity)
{
    if (str == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    return sapi_buffer_init(&str->buf, (void *)storage, capacity);
}

sapi_status_t sapi_string_clear(sapi_string_t *str)
{
    if (str == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    return sapi_buffer_clear(&str->buf);
}

size_t sapi_string_length(const sapi_string_t *str)
{
    if (str == NULL)
    {
        return 0U;
    }
    return str->buf.length;
}

sapi_status_t sapi_string_c_str(sapi_string_t *str, const char **out_cstr)
{
    char *data;

    if ((str == NULL) || (out_cstr == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!sapi_buffer_is_valid(&str->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (str->buf.length >= str->buf.capacity)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    data = (char *)str->buf.data;
    data[str->buf.length] = '\0';
    *out_cstr = (const char *)str->buf.data;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_string_copy_n(sapi_string_t *dest, const char *src, size_t src_len)
{
    if ((dest == NULL) || (src == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    return sapi_buffer_copy_in(&dest->buf, (const void *)src, src_len);
}

sapi_status_t sapi_string_copy(sapi_string_t *dest, const char *src)
{
    size_t max_scan;
    size_t i;
    bool found_nul = false;

    if ((dest == NULL) || (src == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!sapi_buffer_is_valid(&dest->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* A NUL at index == capacity is acceptable (content length == capacity
     * exactly fills dest); scan capacity+1 positions, 0..capacity inclusive. */
    max_scan = dest->buf.capacity;
    for (i = 0U; i <= max_scan; i++)
    {
        if (src[i] == '\0')
        {
            found_nul = true;
            break;
        }
    }
    if (!found_nul)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    return sapi_string_copy_n(dest, src, i);
}

sapi_status_t sapi_string_concat(sapi_string_t *dest, const char *src)
{
    size_t remaining;
    size_t i;
    bool found_nul = false;
    char *data;

    if ((dest == NULL) || (src == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!sapi_buffer_is_valid(&dest->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* Same off-by-one reasoning as sapi_string_copy(): a NUL at
     * index == remaining is acceptable (appended content exactly fills
     * the remaining capacity). */
    remaining = dest->buf.capacity - dest->buf.length;
    for (i = 0U; i <= remaining; i++)
    {
        if (src[i] == '\0')
        {
            found_nul = true;
            break;
        }
    }
    if (!found_nul)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    data = (char *)dest->buf.data;
    if (i > 0U)
    {
        (void)memcpy(&data[dest->buf.length], src, i);
    }
    dest->buf.length += i;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_string_compare(const sapi_string_t *a, const sapi_string_t *b, int32_t *out_cmp)
{
    size_t min_len;
    size_t i;
    int32_t result = 0;
    const unsigned char *pa;
    const unsigned char *pb;

    if ((a == NULL) || (b == NULL) || (out_cmp == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((!sapi_buffer_is_valid(&a->buf)) || (!sapi_buffer_is_valid(&b->buf)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    min_len = (a->buf.length < b->buf.length) ? a->buf.length : b->buf.length;
    pa = (const unsigned char *)a->buf.data;
    pb = (const unsigned char *)b->buf.data;
    for (i = 0U; i < min_len; i++)
    {
        if (pa[i] != pb[i])
        {
            result = (pa[i] < pb[i]) ? -1 : 1;
            break;
        }
    }
    if (result == 0)
    {
        if (a->buf.length < b->buf.length)
        {
            result = -1;
        }
        else if (a->buf.length > b->buf.length)
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }
    *out_cmp = result;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_string_find_char(const sapi_string_t *str, char c,
                                     bool *out_found, size_t *out_index)
{
    const char *data;
    size_t i;

    if ((str == NULL) || (out_found == NULL) || (out_index == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!sapi_buffer_is_valid(&str->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    data = (const char *)str->buf.data;
    for (i = 0U; i < str->buf.length; i++)
    {
        if (data[i] == c)
        {
            *out_found = true;
            *out_index = i;
            return SAPI_STATUS_OK;
        }
    }
    *out_found = false;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_string_find_substr(const sapi_string_t *haystack, const sapi_string_t *needle,
                                       bool *out_found, size_t *out_index)
{
    const char *hdata;
    const char *ndata;
    size_t max_start;
    size_t i;

    if ((haystack == NULL) || (needle == NULL) || (out_found == NULL) || (out_index == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((!sapi_buffer_is_valid(&haystack->buf)) || (!sapi_buffer_is_valid(&needle->buf)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (needle->buf.length == 0U)
    {
        *out_found = true;
        *out_index = 0U;
        return SAPI_STATUS_OK;
    }
    if (needle->buf.length > haystack->buf.length)
    {
        *out_found = false;
        return SAPI_STATUS_OK;
    }
    hdata = (const char *)haystack->buf.data;
    ndata = (const char *)needle->buf.data;
    max_start = haystack->buf.length - needle->buf.length;
    for (i = 0U; i <= max_start; i++)
    {
        if (memcmp(&hdata[i], ndata, needle->buf.length) == 0)
        {
            *out_found = true;
            *out_index = i;
            return SAPI_STATUS_OK;
        }
    }
    *out_found = false;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_string_split_next(const sapi_string_t *str, char delimiter,
                                      size_t *io_cursor,
                                      sapi_const_buffer_t *out_token,
                                      bool *out_has_token)
{
    const char *data;
    size_t start;
    size_t i;

    if ((str == NULL) || (io_cursor == NULL) || (out_token == NULL) || (out_has_token == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!sapi_buffer_is_valid(&str->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (*io_cursor > str->buf.length)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (*io_cursor == str->buf.length)
    {
        *out_has_token = false;
        return SAPI_STATUS_OK;
    }
    data = (const char *)str->buf.data;
    start = *io_cursor;
    i = start;
    while ((i < str->buf.length) && (data[i] != delimiter))
    {
        i++;
    }
    out_token->data = &data[start];
    out_token->length = i - start;
    *out_has_token = true;
    if (i < str->buf.length)
    {
        *io_cursor = i + 1U; /* skip the delimiter */
    }
    else
    {
        *io_cursor = i;
    }
    return SAPI_STATUS_OK;
}

/**
 * @brief Fills out[0..*out_len) with value's base-10 digits,
 *        most-significant first, no leading zeros (except "0" itself).
 * @param value    Value to format.
 * @param out      Destination buffer; must have room for at least 20
 *                 characters (UINT64_MAX has 20 decimal digits).
 * @param out_len  Receives the number of digit characters written.
 */
static void sapi_string_format_u64_digits(uint64_t value, char *out, size_t *out_len)
{
    char rev[20];
    size_t n = 0U;
    uint64_t v = value;

    if (v == 0U)
    {
        out[0] = '0';
        *out_len = 1U;
        return;
    }
    while (v > 0U)
    {
        char digit = (char)(v % 10U);
        rev[n] = (char)('0' + digit);
        n++;
        v /= 10U;
    }
    {
        size_t i;
        for (i = 0U; i < n; i++)
        {
            out[i] = rev[(n - 1U) - i];
        }
    }
    *out_len = n;
}

sapi_status_t sapi_string_from_u64(sapi_string_t *dest, uint64_t value)
{
    char digits[20];
    size_t len = 0U;

    if (dest == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    sapi_string_format_u64_digits(value, digits, &len);
    return sapi_string_copy_n(dest, digits, len);
}

sapi_status_t sapi_string_from_i64(sapi_string_t *dest, int64_t value)
{
    char buf[21]; /* 1 sign byte + up to 20 digits */
    size_t len = 0U;
    uint64_t magnitude;
    bool negative;

    if (dest == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    negative = (value < 0);
    if (negative)
    {
        /* Safe negation avoiding overflow for INT64_MIN: -(value+1) always
         * fits int64_t since value >= INT64_MIN implies value+1 >= INT64_MIN+1. */
        magnitude = (uint64_t)(-(value + 1)) + 1U;
    }
    else
    {
        magnitude = (uint64_t)value;
    }
    if (negative)
    {
        buf[0] = '-';
        sapi_string_format_u64_digits(magnitude, &buf[1], &len);
        return sapi_string_copy_n(dest, buf, len + 1U);
    }
    sapi_string_format_u64_digits(magnitude, buf, &len);
    return sapi_string_copy_n(dest, buf, len);
}

sapi_status_t sapi_string_from_u32(sapi_string_t *dest, uint32_t value)
{
    uint64_t widened = 0U;
    sapi_status_t st;

    if (dest == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_cast_u32_to_u64(value, &widened);
    if (st != SAPI_STATUS_OK) /* GCOVR_EXCL_START - sapi_cast_u32_to_u64() can
                                * only fail for a NULL out pointer, and
                                * &widened is always a valid local address. */
    {
        return st;
    } /* GCOVR_EXCL_STOP */
    return sapi_string_from_u64(dest, widened);
}

sapi_status_t sapi_string_from_i32(sapi_string_t *dest, int32_t value)
{
    int64_t widened = 0;
    sapi_status_t st;

    if (dest == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_cast_i32_to_i64(value, &widened);
    if (st != SAPI_STATUS_OK) /* GCOVR_EXCL_START - sapi_cast_i32_to_i64() can
                                * only fail for a NULL out pointer, and
                                * &widened is always a valid local address. */
    {
        return st;
    } /* GCOVR_EXCL_STOP */
    return sapi_string_from_i64(dest, widened);
}

sapi_status_t sapi_string_to_u64(const sapi_string_t *str, uint64_t *out_value)
{
    size_t i;
    uint64_t acc = 0U;
    const char *data;

    if ((str == NULL) || (out_value == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!sapi_buffer_is_valid(&str->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (str->buf.length == 0U)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    data = (const char *)str->buf.data;
    for (i = 0U; i < str->buf.length; i++)
    {
        char c = data[i];
        uint64_t digit;

        if ((c < '0') || (c > '9'))
        {
            return SAPI_STATUS_INVALID_PARAM;
        }
        digit = (uint64_t)(c - '0');
        if (acc > ((UINT64_MAX - digit) / 10U))
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
        acc = (acc * 10U) + digit;
    }
    *out_value = acc;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_string_to_i64(const sapi_string_t *str, int64_t *out_value)
{
    size_t i;
    size_t start;
    bool negative;
    uint64_t acc = 0U;
    const char *data;

    if ((str == NULL) || (out_value == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!sapi_buffer_is_valid(&str->buf))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (str->buf.length == 0U)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    data = (const char *)str->buf.data;
    negative = false;
    start = 0U;
    if (data[0] == '-')
    {
        negative = true;
        start = 1U;
    }
    if (start >= str->buf.length)
    {
        return SAPI_STATUS_INVALID_PARAM; /* "-" with no digits */
    }
    for (i = start; i < str->buf.length; i++)
    {
        char c = data[i];
        uint64_t digit;

        if ((c < '0') || (c > '9'))
        {
            return SAPI_STATUS_INVALID_PARAM;
        }
        digit = (uint64_t)(c - '0');
        if (acc > ((UINT64_MAX - digit) / 10U))
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
        acc = (acc * 10U) + digit;
    }
    if (negative)
    {
        uint64_t min_magnitude = (uint64_t)INT64_MAX + 1U;
        if (acc > min_magnitude)
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
        if (acc == min_magnitude)
        {
            *out_value = INT64_MIN;
        }
        else
        {
            *out_value = -(int64_t)acc;
        }
    }
    else
    {
        if (acc > (uint64_t)INT64_MAX)
        {
            return SAPI_STATUS_VALUE_OUT_OF_RANGE;
        }
        *out_value = (int64_t)acc;
    }
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_string_to_u32(const sapi_string_t *str, uint32_t *out_value)
{
    uint64_t v = 0U;
    sapi_status_t st;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_string_to_u64(str, &v);
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    return sapi_cast_u64_to_u32(v, out_value);
}

sapi_status_t sapi_string_to_i32(const sapi_string_t *str, int32_t *out_value)
{
    int64_t v = 0;
    sapi_status_t st;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_string_to_i64(str, &v);
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    return sapi_cast_i64_to_i32(v, out_value);
}

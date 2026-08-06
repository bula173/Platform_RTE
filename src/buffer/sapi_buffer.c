/**
 * @file sapi_buffer.c
 * @brief Implementation of the cross-layer data buffer abstraction (ADR-002).
 * @ingroup BUFFER
 */
#include "safeapi/buffer/sapi_buffer.h"
#include <string.h>

sapi_status_t sapi_buffer_init(sapi_buffer_t *buf, void *storage, size_t capacity)
{
    if ((buf == NULL) || (storage == NULL) || (capacity == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    buf->data     = storage;
    buf->capacity = capacity;
    buf->length   = 0U;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_clear(sapi_buffer_t *buf)
{
    if ((buf == NULL) || (!sapi_buffer_is_valid(buf)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    buf->length = 0U;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_set_length(sapi_buffer_t *buf, size_t length)
{
    if ((buf == NULL) || (buf->data == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (length > buf->capacity)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    buf->length = length;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_copy_in(sapi_buffer_t *buf, const void *src, size_t src_len)
{
    if ((buf == NULL) || (buf->data == NULL) || (src == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (src_len > buf->capacity)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    if (src_len > 0U)
    {
        (void)memcpy(buf->data, src, src_len);
    }
    buf->length = src_len;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_copy_out(const sapi_buffer_t *buf,
                                    void *dest,
                                    size_t dest_capacity,
                                    size_t *out_copied)
{
    if ((buf == NULL) || (buf->data == NULL) || (dest == NULL) || (out_copied == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (buf->length > dest_capacity)
    {
        *out_copied = 0U;
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    if (buf->length > 0U)
    {
        (void)memcpy(dest, buf->data, buf->length);
    }
    *out_copied = buf->length;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_as_const(const sapi_buffer_t *buf, sapi_const_buffer_t *out_view)
{
    if ((buf == NULL) || (buf->data == NULL) || (out_view == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (buf->length > buf->capacity)
    {
        return SAPI_STATUS_INTERNAL_ERROR;
    }
    out_view->data   = buf->data;
    out_view->length = buf->length;
    return SAPI_STATUS_OK;
}

bool sapi_buffer_is_valid(const sapi_buffer_t *buf)
{
    if (buf == NULL)
    {
        return false;
    }
    if (buf->data == NULL)
    {
        return false;
    }
    return (buf->length <= buf->capacity);
}

/**
 * @brief Appends n raw bytes at buf's current length, advancing it. Shared
 *        by every sapi_buffer_write_*_le/be function.
 * @param buf    Destination buffer. Must not be NULL.
 * @param bytes  Raw bytes to append. Must not be NULL.
 * @param n      Number of bytes to append.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if buf or
 *         bytes is NULL or buf fails its validity check;
 *         SAPI_STATUS_RESOURCE_EXHAUSTED if n bytes will not fit.
 */
static sapi_status_t sapi_buffer_append_bytes(sapi_buffer_t *buf, const unsigned char *bytes, size_t n)
{
    unsigned char *dest;

    if ((buf == NULL) || (bytes == NULL) || (!sapi_buffer_is_valid(buf)))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((buf->capacity - buf->length) < n)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    dest = (unsigned char *)buf->data;
    (void)memcpy(&dest[buf->length], bytes, n);
    buf->length += n;
    return SAPI_STATUS_OK;
}

/**
 * @brief Reads n raw bytes starting at offset, without mutating buf.
 *        Shared by every sapi_buffer_read_*_le/be function.
 * @param buf        Source buffer. Must not be NULL.
 * @param offset     Byte offset within buf's valid bytes to read from.
 * @param out_bytes  Destination for the raw bytes. Must not be NULL.
 * @param n          Number of bytes to read.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM for a NULL
 *         pointer; SAPI_STATUS_RESOURCE_EXHAUSTED if offset+n exceeds
 *         buf->length.
 */
static sapi_status_t sapi_buffer_peek_bytes(const sapi_buffer_t *buf, size_t offset,
                                             unsigned char *out_bytes, size_t n)
{
    const unsigned char *src;

    if ((buf == NULL) || (out_bytes == NULL) || (buf->data == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((offset > buf->length) || ((buf->length - offset) < n))
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    src = (const unsigned char *)buf->data;
    (void)memcpy(out_bytes, &src[offset], n);
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_write_u16_le(sapi_buffer_t *buf, uint16_t value)
{
    unsigned char bytes[2];
    bytes[0] = (unsigned char)(value & 0xFFU);
    bytes[1] = (unsigned char)((value >> 8) & 0xFFU);
    return sapi_buffer_append_bytes(buf, bytes, sizeof(bytes));
}

sapi_status_t sapi_buffer_write_u16_be(sapi_buffer_t *buf, uint16_t value)
{
    unsigned char bytes[2];
    bytes[0] = (unsigned char)((value >> 8) & 0xFFU);
    bytes[1] = (unsigned char)(value & 0xFFU);
    return sapi_buffer_append_bytes(buf, bytes, sizeof(bytes));
}

sapi_status_t sapi_buffer_write_u32_le(sapi_buffer_t *buf, uint32_t value)
{
    unsigned char bytes[4];
    bytes[0] = (unsigned char)(value & 0xFFU);
    bytes[1] = (unsigned char)((value >> 8) & 0xFFU);
    bytes[2] = (unsigned char)((value >> 16) & 0xFFU);
    bytes[3] = (unsigned char)((value >> 24) & 0xFFU);
    return sapi_buffer_append_bytes(buf, bytes, sizeof(bytes));
}

sapi_status_t sapi_buffer_write_u32_be(sapi_buffer_t *buf, uint32_t value)
{
    unsigned char bytes[4];
    bytes[0] = (unsigned char)((value >> 24) & 0xFFU);
    bytes[1] = (unsigned char)((value >> 16) & 0xFFU);
    bytes[2] = (unsigned char)((value >> 8) & 0xFFU);
    bytes[3] = (unsigned char)(value & 0xFFU);
    return sapi_buffer_append_bytes(buf, bytes, sizeof(bytes));
}

sapi_status_t sapi_buffer_write_u64_le(sapi_buffer_t *buf, uint64_t value)
{
    unsigned char bytes[8];
    size_t i;
    for (i = 0U; i < sizeof(bytes); i++)
    {
        bytes[i] = (unsigned char)((value >> (8U * (uint64_t)i)) & 0xFFU);
    }
    return sapi_buffer_append_bytes(buf, bytes, sizeof(bytes));
}

sapi_status_t sapi_buffer_write_u64_be(sapi_buffer_t *buf, uint64_t value)
{
    unsigned char bytes[8];
    size_t i;
    for (i = 0U; i < sizeof(bytes); i++)
    {
        size_t shift = (sizeof(bytes) - 1U - i) * 8U;
        bytes[i] = (unsigned char)((value >> (uint64_t)shift) & 0xFFU);
    }
    return sapi_buffer_append_bytes(buf, bytes, sizeof(bytes));
}

sapi_status_t sapi_buffer_read_u16_le(const sapi_buffer_t *buf, size_t offset, uint16_t *out_value)
{
    unsigned char bytes[2];
    sapi_status_t st;
    uint32_t v;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_buffer_peek_bytes(buf, offset, bytes, sizeof(bytes));
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    v = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8);
    *out_value = (uint16_t)v;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_read_u16_be(const sapi_buffer_t *buf, size_t offset, uint16_t *out_value)
{
    unsigned char bytes[2];
    sapi_status_t st;
    uint32_t v;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_buffer_peek_bytes(buf, offset, bytes, sizeof(bytes));
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    v = ((uint32_t)bytes[0] << 8) | (uint32_t)bytes[1];
    *out_value = (uint16_t)v;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_read_u32_le(const sapi_buffer_t *buf, size_t offset, uint32_t *out_value)
{
    unsigned char bytes[4];
    sapi_status_t st;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_buffer_peek_bytes(buf, offset, bytes, sizeof(bytes));
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    *out_value = (uint32_t)bytes[0]
               | ((uint32_t)bytes[1] << 8)
               | ((uint32_t)bytes[2] << 16)
               | ((uint32_t)bytes[3] << 24);
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_read_u32_be(const sapi_buffer_t *buf, size_t offset, uint32_t *out_value)
{
    unsigned char bytes[4];
    sapi_status_t st;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_buffer_peek_bytes(buf, offset, bytes, sizeof(bytes));
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    *out_value = ((uint32_t)bytes[0] << 24)
               | ((uint32_t)bytes[1] << 16)
               | ((uint32_t)bytes[2] << 8)
               | (uint32_t)bytes[3];
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_read_u64_le(const sapi_buffer_t *buf, size_t offset, uint64_t *out_value)
{
    unsigned char bytes[8];
    sapi_status_t st;
    size_t i;
    uint64_t v = 0U;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_buffer_peek_bytes(buf, offset, bytes, sizeof(bytes));
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    for (i = 0U; i < sizeof(bytes); i++)
    {
        v |= ((uint64_t)bytes[i]) << (8U * (uint64_t)i);
    }
    *out_value = v;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_buffer_read_u64_be(const sapi_buffer_t *buf, size_t offset, uint64_t *out_value)
{
    unsigned char bytes[8];
    sapi_status_t st;
    size_t i;
    uint64_t v = 0U;

    if (out_value == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    st = sapi_buffer_peek_bytes(buf, offset, bytes, sizeof(bytes));
    if (st != SAPI_STATUS_OK)
    {
        return st;
    }
    for (i = 0U; i < sizeof(bytes); i++)
    {
        size_t shift = (sizeof(bytes) - 1U - i) * 8U;
        v |= ((uint64_t)bytes[i]) << (uint64_t)shift;
    }
    *out_value = v;
    return SAPI_STATUS_OK;
}

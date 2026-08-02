/* Smoke test for sapi_buffer_t: init, bounds-checked copy in/out, const
 * view, and defensive validity checks. */
#include <assert.h>
#include <string.h>
#include "safeapi/buffer/sapi_buffer.h"

int main(void)
{
    unsigned char storage[8] __attribute__((unused));
    sapi_buffer_t buf __attribute__((unused));

    /* Invalid init. */
    assert(sapi_buffer_init(NULL, storage, sizeof(storage)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_init(&buf, NULL, sizeof(storage)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_init(&buf, storage, 0U) == SAPI_STATUS_INVALID_PARAM);

    /* Valid init. */
    assert(sapi_buffer_init(&buf, storage, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_buffer_is_valid(&buf));
    assert(buf.length == 0U);
    assert(buf.capacity == sizeof(storage));

    /* copy_in within capacity. */
    const unsigned char src[4] __attribute__((unused)) = {1, 2, 3, 4};
    assert(sapi_buffer_copy_in(&buf, src, sizeof(src)) == SAPI_STATUS_OK);
    assert(buf.length == sizeof(src));
    assert(memcmp(storage, src, sizeof(src)) == 0);

    /* copy_in exceeding capacity is rejected and does not corrupt state
     * expectations (length is left at the caller to re-check via status). */
    const unsigned char too_big[16] __attribute__((unused)) = {0};
    assert(sapi_buffer_copy_in(&buf, too_big, sizeof(too_big)) == SAPI_STATUS_RESOURCE_EXHAUSTED);

    /* copy_out into a big-enough destination. */
    unsigned char dest[8] __attribute__((unused)) = {0};
    size_t copied __attribute__((unused)) = 0U;
    assert(sapi_buffer_copy_in(&buf, src, sizeof(src)) == SAPI_STATUS_OK);
    assert(sapi_buffer_copy_out(&buf, dest, sizeof(dest), &copied) == SAPI_STATUS_OK);
    assert(copied == sizeof(src));
    assert(memcmp(dest, src, sizeof(src)) == 0);

    /* copy_out into too-small destination. */
    unsigned char small_dest[2] __attribute__((unused));
    assert(sapi_buffer_copy_out(&buf, small_dest, sizeof(small_dest), &copied)
           == SAPI_STATUS_RESOURCE_EXHAUSTED);
    assert(copied == 0U);

    /* const view reflects current length, not capacity. */
    sapi_const_buffer_t view __attribute__((unused));
    assert(sapi_buffer_as_const(&buf, &view) == SAPI_STATUS_OK);
    assert(view.length == buf.length);
    assert(view.data == buf.data);

    /* clear resets length only. */
    assert(sapi_buffer_clear(&buf) == SAPI_STATUS_OK);
    assert(buf.length == 0U);
    assert(buf.capacity == sizeof(storage));

    /* set_length bounds check. */
    assert(sapi_buffer_set_length(&buf, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_buffer_set_length(&buf, sizeof(storage) + 1U) == SAPI_STATUS_RESOURCE_EXHAUSTED);

    /* defensive validity check. */
    assert(!sapi_buffer_is_valid(NULL));

    /* Endianness-safe access (ADR-006 section 2.5). */
    {
        unsigned char raw[32] __attribute__((unused));
        sapi_buffer_t eb __attribute__((unused));
        assert(sapi_buffer_init(&eb, raw, sizeof(raw)) == SAPI_STATUS_OK);

        assert(sapi_buffer_write_u16_le(&eb, 0x1234U) == SAPI_STATUS_OK);
        assert(eb.length == 2U);
        assert(raw[0] == 0x34U);
        assert(raw[1] == 0x12U);

        assert(sapi_buffer_write_u16_be(&eb, 0x1234U) == SAPI_STATUS_OK);
        assert(eb.length == 4U);
        assert(raw[2] == 0x12U);
        assert(raw[3] == 0x34U);

        assert(sapi_buffer_write_u32_le(&eb, 0xAABBCCDDU) == SAPI_STATUS_OK);
        assert(raw[4] == 0xDDU); assert(raw[5] == 0xCCU);
        assert(raw[6] == 0xBBU); assert(raw[7] == 0xAAU);

        assert(sapi_buffer_write_u32_be(&eb, 0xAABBCCDDU) == SAPI_STATUS_OK);
        assert(raw[8] == 0xAAU); assert(raw[9] == 0xBBU);
        assert(raw[10] == 0xCCU); assert(raw[11] == 0xDDU);

        assert(sapi_buffer_write_u64_le(&eb, 0x0102030405060708ULL) == SAPI_STATUS_OK);
        assert(raw[12] == 0x08U); assert(raw[19] == 0x01U);

        assert(sapi_buffer_write_u64_be(&eb, 0x0102030405060708ULL) == SAPI_STATUS_OK);
        assert(raw[20] == 0x01U); assert(raw[27] == 0x08U);

        assert(eb.length == 28U);

        uint16_t u16 __attribute__((unused));
        uint32_t u32 __attribute__((unused));
        uint64_t u64 __attribute__((unused));
        assert(sapi_buffer_read_u16_le(&eb, 0U, &u16) == SAPI_STATUS_OK);
        assert(u16 == 0x1234U);
        assert(sapi_buffer_read_u16_be(&eb, 2U, &u16) == SAPI_STATUS_OK);
        assert(u16 == 0x1234U);
        assert(sapi_buffer_read_u32_le(&eb, 4U, &u32) == SAPI_STATUS_OK);
        assert(u32 == 0xAABBCCDDU);
        assert(sapi_buffer_read_u32_be(&eb, 8U, &u32) == SAPI_STATUS_OK);
        assert(u32 == 0xAABBCCDDU);
        assert(sapi_buffer_read_u64_le(&eb, 12U, &u64) == SAPI_STATUS_OK);
        assert(u64 == 0x0102030405060708ULL);
        assert(sapi_buffer_read_u64_be(&eb, 20U, &u64) == SAPI_STATUS_OK);
        assert(u64 == 0x0102030405060708ULL);

        /* Out-of-range read/write. */
        assert(sapi_buffer_read_u32_le(&eb, 27U, &u32) == SAPI_STATUS_RESOURCE_EXHAUSTED);
        sapi_buffer_t tiny __attribute__((unused));
        unsigned char tiny_storage[1] __attribute__((unused));
        assert(sapi_buffer_init(&tiny, tiny_storage, sizeof(tiny_storage)) == SAPI_STATUS_OK);
        assert(sapi_buffer_write_u16_le(&tiny, 1U) == SAPI_STATUS_RESOURCE_EXHAUSTED);
    }

    return 0;
}

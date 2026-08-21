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

        /* out_value == NULL for every read_*_le/be function. */
        assert(sapi_buffer_read_u16_le(&eb, 0U, NULL) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_read_u16_be(&eb, 0U, NULL) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_read_u32_le(&eb, 0U, NULL) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_read_u32_be(&eb, 0U, NULL) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_read_u64_le(&eb, 0U, NULL) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_read_u64_be(&eb, 0U, NULL) == SAPI_STATUS_INVALID_PARAM);

        /* Out-of-range read passthrough for the remaining le/be pairs
         * (u32_le's is already covered above). */
        assert(sapi_buffer_read_u16_le(&eb, 27U, &u16) == SAPI_STATUS_RESOURCE_EXHAUSTED);
        assert(sapi_buffer_read_u16_be(&eb, 27U, &u16) == SAPI_STATUS_RESOURCE_EXHAUSTED);
        assert(sapi_buffer_read_u32_be(&eb, 25U, &u32) == SAPI_STATUS_RESOURCE_EXHAUSTED);
        assert(sapi_buffer_read_u64_le(&eb, 21U, &u64) == SAPI_STATUS_RESOURCE_EXHAUSTED);
        assert(sapi_buffer_read_u64_be(&eb, 21U, &u64) == SAPI_STATUS_RESOURCE_EXHAUSTED);

        /* buf == NULL for the shared append/peek helpers, reached through
         * any write_ or read_ entry point. */
        assert(sapi_buffer_write_u16_le(NULL, 1U) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_read_u16_le(NULL, 0U, &u16) == SAPI_STATUS_INVALID_PARAM);
    }

    /* clear()/set_length()/copy_in()/copy_out()/as_const(): NULL-param and
     * NULL-data-member branches not yet hit above. */
    assert(sapi_buffer_clear(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_set_length(NULL, 1U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_copy_in(NULL, src, sizeof(src)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_copy_in(&buf, NULL, sizeof(src)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_copy_out(NULL, dest, sizeof(dest), &copied) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_copy_out(&buf, NULL, sizeof(dest), &copied) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_copy_out(&buf, dest, sizeof(dest), NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_as_const(NULL, &view) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_buffer_as_const(&buf, NULL) == SAPI_STATUS_INVALID_PARAM);

    /* A buffer whose data pointer was never set (zero-initialized, not
     * sapi_buffer_init()'d) is invalid, and every entry point that checks
     * buf->data == NULL directly (not through is_valid()) must reject it
     * too. */
    {
        sapi_buffer_t uninit_buf __attribute__((unused));
        memset(&uninit_buf, 0, sizeof(uninit_buf));
        assert(!sapi_buffer_is_valid(&uninit_buf));
        assert(sapi_buffer_set_length(&uninit_buf, 1U) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_copy_in(&uninit_buf, src, sizeof(src)) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_copy_out(&uninit_buf, dest, sizeof(dest), &copied) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_buffer_as_const(&uninit_buf, &view) == SAPI_STATUS_INVALID_PARAM);
    }

    /* as_const()'s defensive length > capacity check: not reachable through
     * any normal API sequence (every mutator bounds-checks length against
     * capacity before assigning it), only by directly corrupting
     * caller-owned storage, which is exactly what this simulates. */
    {
        sapi_buffer_t corrupt_buf __attribute__((unused));
        unsigned char corrupt_storage[4] __attribute__((unused));
        assert(sapi_buffer_init(&corrupt_buf, corrupt_storage, sizeof(corrupt_storage)) == SAPI_STATUS_OK);
        corrupt_buf.length = sizeof(corrupt_storage) + 1U;
        assert(sapi_buffer_as_const(&corrupt_buf, &view) == SAPI_STATUS_INTERNAL_ERROR);
    }

    return 0;
}

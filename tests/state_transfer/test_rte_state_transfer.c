/** @file test_rte_state_transfer.c
 *  @brief Unit tests for rte_state_transfer.
 */
#include <assert.h>
#include <string.h>

#include "safeapi/redundancy/state_transfer/rte_state_transfer.h"

typedef struct {
    uint32_t a;
    uint8_t  b[4];
} sample_field_t;

static void test_register_and_encoded_size(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t x = 1U;
    sample_field_t y = {0};

    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_encoded_size(&registry) == 0U);
    assert(rte_state_transfer_register_field(&registry, "x", &x, sizeof(x)) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field(&registry, "y", &y, sizeof(y)) == RTE_STATUS_OK);
    assert(rte_state_transfer_encoded_size(&registry) == sizeof(x) + sizeof(y));
}

static void test_encode_decode_roundtrip(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t src_x = 0xDEADBEEFU;
    sample_field_t src_y = {0x12345678U, {1U, 2U, 3U, 4U}};
    uint32_t dst_x = 0U;
    sample_field_t dst_y;
    uint8_t buf[64];
    size_t out_len = 0U;

    memset(&dst_y, 0, sizeof(dst_y));
    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field(&registry, "x", &src_x, sizeof(src_x)) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field(&registry, "y", &src_y, sizeof(src_y)) == RTE_STATUS_OK);

    assert(rte_state_transfer_encode(&registry, buf, sizeof(buf), &out_len) == RTE_STATUS_OK);
    assert(out_len == sizeof(src_x) + sizeof(src_y));

    {
        rte_state_transfer_registry_t dst_registry;
        assert(rte_state_transfer_registry_init(&dst_registry) == RTE_STATUS_OK);
        assert(rte_state_transfer_register_field(&dst_registry, "x", &dst_x, sizeof(dst_x)) == RTE_STATUS_OK);
        assert(rte_state_transfer_register_field(&dst_registry, "y", &dst_y, sizeof(dst_y)) == RTE_STATUS_OK);
        assert(rte_state_transfer_decode(&dst_registry, buf, out_len) == RTE_STATUS_OK);
    }

    assert(dst_x == src_x);
    assert(memcmp(&dst_y, &src_y, sizeof(src_y)) == 0);
}

static void test_encode_buffer_too_small_rejected(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t x = 1U;
    uint8_t buf[2];

    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field(&registry, "x", &x, sizeof(x)) == RTE_STATUS_OK);
    assert(rte_state_transfer_encode(&registry, buf, sizeof(buf), NULL) == RTE_STATUS_RESOURCE_EXHAUSTED);
}

static void test_decode_buffer_too_small_rejected(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t x = 1U;
    uint8_t buf[2] = {0U, 0U};

    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field(&registry, "x", &x, sizeof(x)) == RTE_STATUS_OK);
    assert(rte_state_transfer_decode(&registry, buf, sizeof(buf)) == RTE_STATUS_RESOURCE_EXHAUSTED);
}

static void test_max_fields_exhausted(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t values[RTE_STATE_TRANSFER_MAX_FIELDS + 1U];
    uint32_t i;

    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    for (i = 0U; i < RTE_STATE_TRANSFER_MAX_FIELDS; i++)
    {
        assert(rte_state_transfer_register_field(&registry, NULL, &values[i], sizeof(values[i])) == RTE_STATUS_OK);
    }
    assert(rte_state_transfer_register_field(&registry, NULL, &values[RTE_STATE_TRANSFER_MAX_FIELDS],
                                               sizeof(values[0])) == RTE_STATUS_RESOURCE_EXHAUSTED);
}

/** Custom codec: doubles the value on encode, halves it on decode - not a
 *  realistic transform, just something observably different from raw
 *  memcpy so the test can tell the callback actually ran. */
static rte_status_t encode_doubled(const void *data, uint8_t *out, size_t size)
{
    uint32_t value = *(const uint32_t *)data;
    uint32_t doubled = value * 2U;
    assert(size == sizeof(doubled));
    memcpy(out, &doubled, sizeof(doubled));
    return RTE_STATUS_OK;
}

static rte_status_t decode_halved(void *data, const uint8_t *in, size_t size)
{
    uint32_t encoded;
    assert(size == sizeof(encoded));
    memcpy(&encoded, in, sizeof(encoded));
    *(uint32_t *)data = encoded / 2U;
    return RTE_STATUS_OK;
}

static rte_status_t encode_always_fails(const void *data, uint8_t *out, size_t size)
{
    (void)data;
    (void)out;
    (void)size;
    return RTE_STATUS_INTERNAL_ERROR;
}

static void test_custom_codec_field(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t src = 21U;
    uint32_t dst = 0U;
    uint8_t buf[4];
    size_t out_len = 0U;

    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field_ex(&registry, "x", &src, sizeof(src),
                                                  encode_doubled, decode_halved) == RTE_STATUS_OK);
    assert(rte_state_transfer_encode(&registry, buf, sizeof(buf), &out_len) == RTE_STATUS_OK);
    assert(out_len == sizeof(src));
    {
        uint32_t encoded_value;
        memcpy(&encoded_value, buf, sizeof(encoded_value));
        assert(encoded_value == 42U); /* 21 doubled, not a raw copy of 21 */
    }

    {
        rte_state_transfer_registry_t dst_registry;
        assert(rte_state_transfer_registry_init(&dst_registry) == RTE_STATUS_OK);
        assert(rte_state_transfer_register_field_ex(&dst_registry, "x", &dst, sizeof(dst),
                                                      encode_doubled, decode_halved) == RTE_STATUS_OK);
        assert(rte_state_transfer_decode(&dst_registry, buf, sizeof(buf)) == RTE_STATUS_OK);
    }
    assert(dst == src);
}

static void test_custom_codec_failure_propagates(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t src = 1U;
    uint8_t buf[4];

    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field_ex(&registry, "x", &src, sizeof(src),
                                                  encode_always_fails, decode_halved) == RTE_STATUS_OK);
    assert(rte_state_transfer_encode(&registry, buf, sizeof(buf), NULL) == RTE_STATUS_INTERNAL_ERROR);
}

static void test_mismatched_codec_pair_rejected(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t x = 1U;

    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field_ex(&registry, "x", &x, sizeof(x), encode_doubled, NULL) ==
           RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_register_field_ex(&registry, "x", &x, sizeof(x), NULL, decode_halved) ==
           RTE_STATUS_INVALID_PARAM);
}

static void test_null_params_rejected(void)
{
    rte_state_transfer_registry_t registry;
    uint32_t x = 1U;
    uint8_t buf[4];

    assert(rte_state_transfer_registry_init(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_register_field(NULL, "x", &x, sizeof(x)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_registry_init(&registry) == RTE_STATUS_OK);
    assert(rte_state_transfer_register_field(&registry, "x", NULL, sizeof(x)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_register_field(&registry, "x", &x, 0U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_encode(NULL, buf, sizeof(buf), NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_encode(&registry, NULL, sizeof(buf), NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_decode(NULL, buf, sizeof(buf)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_decode(&registry, NULL, sizeof(buf)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_state_transfer_encoded_size(NULL) == 0U);
}

int main(void)
{
    test_register_and_encoded_size();
    test_encode_decode_roundtrip();
    test_encode_buffer_too_small_rejected();
    test_decode_buffer_too_small_rejected();
    test_max_fields_exhausted();
    test_custom_codec_field();
    test_custom_codec_failure_propagates();
    test_mismatched_codec_pair_rejected();
    test_null_params_rejected();
    return 0;
}

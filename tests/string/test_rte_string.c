/* Tests for rte_string (ADR-006). */
#include <assert.h>
#include <string.h>
#include "rte/utils/string/rte_string.h"

static void test_copy_and_cstr(void)
{
    char storage[8] __attribute__((unused));
    rte_string_t s __attribute__((unused));
    const char *cstr __attribute__((unused));

    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);
    assert(rte_string_length(&s) == 0U);

    assert(rte_string_copy(&s, "hello") == RTE_STATUS_OK);
    assert(rte_string_length(&s) == 5U);

    assert(rte_string_c_str(&s, &cstr) == RTE_STATUS_OK);
    assert(strcmp(cstr, "hello") == 0);

    /* Source with no NUL within capacity is rejected. */
    assert(rte_string_copy(&s, "toolongtofit") == RTE_STATUS_RESOURCE_EXHAUSTED);

    /* c_str fails when there's no spare byte for the terminator. */
    char tight_storage[3] __attribute__((unused));
    rte_string_t tight __attribute__((unused));
    assert(rte_string_init(&tight, tight_storage, sizeof(tight_storage)) == RTE_STATUS_OK);
    assert(rte_string_copy(&tight, "abc") == RTE_STATUS_OK); /* fills capacity exactly */
    assert(rte_string_c_str(&tight, &cstr) == RTE_STATUS_RESOURCE_EXHAUSTED);
}

static void test_copy_n_and_concat(void)
{
    char storage[8] __attribute__((unused));
    rte_string_t s __attribute__((unused));

    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);
    assert(rte_string_copy_n(&s, "ab\0cd", 5U) == RTE_STATUS_OK);
    assert(rte_string_length(&s) == 5U);
    assert(memcmp(storage, "ab\0cd", 5U) == 0);

    assert(rte_string_clear(&s) == RTE_STATUS_OK);
    assert(rte_string_copy(&s, "ab") == RTE_STATUS_OK);
    assert(rte_string_concat(&s, "cd") == RTE_STATUS_OK);
    assert(rte_string_length(&s) == 4U);
    assert(memcmp(storage, "abcd", 4U) == 0);

    /* Concat that doesn't fit leaves the string unmodified. */
    assert(rte_string_concat(&s, "12345") == RTE_STATUS_RESOURCE_EXHAUSTED);
    assert(rte_string_length(&s) == 4U);
}

static void test_compare(void)
{
    char sa[8] __attribute__((unused));
    char sb[8] __attribute__((unused));
    rte_string_t a __attribute__((unused));
    rte_string_t b __attribute__((unused));
    int32_t cmp __attribute__((unused));

    assert(rte_string_init(&a, sa, sizeof(sa)) == RTE_STATUS_OK);
    assert(rte_string_init(&b, sb, sizeof(sb)) == RTE_STATUS_OK);

    assert(rte_string_copy(&a, "abc") == RTE_STATUS_OK);
    assert(rte_string_copy(&b, "abc") == RTE_STATUS_OK);
    assert(rte_string_compare(&a, &b, &cmp) == RTE_STATUS_OK);
    assert(cmp == 0);

    assert(rte_string_copy(&b, "abd") == RTE_STATUS_OK);
    assert(rte_string_compare(&a, &b, &cmp) == RTE_STATUS_OK);
    assert(cmp < 0);

    assert(rte_string_copy(&b, "ab") == RTE_STATUS_OK);
    assert(rte_string_compare(&a, &b, &cmp) == RTE_STATUS_OK);
    assert(cmp > 0); /* "abc" > "ab" (longer, equal prefix) */
}

static void test_find(void)
{
    char storage[16] __attribute__((unused));
    char needle_storage[4] __attribute__((unused));
    rte_string_t s __attribute__((unused));
    rte_string_t needle __attribute__((unused));
    bool found __attribute__((unused));
    size_t idx __attribute__((unused));

    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);
    assert(rte_string_copy(&s, "hello world") == RTE_STATUS_OK);

    assert(rte_string_find_char(&s, 'w', &found, &idx) == RTE_STATUS_OK);
    assert(found);
    assert(idx == 6U);

    assert(rte_string_find_char(&s, 'z', &found, &idx) == RTE_STATUS_OK);
    assert(!found);

    assert(rte_string_init(&needle, needle_storage, sizeof(needle_storage)) == RTE_STATUS_OK);
    assert(rte_string_copy(&needle, "wor") == RTE_STATUS_OK);
    assert(rte_string_find_substr(&s, &needle, &found, &idx) == RTE_STATUS_OK);
    assert(found);
    assert(idx == 6U);

    assert(rte_string_copy(&needle, "xyz") == RTE_STATUS_OK);
    assert(rte_string_find_substr(&s, &needle, &found, &idx) == RTE_STATUS_OK);
    assert(!found);
}

static void test_split(void)
{
    char storage[16] __attribute__((unused));
    rte_string_t s __attribute__((unused));
    size_t cursor __attribute__((unused)) = 0U;
    rte_const_buffer_t token __attribute__((unused));
    bool has_token __attribute__((unused));

    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);
    assert(rte_string_copy(&s, "a,bb,ccc") == RTE_STATUS_OK);

    assert(rte_string_split_next(&s, ',', &cursor, &token, &has_token) == RTE_STATUS_OK);
    assert(has_token);
    assert(token.length == 1U);
    assert(memcmp(token.data, "a", 1U) == 0);

    assert(rte_string_split_next(&s, ',', &cursor, &token, &has_token) == RTE_STATUS_OK);
    assert(has_token);
    assert(token.length == 2U);
    assert(memcmp(token.data, "bb", 2U) == 0);

    assert(rte_string_split_next(&s, ',', &cursor, &token, &has_token) == RTE_STATUS_OK);
    assert(has_token);
    assert(token.length == 3U);
    assert(memcmp(token.data, "ccc", 3U) == 0);

    assert(rte_string_split_next(&s, ',', &cursor, &token, &has_token) == RTE_STATUS_OK);
    assert(!has_token);
}

static void test_numeric(void)
{
    char storage[24] __attribute__((unused));
    rte_string_t s __attribute__((unused));
    uint32_t u32 __attribute__((unused));
    int32_t i32 __attribute__((unused));
    uint64_t u64 __attribute__((unused));
    int64_t i64 __attribute__((unused));

    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);

    assert(rte_string_from_u32(&s, 4294967295U) == RTE_STATUS_OK);
    assert(rte_string_to_u32(&s, &u32) == RTE_STATUS_OK);
    assert(u32 == 4294967295U);

    assert(rte_string_from_i32(&s, -2147483647 - 1) == RTE_STATUS_OK); /* INT32_MIN */
    assert(rte_string_to_i32(&s, &i32) == RTE_STATUS_OK);
    assert(i32 == (-2147483647 - 1));

    assert(rte_string_from_u64(&s, 18446744073709551615ULL) == RTE_STATUS_OK);
    assert(rte_string_to_u64(&s, &u64) == RTE_STATUS_OK);
    assert(u64 == 18446744073709551615ULL);

    assert(rte_string_from_i64(&s, INT64_MIN) == RTE_STATUS_OK);
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_OK);
    assert(i64 == INT64_MIN);

    assert(rte_string_from_i64(&s, 0) == RTE_STATUS_OK);
    assert(rte_string_length(&s) == 1U);
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_OK);
    assert(i64 == 0);

    /* Malformed / out-of-range parsing. */
    assert(rte_string_copy(&s, "12a") == RTE_STATUS_OK);
    assert(rte_string_to_u32(&s, &u32) == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_copy(&s, "99999999999") == RTE_STATUS_OK); /* > UINT32_MAX */
    assert(rte_string_to_u32(&s, &u32) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    assert(rte_string_copy(&s, "-") == RTE_STATUS_OK);
    assert(rte_string_to_i32(&s, &i32) == RTE_STATUS_INVALID_PARAM);
}

static void test_append_numeric(void)
{
    char storage[64] __attribute__((unused));
    char tiny[4] __attribute__((unused));
    rte_string_t s __attribute__((unused));
    rte_string_t t __attribute__((unused));
    const char *c __attribute__((unused));

    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);
    /* Compound line assembly - the snprintf replacement use case. */
    assert(rte_string_copy(&s, "id=") == RTE_STATUS_OK);
    assert(rte_string_append_u32(&s, 4294967295U) == RTE_STATUS_OK);
    assert(rte_string_concat(&s, " d=") == RTE_STATUS_OK);
    assert(rte_string_append_i32(&s, -5) == RTE_STATUS_OK);
    assert(rte_string_c_str(&s, &c) == RTE_STATUS_OK);
    assert(strcmp(c, "id=4294967295 d=-5") == 0);

    assert(rte_string_clear(&s) == RTE_STATUS_OK);
    assert(rte_string_append_u64(&s, 18446744073709551615ULL) == RTE_STATUS_OK);
    assert(rte_string_append_i64(&s, INT64_MIN) == RTE_STATUS_OK);
    assert(rte_string_c_str(&s, &c) == RTE_STATUS_OK);
    assert(strcmp(c, "18446744073709551615-9223372036854775808") == 0);

    /* Hex: min-digit zero padding, and no truncation when the value is wider. */
    assert(rte_string_clear(&s) == RTE_STATUS_OK);
    assert(rte_string_append_hex_u32(&s, 0x2AU, 4U) == RTE_STATUS_OK);
    assert(rte_string_append_hex_u32(&s, 0U, 2U) == RTE_STATUS_OK);
    assert(rte_string_append_hex_u32(&s, 0xDEADBEEFU, 2U) == RTE_STATUS_OK);
    assert(rte_string_append_hex_u32(&s, 0xFFU, 0U) == RTE_STATUS_OK); /* clamps to 1 */
    assert(rte_string_c_str(&s, &c) == RTE_STATUS_OK);
    assert(strcmp(c, "002a00deadbeefff") == 0);

    /* Bounds: append that would overflow remaining capacity leaves dest intact. */
    assert(rte_string_init(&t, tiny, sizeof(tiny)) == RTE_STATUS_OK);
    assert(rte_string_copy(&t, "ab") == RTE_STATUS_OK);
    assert(rte_string_append_u32(&t, 99999U) == RTE_STATUS_RESOURCE_EXHAUSTED);
    assert(rte_string_length(&t) == 2U);
    assert(rte_string_append_u32(&t, 7U) == RTE_STATUS_OK); /* "ab7" fits (cap 4) */
    assert(rte_string_length(&t) == 3U);

    /* NULL dest. */
    assert(rte_string_append_u32(NULL, 1U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_append_i64(NULL, 1) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_append_hex_u32(NULL, 1U, 1U) == RTE_STATUS_INVALID_PARAM);
}

static void test_null_and_invalid_params(void)
{
    char storage[32] __attribute__((unused));
    char storage2[32] __attribute__((unused));
    rte_string_t s __attribute__((unused));
    rte_string_t s2 __attribute__((unused));
    rte_string_t uninit __attribute__((unused));
    const char *cstr __attribute__((unused));
    int32_t cmp __attribute__((unused));
    bool found __attribute__((unused));
    size_t idx __attribute__((unused));
    size_t cursor __attribute__((unused));
    rte_const_buffer_t token __attribute__((unused));
    bool has_token __attribute__((unused));
    uint32_t u32 __attribute__((unused));
    int32_t i32 __attribute__((unused));
    uint64_t u64 __attribute__((unused));
    int64_t i64 __attribute__((unused));

    memset(&uninit, 0, sizeof(uninit)); /* buf.data == NULL: fails is_valid() */

    assert(rte_string_init(NULL, storage, sizeof(storage)) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_clear(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_length(NULL) == 0U);

    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);
    assert(rte_string_init(&s2, storage2, sizeof(storage2)) == RTE_STATUS_OK);

    assert(rte_string_c_str(NULL, &cstr) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_c_str(&s, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_c_str(&uninit, &cstr) == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_copy_n(NULL, "x", 1U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_copy_n(&s, NULL, 1U) == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_copy(NULL, "x") == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_copy(&s, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_copy(&uninit, "x") == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_concat(NULL, "x") == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_concat(&s, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_concat(&uninit, "x") == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_compare(NULL, &s2, &cmp) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_compare(&s, NULL, &cmp) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_compare(&s, &s2, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_compare(&uninit, &s2, &cmp) == RTE_STATUS_INVALID_PARAM);

    /* a strictly shorter than b, equal over the shared prefix: the
     * a->buf.length < b->buf.length branch (result = -1). */
    assert(rte_string_copy(&s, "ab") == RTE_STATUS_OK);
    assert(rte_string_copy(&s2, "abc") == RTE_STATUS_OK);
    assert(rte_string_compare(&s, &s2, &cmp) == RTE_STATUS_OK);
    assert(cmp < 0);

    assert(rte_string_find_char(NULL, 'x', &found, &idx) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_find_char(&s, 'x', NULL, &idx) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_find_char(&s, 'x', &found, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_find_char(&uninit, 'x', &found, &idx) == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_find_substr(NULL, &s2, &found, &idx) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_find_substr(&s, NULL, &found, &idx) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_find_substr(&s, &s2, NULL, &idx) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_find_substr(&s, &s2, &found, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_find_substr(&uninit, &s2, &found, &idx) == RTE_STATUS_INVALID_PARAM);

    /* Empty needle: always "found" at index 0. */
    assert(rte_string_clear(&s2) == RTE_STATUS_OK);
    assert(rte_string_find_substr(&s, &s2, &found, &idx) == RTE_STATUS_OK);
    assert(found);
    assert(idx == 0U);

    /* Needle longer than haystack: never found, without scanning. */
    assert(rte_string_copy(&s2, "abcdef") == RTE_STATUS_OK); /* longer than s's "ab" */
    assert(rte_string_find_substr(&s, &s2, &found, &idx) == RTE_STATUS_OK);
    assert(!found);

    cursor = 0U;
    assert(rte_string_split_next(NULL, ',', &cursor, &token, &has_token) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_split_next(&s, ',', NULL, &token, &has_token) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_split_next(&s, ',', &cursor, NULL, &has_token) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_split_next(&s, ',', &cursor, &token, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_split_next(&uninit, ',', &cursor, &token, &has_token) == RTE_STATUS_INVALID_PARAM);
    cursor = rte_string_length(&s) + 1U; /* past the end: out of range */
    assert(rte_string_split_next(&s, ',', &cursor, &token, &has_token) == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_from_u64(NULL, 1U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_from_i64(NULL, 1) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_from_u32(NULL, 1U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_from_i32(NULL, 1) == RTE_STATUS_INVALID_PARAM);

    assert(rte_string_to_u64(NULL, &u64) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_to_u64(&s, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_to_u64(&uninit, &u64) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_clear(&s) == RTE_STATUS_OK);
    assert(rte_string_to_u64(&s, &u64) == RTE_STATUS_INVALID_PARAM); /* empty */

    assert(rte_string_to_i64(NULL, &i64) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_to_i64(&s, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_to_i64(&uninit, &i64) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_INVALID_PARAM); /* empty */

    /* Malformed digit specifically via the 64-bit parsers (32-bit variants
     * already exercised in test_numeric()). */
    assert(rte_string_copy(&s, "12a") == RTE_STATUS_OK);
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_INVALID_PARAM);

    /* Overflow beyond UINT64_MAX/INT64_MAX/INT64_MIN: needs >20 digits. */
    assert(rte_string_init(&s, storage, sizeof(storage)) == RTE_STATUS_OK);
    assert(rte_string_copy(&s, "99999999999999999999") == RTE_STATUS_OK); /* > UINT64_MAX */
    assert(rte_string_to_u64(&s, &u64) == RTE_STATUS_VALUE_OUT_OF_RANGE);
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    assert(rte_string_copy(&s, "-99999999999999999999") == RTE_STATUS_OK); /* overflows u64 accumulation itself */
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    /* Magnitude fits within uint64_t (so the accumulation loop itself
     * never overflows) but exceeds INT64_MIN's own magnitude - the
     * negative-specific overflow branch, distinct from the accumulation
     * overflow above. */
    assert(rte_string_copy(&s, "-9223372036854775809") == RTE_STATUS_OK); /* INT64_MIN magnitude + 1 */
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    assert(rte_string_copy(&s, "9223372036854775808") == RTE_STATUS_OK); /* INT64_MAX + 1, positive */
    assert(rte_string_to_i64(&s, &i64) == RTE_STATUS_VALUE_OUT_OF_RANGE);

    assert(rte_string_to_u32(&s, NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_string_to_i32(&s, NULL) == RTE_STATUS_INVALID_PARAM);
    (void)u32;
    (void)i32;
}

int main(void)
{
    test_copy_and_cstr();
    test_copy_n_and_concat();
    test_compare();
    test_find();
    test_split();
    test_numeric();
    test_append_numeric();
    test_null_and_invalid_params();
    return 0;
}

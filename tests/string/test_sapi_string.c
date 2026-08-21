/* Tests for sapi_string (ADR-006). */
#include <assert.h>
#include <string.h>
#include "safeapi/string/sapi_string.h"

static void test_copy_and_cstr(void)
{
    char storage[8] __attribute__((unused));
    sapi_string_t s __attribute__((unused));
    const char *cstr __attribute__((unused));

    assert(sapi_string_init(&s, storage, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_string_length(&s) == 0U);

    assert(sapi_string_copy(&s, "hello") == SAPI_STATUS_OK);
    assert(sapi_string_length(&s) == 5U);

    assert(sapi_string_c_str(&s, &cstr) == SAPI_STATUS_OK);
    assert(strcmp(cstr, "hello") == 0);

    /* Source with no NUL within capacity is rejected. */
    assert(sapi_string_copy(&s, "toolongtofit") == SAPI_STATUS_RESOURCE_EXHAUSTED);

    /* c_str fails when there's no spare byte for the terminator. */
    char tight_storage[3] __attribute__((unused));
    sapi_string_t tight __attribute__((unused));
    assert(sapi_string_init(&tight, tight_storage, sizeof(tight_storage)) == SAPI_STATUS_OK);
    assert(sapi_string_copy(&tight, "abc") == SAPI_STATUS_OK); /* fills capacity exactly */
    assert(sapi_string_c_str(&tight, &cstr) == SAPI_STATUS_RESOURCE_EXHAUSTED);
}

static void test_copy_n_and_concat(void)
{
    char storage[8] __attribute__((unused));
    sapi_string_t s __attribute__((unused));

    assert(sapi_string_init(&s, storage, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_string_copy_n(&s, "ab\0cd", 5U) == SAPI_STATUS_OK);
    assert(sapi_string_length(&s) == 5U);
    assert(memcmp(storage, "ab\0cd", 5U) == 0);

    assert(sapi_string_clear(&s) == SAPI_STATUS_OK);
    assert(sapi_string_copy(&s, "ab") == SAPI_STATUS_OK);
    assert(sapi_string_concat(&s, "cd") == SAPI_STATUS_OK);
    assert(sapi_string_length(&s) == 4U);
    assert(memcmp(storage, "abcd", 4U) == 0);

    /* Concat that doesn't fit leaves the string unmodified. */
    assert(sapi_string_concat(&s, "12345") == SAPI_STATUS_RESOURCE_EXHAUSTED);
    assert(sapi_string_length(&s) == 4U);
}

static void test_compare(void)
{
    char sa[8] __attribute__((unused));
    char sb[8] __attribute__((unused));
    sapi_string_t a __attribute__((unused));
    sapi_string_t b __attribute__((unused));
    int32_t cmp __attribute__((unused));

    assert(sapi_string_init(&a, sa, sizeof(sa)) == SAPI_STATUS_OK);
    assert(sapi_string_init(&b, sb, sizeof(sb)) == SAPI_STATUS_OK);

    assert(sapi_string_copy(&a, "abc") == SAPI_STATUS_OK);
    assert(sapi_string_copy(&b, "abc") == SAPI_STATUS_OK);
    assert(sapi_string_compare(&a, &b, &cmp) == SAPI_STATUS_OK);
    assert(cmp == 0);

    assert(sapi_string_copy(&b, "abd") == SAPI_STATUS_OK);
    assert(sapi_string_compare(&a, &b, &cmp) == SAPI_STATUS_OK);
    assert(cmp < 0);

    assert(sapi_string_copy(&b, "ab") == SAPI_STATUS_OK);
    assert(sapi_string_compare(&a, &b, &cmp) == SAPI_STATUS_OK);
    assert(cmp > 0); /* "abc" > "ab" (longer, equal prefix) */
}

static void test_find(void)
{
    char storage[16] __attribute__((unused));
    char needle_storage[4] __attribute__((unused));
    sapi_string_t s __attribute__((unused));
    sapi_string_t needle __attribute__((unused));
    bool found __attribute__((unused));
    size_t idx __attribute__((unused));

    assert(sapi_string_init(&s, storage, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_string_copy(&s, "hello world") == SAPI_STATUS_OK);

    assert(sapi_string_find_char(&s, 'w', &found, &idx) == SAPI_STATUS_OK);
    assert(found);
    assert(idx == 6U);

    assert(sapi_string_find_char(&s, 'z', &found, &idx) == SAPI_STATUS_OK);
    assert(!found);

    assert(sapi_string_init(&needle, needle_storage, sizeof(needle_storage)) == SAPI_STATUS_OK);
    assert(sapi_string_copy(&needle, "wor") == SAPI_STATUS_OK);
    assert(sapi_string_find_substr(&s, &needle, &found, &idx) == SAPI_STATUS_OK);
    assert(found);
    assert(idx == 6U);

    assert(sapi_string_copy(&needle, "xyz") == SAPI_STATUS_OK);
    assert(sapi_string_find_substr(&s, &needle, &found, &idx) == SAPI_STATUS_OK);
    assert(!found);
}

static void test_split(void)
{
    char storage[16] __attribute__((unused));
    sapi_string_t s __attribute__((unused));
    size_t cursor __attribute__((unused)) = 0U;
    sapi_const_buffer_t token __attribute__((unused));
    bool has_token __attribute__((unused));

    assert(sapi_string_init(&s, storage, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_string_copy(&s, "a,bb,ccc") == SAPI_STATUS_OK);

    assert(sapi_string_split_next(&s, ',', &cursor, &token, &has_token) == SAPI_STATUS_OK);
    assert(has_token);
    assert(token.length == 1U);
    assert(memcmp(token.data, "a", 1U) == 0);

    assert(sapi_string_split_next(&s, ',', &cursor, &token, &has_token) == SAPI_STATUS_OK);
    assert(has_token);
    assert(token.length == 2U);
    assert(memcmp(token.data, "bb", 2U) == 0);

    assert(sapi_string_split_next(&s, ',', &cursor, &token, &has_token) == SAPI_STATUS_OK);
    assert(has_token);
    assert(token.length == 3U);
    assert(memcmp(token.data, "ccc", 3U) == 0);

    assert(sapi_string_split_next(&s, ',', &cursor, &token, &has_token) == SAPI_STATUS_OK);
    assert(!has_token);
}

static void test_numeric(void)
{
    char storage[24] __attribute__((unused));
    sapi_string_t s __attribute__((unused));
    uint32_t u32 __attribute__((unused));
    int32_t i32 __attribute__((unused));
    uint64_t u64 __attribute__((unused));
    int64_t i64 __attribute__((unused));

    assert(sapi_string_init(&s, storage, sizeof(storage)) == SAPI_STATUS_OK);

    assert(sapi_string_from_u32(&s, 4294967295U) == SAPI_STATUS_OK);
    assert(sapi_string_to_u32(&s, &u32) == SAPI_STATUS_OK);
    assert(u32 == 4294967295U);

    assert(sapi_string_from_i32(&s, -2147483647 - 1) == SAPI_STATUS_OK); /* INT32_MIN */
    assert(sapi_string_to_i32(&s, &i32) == SAPI_STATUS_OK);
    assert(i32 == (-2147483647 - 1));

    assert(sapi_string_from_u64(&s, 18446744073709551615ULL) == SAPI_STATUS_OK);
    assert(sapi_string_to_u64(&s, &u64) == SAPI_STATUS_OK);
    assert(u64 == 18446744073709551615ULL);

    assert(sapi_string_from_i64(&s, INT64_MIN) == SAPI_STATUS_OK);
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_OK);
    assert(i64 == INT64_MIN);

    assert(sapi_string_from_i64(&s, 0) == SAPI_STATUS_OK);
    assert(sapi_string_length(&s) == 1U);
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_OK);
    assert(i64 == 0);

    /* Malformed / out-of-range parsing. */
    assert(sapi_string_copy(&s, "12a") == SAPI_STATUS_OK);
    assert(sapi_string_to_u32(&s, &u32) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_copy(&s, "99999999999") == SAPI_STATUS_OK); /* > UINT32_MAX */
    assert(sapi_string_to_u32(&s, &u32) == SAPI_STATUS_VALUE_OUT_OF_RANGE);

    assert(sapi_string_copy(&s, "-") == SAPI_STATUS_OK);
    assert(sapi_string_to_i32(&s, &i32) == SAPI_STATUS_INVALID_PARAM);
}

static void test_null_and_invalid_params(void)
{
    char storage[32] __attribute__((unused));
    char storage2[32] __attribute__((unused));
    sapi_string_t s __attribute__((unused));
    sapi_string_t s2 __attribute__((unused));
    sapi_string_t uninit __attribute__((unused));
    const char *cstr __attribute__((unused));
    int32_t cmp __attribute__((unused));
    bool found __attribute__((unused));
    size_t idx __attribute__((unused));
    size_t cursor __attribute__((unused));
    sapi_const_buffer_t token __attribute__((unused));
    bool has_token __attribute__((unused));
    uint32_t u32 __attribute__((unused));
    int32_t i32 __attribute__((unused));
    uint64_t u64 __attribute__((unused));
    int64_t i64 __attribute__((unused));

    memset(&uninit, 0, sizeof(uninit)); /* buf.data == NULL: fails is_valid() */

    assert(sapi_string_init(NULL, storage, sizeof(storage)) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_clear(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_length(NULL) == 0U);

    assert(sapi_string_init(&s, storage, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_string_init(&s2, storage2, sizeof(storage2)) == SAPI_STATUS_OK);

    assert(sapi_string_c_str(NULL, &cstr) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_c_str(&s, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_c_str(&uninit, &cstr) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_copy_n(NULL, "x", 1U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_copy_n(&s, NULL, 1U) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_copy(NULL, "x") == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_copy(&s, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_copy(&uninit, "x") == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_concat(NULL, "x") == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_concat(&s, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_concat(&uninit, "x") == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_compare(NULL, &s2, &cmp) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_compare(&s, NULL, &cmp) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_compare(&s, &s2, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_compare(&uninit, &s2, &cmp) == SAPI_STATUS_INVALID_PARAM);

    /* a strictly shorter than b, equal over the shared prefix: the
     * a->buf.length < b->buf.length branch (result = -1). */
    assert(sapi_string_copy(&s, "ab") == SAPI_STATUS_OK);
    assert(sapi_string_copy(&s2, "abc") == SAPI_STATUS_OK);
    assert(sapi_string_compare(&s, &s2, &cmp) == SAPI_STATUS_OK);
    assert(cmp < 0);

    assert(sapi_string_find_char(NULL, 'x', &found, &idx) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_find_char(&s, 'x', NULL, &idx) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_find_char(&s, 'x', &found, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_find_char(&uninit, 'x', &found, &idx) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_find_substr(NULL, &s2, &found, &idx) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_find_substr(&s, NULL, &found, &idx) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_find_substr(&s, &s2, NULL, &idx) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_find_substr(&s, &s2, &found, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_find_substr(&uninit, &s2, &found, &idx) == SAPI_STATUS_INVALID_PARAM);

    /* Empty needle: always "found" at index 0. */
    assert(sapi_string_clear(&s2) == SAPI_STATUS_OK);
    assert(sapi_string_find_substr(&s, &s2, &found, &idx) == SAPI_STATUS_OK);
    assert(found);
    assert(idx == 0U);

    /* Needle longer than haystack: never found, without scanning. */
    assert(sapi_string_copy(&s2, "abcdef") == SAPI_STATUS_OK); /* longer than s's "ab" */
    assert(sapi_string_find_substr(&s, &s2, &found, &idx) == SAPI_STATUS_OK);
    assert(!found);

    cursor = 0U;
    assert(sapi_string_split_next(NULL, ',', &cursor, &token, &has_token) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_split_next(&s, ',', NULL, &token, &has_token) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_split_next(&s, ',', &cursor, NULL, &has_token) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_split_next(&s, ',', &cursor, &token, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_split_next(&uninit, ',', &cursor, &token, &has_token) == SAPI_STATUS_INVALID_PARAM);
    cursor = sapi_string_length(&s) + 1U; /* past the end: out of range */
    assert(sapi_string_split_next(&s, ',', &cursor, &token, &has_token) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_from_u64(NULL, 1U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_from_i64(NULL, 1) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_from_u32(NULL, 1U) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_from_i32(NULL, 1) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_string_to_u64(NULL, &u64) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_to_u64(&s, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_to_u64(&uninit, &u64) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_clear(&s) == SAPI_STATUS_OK);
    assert(sapi_string_to_u64(&s, &u64) == SAPI_STATUS_INVALID_PARAM); /* empty */

    assert(sapi_string_to_i64(NULL, &i64) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_to_i64(&s, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_to_i64(&uninit, &i64) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_INVALID_PARAM); /* empty */

    /* Malformed digit specifically via the 64-bit parsers (32-bit variants
     * already exercised in test_numeric()). */
    assert(sapi_string_copy(&s, "12a") == SAPI_STATUS_OK);
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_INVALID_PARAM);

    /* Overflow beyond UINT64_MAX/INT64_MAX/INT64_MIN: needs >20 digits. */
    assert(sapi_string_init(&s, storage, sizeof(storage)) == SAPI_STATUS_OK);
    assert(sapi_string_copy(&s, "99999999999999999999") == SAPI_STATUS_OK); /* > UINT64_MAX */
    assert(sapi_string_to_u64(&s, &u64) == SAPI_STATUS_VALUE_OUT_OF_RANGE);
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_VALUE_OUT_OF_RANGE);

    assert(sapi_string_copy(&s, "-99999999999999999999") == SAPI_STATUS_OK); /* overflows u64 accumulation itself */
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_VALUE_OUT_OF_RANGE);

    /* Magnitude fits within uint64_t (so the accumulation loop itself
     * never overflows) but exceeds INT64_MIN's own magnitude - the
     * negative-specific overflow branch, distinct from the accumulation
     * overflow above. */
    assert(sapi_string_copy(&s, "-9223372036854775809") == SAPI_STATUS_OK); /* INT64_MIN magnitude + 1 */
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_VALUE_OUT_OF_RANGE);

    assert(sapi_string_copy(&s, "9223372036854775808") == SAPI_STATUS_OK); /* INT64_MAX + 1, positive */
    assert(sapi_string_to_i64(&s, &i64) == SAPI_STATUS_VALUE_OUT_OF_RANGE);

    assert(sapi_string_to_u32(&s, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_string_to_i32(&s, NULL) == SAPI_STATUS_INVALID_PARAM);
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
    test_null_and_invalid_params();
    return 0;
}

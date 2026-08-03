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
    char storage[16];
    char needle_storage[4];
    sapi_string_t s;
    sapi_string_t needle;
    bool found;
    size_t idx;

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
    char storage[16];
    sapi_string_t s;
    size_t cursor = 0U;
    sapi_const_buffer_t token;
    bool has_token;

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
    char storage[24];
    sapi_string_t s;
    uint32_t u32;
    int32_t i32;
    uint64_t u64;
    int64_t i64;

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

int main(void)
{
    test_copy_and_cstr();
    test_copy_n_and_concat();
    test_compare();
    test_find();
    test_split();
    test_numeric();
    return 0;
}

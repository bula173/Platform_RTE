/* Tests for sapi_channel (ADR-008).
 *
 * This file is compiled once per channel build (real dual-channel builds
 * use cmake/toolchain-channel-a.cmake and toolchain-channel-b.cmake, each
 * defining SAPI_CHANNEL_BUILD_A / SAPI_CHANNEL_BUILD_B for every
 * translation unit including sapi_channel.h - see ADR-008 section 2.2 and
 * REQ-COMMON-CHANNEL-001). The identity assertion below is written to
 * pass under either build rather than assuming one.
 *
 * sapi_channel_compare_and_enter_safestate() is documented as never
 * returning on a mismatch (it calls sapi_safestate_enter() at
 * SAPI_SAFESTATE_LEVEL_SAFE). As in test_sapi_safestate.c, a
 * setjmp/longjmp diverting handler is used to verify that path without
 * actually hanging the test on the defensive infinite loop.
 */
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include "safeapi/channel/sapi_channel.h"

static jmp_buf g_jmp;
static sapi_safestate_level_t g_captured_level;
static sapi_safestate_reason_t g_captured_reason;
static int g_handler_calls;

static void diverting_handler(sapi_safestate_level_t level,
                               sapi_safestate_reason_t reason,
                               const char *file,
                               int32_t line,
                               const char *message)
{
    (void)file;
    (void)line;
    (void)message;
    g_captured_level = level;
    g_captured_reason = reason;
    g_handler_calls++;
    longjmp(g_jmp, 1);
}

static void test_local_id_matches_build_macro(void)
{
#if defined(SAPI_CHANNEL_BUILD_A)
    assert(sapi_channel_local_id() == SAPI_CHANNEL_ID_A);
#else
    assert(sapi_channel_local_id() == SAPI_CHANNEL_ID_B);
#endif
}

static void test_id_to_string(void)
{
    assert(strcmp(sapi_channel_id_to_string(SAPI_CHANNEL_ID_A), "CHANNEL_A") == 0);
    assert(strcmp(sapi_channel_id_to_string(SAPI_CHANNEL_ID_B), "CHANNEL_B") == 0);
    /* Defensive: an out-of-range enum value must not return NULL. */
    assert(sapi_channel_id_to_string((sapi_channel_id_t)99) != NULL);
}

static void test_compare_invalid_params(void)
{
    unsigned char storage[4] = { 1, 2, 3, 4 };
    sapi_const_buffer_t local = { storage, sizeof(storage) };
    sapi_const_buffer_t peer = { storage, sizeof(storage) };
    sapi_channel_compare_result_t result = SAPI_CHANNEL_COMPARE_MISMATCH;
    sapi_const_buffer_t bad_nonnull_len = { NULL, 1U };

    assert(sapi_channel_compare(local, peer, NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_compare(bad_nonnull_len, peer, &result) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_channel_compare(local, bad_nonnull_len, &result) == SAPI_STATUS_INVALID_PARAM);
}

static void test_compare_match(void)
{
    unsigned char a[4] = { 1, 2, 3, 4 };
    unsigned char b[4] = { 1, 2, 3, 4 };
    sapi_const_buffer_t local = { a, sizeof(a) };
    sapi_const_buffer_t peer = { b, sizeof(b) };
    sapi_channel_compare_result_t result = SAPI_CHANNEL_COMPARE_MISMATCH;

    assert(sapi_channel_compare(local, peer, &result) == SAPI_STATUS_OK);
    assert(result == SAPI_CHANNEL_COMPARE_MATCH);
}

static void test_compare_match_both_empty(void)
{
    sapi_const_buffer_t local = { NULL, 0U };
    sapi_const_buffer_t peer = { NULL, 0U };
    sapi_channel_compare_result_t result = SAPI_CHANNEL_COMPARE_MISMATCH;

    assert(sapi_channel_compare(local, peer, &result) == SAPI_STATUS_OK);
    assert(result == SAPI_CHANNEL_COMPARE_MATCH);
}

static void test_compare_mismatch_content(void)
{
    unsigned char a[4] = { 1, 2, 3, 4 };
    unsigned char b[4] = { 1, 2, 3, 5 };
    sapi_const_buffer_t local = { a, sizeof(a) };
    sapi_const_buffer_t peer = { b, sizeof(b) };
    sapi_channel_compare_result_t result = SAPI_CHANNEL_COMPARE_MATCH;

    assert(sapi_channel_compare(local, peer, &result) == SAPI_STATUS_OK);
    assert(result == SAPI_CHANNEL_COMPARE_MISMATCH);
}

static void test_compare_mismatch_length(void)
{
    unsigned char a[4] = { 1, 2, 3, 4 };
    unsigned char b[3] = { 1, 2, 3 };
    sapi_const_buffer_t local = { a, sizeof(a) };
    sapi_const_buffer_t peer = { b, sizeof(b) };
    sapi_channel_compare_result_t result = SAPI_CHANNEL_COMPARE_MATCH;

    assert(sapi_channel_compare(local, peer, &result) == SAPI_STATUS_OK);
    assert(result == SAPI_CHANNEL_COMPARE_MISMATCH);
}

static void test_compare_and_enter_safestate_match_returns(void)
{
    unsigned char a[2] = { 9, 9 };
    unsigned char b[2] = { 9, 9 };
    sapi_const_buffer_t local = { a, sizeof(a) };
    sapi_const_buffer_t peer = { b, sizeof(b) };

    g_handler_calls = 0;
    assert(sapi_channel_compare_and_enter_safestate(local, peer, SAPI_SAFESTATE_REASON_UNSPECIFIED)
           == SAPI_STATUS_OK);
    /* Reaching this point at all proves a matching comparison returned
     * control normally instead of entering safe-state. */
    assert(g_handler_calls == 0);
}

static void test_compare_and_enter_safestate_invalid_param_returns(void)
{
    sapi_const_buffer_t bad = { NULL, 1U };
    sapi_const_buffer_t peer = { NULL, 0U };

    g_handler_calls = 0;
    assert(sapi_channel_compare_and_enter_safestate(bad, peer, SAPI_SAFESTATE_REASON_UNSPECIFIED)
           == SAPI_STATUS_INVALID_PARAM);
    /* An input that couldn't even be evaluated must not trigger a
     * safe-state transition - there's nothing confirmed to disagree on. */
    assert(g_handler_calls == 0);
}

static void test_compare_and_enter_safestate_mismatch_diverts(void)
{
    unsigned char a[2] = { 1, 2 };
    unsigned char b[2] = { 3, 4 };
    sapi_const_buffer_t local = { a, sizeof(a) };
    sapi_const_buffer_t peer = { b, sizeof(b) };

    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler) == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_channel_compare_and_enter_safestate(local, peer, SAPI_SAFESTATE_REASON_UNSPECIFIED);
        /* Must never reach here: a mismatch always diverts via longjmp. */
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == SAPI_SAFESTATE_LEVEL_SAFE);
        assert(g_captured_reason == SAPI_SAFESTATE_REASON_CHANNEL_MISMATCH);
    }
}

static void test_compare_and_enter_safestate_explicit_reason(void)
{
    unsigned char a[1] = { 1 };
    unsigned char b[1] = { 2 };
    sapi_const_buffer_t local = { a, sizeof(a) };
    sapi_const_buffer_t peer = { b, sizeof(b) };
    const sapi_safestate_reason_t custom_reason = (sapi_safestate_reason_t)5000U;

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        (void)sapi_channel_compare_and_enter_safestate(local, peer, custom_reason);
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_reason == custom_reason);
    }
}

int main(void)
{
    test_local_id_matches_build_macro();
    test_id_to_string();
    test_compare_invalid_params();
    test_compare_match();
    test_compare_match_both_empty();
    test_compare_mismatch_content();
    test_compare_mismatch_length();
    test_compare_and_enter_safestate_match_returns();
    test_compare_and_enter_safestate_invalid_param_returns();
    test_compare_and_enter_safestate_mismatch_diverts();
    test_compare_and_enter_safestate_explicit_reason();
    return 0;
}

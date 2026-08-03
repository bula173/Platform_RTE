/* Tests for sapi_safestate (ADR-004).
 *
 * SAPI_SAFESTATE_LEVEL_SAFE and SAPI_SAFESTATE_LEVEL_REBOOT are documented
 * as never returning to the caller (REQ-COMMON-SAFESTATE-002); the shipped
 * implementation enforces that defensively with an infinite loop if a
 * handler misbehaves. That can't be exercised by a normal synchronous unit
 * test without hanging it, so the handlers registered below use
 * setjmp/longjmp - test-only tooling, not part of the shipped library - to
 * divert control flow away from sapi_safestate_enter() the same way a real
 * handler's "never returns" action (driving outputs safe, then resetting
 * the CPU) would. This lets the test verify the handler was invoked with
 * the correct level/reason/file/line, and that execution never falls
 * through past the call, without actually testing the infinite-loop
 * fallback itself (which requires a specialized harness, not a unit test).
 */
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include "safeapi/safestate/sapi_safestate.h"

static jmp_buf g_jmp;
static sapi_safestate_level_t g_captured_level;
static sapi_safestate_reason_t g_captured_reason;
static const char *g_captured_file;
static int32_t g_captured_line;
static const char *g_captured_message;
static int g_handler_calls;

static void diverting_handler(sapi_safestate_level_t level,
                               sapi_safestate_reason_t reason,
                               const char *file,
                               int32_t line,
                               const char *message)
{
    g_captured_level = level;
    g_captured_reason = reason;
    g_captured_file = file;
    g_captured_line = line;
    g_captured_message = message;
    g_handler_calls++;
    longjmp(g_jmp, 1);
}

static int g_degraded_calls;

static void returning_handler(sapi_safestate_level_t level __attribute__((unused)),
                               sapi_safestate_reason_t reason __attribute__((unused)),
                               const char *file __attribute__((unused)),
                               int32_t line __attribute__((unused)),
                               const char *message __attribute__((unused)))
{
    (void)file;
    (void)line;
    g_captured_level = level;
    g_captured_reason = reason;
    g_captured_message = message;
    g_degraded_calls++;
    /* Returns normally: DEGRADED is the one level allowed to do this. */
}

static void test_register_handler_validation(void)
{
    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, NULL)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_safestate_register_handler((sapi_safestate_level_t)99, diverting_handler)
           == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_DEGRADED, returning_handler)
           == SAPI_STATUS_OK);
}

static void test_degraded_returns(void)
{
    g_degraded_calls = 0;
    SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_DEGRADED, 7U);
    assert(g_degraded_calls == 1);
    assert(g_captured_level == SAPI_SAFESTATE_LEVEL_DEGRADED);
    assert(g_captured_reason == 7U);
    /* Reaching this point at all proves DEGRADED returned control. */
}

static void test_safe_does_not_return(void)
{
    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_SAFE, diverting_handler)
           == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_SAFE, 42U);
        /* Must never reach here: the handler always longjmp's away. */
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == SAPI_SAFESTATE_LEVEL_SAFE);
        assert(g_captured_reason == 42U);
        assert(g_captured_message == NULL);
    }
}

static void test_reboot_does_not_return(void)
{
    assert(sapi_safestate_register_handler(SAPI_SAFESTATE_LEVEL_REBOOT, diverting_handler)
           == SAPI_STATUS_OK);

    g_handler_calls = 0;
    if (setjmp(g_jmp) == 0)
    {
        SAPI_REBOOT(99U);
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == SAPI_SAFESTATE_LEVEL_REBOOT);
        assert(g_captured_reason == 99U);
    }
}

static void test_assert_macro(void)
{
    g_handler_calls = 0;

    /* True condition: SAPI_ASSERT does nothing, control returns normally. */
    SAPI_ASSERT(1 == 1);
    assert(g_handler_calls == 0);

    /* False condition: diverts through the SAFE handler registered above,
     * with the assert-specific reason code and the stringified expression. */
    if (setjmp(g_jmp) == 0)
    {
        SAPI_ASSERT(1 == 2);
        assert(0);
    }
    else
    {
        assert(g_handler_calls == 1);
        assert(g_captured_level == SAPI_SAFESTATE_LEVEL_SAFE);
        assert(g_captured_reason == SAPI_SAFESTATE_REASON_ASSERT_FAILED);
        assert(g_captured_message != NULL);
        assert(strcmp(g_captured_message, "1 == 2") == 0);
        assert(g_captured_line > 0);
    }
}

int main(void)
{
    test_register_handler_validation();
    test_degraded_returns();
    test_safe_does_not_return();
    test_reboot_does_not_return();
    test_assert_macro();
    return 0;
}

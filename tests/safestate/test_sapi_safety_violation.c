/* Tests for sapi_safety_violation - the opt-in notification hook shared by
 * the three new safety primitives (safe pointer, checked cast arithmetic,
 * bounds check). Unlike sapi_safestate's handler (which never returns),
 * this handler is documented to return normally, so no setjmp/longjmp
 * diversion is needed here (contrast test_sapi_safestate.c).
 */
#include <assert.h>
#include <string.h>

#include "safeapi/utils/safestate/sapi_safety_violation.h"

static sapi_safety_violation_kind_t g_captured_kind;
static const char *g_captured_file;
static int32_t g_captured_line;
static const char *g_captured_message;
static int g_handler_calls;

static void capturing_handler(sapi_safety_violation_kind_t kind,
                               const char *file,
                               int32_t line,
                               const char *message)
{
    g_captured_kind = kind;
    g_captured_file = file;
    g_captured_line = line;
    g_captured_message = message;
    g_handler_calls++;
}

static void reset_capture(void)
{
    g_captured_kind = SAPI_SAFETY_VIOLATION_CORRUPTION;
    g_captured_file = NULL;
    g_captured_line = 0;
    g_captured_message = NULL;
    g_handler_calls = 0;
}

static void test_register_handler_rejects_null(void)
{
    assert(sapi_safety_violation_register_handler(NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_report_is_noop_with_no_handler_registered(void)
{
    /* REQ-COMMON-SAFETYVIOLATION-002: with nothing registered, reporting a
     * violation must not crash and must not itself become a new failure
     * mode - there is nothing observable to assert on beyond "it returns". */
    sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_OUT_OF_RANGE, __FILE__, __LINE__, "no handler yet");
}

static void test_registered_handler_is_invoked_with_correct_args(void)
{
    reset_capture();
    assert(sapi_safety_violation_register_handler(capturing_handler) == SAPI_STATUS_OK);

    sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_OVERFLOW, "some_file.c", 42, "overflow detail");

    assert(g_handler_calls == 1);
    assert(g_captured_kind == SAPI_SAFETY_VIOLATION_OVERFLOW);
    assert(strcmp(g_captured_file, "some_file.c") == 0);
    assert(g_captured_line == 42);
    assert(strcmp(g_captured_message, "overflow detail") == 0);
}

static void test_registering_again_replaces_previous_handler(void)
{
    reset_capture();
    assert(sapi_safety_violation_register_handler(capturing_handler) == SAPI_STATUS_OK);
    sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_CORRUPTION, __FILE__, __LINE__, "first");
    assert(g_handler_calls == 1);

    /* A second registration must replace, not stack, the handler - same
     * convention as sapi_safestate_register_handler(). */
    assert(sapi_safety_violation_register_handler(capturing_handler) == SAPI_STATUS_OK);
    sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_CORRUPTION, __FILE__, __LINE__, "second");
    assert(g_handler_calls == 2);
}

int main(void)
{
    test_register_handler_rejects_null();
    test_report_is_noop_with_no_handler_registered();
    test_registered_handler_is_invoked_with_correct_args();
    test_registering_again_replaces_previous_handler();
    return 0;
}

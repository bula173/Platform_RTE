/* Tests for sapi_log_write_event(): the
 * "Site=<site> Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src>
 * Destination=<dst> Type=<type> Info=<info>[ <extra_fields>]"
 * space-separated Key=Value line, NULL info/extra_fields handling,
 * level_to_string mapping, Timestamp degrading to "0" rather than
 * failing when no sapi_timer backend is registered, and the pre-existing
 * sapi_log_write()/no-backend contract being unaffected by this addition.
 *
 * Verified via exact expected-line string comparison rather than
 * tokenizing the emitted line: unlike a pipe-delimited format, a
 * space-separated Key=Value line is not unambiguously re-splittable once
 * Info (or extra_fields) itself contains a space - by design, this format
 * favors human readability over strict re-parseability (best-effort,
 * non-safety diagnostic logging - REQ-OAL-LOG-001), so this test checks
 * what sapi_log_write_event() actually produces, not a round-trip through
 * a naive splitter. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "safeapi/log/sapi_log.h"
#include "safeapi_backend/log/sapi_log_backend.h"
#include "safeapi/timer/sapi_timer.h"
#include "safeapi_backend/timer/sapi_timer_backend.h"

/* --- mock log backend: captures the last (level, tag, message) --- */
static sapi_log_level_t g_last_level;
static char g_last_tag[64];
static char g_last_message[SAPI_LOG_EVENT_LINE_MAX_LEN + 16];
static int g_write_calls = 0;

static void reset_capture(void)
{
    g_last_level = SAPI_LOG_LEVEL_DEBUG;
    memset(g_last_tag, 0, sizeof(g_last_tag));
    memset(g_last_message, 0, sizeof(g_last_message));
    g_write_calls = 0;
}

static void mock_log_write(sapi_log_level_t level, const char *tag, const char *message)
{
    g_last_level = level;
    if (tag != NULL)
    {
        (void)strncpy(g_last_tag, tag, sizeof(g_last_tag) - 1U);
    }
    if (message != NULL)
    {
        (void)strncpy(g_last_message, message, sizeof(g_last_message) - 1U);
    }
    g_write_calls++;
}

static const sapi_log_backend_t g_mock_log_backend = { NULL, mock_log_write };

static int g_mock_init_calls = 0;

static sapi_status_t mock_log_init(void)
{
    g_mock_init_calls++;
    return SAPI_STATUS_OK;
}

static const sapi_log_backend_t g_mock_log_backend_with_init = { mock_log_init, mock_log_write };
static const sapi_log_backend_t g_mock_log_backend_no_write = { mock_log_init, NULL };

/* --- mock timer backend: only `now` populated, fixed value --- */
static sapi_status_t mock_timer_now(sapi_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = 123456789U;
    return SAPI_STATUS_OK;
}

static const sapi_timer_backend_t g_mock_timer_backend = { NULL, NULL, NULL, NULL, mock_timer_now };

int main(void)
{
    char expected[SAPI_LOG_EVENT_LINE_MAX_LEN + 32];

    /* sapi_log_level_to_string(): fixed mapping, defensive UNKNOWN. */
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_DEBUG), "DEBUG") == 0);
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_WARNING), "WARNING") == 0);
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(sapi_log_level_to_string((sapi_log_level_t)99), "UNKNOWN") == 0);

    /* No backend registered yet: silent no-op, same contract as
     * sapi_log_write() - REQ-OAL-LOG-001. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_INFO, "WEST", 7U, "A/WEST", "B", "AB_SAMPLE", "cycle sample", NULL);
    assert(g_write_calls == 0);

    /* sapi_log_write() with no backend registered: silent no-op, same
     * REQ-OAL-LOG-001 contract as sapi_log_write_event() above. */
    reset_capture();
    sapi_log_write(SAPI_LOG_LEVEL_INFO, "TAG", "message");
    assert(g_write_calls == 0);

    /* sapi_log_init() with no backend registered: not an error, just OK. */
    assert(sapi_log_init() == SAPI_STATUS_OK);

    /* Registering NULL is rejected. */
    assert(sapi_log_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_log_register_backend(&g_mock_log_backend) == SAPI_STATUS_OK);

    /* Backend registered but its init slot is NULL: still not an error. */
    assert(sapi_log_init() == SAPI_STATUS_OK);

    /* Backend registered but its write slot is NULL: silent no-op. */
    assert(sapi_log_register_backend(&g_mock_log_backend_no_write) == SAPI_STATUS_OK);
    reset_capture();
    sapi_log_write(SAPI_LOG_LEVEL_INFO, "TAG", "message");
    assert(g_write_calls == 0);

    /* A backend with a non-NULL init slot has it actually dispatched. */
    assert(sapi_log_register_backend(&g_mock_log_backend_with_init) == SAPI_STATUS_OK);
    assert(sapi_log_init() == SAPI_STATUS_OK);
    assert(g_mock_init_calls == 1);

    /* sapi_log_write() dispatches to a registered backend's write slot,
     * independently of sapi_log_write_event()'s own formatting path. */
    reset_capture();
    sapi_log_write(SAPI_LOG_LEVEL_WARNING, "TAG", "message");
    assert(g_write_calls == 1);
    assert(g_last_level == SAPI_LOG_LEVEL_WARNING);
    assert(strcmp(g_last_tag, "TAG") == 0);
    assert(strcmp(g_last_message, "message") == 0);

    assert(sapi_log_register_backend(&g_mock_log_backend) == SAPI_STATUS_OK);

    /* No sapi_timer backend registered: Timestamp field degrades to "0"
     * rather than the call being skipped or failing. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_INFO, "WEST", 7U, "A/WEST", "B", "AB_SAMPLE", "cycle sample", NULL);
    assert(g_write_calls == 1);
    assert(g_last_level == SAPI_LOG_LEVEL_INFO);
    assert(strcmp(g_last_tag, "A/WEST") == 0); /* source is passed as tag */
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=0 Level=INFO Cycle=7 Source=A/WEST Destination=B Type=AB_SAMPLE "
                    "Info=cycle sample");
    assert(strcmp(g_last_message, expected) == 0);

    /* With a timer backend registered, Timestamp reflects it. */
    assert(sapi_timer_register_backend(&g_mock_timer_backend) == SAPI_STATUS_OK);
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_ERROR, "EAST", 42U, "B/EAST", "A", "DISAGREE", "M24 vs M15", NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site=EAST Timestamp=123456789 Level=ERROR Cycle=42 Source=B/EAST Destination=A "
                    "Type=DISAGREE Info=M24 vs M15");
    assert(strcmp(g_last_message, expected) == 0);

    /* NULL site -> empty Site= value, not a dropped field (Site is
     * first, so this also checks the very first field's own NULL
     * handling, distinct from every later field's). */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, NULL, 1U, "C", "-", "M136", "info", NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site= Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 Info=info");
    assert(strcmp(g_last_message, expected) == 0);

    /* NULL info -> empty Info= value, not a dropped field. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", NULL, NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 Info=");
    assert(strcmp(g_last_message, expected) == 0);

    /* extra_fields, when non-NULL, is appended verbatim after Info=...
     * behind one separating space (not wrapped in another key). */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", "info", "D_LRBG=42 CRC=OK");
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 "
                    "Info=info D_LRBG=42 CRC=OK");
    assert(strcmp(g_last_message, expected) == 0);

    /* extra_fields NULL -> no trailing space/text at all after Info=. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", "info", NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 "
                    "Info=info");
    assert(strcmp(g_last_message, expected) == 0);

    return 0;
}

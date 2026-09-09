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
#include "safeapi/oal/log/sapi_log.h"
#include "safeapi_backend/log/sapi_log_backend.h"
#include "safeapi/oal/timer/sapi_timer.h"
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

    /* --- sapi_log_level_from_string(): the four canonical spellings, the
     *     no-match case, and NULL args (REQ-OAL-LOG-016). --- */
    {
        sapi_log_level_t parsed = SAPI_LOG_LEVEL_ERROR;

        assert(sapi_log_level_from_string("DEBUG", &parsed) == SAPI_STATUS_OK && parsed == SAPI_LOG_LEVEL_DEBUG);
        assert(sapi_log_level_from_string("INFO", &parsed) == SAPI_STATUS_OK && parsed == SAPI_LOG_LEVEL_INFO);
        assert(sapi_log_level_from_string("WARNING", &parsed) == SAPI_STATUS_OK && parsed == SAPI_LOG_LEVEL_WARNING);
        assert(sapi_log_level_from_string("ERROR", &parsed) == SAPI_STATUS_OK && parsed == SAPI_LOG_LEVEL_ERROR);
        /* case-sensitive, and unknown spellings rejected without touching *out_level */
        parsed = SAPI_LOG_LEVEL_WARNING;
        assert(sapi_log_level_from_string("debug", &parsed) == SAPI_STATUS_INVALID_PARAM && parsed == SAPI_LOG_LEVEL_WARNING);
        assert(sapi_log_level_from_string("TRACE", &parsed) == SAPI_STATUS_INVALID_PARAM && parsed == SAPI_LOG_LEVEL_WARNING);
        assert(sapi_log_level_from_string(NULL, &parsed) == SAPI_STATUS_INVALID_PARAM);
        assert(sapi_log_level_from_string("INFO", NULL) == SAPI_STATUS_INVALID_PARAM);
    }

    /* --- sapi_log_set_level() / sapi_log_get_level() (REQ-OAL-LOG-015):
     *     the threshold gates both sapi_log_write() and
     *     sapi_log_write_event(); default is DEBUG (nothing filtered). --- */
    assert(sapi_log_get_level() == SAPI_LOG_LEVEL_DEBUG);

    sapi_log_set_level(SAPI_LOG_LEVEL_WARNING);
    assert(sapi_log_get_level() == SAPI_LOG_LEVEL_WARNING);

    reset_capture();
    sapi_log_write(SAPI_LOG_LEVEL_INFO, "TAG", "below threshold");
    assert(g_write_calls == 0); /* INFO < WARNING -> dropped */
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", "below", NULL);
    assert(g_write_calls == 0); /* DEBUG < WARNING -> dropped before formatting */
    sapi_log_write(SAPI_LOG_LEVEL_WARNING, "TAG", "at threshold");
    assert(g_write_calls == 1);
    sapi_log_write(SAPI_LOG_LEVEL_ERROR, "TAG", "above threshold");
    assert(g_write_calls == 2);

    /* Out-of-range set is ignored (threshold unchanged). */
    sapi_log_set_level((sapi_log_level_t)99);
    assert(sapi_log_get_level() == SAPI_LOG_LEVEL_WARNING);

    /* Restore the default so ordering with any future test stays clean. */
    sapi_log_set_level(SAPI_LOG_LEVEL_DEBUG);
    reset_capture();
    sapi_log_write(SAPI_LOG_LEVEL_DEBUG, "TAG", "restored");
    assert(g_write_calls == 1);

    /* --- sapi_log_fields_t builder + sapi_log_write_event_fields()
     *     (REQ-OAL-LOG-017): typed key=value pairs, one separating space
     *     between them and none at the ends, then handed straight to the
     *     event writer as extra_fields. --- */
    {
        sapi_log_fields_t f;

        /* Empty builder -> "" -> no extra text at all after Info= (same
         * as passing NULL). */
        sapi_log_fields_reset(&f);
        assert(strcmp(sapi_log_fields_c_str(&f), "") == 0);
        reset_capture();
        sapi_log_write_event_fields(SAPI_LOG_LEVEL_INFO, "WEST", 3U, "A/WEST", "IL", "ROUTE",
                                     "no fields", &f);
        (void)snprintf(expected, sizeof(expected),
                        "Site=WEST Timestamp=123456789 Level=INFO Cycle=3 Source=A/WEST Destination=IL "
                        "Type=ROUTE Info=no fields");
        assert(strcmp(g_last_message, expected) == 0);

        /* One pair of each type; chaining returns the builder. */
        sapi_log_fields_reset(&f);
        assert(sapi_log_fields_add_u32(&f, "route", 4U) == &f);
        sapi_log_fields_add_i32(&f, "d_lrbg", -12);
        sapi_log_fields_add_u64(&f, "seq", 4294967296ULL);
        sapi_log_fields_add_i64(&f, "off", -1);
        sapi_log_fields_add_hex_u32(&f, "crc", 0xABU, 4U);
        sapi_log_fields_add_bool(&f, "stub", true);
        sapi_log_fields_add_str(&f, "result", "OK");
        sapi_log_fields_add_str(&f, "note", NULL); /* NULL string -> empty value */
        assert(strcmp(sapi_log_fields_c_str(&f),
                      "route=4 d_lrbg=-12 seq=4294967296 off=-1 crc=0x00ab stub=true result=OK note=") == 0);

        reset_capture();
        sapi_log_write_event_fields(SAPI_LOG_LEVEL_DEBUG, "EAST", 9U, "B/EAST", "IL", "ROUTE_FSM",
                                     "step", &f);
        (void)snprintf(expected, sizeof(expected),
                        "Site=EAST Timestamp=123456789 Level=DEBUG Cycle=9 Source=B/EAST Destination=IL "
                        "Type=ROUTE_FSM Info=step "
                        "route=4 d_lrbg=-12 seq=4294967296 off=-1 crc=0x00ab stub=true result=OK note=");
        assert(strcmp(g_last_message, expected) == 0);

        /* NULL builder tolerated by every entry point. */
        sapi_log_fields_reset(NULL);
        assert(sapi_log_fields_add_u32(NULL, "x", 1U) == NULL);
        assert(strcmp(sapi_log_fields_c_str(NULL), "") == 0);
        reset_capture();
        sapi_log_write_event_fields(SAPI_LOG_LEVEL_INFO, "WEST", 1U, "A", "B", "T", "i", NULL);
        (void)snprintf(expected, sizeof(expected),
                        "Site=WEST Timestamp=123456789 Level=INFO Cycle=1 Source=A Destination=B Type=T Info=i");
        assert(strcmp(g_last_message, expected) == 0);

        /* sapi_log_fields_c_str() output is also valid as the `extra_fields`
         * arg of the plain writer. */
        sapi_log_fields_reset(&f);
        sapi_log_fields_add_u32(&f, "n", 7U);
        reset_capture();
        sapi_log_write_event(SAPI_LOG_LEVEL_INFO, "WEST", 2U, "A", "B", "T", "i", sapi_log_fields_c_str(&f));
        (void)snprintf(expected, sizeof(expected),
                        "Site=WEST Timestamp=123456789 Level=INFO Cycle=2 Source=A Destination=B Type=T "
                        "Info=i n=7");
        assert(strcmp(g_last_message, expected) == 0);
    }

    return 0;
}

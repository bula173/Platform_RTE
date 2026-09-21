/* Tests for rte_log_write_event(): the
 * "Site=<site> Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src>
 * Destination=<dst> Type=<type> Info=<info>[ <extra_fields>]"
 * space-separated Key=Value line, NULL info/extra_fields handling,
 * level_to_string mapping, Timestamp degrading to "0" rather than
 * failing when no rte_timer OSAdapter is registered, and the pre-existing
 * rte_log_write()/no-osadapter contract being unaffected by this addition.
 *
 * Verified via exact expected-line string comparison rather than
 * tokenizing the emitted line: unlike a pipe-delimited format, a
 * space-separated Key=Value line is not unambiguously re-splittable once
 * Info (or extra_fields) itself contains a space - by design, this format
 * favors human readability over strict re-parseability (best-effort,
 * non-safety diagnostic logging - REQ-OAL-LOG-001), so this test checks
 * what rte_log_write_event() actually produces, not a round-trip through
 * a naive splitter. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "safeapi/oal/log/rte_log.h"
#include "safeapi_osadapter/log/rte_osadapter_log.h"
#include "safeapi/oal/timer/rte_timer.h"
#include "safeapi_osadapter/timer/rte_osadapter_timer.h"

/* --- mock log OSAdapter: captures the last (level, tag, message) --- */
static rte_log_level_t g_last_level;
static char g_last_tag[64];
static char g_last_message[RTE_LOG_EVENT_LINE_MAX_LEN + 16];
static int g_write_calls = 0;

static void reset_capture(void)
{
    g_last_level = RTE_LOG_LEVEL_DEBUG;
    memset(g_last_tag, 0, sizeof(g_last_tag));
    memset(g_last_message, 0, sizeof(g_last_message));
    g_write_calls = 0;
}

static void mock_log_write(rte_log_level_t level, const char *tag, const char *message)
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

static const rte_osadapter_log_t g_mock_log_osadapter = { NULL, mock_log_write };

static int g_mock_init_calls = 0;

static rte_status_t mock_log_init(void)
{
    g_mock_init_calls++;
    return RTE_STATUS_OK;
}

static const rte_osadapter_log_t g_mock_log_osadapter_with_init = { mock_log_init, mock_log_write };
static const rte_osadapter_log_t g_mock_log_osadapter_no_write = { mock_log_init, NULL };

/* --- mock timer OSAdapter: only `now` populated, fixed value --- */
static rte_status_t mock_timer_now(rte_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = 123456789U;
    return RTE_STATUS_OK;
}

static const rte_osadapter_timer_t g_mock_timer_osadapter = { NULL, NULL, NULL, NULL, mock_timer_now };

int main(void)
{
    char expected[RTE_LOG_EVENT_LINE_MAX_LEN + 32];

    /* rte_log_level_to_string(): fixed mapping, defensive UNKNOWN. */
    assert(strcmp(rte_log_level_to_string(RTE_LOG_LEVEL_DEBUG), "DEBUG") == 0);
    assert(strcmp(rte_log_level_to_string(RTE_LOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(rte_log_level_to_string(RTE_LOG_LEVEL_WARNING), "WARNING") == 0);
    assert(strcmp(rte_log_level_to_string(RTE_LOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(rte_log_level_to_string((rte_log_level_t)99), "UNKNOWN") == 0);

    /* No OSAdapter registered yet: silent no-op, same contract as
     * rte_log_write() - REQ-OAL-LOG-001. */
    reset_capture();
    rte_log_write_event(RTE_LOG_LEVEL_INFO, "WEST", 7U, "A/WEST", "B", "AB_SAMPLE", "cycle sample", NULL);
    assert(g_write_calls == 0);

    /* rte_log_write() with no OSAdapter registered: silent no-op, same
     * REQ-OAL-LOG-001 contract as rte_log_write_event() above. */
    reset_capture();
    rte_log_write(RTE_LOG_LEVEL_INFO, "TAG", "message");
    assert(g_write_calls == 0);

    /* rte_log_init() with no OSAdapter registered: not an error, just OK. */
    assert(rte_log_init() == RTE_STATUS_OK);

    /* Registering NULL is rejected. */
    assert(rte_osadapter_log_register(NULL) == RTE_STATUS_INVALID_PARAM);

    assert(rte_osadapter_log_register(&g_mock_log_osadapter) == RTE_STATUS_OK);

    /* OSAdapter registered but its init slot is NULL: still not an error. */
    assert(rte_log_init() == RTE_STATUS_OK);

    /* OSAdapter registered but its write slot is NULL: silent no-op. */
    assert(rte_osadapter_log_register(&g_mock_log_osadapter_no_write) == RTE_STATUS_OK);
    reset_capture();
    rte_log_write(RTE_LOG_LEVEL_INFO, "TAG", "message");
    assert(g_write_calls == 0);

    /* An OSAdapter with a non-NULL init slot has it actually dispatched. */
    assert(rte_osadapter_log_register(&g_mock_log_osadapter_with_init) == RTE_STATUS_OK);
    assert(rte_log_init() == RTE_STATUS_OK);
    assert(g_mock_init_calls == 1);

    /* rte_log_write() dispatches to a registered OSAdapter's write slot,
     * independently of rte_log_write_event()'s own formatting path. */
    reset_capture();
    rte_log_write(RTE_LOG_LEVEL_WARNING, "TAG", "message");
    assert(g_write_calls == 1);
    assert(g_last_level == RTE_LOG_LEVEL_WARNING);
    assert(strcmp(g_last_tag, "TAG") == 0);
    assert(strcmp(g_last_message, "message") == 0);

    assert(rte_osadapter_log_register(&g_mock_log_osadapter) == RTE_STATUS_OK);

    /* No rte_timer OSAdapter registered: Timestamp field degrades to "0"
     * rather than the call being skipped or failing. */
    reset_capture();
    rte_log_write_event(RTE_LOG_LEVEL_INFO, "WEST", 7U, "A/WEST", "B", "AB_SAMPLE", "cycle sample", NULL);
    assert(g_write_calls == 1);
    assert(g_last_level == RTE_LOG_LEVEL_INFO);
    assert(strcmp(g_last_tag, "A/WEST") == 0); /* source is passed as tag */
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=0 Level=INFO Cycle=7 Source=A/WEST Destination=B Type=AB_SAMPLE "
                    "Info=cycle sample");
    assert(strcmp(g_last_message, expected) == 0);

    /* With a timer OSAdapter registered, Timestamp reflects it. */
    assert(rte_osadapter_timer_register(&g_mock_timer_osadapter) == RTE_STATUS_OK);
    reset_capture();
    rte_log_write_event(RTE_LOG_LEVEL_ERROR, "EAST", 42U, "B/EAST", "A", "DISAGREE", "M24 vs M15", NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site=EAST Timestamp=123456789 Level=ERROR Cycle=42 Source=B/EAST Destination=A "
                    "Type=DISAGREE Info=M24 vs M15");
    assert(strcmp(g_last_message, expected) == 0);

    /* NULL site -> empty Site= value, not a dropped field (Site is
     * first, so this also checks the very first field's own NULL
     * handling, distinct from every later field's). */
    reset_capture();
    rte_log_write_event(RTE_LOG_LEVEL_DEBUG, NULL, 1U, "C", "-", "M136", "info", NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site= Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 Info=info");
    assert(strcmp(g_last_message, expected) == 0);

    /* NULL info -> empty Info= value, not a dropped field. */
    reset_capture();
    rte_log_write_event(RTE_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", NULL, NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 Info=");
    assert(strcmp(g_last_message, expected) == 0);

    /* extra_fields, when non-NULL, is appended verbatim after Info=...
     * behind one separating space (not wrapped in another key). */
    reset_capture();
    rte_log_write_event(RTE_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", "info", "D_LRBG=42 CRC=OK");
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 "
                    "Info=info D_LRBG=42 CRC=OK");
    assert(strcmp(g_last_message, expected) == 0);

    /* extra_fields NULL -> no trailing space/text at all after Info=. */
    reset_capture();
    rte_log_write_event(RTE_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", "info", NULL);
    (void)snprintf(expected, sizeof(expected),
                    "Site=WEST Timestamp=123456789 Level=DEBUG Cycle=1 Source=C Destination=- Type=M136 "
                    "Info=info");
    assert(strcmp(g_last_message, expected) == 0);

    /* --- rte_log_level_from_string(): the four canonical spellings, the
     *     no-match case, and NULL args (REQ-OAL-LOG-016). --- */
    {
        rte_log_level_t parsed = RTE_LOG_LEVEL_ERROR;

        assert(rte_log_level_from_string("DEBUG", &parsed) == RTE_STATUS_OK && parsed == RTE_LOG_LEVEL_DEBUG);
        assert(rte_log_level_from_string("INFO", &parsed) == RTE_STATUS_OK && parsed == RTE_LOG_LEVEL_INFO);
        assert(rte_log_level_from_string("WARNING", &parsed) == RTE_STATUS_OK && parsed == RTE_LOG_LEVEL_WARNING);
        assert(rte_log_level_from_string("ERROR", &parsed) == RTE_STATUS_OK && parsed == RTE_LOG_LEVEL_ERROR);
        /* case-sensitive, and unknown spellings rejected without touching *out_level */
        parsed = RTE_LOG_LEVEL_WARNING;
        assert(rte_log_level_from_string("debug", &parsed) == RTE_STATUS_INVALID_PARAM && parsed == RTE_LOG_LEVEL_WARNING);
        assert(rte_log_level_from_string("TRACE", &parsed) == RTE_STATUS_INVALID_PARAM && parsed == RTE_LOG_LEVEL_WARNING);
        assert(rte_log_level_from_string(NULL, &parsed) == RTE_STATUS_INVALID_PARAM);
        assert(rte_log_level_from_string("INFO", NULL) == RTE_STATUS_INVALID_PARAM);
    }

    /* --- rte_log_set_level() / rte_log_get_level() (REQ-OAL-LOG-015):
     *     the threshold gates both rte_log_write() and
     *     rte_log_write_event(); default is DEBUG (nothing filtered). --- */
    assert(rte_log_get_level() == RTE_LOG_LEVEL_DEBUG);

    rte_log_set_level(RTE_LOG_LEVEL_WARNING);
    assert(rte_log_get_level() == RTE_LOG_LEVEL_WARNING);

    reset_capture();
    rte_log_write(RTE_LOG_LEVEL_INFO, "TAG", "below threshold");
    assert(g_write_calls == 0); /* INFO < WARNING -> dropped */
    rte_log_write_event(RTE_LOG_LEVEL_DEBUG, "WEST", 1U, "C", "-", "M136", "below", NULL);
    assert(g_write_calls == 0); /* DEBUG < WARNING -> dropped before formatting */
    rte_log_write(RTE_LOG_LEVEL_WARNING, "TAG", "at threshold");
    assert(g_write_calls == 1);
    rte_log_write(RTE_LOG_LEVEL_ERROR, "TAG", "above threshold");
    assert(g_write_calls == 2);

    /* Out-of-range set is ignored (threshold unchanged). */
    rte_log_set_level((rte_log_level_t)99);
    assert(rte_log_get_level() == RTE_LOG_LEVEL_WARNING);

    /* Restore the default so ordering with any future test stays clean. */
    rte_log_set_level(RTE_LOG_LEVEL_DEBUG);
    reset_capture();
    rte_log_write(RTE_LOG_LEVEL_DEBUG, "TAG", "restored");
    assert(g_write_calls == 1);

    /* --- rte_log_fields_t builder + rte_log_write_event_fields()
     *     (REQ-OAL-LOG-017): typed key=value pairs, one separating space
     *     between them and none at the ends, then handed straight to the
     *     event writer as extra_fields. --- */
    {
        rte_log_fields_t f;

        /* Empty builder -> "" -> no extra text at all after Info= (same
         * as passing NULL). */
        rte_log_fields_reset(&f);
        assert(strcmp(rte_log_fields_c_str(&f), "") == 0);
        reset_capture();
        rte_log_write_event_fields(RTE_LOG_LEVEL_INFO, "WEST", 3U, "A/WEST", "IL", "ROUTE",
                                     "no fields", &f);
        (void)snprintf(expected, sizeof(expected),
                        "Site=WEST Timestamp=123456789 Level=INFO Cycle=3 Source=A/WEST Destination=IL "
                        "Type=ROUTE Info=no fields");
        assert(strcmp(g_last_message, expected) == 0);

        /* One pair of each type; chaining returns the builder. */
        rte_log_fields_reset(&f);
        assert(rte_log_fields_add_u32(&f, "route", 4U) == &f);
        rte_log_fields_add_i32(&f, "d_lrbg", -12);
        rte_log_fields_add_u64(&f, "seq", 4294967296ULL);
        rte_log_fields_add_i64(&f, "off", -1);
        rte_log_fields_add_hex_u32(&f, "crc", 0xABU, 4U);
        rte_log_fields_add_bool(&f, "stub", true);
        rte_log_fields_add_str(&f, "result", "OK");
        rte_log_fields_add_str(&f, "note", NULL); /* NULL string -> empty value */
        assert(strcmp(rte_log_fields_c_str(&f),
                      "route=4 d_lrbg=-12 seq=4294967296 off=-1 crc=0x00ab stub=true result=OK note=") == 0);

        reset_capture();
        rte_log_write_event_fields(RTE_LOG_LEVEL_DEBUG, "EAST", 9U, "B/EAST", "IL", "ROUTE_FSM",
                                     "step", &f);
        (void)snprintf(expected, sizeof(expected),
                        "Site=EAST Timestamp=123456789 Level=DEBUG Cycle=9 Source=B/EAST Destination=IL "
                        "Type=ROUTE_FSM Info=step "
                        "route=4 d_lrbg=-12 seq=4294967296 off=-1 crc=0x00ab stub=true result=OK note=");
        assert(strcmp(g_last_message, expected) == 0);

        /* NULL builder tolerated by every entry point. */
        rte_log_fields_reset(NULL);
        assert(rte_log_fields_add_u32(NULL, "x", 1U) == NULL);
        assert(strcmp(rte_log_fields_c_str(NULL), "") == 0);
        reset_capture();
        rte_log_write_event_fields(RTE_LOG_LEVEL_INFO, "WEST", 1U, "A", "B", "T", "i", NULL);
        (void)snprintf(expected, sizeof(expected),
                        "Site=WEST Timestamp=123456789 Level=INFO Cycle=1 Source=A Destination=B Type=T Info=i");
        assert(strcmp(g_last_message, expected) == 0);

        /* rte_log_fields_c_str() output is also valid as the `extra_fields`
         * arg of the plain writer. */
        rte_log_fields_reset(&f);
        rte_log_fields_add_u32(&f, "n", 7U);
        reset_capture();
        rte_log_write_event(RTE_LOG_LEVEL_INFO, "WEST", 2U, "A", "B", "T", "i", rte_log_fields_c_str(&f));
        (void)snprintf(expected, sizeof(expected),
                        "Site=WEST Timestamp=123456789 Level=INFO Cycle=2 Source=A Destination=B Type=T "
                        "Info=i n=7");
        assert(strcmp(g_last_message, expected) == 0);
    }

    return 0;
}

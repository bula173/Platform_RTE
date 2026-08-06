/* Tests for sapi_log_write_event() (ADR-019-era addition): field order and
 * delimiters of the TIMESTAMP|LEVEL|CYCLE|SOURCE|DESTINATION|TYPE|INFO
 * [|EXTRA] line, NULL info/extra_fields handling, level_to_string mapping,
 * TIMESTAMP degrading to "0" rather than failing when no sapi_timer backend
 * is registered, and the pre-existing sapi_log_write()/no-backend contract
 * being unaffected by this addition. */
#include <assert.h>
#include <string.h>
#include "safeapi/log/sapi_log.h"
#include "safeapi/timer/sapi_timer.h"

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

/* --- mock timer backend: only `now` populated, fixed value --- */
static sapi_status_t mock_timer_now(sapi_timestamp_ms_t *out_now_ms)
{
    *out_now_ms = 123456789U;
    return SAPI_STATUS_OK;
}

static const sapi_timer_backend_t g_mock_timer_backend = { NULL, NULL, NULL, NULL, mock_timer_now };

/* Splits a pipe-delimited line into up to max_fields tokens (simple,
 * test-only - not using sapi_string_split_next() to keep this test
 * independent of the module under indirect test). Returns field count. */
static size_t split_pipe(const char *line, char fields[][128], size_t max_fields)
{
    size_t field_count = 0;
    size_t char_index = 0;
    size_t field_char_index = 0;

    while ((line[char_index] != '\0') && (field_count < max_fields))
    {
        if (line[char_index] == '|')
        {
            fields[field_count][field_char_index] = '\0';
            field_count++;
            field_char_index = 0;
        }
        else if (field_char_index < 127U)
        {
            fields[field_count][field_char_index] = line[char_index];
            field_char_index++;
        }
        else
        {
            /* field too long for this test's own buffer - ignore extra */
        }
        char_index++;
    }
    if (field_count < max_fields)
    {
        fields[field_count][field_char_index] = '\0';
        field_count++;
    }
    return field_count;
}

int main(void)
{
    char fields[9][128];

    /* sapi_log_level_to_string(): fixed mapping, defensive UNKNOWN. */
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_DEBUG), "DEBUG") == 0);
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_WARNING), "WARNING") == 0);
    assert(strcmp(sapi_log_level_to_string(SAPI_LOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(sapi_log_level_to_string((sapi_log_level_t)99), "UNKNOWN") == 0);

    /* No backend registered yet: silent no-op, same contract as
     * sapi_log_write() - REQ-OAL-LOG-001. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_INFO, 7U, "A/WEST", "B", "AB_SAMPLE", "cycle sample", NULL);
    assert(g_write_calls == 0);

    assert(sapi_log_register_backend(&g_mock_log_backend) == SAPI_STATUS_OK);

    /* No sapi_timer backend registered: TIMESTAMP field degrades to "0"
     * rather than the call being skipped or failing. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_INFO, 7U, "A/WEST", "B", "AB_SAMPLE", "cycle sample", NULL);
    assert(g_write_calls == 1);
    assert(g_last_level == SAPI_LOG_LEVEL_INFO);
    assert(strcmp(g_last_tag, "A/WEST") == 0); /* source is passed as tag */
    {
        size_t field_count = split_pipe(g_last_message, fields, 9U);
        assert(field_count == 7U);
        assert(strcmp(fields[0], "0") == 0); /* TIMESTAMP degraded */
        assert(strcmp(fields[1], "INFO") == 0);
        assert(strcmp(fields[2], "7") == 0);
        assert(strcmp(fields[3], "A/WEST") == 0);
        assert(strcmp(fields[4], "B") == 0);
        assert(strcmp(fields[5], "AB_SAMPLE") == 0);
        assert(strcmp(fields[6], "cycle sample") == 0);
    }

    /* With a timer backend registered, TIMESTAMP reflects it. */
    assert(sapi_timer_register_backend(&g_mock_timer_backend) == SAPI_STATUS_OK);
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_ERROR, 42U, "B/EAST", "A", "DISAGREE", "M24 vs M15", NULL);
    {
        size_t field_count = split_pipe(g_last_message, fields, 9U);
        assert(field_count == 7U);
        assert(strcmp(fields[0], "123456789") == 0);
        assert(strcmp(fields[1], "ERROR") == 0);
        assert(strcmp(fields[2], "42") == 0);
        assert(strcmp(fields[3], "B/EAST") == 0);
        assert(strcmp(fields[4], "A") == 0);
        assert(strcmp(fields[5], "DISAGREE") == 0);
        assert(strcmp(fields[6], "M24 vs M15") == 0);
    }

    /* NULL info -> empty INFO field, not a dropped field. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, 1U, "C", "-", "M136", NULL, NULL);
    {
        size_t field_count = split_pipe(g_last_message, fields, 9U);
        assert(field_count == 7U);
        assert(strcmp(fields[6], "") == 0);
    }

    /* extra_fields, when non-NULL, is appended as an 8th field. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, 1U, "C", "-", "M136", "info", "D_LRBG=42 CRC=OK");
    {
        size_t field_count = split_pipe(g_last_message, fields, 9U);
        assert(field_count == 8U);
        assert(strcmp(fields[7], "D_LRBG=42 CRC=OK") == 0);
    }

    /* extra_fields NULL -> no 8th field/trailing delimiter at all. */
    reset_capture();
    sapi_log_write_event(SAPI_LOG_LEVEL_DEBUG, 1U, "C", "-", "M136", "info", NULL);
    {
        size_t field_count = split_pipe(g_last_message, fields, 9U);
        assert(field_count == 7U);
    }

    return 0;
}

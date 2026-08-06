/**
 * @file sapi_log.c
 * @ingroup LOG
 * @brief Logging service: dispatches to the backend registered via
 *        sapi_log_register_backend() (ADR-005). No backend registered is
 *        not an error for this service - see sapi_log.h.
 *
 * sapi_log_write_event()'s structured-line formatting (added alongside
 * this file's own module) is what introduces this module's only two new
 * dependencies, `safeapi::string` (bounded formatting - ADR-006) and
 * `safeapi::timer` (TIMESTAMP field - sapi_timer_now()); sapi_log_write()
 * itself and the backend dispatch below are unchanged and still have
 * neither dependency.
 */
#include "safeapi/log/sapi_log.h"
#include "safeapi/string/sapi_string.h"
#include "safeapi/timer/sapi_timer.h"

/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_log_backend_t *s_backend = NULL;

sapi_status_t sapi_log_register_backend(const sapi_log_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_log_init(void)
{
    if ((s_backend == NULL) || (s_backend->init == NULL))
    {
        return SAPI_STATUS_OK;
    }
    return s_backend->init();
}

void sapi_log_write(sapi_log_level_t level, const char *tag, const char *message)
{
    if ((s_backend == NULL) || (s_backend->write == NULL))
    {
        /* No backend registered: intentionally a silent no-op, not an
         * error - REQ-OAL-LOG-001 requires logging to never affect the
         * caller's control flow. */
        return;
    }
    s_backend->write(level, tag, message);
}

const char *sapi_log_level_to_string(sapi_log_level_t level)
{
    const char *result;

    switch (level)
    {
        case SAPI_LOG_LEVEL_DEBUG:
            result = "DEBUG";
            break;
        case SAPI_LOG_LEVEL_INFO:
            result = "INFO";
            break;
        case SAPI_LOG_LEVEL_WARNING:
            result = "WARNING";
            break;
        case SAPI_LOG_LEVEL_ERROR:
            result = "ERROR";
            break;
        default:
            /* Defensive only - not reachable through the public enum. */
            result = "UNKNOWN";
            break;
    }
    return result;
}

/**
 * @brief Appends "|" then (value, or "" if value is NULL) to *line.
 *        Best-effort: a SAPI_STATUS_RESOURCE_EXHAUSTED from either
 *        concat is silently accepted (line is left truncated at
 *        whatever fit) - REQ-OAL-LOG-001, this must never fail the
 *        caller's control flow, so there is nothing to report here.
 * @param line   Line being built. Must not be NULL.
 * @param value  Field value to append; NULL is treated as an empty field.
 */
static void sapi_log_append_event_field(sapi_string_t *line, const char *value)
{
    (void)sapi_string_concat(line, "|");
    (void)sapi_string_concat(line, (value != NULL) ? value : "");
}

/**
 * @brief Formats value in base 10 and appends it as the next field of
 *        *line via sapi_log_append_event_field(). Uses a small local
 *        sapi_string_t/buffer, independent of *line's own storage.
 * @param line   Line being built. Must not be NULL.
 * @param value  Value to format.
 */
static void sapi_log_append_event_field_u32(sapi_string_t *line, uint32_t value)
{
    char num_storage[16];
    sapi_string_t num;
    const char *num_cstr = NULL;

    (void)sapi_string_init(&num, num_storage, sizeof(num_storage));
    (void)sapi_string_from_u32(&num, value);
    (void)sapi_string_c_str(&num, &num_cstr);
    sapi_log_append_event_field(line, num_cstr);
}

void sapi_log_write_event(sapi_log_level_t level,
                           uint32_t cycle,
                           const char *source,
                           const char *destination,
                           const char *type,
                           const char *info,
                           const char *extra_fields)
{
    char line_storage[SAPI_LOG_EVENT_LINE_MAX_LEN];
    char ts_storage[24];
    sapi_string_t line;
    sapi_string_t ts;
    sapi_timestamp_ms_t now_ms = 0U;
    const char *ts_cstr = NULL;
    const char *line_cstr = NULL;

    if ((s_backend == NULL) || (s_backend->write == NULL))
    {
        /* Same silent-no-op contract as sapi_log_write() above - avoid
         * even the formatting work below when there is nowhere for it
         * to go. */
        return;
    }

    (void)sapi_string_init(&line, line_storage, sizeof(line_storage));
    (void)sapi_string_init(&ts, ts_storage, sizeof(ts_storage));

    /* TIMESTAMP: best-effort - stays 0 (its declared initializer) if no
     * sapi_timer backend is registered or the call otherwise fails; a
     * timer problem must never prevent this event from being logged
     * (REQ-OAL-LOG-001), so the field is degraded, not the whole call. */
    (void)sapi_timer_now(&now_ms);
    (void)sapi_string_from_u64(&ts, now_ms);
    (void)sapi_string_c_str(&ts, &ts_cstr);
    (void)sapi_string_concat(&line, (ts_cstr != NULL) ? ts_cstr : "0");

    sapi_log_append_event_field(&line, sapi_log_level_to_string(level));
    sapi_log_append_event_field_u32(&line, cycle);
    sapi_log_append_event_field(&line, source);
    sapi_log_append_event_field(&line, destination);
    sapi_log_append_event_field(&line, type);
    sapi_log_append_event_field(&line, info);

    if (extra_fields != NULL)
    {
        sapi_log_append_event_field(&line, extra_fields);
    }

    (void)sapi_string_c_str(&line, &line_cstr);
    s_backend->write(level, source, (line_cstr != NULL) ? line_cstr : "");
}

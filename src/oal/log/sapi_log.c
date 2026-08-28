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
#include "safeapi/oal/log/sapi_log.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi_backend/log/sapi_log_backend.h"
#include "safeapi/utils/string/sapi_string.h"
#include "safeapi/oal/timer/sapi_timer.h"

#include <string.h>

/** Local makros */

/** Local types declarations */
/** @brief One canonical level spelling <-> value pair for
 *         sapi_log_level_from_string(). */
typedef struct
{
    const char      *name;
    sapi_log_level_t level;
} sapi_log_level_name_t;

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_log_backend_t *s_backend = NULL;

/** @brief Minimum severity forwarded to the backend (sapi_log_set_level()).
 *         SAPI_LOG_LEVEL_DEBUG = nothing filtered - the historical
 *         behaviour before this threshold existed. */
static sapi_log_level_t s_min_level = SAPI_LOG_LEVEL_DEBUG;

/** @brief The four canonical spellings, kept in sync with
 *         sapi_log_level_to_string(). */
static const sapi_log_level_name_t s_level_names[] = {
    { "DEBUG",   SAPI_LOG_LEVEL_DEBUG   },
    { "INFO",    SAPI_LOG_LEVEL_INFO    },
    { "WARNING", SAPI_LOG_LEVEL_WARNING },
    { "ERROR",   SAPI_LOG_LEVEL_ERROR   }
};

/** Global variables declarations */

/** Local function declarations */
/**
 * @brief Appends " Key=Value" to *line (a leading space, then key, "=",
 *        then value or "" if value is NULL) - the Key=Value pair
 *        convention sapi_log_write_event() uses for every field.
 *        Best-effort: a SAPI_STATUS_RESOURCE_EXHAUSTED from any concat
 *        is silently accepted (line is left truncated at whatever fit) -
 *        REQ-OAL-LOG-001, this must never fail the caller's control
 *        flow, so there is nothing to report here.
 * @param line   Line being built. Must not be NULL.
 * @param key    Field key/label (e.g. "Cycle"). Must not be NULL.
 * @param value  Field value to append; NULL is treated as an empty value.
 */
static void sapi_log_append_event_field(sapi_string_t *line, const char *key, const char *value);
/**
 * @brief Formats value in base 10 and appends it as "Key=value" via
 *        sapi_log_append_event_field(). Uses a small local
 *        sapi_string_t/buffer, independent of *line's own storage.
 * @param line   Line being built. Must not be NULL.
 * @param key    Field key/label. Must not be NULL.
 * @param value  Value to format.
 */
static void sapi_log_append_event_field_u32(sapi_string_t *line, const char *key, uint32_t value);


/** Global functions */
sapi_status_t sapi_log_register_backend(const sapi_log_backend_t *backend)
{
    sapi_status_t lifecycle_status;

    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
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
    if ((int)level < (int)s_min_level)
    {
        /* Below the runtime threshold (sapi_log_set_level()) - dropped. */
        return;
    }
    s_backend->write(level, tag, message);
}

sapi_status_t sapi_log_level_from_string(const char *name, sapi_log_level_t *out_level)
{
    sapi_status_t status = SAPI_STATUS_INVALID_PARAM;

    if ((name != NULL) && (out_level != NULL))
    {
        size_t i;

        for (i = 0U; i < (sizeof(s_level_names) / sizeof(s_level_names[0])); i++)
        {
            if (strcmp(name, s_level_names[i].name) == 0)
            {
                *out_level = s_level_names[i].level;
                status = SAPI_STATUS_OK;
                break;
            }
        }
    }
    return status;
}

void sapi_log_set_level(sapi_log_level_t min_level)
{
    /* Only an upper bound is checked: the enum's underlying type is
     * unsigned (values 0..3), so `>= SAPI_LOG_LEVEL_DEBUG` would be a
     * tautology. A wrapped/garbage value lands above ERROR and is ignored. */
    if ((int)min_level <= (int)SAPI_LOG_LEVEL_ERROR)
    {
        s_min_level = min_level;
    }
}

sapi_log_level_t sapi_log_get_level(void)
{
    return s_min_level;
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

void sapi_log_write_event(sapi_log_level_t level,
                           const char *site,
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

    if ((s_backend == NULL) || (s_backend->write == NULL) || ((int)level < (int)s_min_level))
    {
        /* Same silent-no-op contract as sapi_log_write() above - and the
         * same runtime-threshold drop (sapi_log_set_level()). Bail before
         * the formatting work below when there is nowhere for it to go, or
         * when it would be dropped anyway. */
        return;
    }

    (void)sapi_string_init(&line, line_storage, sizeof(line_storage));
    (void)sapi_string_init(&ts, ts_storage, sizeof(ts_storage));

    /* Site: written directly (no leading space/key), first field. */
    (void)sapi_string_concat(&line, "Site=");
    (void)sapi_string_concat(&line, (site != NULL) ? site : "");

    /* TIMESTAMP: best-effort - stays 0 (its declared initializer) if no
     * sapi_timer backend is registered or the call otherwise fails; a
     * timer problem must never prevent this event from being logged
     * (REQ-OAL-LOG-001), so the field is degraded, not the whole call. */
    (void)sapi_timer_now(&now_ms);
    (void)sapi_string_from_u64(&ts, now_ms);
    (void)sapi_string_c_str(&ts, &ts_cstr);
    sapi_log_append_event_field(&line, "Timestamp", (ts_cstr != NULL) ? ts_cstr : "0");

    sapi_log_append_event_field(&line, "Level", sapi_log_level_to_string(level));
    sapi_log_append_event_field_u32(&line, "Cycle", cycle);
    sapi_log_append_event_field(&line, "Source", source);
    sapi_log_append_event_field(&line, "Destination", destination);
    sapi_log_append_event_field(&line, "Type", type);
    sapi_log_append_event_field(&line, "Info", info);

    if (extra_fields != NULL)
    {
        /* extra_fields is already caller-formatted Key=Value text (see
         * this function's own doc) - appended verbatim behind a single
         * separating space, not wrapped in another key, so it reads as
         * more of the same space-separated Key=Value convention rather
         * than a nested field. */
        (void)sapi_string_concat(&line, " ");
        (void)sapi_string_concat(&line, extra_fields);
    }

    (void)sapi_string_c_str(&line, &line_cstr);
    s_backend->write(level, source, (line_cstr != NULL) ? line_cstr : "");
}


/****Local functions ****/

static void sapi_log_append_event_field(sapi_string_t *line, const char *key, const char *value)
{
    (void)sapi_string_concat(line, " ");
    (void)sapi_string_concat(line, key);
    (void)sapi_string_concat(line, "=");
    (void)sapi_string_concat(line, (value != NULL) ? value : "");
}

static void sapi_log_append_event_field_u32(sapi_string_t *line, const char *key, uint32_t value)
{
    char num_storage[16];
    sapi_string_t num;
    const char *num_cstr = NULL;

    (void)sapi_string_init(&num, num_storage, sizeof(num_storage));
    (void)sapi_string_from_u32(&num, value);
    (void)sapi_string_c_str(&num, &num_cstr);
    sapi_log_append_event_field(line, key, num_cstr);
}

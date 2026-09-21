/**
 * @file rte_log.c
 * @ingroup LOG
 * @brief Logging service: dispatches to the OSAdapter registered via
 *        rte_osadapter_log_register() (ADR-005). No OSAdapter registered is
 *        not an error for this service - see rte_log.h.
 *
 * rte_log_write_event()'s structured-line formatting (added alongside
 * this file's own module) is what introduces this module's only two new
 * dependencies, `safeapi::string` (bounded formatting - ADR-006) and
 * `safeapi::timer` (TIMESTAMP field - rte_timer_now()); rte_log_write()
 * itself and the OSAdapter dispatch below are unchanged and still have
 * neither dependency.
 */
#include "safeapi/oal/log/rte_log.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_osadapter/log/rte_osadapter_log.h"
#include "safeapi/utils/string/rte_string.h"
#include "safeapi/oal/timer/rte_timer.h"

#include <string.h>

/** Local makros */

/** Local types declarations */
/** @brief One canonical level spelling <-> value pair for
 *         rte_log_level_from_string(). */
typedef struct
{
    const char      *name;
    rte_log_level_t level;
} rte_log_level_name_t;

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_log_t *s_osadapter = NULL;

/** @brief Minimum severity forwarded to the OSAdapter (rte_log_set_level()).
 *         RTE_LOG_LEVEL_DEBUG = nothing filtered - the historical
 *         behaviour before this threshold existed. */
static rte_log_level_t s_min_level = RTE_LOG_LEVEL_DEBUG;

/** @brief The four canonical spellings, kept in sync with
 *         rte_log_level_to_string(). */
static const rte_log_level_name_t s_level_names[] = {
    { "DEBUG",   RTE_LOG_LEVEL_DEBUG   },
    { "INFO",    RTE_LOG_LEVEL_INFO    },
    { "WARNING", RTE_LOG_LEVEL_WARNING },
    { "ERROR",   RTE_LOG_LEVEL_ERROR   }
};

/** Global variables declarations */

/** Local function declarations */
/**
 * @brief Appends " Key=Value" to *line (a leading space, then key, "=",
 *        then value or "" if value is NULL) - the Key=Value pair
 *        convention rte_log_write_event() uses for every field.
 *        Best-effort: a RTE_STATUS_RESOURCE_EXHAUSTED from any concat
 *        is silently accepted (line is left truncated at whatever fit) -
 *        REQ-OAL-LOG-001, this must never fail the caller's control
 *        flow, so there is nothing to report here.
 * @param line   Line being built. Must not be NULL.
 * @param key    Field key/label (e.g. "Cycle"). Must not be NULL.
 * @param value  Field value to append; NULL is treated as an empty value.
 */
static void rte_log_append_event_field(rte_string_t *line, const char *key, const char *value);
/**
 * @brief Formats value in base 10 and appends it as "Key=value" via
 *        rte_log_append_event_field(). Uses a small local
 *        rte_string_t/buffer, independent of *line's own storage.
 * @param line   Line being built. Must not be NULL.
 * @param key    Field key/label. Must not be NULL.
 * @param value  Value to format.
 */
static void rte_log_append_event_field_u32(rte_string_t *line, const char *key, uint32_t value);


/** Global functions */
rte_status_t rte_osadapter_log_register(const rte_osadapter_log_t *osadapter)
{
    rte_status_t lifecycle_status;

    if (osadapter == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering an OSAdapter is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_osadapter = osadapter;
    return RTE_STATUS_OK;
}

rte_status_t rte_log_init(void)
{
    if ((s_osadapter == NULL) || (s_osadapter->init == NULL))
    {
        return RTE_STATUS_OK;
    }
    return s_osadapter->init();
}

void rte_log_write(rte_log_level_t level, const char *tag, const char *message)
{
    if ((s_osadapter == NULL) || (s_osadapter->write == NULL))
    {
        /* No OSAdapter registered: intentionally a silent no-op, not an
         * error - REQ-OAL-LOG-001 requires logging to never affect the
         * caller's control flow. */
        return;
    }
    if ((int)level < (int)s_min_level)
    {
        /* Below the runtime threshold (rte_log_set_level()) - dropped. */
        return;
    }
    s_osadapter->write(level, tag, message);
}

rte_status_t rte_log_level_from_string(const char *name, rte_log_level_t *out_level)
{
    rte_status_t status = RTE_STATUS_INVALID_PARAM;

    if ((name != NULL) && (out_level != NULL))
    {
        size_t i;

        for (i = 0U; i < (sizeof(s_level_names) / sizeof(s_level_names[0])); i++)
        {
            if (strcmp(name, s_level_names[i].name) == 0)
            {
                *out_level = s_level_names[i].level;
                status = RTE_STATUS_OK;
                break;
            }
        }
    }
    return status;
}

void rte_log_set_level(rte_log_level_t min_level)
{
    /* Only an upper bound is checked: the enum's underlying type is
     * unsigned (values 0..3), so `>= RTE_LOG_LEVEL_DEBUG` would be a
     * tautology. A wrapped/garbage value lands above ERROR and is ignored. */
    if ((int)min_level <= (int)RTE_LOG_LEVEL_ERROR)
    {
        s_min_level = min_level;
    }
}

rte_log_level_t rte_log_get_level(void)
{
    return s_min_level;
}

const char *rte_log_level_to_string(rte_log_level_t level)
{
    const char *result;

    switch (level)
    {
        case RTE_LOG_LEVEL_DEBUG:
            result = "DEBUG";
            break;
        case RTE_LOG_LEVEL_INFO:
            result = "INFO";
            break;
        case RTE_LOG_LEVEL_WARNING:
            result = "WARNING";
            break;
        case RTE_LOG_LEVEL_ERROR:
            result = "ERROR";
            break;
        default:
            /* Defensive only - not reachable through the public enum. */
            result = "UNKNOWN";
            break;
    }
    return result;
}

void rte_log_write_event(rte_log_level_t level,
                           const char *site,
                           uint32_t cycle,
                           const char *source,
                           const char *destination,
                           const char *type,
                           const char *info,
                           const char *extra_fields)
{
    char line_storage[RTE_LOG_EVENT_LINE_MAX_LEN];
    char ts_storage[24];
    rte_string_t line;
    rte_string_t ts;
    rte_timestamp_ms_t now_ms = 0U;
    const char *ts_cstr = NULL;
    const char *line_cstr = NULL;

    if ((s_osadapter == NULL) || (s_osadapter->write == NULL) || ((int)level < (int)s_min_level))
    {
        /* Same silent-no-op contract as rte_log_write() above - and the
         * same runtime-threshold drop (rte_log_set_level()). Bail before
         * the formatting work below when there is nowhere for it to go, or
         * when it would be dropped anyway. */
        return;
    }

    (void)rte_string_init(&line, line_storage, sizeof(line_storage));
    (void)rte_string_init(&ts, ts_storage, sizeof(ts_storage));

    /* Site: written directly (no leading space/key), first field. */
    (void)rte_string_concat(&line, "Site=");
    (void)rte_string_concat(&line, (site != NULL) ? site : "");

    /* TIMESTAMP: best-effort - stays 0 (its declared initializer) if no
     * rte_timer OSAdapter is registered or the call otherwise fails; a
     * timer problem must never prevent this event from being logged
     * (REQ-OAL-LOG-001), so the field is degraded, not the whole call. */
    (void)rte_timer_now(&now_ms);
    (void)rte_string_from_u64(&ts, now_ms);
    (void)rte_string_c_str(&ts, &ts_cstr);
    rte_log_append_event_field(&line, "Timestamp", (ts_cstr != NULL) ? ts_cstr : "0");

    rte_log_append_event_field(&line, "Level", rte_log_level_to_string(level));
    rte_log_append_event_field_u32(&line, "Cycle", cycle);
    rte_log_append_event_field(&line, "Source", source);
    rte_log_append_event_field(&line, "Destination", destination);
    rte_log_append_event_field(&line, "Type", type);
    rte_log_append_event_field(&line, "Info", info);

    if ((extra_fields != NULL) && (extra_fields[0] != '\0'))
    {
        /* extra_fields is already caller-formatted Key=Value text (see
         * this function's own doc) - appended verbatim behind a single
         * separating space, not wrapped in another key, so it reads as
         * more of the same space-separated Key=Value convention rather
         * than a nested field. An empty string is treated exactly like
         * NULL (no field, no trailing space) so an empty
         * rte_log_fields_t builder round-trips cleanly. */
        (void)rte_string_concat(&line, " ");
        (void)rte_string_concat(&line, extra_fields);
    }

    (void)rte_string_c_str(&line, &line_cstr);
    s_osadapter->write(level, source, (line_cstr != NULL) ? line_cstr : "");
}

void rte_log_fields_reset(rte_log_fields_t *fields)
{
    if (fields != NULL)
    {
        (void)rte_string_init(&fields->str, fields->storage, sizeof(fields->storage));
    }
}

/**
 * @brief Appends the "[ ]key=" prefix of one pair (leading space only if
 *        the builder already holds something). Best-effort; a truncated
 *        concat is silently accepted (REQ-OAL-LOG-001).
 * @return true if @p fields and @p key are both non-NULL (i.e. the caller
 *         should append the value), false otherwise.
 */
static bool rte_log_fields_begin_pair(rte_log_fields_t *fields, const char *key)
{
    bool ok = (fields != NULL) && (key != NULL);

    if (ok)
    {
        if (rte_string_length(&fields->str) > 0U)
        {
            (void)rte_string_concat(&fields->str, " ");
        }
        (void)rte_string_concat(&fields->str, key);
        (void)rte_string_concat(&fields->str, "=");
    }
    return ok;
}

rte_log_fields_t *rte_log_fields_add_str(rte_log_fields_t *fields, const char *key, const char *value)
{
    if (rte_log_fields_begin_pair(fields, key))
    {
        (void)rte_string_concat(&fields->str, (value != NULL) ? value : "");
    }
    return fields;
}

rte_log_fields_t *rte_log_fields_add_u32(rte_log_fields_t *fields, const char *key, uint32_t value)
{
    if (rte_log_fields_begin_pair(fields, key))
    {
        (void)rte_string_append_u32(&fields->str, value);
    }
    return fields;
}

rte_log_fields_t *rte_log_fields_add_i32(rte_log_fields_t *fields, const char *key, int32_t value)
{
    if (rte_log_fields_begin_pair(fields, key))
    {
        (void)rte_string_append_i32(&fields->str, value);
    }
    return fields;
}

rte_log_fields_t *rte_log_fields_add_u64(rte_log_fields_t *fields, const char *key, uint64_t value)
{
    if (rte_log_fields_begin_pair(fields, key))
    {
        (void)rte_string_append_u64(&fields->str, value);
    }
    return fields;
}

rte_log_fields_t *rte_log_fields_add_i64(rte_log_fields_t *fields, const char *key, int64_t value)
{
    if (rte_log_fields_begin_pair(fields, key))
    {
        (void)rte_string_append_i64(&fields->str, value);
    }
    return fields;
}

rte_log_fields_t *rte_log_fields_add_hex_u32(rte_log_fields_t *fields, const char *key,
                                               uint32_t value, uint8_t min_digits)
{
    if (rte_log_fields_begin_pair(fields, key))
    {
        (void)rte_string_concat(&fields->str, "0x");
        (void)rte_string_append_hex_u32(&fields->str, value, min_digits);
    }
    return fields;
}

rte_log_fields_t *rte_log_fields_add_bool(rte_log_fields_t *fields, const char *key, bool value)
{
    if (rte_log_fields_begin_pair(fields, key))
    {
        (void)rte_string_concat(&fields->str, value ? "true" : "false");
    }
    return fields;
}

const char *rte_log_fields_c_str(rte_log_fields_t *fields)
{
    const char *out = "";

    if (fields != NULL)
    {
        const char *tmp = NULL;

        if (rte_string_c_str(&fields->str, &tmp) == RTE_STATUS_OK)
        {
            out = tmp;
        }
    }
    return out;
}

void rte_log_write_event_fields(rte_log_level_t level,
                                  const char *site,
                                  uint32_t cycle,
                                  const char *source,
                                  const char *destination,
                                  const char *type,
                                  const char *info,
                                  rte_log_fields_t *fields)
{
    /* c_str() yields "" for a NULL/empty builder; rte_log_write_event()
     * now treats "" exactly like NULL, so no special-casing needed. */
    rte_log_write_event(level, site, cycle, source, destination, type, info,
                          rte_log_fields_c_str(fields));
}


/****Local functions ****/

static void rte_log_append_event_field(rte_string_t *line, const char *key, const char *value)
{
    (void)rte_string_concat(line, " ");
    (void)rte_string_concat(line, key);
    (void)rte_string_concat(line, "=");
    (void)rte_string_concat(line, (value != NULL) ? value : "");
}

static void rte_log_append_event_field_u32(rte_string_t *line, const char *key, uint32_t value)
{
    char num_storage[16];
    rte_string_t num;
    const char *num_cstr = NULL;

    (void)rte_string_init(&num, num_storage, sizeof(num_storage));
    (void)rte_string_from_u32(&num, value);
    (void)rte_string_c_str(&num, &num_cstr);
    rte_log_append_event_field(line, key, num_cstr);
}

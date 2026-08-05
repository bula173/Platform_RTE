/**
 * @file sapi_posix_backend_log.c
 * @brief POSIX sapi_log backend: non-blocking best-effort write to stderr
 *        (ADR-018 section 2.3), matching REQ-OAL-LOG-001 ("best-effort,
 *        non-blocking, must never affect caller control flow").
 *
 * Known simplification (stated in ADR-018): no queue, no separate drain
 * thread - a direct fwrite() to stderr. stderr writes are typically fast
 * enough in practice not to violate the non-blocking intent for a first
 * cut, but a deployment wanting a hard non-blocking guarantee (stderr
 * could be a slow pipe/tty under adversarial conditions) should route
 * through a lock-free ring buffer drained by a dedicated low-priority
 * thread instead. write() failures are silently ignored, per
 * REQ-OAL-LOG-001 - logging must never affect caller control flow, so
 * there is nothing meaningful to report back to the caller here.
 */
#include "safeapi/posix_backend/sapi_posix_backend.h"

#include <stdio.h>

static const char *level_to_prefix(sapi_log_level_t level)
{
    const char *prefix;

    switch (level)
    {
        case SAPI_LOG_LEVEL_DEBUG:
            prefix = "DEBUG";
            break;
        case SAPI_LOG_LEVEL_INFO:
            prefix = "INFO";
            break;
        case SAPI_LOG_LEVEL_WARNING:
            prefix = "WARNING";
            break;
        case SAPI_LOG_LEVEL_ERROR:
            prefix = "ERROR";
            break;
        default:
            prefix = "UNKNOWN";
            break;
    }
    return prefix;
}

static sapi_status_t backend_init(void)
{
    return SAPI_STATUS_OK;
}

static void backend_write(sapi_log_level_t level, const char *tag, const char *message)
{
    (void)fprintf(stderr, "[%s] %s: %s\n", level_to_prefix(level), (tag != NULL) ? tag : "-",
                  (message != NULL) ? message : "-");
}

static const sapi_log_backend_t s_posix_log_backend = { backend_init, backend_write };

const sapi_log_backend_t *sapi_posix_backend_log(void)
{
    return &s_posix_log_backend;
}

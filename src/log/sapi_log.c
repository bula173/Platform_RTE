/**
 * @file sapi_log.c
 * @ingroup LOG
 * @brief Logging service: dispatches to the backend registered via
 *        sapi_log_register_backend() (ADR-005). No backend registered is
 *        not an error for this service - see sapi_log.h.
 */
#include "safeapi/log/sapi_log.h"

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

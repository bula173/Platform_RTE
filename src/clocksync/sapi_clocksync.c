/**
 * @file sapi_clocksync.c
 * @brief Implementation of the pluggable clock-sync backend (ADR-017).
 * @ingroup CLOCKSYNC
 */
#include "safeapi/clocksync/sapi_clocksync.h"
#include "safeapi/lifecycle/sapi_lifecycle.h"
#include "safeapi_backend/clocksync/sapi_clocksync_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** Single global backend, following ADR-005's established convention
 *  (see sapi_timer/sapi_ipc). NULL until sapi_clocksync_register_backend()
 *  is called. */
static const sapi_clocksync_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
sapi_status_t sapi_clocksync_register_backend(const sapi_clocksync_backend_t *backend)
{
    sapi_status_t status = SAPI_STATUS_OK;

    if (backend == NULL)
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else
    {
        /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
         * action - refuse once the application's setup phase has been locked. */
        status = sapi_lifecycle_check_setup_allowed();
        if (status == SAPI_STATUS_OK)
        {
            s_backend = backend;
        }
    }

    return status;
}

sapi_status_t sapi_clocksync_get_offset_ms(int64_t *out_offset_ms)
{
    sapi_status_t status = SAPI_STATUS_OK;

    if (out_offset_ms == NULL)
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else if (s_backend == NULL)
    {
        status = SAPI_STATUS_NOT_INITIALIZED;
    }
    else if (s_backend->get_offset_ms == NULL)
    {
        status = SAPI_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_backend->get_offset_ms(out_offset_ms);
    }

    return status;
}

sapi_status_t sapi_clocksync_get_quality(sapi_clocksync_quality_t *out_quality)
{
    sapi_status_t status = SAPI_STATUS_OK;

    if (out_quality == NULL)
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else if (s_backend == NULL)
    {
        status = SAPI_STATUS_NOT_INITIALIZED;
    }
    else if (s_backend->get_quality == NULL)
    {
        status = SAPI_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_backend->get_quality(out_quality);
    }

    return status;
}

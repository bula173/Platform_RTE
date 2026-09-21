/**
 * @file rte_clocksync.c
 * @brief Implementation of the pluggable clock-sync backend (ADR-017).
 * @ingroup CLOCKSYNC
 */
#include "safeapi/oal/clocksync/rte_clocksync.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_backend/clocksync/rte_clocksync_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** Single global backend, following ADR-005's established convention
 *  (see rte_timer/rte_ipc). NULL until rte_clocksync_register_backend()
 *  is called. */
static const rte_clocksync_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_clocksync_register_backend(const rte_clocksync_backend_t *backend)
{
    rte_status_t status = RTE_STATUS_OK;

    if (backend == NULL)
    {
        status = RTE_STATUS_INVALID_PARAM;
    }
    else
    {
        /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
         * action - refuse once the application's setup phase has been locked. */
        status = rte_lifecycle_check_setup_allowed();
        if (status == RTE_STATUS_OK)
        {
            s_backend = backend;
        }
    }

    return status;
}

rte_status_t rte_clocksync_get_offset_ms(int64_t *out_offset_ms)
{
    rte_status_t status = RTE_STATUS_OK;

    if (out_offset_ms == NULL)
    {
        status = RTE_STATUS_INVALID_PARAM;
    }
    else if (s_backend == NULL)
    {
        status = RTE_STATUS_NOT_INITIALIZED;
    }
    else if (s_backend->get_offset_ms == NULL)
    {
        status = RTE_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_backend->get_offset_ms(out_offset_ms);
    }

    return status;
}

rte_status_t rte_clocksync_get_quality(rte_clocksync_quality_t *out_quality)
{
    rte_status_t status = RTE_STATUS_OK;

    if (out_quality == NULL)
    {
        status = RTE_STATUS_INVALID_PARAM;
    }
    else if (s_backend == NULL)
    {
        status = RTE_STATUS_NOT_INITIALIZED;
    }
    else if (s_backend->get_quality == NULL)
    {
        status = RTE_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_backend->get_quality(out_quality);
    }

    return status;
}

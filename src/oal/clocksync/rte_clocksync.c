/**
 * @file rte_clocksync.c
 * @brief Implementation of the pluggable clock-sync OSAdapter (ADR-017).
 * @ingroup CLOCKSYNC
 */
#include "rte/oal/clocksync/rte_clocksync.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte_osadapter/clocksync/rte_osadapter_clocksync.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** Single global OSAdapter, following ADR-005's established convention
 *  (see rte_timer/rte_ipc). NULL until rte_osadapter_clocksync_register()
 *  is called. */
static const rte_osadapter_clocksync_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_clocksync_register(const rte_osadapter_clocksync_t *osadapter)
{
    rte_status_t status = RTE_STATUS_OK;

    if (osadapter == NULL)
    {
        status = RTE_STATUS_INVALID_PARAM;
    }
    else
    {
        /* REQ-LIFECYCLE-001 (ADR-026): registering an OSAdapter is a setup-only
         * action - refuse once the application's setup phase has been locked. */
        status = rte_lifecycle_check_setup_allowed();
        if (status == RTE_STATUS_OK)
        {
            s_osadapter = osadapter;
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
    else if (s_osadapter == NULL)
    {
        status = RTE_STATUS_NOT_INITIALIZED;
    }
    else if (s_osadapter->get_offset_ms == NULL)
    {
        status = RTE_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_osadapter->get_offset_ms(out_offset_ms);
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
    else if (s_osadapter == NULL)
    {
        status = RTE_STATUS_NOT_INITIALIZED;
    }
    else if (s_osadapter->get_quality == NULL)
    {
        status = RTE_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_osadapter->get_quality(out_quality);
    }

    return status;
}

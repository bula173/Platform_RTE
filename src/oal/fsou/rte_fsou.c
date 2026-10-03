/**
 * @file rte_fsou.c
 * @brief Implementation of the optional FSOU output-permission OSAdapter seam (ADR-041).
 * @ingroup FSOU
 */
#include "rte/oal/fsou/rte_fsou.h"
#include "rte/oal/log/rte_log.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte_osadapter/fsou/rte_osadapter_fsou.h"

/** Local makros */

/** Log tag of this module. */
#define RTE_FSOU_LOG_TAG "FSOU"

/** Local types declarations */

/** Local variables declarations */
/** Single global OSAdapter, following ADR-005's established convention. NULL until
 *  rte_osadapter_fsou_register() is called; NULL means no FSOU. */
static const rte_osadapter_fsou_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_fsou_register(const rte_osadapter_fsou_t *adapter)
{
    rte_status_t status = RTE_STATUS_OK;

    /* REQ-FSOU-001 */
    if (adapter == NULL)
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
            s_osadapter = adapter;
        }
    }

    return status;
}

rte_status_t rte_fsou_grant(uint32_t cycle_id)
{
    rte_status_t status = RTE_STATUS_OK;

    /* REQ-FSOU-002 */
    if (s_osadapter == NULL)
    {
        status = RTE_STATUS_NOT_INITIALIZED;
    }
    else if (s_osadapter->grant == NULL)
    {
        status = RTE_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_osadapter->grant(cycle_id);
    }

    return status;
}

rte_status_t rte_fsou_revoke(rte_safestate_reason_t reason)
{
    rte_status_t status = RTE_STATUS_OK;

    /* REQ-FSOU-002 */
    if (s_osadapter == NULL)
    {
        status = RTE_STATUS_NOT_INITIALIZED;
    }
    else if (s_osadapter->revoke == NULL)
    {
        status = RTE_STATUS_NOT_SUPPORTED;
    }
    else
    {
        status = s_osadapter->revoke(reason);
    }

    return status;
}

bool rte_fsou_is_present(void)
{
    return (s_osadapter != NULL);
}

void rte_fsou_report_presence(void)
{
    /** Latch: true once the presence line was logged (block scope, MISRA rule 8.9). */
    static bool s_presence_reported = false;

    /* REQ-FSOU-003: one line per process, latched. */
    if (!s_presence_reported)
    {
        s_presence_reported = true;
        if (s_osadapter != NULL)
        {
            rte_log_write(RTE_LOG_LEVEL_INFO, RTE_FSOU_LOG_TAG, "FSOU present");
        }
        else
        {
            rte_log_write(RTE_LOG_LEVEL_INFO, RTE_FSOU_LOG_TAG, "no FSOU registered");
        }
    }
}

/**
 * @file rte_reboot.c
 * @ingroup REBOOT
 * @brief Reboot service: dispatches to the OSAdapter registered via
 *        rte_osadapter_reboot_register() (ADR-005).
 */
#include "rte/oal/reboot/rte_reboot.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte_osadapter/reboot/rte_osadapter_reboot.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_reboot_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_reboot_register(const rte_osadapter_reboot_t *osadapter)
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

rte_status_t rte_reboot_request(uint16_t reason_code)
{
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->request == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->request(reason_code);
}

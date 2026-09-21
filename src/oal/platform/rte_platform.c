/**
 * @file rte_platform.c
 * @ingroup PLATFORM
 * @brief Real-time platform configuration service: validates parameters,
 *        then dispatches to the OSAdapter registered via
 *        rte_osadapter_platform_register() (ADR-005, ADR-035).
 */
#include "safeapi/oal/platform/rte_platform.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_osadapter/platform/rte_osadapter_platform.h"

/** Local makros */
/** @brief Highest real-time priority rte_platform_realtime_init() accepts. */
#define RTE_PLATFORM_RT_PRIORITY_MAX 99U

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_platform_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_platform_register(const rte_osadapter_platform_t *osadapter)
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

rte_status_t rte_platform_realtime_init(uint32_t rt_priority)
{
    if (rt_priority > RTE_PLATFORM_RT_PRIORITY_MAX)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->realtime_init == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->realtime_init(rt_priority);
}

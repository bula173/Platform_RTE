/**
 * @file rte_timer.c
 * @ingroup TIMER
 * @brief Timer service: validates parameters, then dispatches to the
 *        OSAdapter registered via rte_osadapter_timer_register() (ADR-005).
 */
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi/oal/timer/rte_timer.h"
#include "safeapi_osadapter/timer/rte_osadapter_timer.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_timer_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_timer_register(const rte_osadapter_timer_t *osadapter)
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

rte_status_t rte_timer_create(rte_timer_storage_t *storage,
                                 const rte_timer_config_t *config,
                                 rte_timer_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->callback == NULL) || (config->period_ms == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    /* REQ-LIFECYCLE-001 (ADR-026): a timer is a setup-only resource - refuse once the
     * application's setup phase has been locked (rte_appmanager_run(),
     * after ops->init() succeeds). */
    {
        rte_status_t lifecycle_status = rte_lifecycle_check_setup_allowed();

        if (lifecycle_status != RTE_STATUS_OK)
        {
            return lifecycle_status;
        }
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->create == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->create(storage, config, out_handle);
}

rte_status_t rte_timer_start(rte_timer_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->start == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->start(handle);
}

rte_status_t rte_timer_stop(rte_timer_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->stop == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->stop(handle);
}

rte_status_t rte_timer_destroy(rte_timer_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->destroy == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->destroy(handle);
}

rte_status_t rte_timer_now(rte_timestamp_ms_t *out_now_ms)
{
    if (out_now_ms == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_now_ms = 0U;
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->now == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->now(out_now_ms);
}

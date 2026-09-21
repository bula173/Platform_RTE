/**
 * @file rte_mutex.c
 * @ingroup MUTEX
 * @brief Mutex service: validates parameters, then dispatches to the
 *        OSAdapter registered via rte_osadapter_mutex_register() (ADR-033).
 */
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi/oal/mutex/rte_mutex.h"
#include "safeapi_osadapter/mutex/rte_osadapter_mutex.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-033). */
static const rte_osadapter_mutex_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_mutex_register(const rte_osadapter_mutex_t *osadapter)
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

rte_status_t rte_mutex_create(rte_mutex_storage_t *storage, rte_mutex_handle_t *out_handle)
{
    if ((storage == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    /* REQ-LIFECYCLE-001 (ADR-026): a mutex is a setup-only resource - refuse
     * once the application's setup phase has been locked (rte_appmanager_run(),
     * after ops->init() succeeds) - same posture as rte_timer_create()/
     * rte_task_create(). */
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
    return s_osadapter->create(storage, out_handle);
}

rte_status_t rte_mutex_lock(rte_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->lock == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->lock(handle);
}

rte_status_t rte_mutex_unlock(rte_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->unlock == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->unlock(handle);
}

rte_status_t rte_mutex_destroy(rte_mutex_handle_t handle)
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

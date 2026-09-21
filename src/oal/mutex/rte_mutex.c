/**
 * @file rte_mutex.c
 * @ingroup MUTEX
 * @brief Mutex service: validates parameters, then dispatches to the
 *        backend registered via rte_mutex_register_backend() (ADR-033).
 */
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi/oal/mutex/rte_mutex.h"
#include "safeapi_backend/mutex/rte_mutex_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-033). */
static const rte_mutex_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_mutex_register_backend(const rte_mutex_backend_t *backend)
{
    rte_status_t lifecycle_status;

    if (backend == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_backend = backend;
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
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, out_handle);
}

rte_status_t rte_mutex_lock(rte_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->lock == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->lock(handle);
}

rte_status_t rte_mutex_unlock(rte_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->unlock == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->unlock(handle);
}

rte_status_t rte_mutex_destroy(rte_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->destroy == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->destroy(handle);
}

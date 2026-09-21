/**
 * @file rte_task.c
 * @ingroup TASK
 * @brief Task service: validates parameters, then dispatches to the
 *        backend registered via rte_task_register_backend() (ADR-005).
 */
#include "safeapi/oal/task/rte_task.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_backend/task/rte_task_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const rte_task_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_task_register_backend(const rte_task_backend_t *backend)
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

rte_status_t rte_task_create(rte_task_storage_t *storage,
                                const rte_task_config_t *config,
                                rte_task_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (config->entry == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, config, out_handle);
}

rte_status_t rte_task_start(rte_task_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->start == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->start(handle);
}

rte_status_t rte_task_suspend(rte_task_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->suspend == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->suspend(handle);
}

rte_status_t rte_task_destroy(rte_task_handle_t handle)
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

/**
 * @file sapi_mutex.c
 * @ingroup MUTEX
 * @brief Mutex service: validates parameters, then dispatches to the
 *        backend registered via sapi_mutex_register_backend() (ADR-033).
 */
#include "safeapi/lifecycle/sapi_lifecycle.h"
#include "safeapi/mutex/sapi_mutex.h"
#include "safeapi_backend/mutex/sapi_mutex_backend.h"

/** @brief Currently registered backend, or NULL if none (ADR-033). */
static const sapi_mutex_backend_t *s_backend = NULL;

sapi_status_t sapi_mutex_register_backend(const sapi_mutex_backend_t *backend)
{
    sapi_status_t lifecycle_status;

    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_mutex_create(sapi_mutex_storage_t *storage, sapi_mutex_handle_t *out_handle)
{
    if ((storage == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    /* REQ-LIFECYCLE-001 (ADR-026): a mutex is a setup-only resource - refuse
     * once the application's setup phase has been locked (sapi_appmanager_run(),
     * after ops->init() succeeds) - same posture as sapi_timer_create()/
     * sapi_task_create(). */
    {
        sapi_status_t lifecycle_status = sapi_lifecycle_check_setup_allowed();

        if (lifecycle_status != SAPI_STATUS_OK)
        {
            return lifecycle_status;
        }
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, out_handle);
}

sapi_status_t sapi_mutex_lock(sapi_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->lock == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->lock(handle);
}

sapi_status_t sapi_mutex_unlock(sapi_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->unlock == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->unlock(handle);
}

sapi_status_t sapi_mutex_destroy(sapi_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->destroy == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->destroy(handle);
}

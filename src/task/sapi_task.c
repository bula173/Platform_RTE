/**
 * @file sapi_task.c
 * @ingroup TASK
 * @brief Task service: validates parameters, then dispatches to the
 *        backend registered via sapi_task_register_backend() (ADR-005).
 */
#include "safeapi/task/sapi_task.h"
#include "safeapi_backend/task/sapi_task_backend.h"

/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_task_backend_t *s_backend = NULL;

sapi_status_t sapi_task_register_backend(const sapi_task_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_task_create(sapi_task_storage_t *storage,
                                const sapi_task_config_t *config,
                                sapi_task_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (config->entry == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, config, out_handle);
}

sapi_status_t sapi_task_start(sapi_task_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->start == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->start(handle);
}

sapi_status_t sapi_task_suspend(sapi_task_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->suspend == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->suspend(handle);
}

sapi_status_t sapi_task_destroy(sapi_task_handle_t handle)
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

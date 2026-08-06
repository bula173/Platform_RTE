/**
 * @file sapi_ipc.c
 * @ingroup IPC
 * @brief IPC service: validates parameters, then dispatches to the backend
 *        registered via sapi_ipc_register_backend() (ADR-005).
 */
#include "safeapi/ipc/sapi_ipc.h"
#include "safeapi_backend/ipc/sapi_ipc_backend.h"

/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_ipc_backend_t *s_backend = NULL;

sapi_status_t sapi_ipc_register_backend(const sapi_ipc_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_create(sapi_ipc_storage_t *storage,
                               const sapi_ipc_config_t *config,
                               sapi_ipc_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->message_size == 0U) || (config->queue_depth == 0U))
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

sapi_status_t sapi_ipc_send(sapi_ipc_handle_t handle,
                             const void *message,
                             size_t message_size,
                             sapi_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (message == NULL) || (message_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->send == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->send(handle, message, message_size, timeout_ms);
}

sapi_status_t sapi_ipc_receive(sapi_ipc_handle_t handle,
                                void *out_message,
                                size_t buffer_size,
                                sapi_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (out_message == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->receive == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->receive(handle, out_message, buffer_size, timeout_ms);
}

sapi_status_t sapi_ipc_destroy(sapi_ipc_handle_t handle)
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

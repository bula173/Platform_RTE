/**
 * @file sapi_nvm.c
 * @brief NVM service: validates parameters, then dispatches to the backend
 *        registered via sapi_nvm_register_backend() (ADR-005).
 */
#include "safeapi/nvm/sapi_nvm.h"

static const sapi_nvm_backend_t *s_backend = NULL;

sapi_status_t sapi_nvm_register_backend(const sapi_nvm_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_nvm_open(sapi_nvm_storage_t *storage,
                             const sapi_nvm_config_t *config,
                             sapi_nvm_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->region_name == NULL) || (config->region_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->open == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->open(storage, config, out_handle);
}

sapi_status_t sapi_nvm_read(sapi_nvm_handle_t handle,
                             size_t offset,
                             void *out_buffer,
                             size_t buffer_size)
{
    if ((handle == NULL) || (out_buffer == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->read == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->read(handle, offset, out_buffer, buffer_size);
}

sapi_status_t sapi_nvm_write(sapi_nvm_handle_t handle,
                              size_t offset,
                              const void *buffer,
                              size_t buffer_size)
{
    if ((handle == NULL) || (buffer == NULL) || (buffer_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->write == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->write(handle, offset, buffer, buffer_size);
}

sapi_status_t sapi_nvm_sync(sapi_nvm_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->sync == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->sync(handle);
}

sapi_status_t sapi_nvm_close(sapi_nvm_handle_t handle)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->close == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->close(handle);
}

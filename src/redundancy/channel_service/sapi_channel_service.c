/**
 * @file sapi_channel_service.c
 * @brief Validation and dispatch for the named channel service.
 */

#include "safeapi/redundancy/channel_service/sapi_channel_service.h"

static const sapi_channel_service_backend_t *s_backend;

static sapi_status_t validate_backend(void)
{
    if ((s_backend == NULL) || (s_backend->setup == NULL) || (s_backend->read == NULL) ||
        (s_backend->send == NULL) || (s_backend->close == NULL))
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_channel_service_register_backend(const sapi_channel_service_backend_t *backend)
{
    if ((backend == NULL) || (backend->setup == NULL) || (backend->read == NULL) ||
        (backend->send == NULL) || (backend->close == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_channel_service_setup(sapi_channel_service_t *storage, const char *channel_name)
{
    sapi_status_t status;

    if ((storage == NULL) || (channel_name == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    status = validate_backend();
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    return s_backend->setup(storage, channel_name);
}

sapi_status_t sapi_channel_service_read(sapi_channel_service_t *storage,
                                         void *data,
                                         size_t data_size,
                                         sapi_duration_ms_t timeout_ms)
{
    sapi_status_t status;

    if ((storage == NULL) || (data == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    status = validate_backend();
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    return s_backend->read(storage, data, data_size, timeout_ms);
}

sapi_status_t sapi_channel_service_send(sapi_channel_service_t *storage,
                                         const void *data,
                                         size_t data_size,
                                         sapi_duration_ms_t timeout_ms)
{
    sapi_status_t status;

    if ((storage == NULL) || (data == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    status = validate_backend();
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    return s_backend->send(storage, data, data_size, timeout_ms);
}

sapi_status_t sapi_channel_service_close(sapi_channel_service_t *storage)
{
    sapi_status_t status;

    if (storage == NULL)
    {
        return SAPI_STATUS_OK;
    }
    status = validate_backend();
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    return s_backend->close(storage);
}
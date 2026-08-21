/**
 * @file sapi_channel.c
 * @ingroup channel_link
 * @brief One redundant channel: validates parameters, then dispatches to
 *        its configured send/recv callbacks, tracking health as a side
 *        effect (ADR-025).
 */
#include "safeapi/channel_link/sapi_channel.h"
#include "safeapi/lifecycle/sapi_lifecycle.h"

sapi_status_t sapi_channel_init(sapi_channel_storage_t *storage,
                                       const sapi_channel_config_t *config)
{
    sapi_status_t lifecycle_status;

    if ((storage == NULL) || (config == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->send == NULL) || (config->recv == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): a channel is a setup-only resource - refuse once the
     * application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }

    storage->config = *config;
    storage->health.send_count = 0U;
    storage->health.send_error_count = 0U;
    storage->health.receive_count = 0U;
    storage->health.receive_error_count = 0U;
    storage->health.is_healthy = true;
    storage->health.last_error = SAPI_STATUS_OK;
    storage->initialized = true;

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_channel_send(sapi_channel_t *handle,
                                       const void *data,
                                       size_t data_size)
{
    sapi_status_t status;

    if ((handle == NULL) || (data == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!handle->initialized)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }

    status = handle->config.send(handle->config.channel_handle, data, data_size);
    if (status == SAPI_STATUS_OK)
    {
        handle->health.send_count++;
    }
    else
    {
        handle->health.send_error_count++;
        handle->health.last_error = status;
    }
    return status;
}

sapi_status_t sapi_channel_receive(sapi_channel_t *handle,
                                          void *data,
                                          size_t data_size,
                                          uint32_t timeout_ms)
{
    sapi_status_t status;

    if ((handle == NULL) || (data == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!handle->initialized)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }

    status = handle->config.recv(handle->config.channel_handle, data, data_size, timeout_ms);
    if (status == SAPI_STATUS_OK)
    {
        handle->health.receive_count++;
    }
    else
    {
        handle->health.receive_error_count++;
        handle->health.last_error = status;
    }
    return status;
}

sapi_status_t sapi_channel_get_health(const sapi_channel_t *handle,
                                             sapi_channel_health_t *out_health)
{
    if ((handle == NULL) || (out_health == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_health = handle->health;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_channel_set_healthy(sapi_channel_t *handle,
                                              bool is_healthy)
{
    if (handle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    handle->health.is_healthy = is_healthy;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_channel_destroy(sapi_channel_t *handle)
{
    /* No dynamic memory to free - handle is caller-allocated. */
    if (handle == NULL)
    {
        return SAPI_STATUS_OK;
    }
    handle->initialized = false;
    return SAPI_STATUS_OK;
}

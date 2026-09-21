/**
 * @file rte_channel.c
 * @ingroup channel_link
 * @brief One redundant channel: validates parameters, then dispatches to
 *        its configured send/recv callbacks, tracking health as a side
 *        effect (ADR-025).
 */
#include "safeapi/redundancy/channel_link/rte_channel.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_channel_init(rte_channel_storage_t *storage,
                                       const rte_channel_config_t *config)
{
    rte_status_t lifecycle_status;

    if ((storage == NULL) || (config == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->send == NULL) || (config->recv == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): a channel is a setup-only resource - refuse once the
     * application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }

    storage->config = *config;
    storage->health.send_count = 0U;
    storage->health.send_error_count = 0U;
    storage->health.receive_count = 0U;
    storage->health.receive_error_count = 0U;
    storage->health.is_healthy = true;
    storage->health.last_error = RTE_STATUS_OK;
    storage->initialized = true;

    return RTE_STATUS_OK;
}

rte_status_t rte_channel_send(rte_channel_t *handle,
                                       const void *data,
                                       size_t data_size)
{
    rte_status_t status;

    if ((handle == NULL) || (data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (!handle->initialized)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }

    status = handle->config.send(handle->config.channel_handle, data, data_size);
    if (status == RTE_STATUS_OK)
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

rte_status_t rte_channel_receive(rte_channel_t *handle,
                                          void *data,
                                          size_t data_size,
                                          uint32_t timeout_ms)
{
    rte_status_t status;

    if ((handle == NULL) || (data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (!handle->initialized)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }

    status = handle->config.recv(handle->config.channel_handle, data, data_size, timeout_ms);
    if (status == RTE_STATUS_OK)
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

rte_status_t rte_channel_get_health(const rte_channel_t *handle,
                                             rte_channel_health_t *out_health)
{
    if ((handle == NULL) || (out_health == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_health = handle->health;
    return RTE_STATUS_OK;
}

const char *rte_channel_get_name(const rte_channel_t *handle)
{
    if (handle == NULL)
    {
        return NULL;
    }
    return handle->config.name;
}

rte_status_t rte_channel_set_healthy(rte_channel_t *handle,
                                              bool is_healthy)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    handle->health.is_healthy = is_healthy;
    return RTE_STATUS_OK;
}

rte_status_t rte_channel_destroy(rte_channel_t *handle)
{
    /* No dynamic memory to free - handle is caller-allocated. */
    if (handle == NULL)
    {
        return RTE_STATUS_OK;
    }
    handle->initialized = false;
    return RTE_STATUS_OK;
}

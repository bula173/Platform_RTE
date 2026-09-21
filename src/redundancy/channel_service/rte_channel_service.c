/**
 * @file rte_channel_service.c
 * @brief Validation and dispatch for the named channel service.
 */

#include "safeapi/redundancy/channel_service/rte_channel_service.h"
#include "safeapi/redundancy/config/rte_redundancy_config.h"

static const rte_osadapter_channel_service_t *s_osadapter;

static rte_status_t validate_osadapter(void)
{
    if ((s_osadapter == NULL) || (s_osadapter->setup == NULL) || (s_osadapter->read == NULL) ||
        (s_osadapter->send == NULL) || (s_osadapter->close == NULL))
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    return RTE_STATUS_OK;
}

rte_status_t rte_osadapter_channel_service_register(const rte_osadapter_channel_service_t *osadapter)
{
    if ((osadapter == NULL) || (osadapter->setup == NULL) || (osadapter->read == NULL) ||
        (osadapter->send == NULL) || (osadapter->close == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    s_osadapter = osadapter;
    return RTE_STATUS_OK;
}

rte_status_t rte_channel_service_setup(rte_channel_service_t *storage, const char *channel_name)
{
    rte_status_t status;

    if ((storage == NULL) || (channel_name == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    status = validate_osadapter();
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    return s_osadapter->setup(storage, channel_name);
}

rte_status_t rte_channel_service_setup_by_id(rte_channel_service_t *storage, uint32_t channel_id)
{
    const rte_redundancy_config_t *active = rte_redundancy_config_get_active();
    rte_channel_def_t def;
    rte_status_t status;

    if (storage == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (active == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    status = rte_redundancy_config_find_channel_by_id(active, channel_id, &def);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    return rte_channel_service_setup(storage, def.name);
}

rte_status_t rte_channel_service_read(rte_channel_service_t *storage,
                                         void *data,
                                         size_t data_size,
                                         rte_duration_ms_t timeout_ms)
{
    rte_status_t status;

    if ((storage == NULL) || (data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    status = validate_osadapter();
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    return s_osadapter->read(storage, data, data_size, timeout_ms);
}

rte_status_t rte_channel_service_send(rte_channel_service_t *storage,
                                         const void *data,
                                         size_t data_size,
                                         rte_duration_ms_t timeout_ms)
{
    rte_status_t status;

    if ((storage == NULL) || (data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    status = validate_osadapter();
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    return s_osadapter->send(storage, data, data_size, timeout_ms);
}

rte_status_t rte_channel_service_close(rte_channel_service_t *storage)
{
    rte_status_t status;

    if (storage == NULL)
    {
        return RTE_STATUS_OK;
    }
    status = validate_osadapter();
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    return s_osadapter->close(storage);
}
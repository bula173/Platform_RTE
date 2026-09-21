/**
 * @file rte_flow.c
 * @ingroup FLOW
 * @brief Flow service: validates parameters, then dispatches to the
 *        OSAdapter registered via rte_osadapter_flow_register() (ADR-005).
 *        See rte_netlink.c for the pattern this follows.
 */
#include "safeapi/oal/flow/rte_flow.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_osadapter/flow/rte_osadapter_flow.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_flow_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */
static bool oflags_select_exactly_one_role(uint32_t oflags);

/** Local functions */
/** @brief Exactly one of PUBLISHER/SUBSCRIBER/REQUESTER/RESPONDER must be
 *  set - a Flow has one, unambiguous role for its whole lifetime.
 *  @param oflags The rte_flow_oflag_t bitmask to check.
 *  @return true if exactly one role bit is set; false otherwise. */
static bool oflags_select_exactly_one_role(uint32_t oflags)
{
    uint32_t role_bits = oflags & ((uint32_t)RTE_FLOW_O_REQUESTER | (uint32_t)RTE_FLOW_O_RESPONDER |
                                    (uint32_t)RTE_FLOW_O_PUBLISHER | (uint32_t)RTE_FLOW_O_SUBSCRIBER);

    return (role_bits != 0U) && ((role_bits & (role_bits - 1U)) == 0U);
}

/** Global functions */
rte_status_t rte_osadapter_flow_register(const rte_osadapter_flow_t *osadapter)
{
    rte_status_t lifecycle_status;

    if (osadapter == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering an OSAdapter is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_osadapter = osadapter;
    return RTE_STATUS_OK;
}

rte_status_t rte_flow_open(rte_flow_storage_t *storage,
                              const rte_flow_config_t *config,
                              rte_flow_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->name == NULL) || (config->message_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (!oflags_select_exactly_one_role(config->oflags))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->open == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->open(storage, config, out_handle);
}

rte_status_t rte_flow_send(rte_flow_handle_t handle,
                              const void *data,
                              size_t data_size,
                              rte_flow_channel_t channel,
                              rte_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((channel != RTE_FLOW_CHANNEL_USER) && (channel != RTE_FLOW_CHANNEL_CTRL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->send == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->send(handle, data, data_size, channel, timeout_ms);
}

rte_status_t rte_flow_receive(rte_flow_handle_t handle,
                                 void *out_data,
                                 size_t buffer_size,
                                 rte_flow_channel_t *out_channel,
                                 rte_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (out_data == NULL) || (buffer_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->receive == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->receive(handle, out_data, buffer_size, out_channel, timeout_ms);
}

rte_status_t rte_flow_close(rte_flow_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->close == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->close(handle);
}

rte_status_t rte_flow_getattr(rte_flow_handle_t handle, rte_flow_attr_t *out_attr)
{
    if ((handle == NULL) || (out_attr == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->getattr == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->getattr(handle, out_attr);
}

rte_status_t rte_flow_setattr(rte_flow_handle_t handle,
                                 const rte_flow_attr_t *new_attr,
                                 rte_flow_attr_t *out_old_attr)
{
    if ((handle == NULL) || (new_attr == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->setattr == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->setattr(handle, new_attr, out_old_attr);
}

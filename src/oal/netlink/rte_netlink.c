/**
 * @file rte_netlink.c
 * @ingroup NETLINK
 * @brief Network link service: validates parameters, then dispatches to
 *        the OSAdapter registered via rte_osadapter_netlink_register()
 *        (ADR-005). See rte_ipc.c for the pattern this follows.
 */
#include "safeapi/oal/netlink/rte_netlink.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_osadapter/netlink/rte_osadapter_netlink.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_netlink_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_netlink_register(const rte_osadapter_netlink_t *osadapter)
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

rte_status_t rte_netlink_open(rte_netlink_storage_t *storage,
                                 const rte_netlink_config_t *config,
                                 rte_netlink_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (config->message_size == 0U)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->role == RTE_NETLINK_ROLE_CONNECT) && (config->host == NULL))
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

rte_status_t rte_netlink_send(rte_netlink_handle_t handle,
                                 const void *message,
                                 size_t message_size,
                                 rte_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (message == NULL) || (message_size == 0U))
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
    return s_osadapter->send(handle, message, message_size, timeout_ms);
}

rte_status_t rte_netlink_receive(rte_netlink_handle_t handle,
                                    void *out_message,
                                    size_t buffer_size,
                                    rte_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (out_message == NULL) || (buffer_size == 0U))
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
    return s_osadapter->receive(handle, out_message, buffer_size, timeout_ms);
}

rte_status_t rte_netlink_close(rte_netlink_handle_t handle)
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

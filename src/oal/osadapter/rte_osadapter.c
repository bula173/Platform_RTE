/**
 * @file rte_osadapter.c
 * @brief OS Abstraction Layer - Master OSAdapter and Socket Registration.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */

#include "safeapi_osadapter/rte_osadapter.h"

static const rte_os_socket_ops_t *s_socket_ops = NULL;

rte_status_t rte_osadapter_register_socket_ops(const rte_os_socket_ops_t *ops)
{
    if (ops == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    s_socket_ops = ops;
    return RTE_STATUS_OK;
}

const rte_os_socket_ops_t *rte_osadapter_get_socket_ops(void)
{
    return s_socket_ops;
}

rte_status_t rte_osadapter_register_all(const rte_osadapter_bundle_t *bundle)
{
    rte_status_t status = RTE_STATUS_OK;

    if (bundle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    if (bundle->memory != NULL)
    {
        status = rte_mem_pool_register_backend(bundle->memory);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->clocksync != NULL)
    {
        status = rte_clocksync_register_backend(bundle->clocksync);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->timer != NULL)
    {
        status = rte_timer_register_backend(bundle->timer);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->mutex != NULL)
    {
        status = rte_mutex_register_backend(bundle->mutex);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->task != NULL)
    {
        status = rte_task_register_backend(bundle->task);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->log != NULL)
    {
        status = rte_log_register_backend(bundle->log);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->nvm != NULL)
    {
        status = rte_nvm_register_backend(bundle->nvm);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->platform != NULL)
    {
        status = rte_platform_register_backend(bundle->platform);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->reboot != NULL)
    {
        status = rte_reboot_register_backend(bundle->reboot);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->ipc != NULL)
    {
        status = rte_ipc_register_backend(bundle->ipc);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->netlink != NULL)
    {
        status = rte_netlink_register_backend(bundle->netlink);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->flow != NULL)
    {
        status = rte_flow_register_backend(bundle->flow);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->sockets != NULL)
    {
        status = rte_osadapter_register_socket_ops(bundle->sockets);
        if (status != RTE_STATUS_OK) { return status; }
    }

    return RTE_STATUS_OK;
}

rte_status_t rte_osadapter_memory_register(const rte_osadapter_memory_t *adapter)
{
    return rte_mem_pool_register_backend(adapter);
}

rte_status_t rte_osadapter_clocksync_register(const rte_osadapter_clocksync_t *adapter)
{
    return rte_clocksync_register_backend(adapter);
}

rte_status_t rte_osadapter_timer_register(const rte_osadapter_timer_t *adapter)
{
    return rte_timer_register_backend(adapter);
}

rte_status_t rte_osadapter_mutex_register(const rte_osadapter_mutex_t *adapter)
{
    return rte_mutex_register_backend(adapter);
}

rte_status_t rte_osadapter_task_register(const rte_osadapter_task_t *adapter)
{
    return rte_task_register_backend(adapter);
}

rte_status_t rte_osadapter_log_register(const rte_osadapter_log_t *adapter)
{
    return rte_log_register_backend(adapter);
}

rte_status_t rte_osadapter_nvm_register(const rte_osadapter_nvm_t *adapter)
{
    return rte_nvm_register_backend(adapter);
}

rte_status_t rte_osadapter_platform_register(const rte_osadapter_platform_t *adapter)
{
    return rte_platform_register_backend(adapter);
}

rte_status_t rte_osadapter_reboot_register(const rte_osadapter_reboot_t *adapter)
{
    return rte_reboot_register_backend(adapter);
}

rte_status_t rte_osadapter_ipc_register(const rte_osadapter_ipc_t *adapter)
{
    return rte_ipc_register_backend(adapter);
}

rte_status_t rte_osadapter_netlink_register(const rte_osadapter_netlink_t *adapter)
{
    return rte_netlink_register_backend(adapter);
}

rte_status_t rte_osadapter_flow_register(const rte_osadapter_flow_t *adapter)
{
    return rte_flow_register_backend(adapter);
}


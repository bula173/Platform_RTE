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
        status = rte_osadapter_memory_register(bundle->memory);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->clocksync != NULL)
    {
        status = rte_osadapter_clocksync_register(bundle->clocksync);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->timer != NULL)
    {
        status = rte_osadapter_timer_register(bundle->timer);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->mutex != NULL)
    {
        status = rte_osadapter_mutex_register(bundle->mutex);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->task != NULL)
    {
        status = rte_osadapter_task_register(bundle->task);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->log != NULL)
    {
        status = rte_osadapter_log_register(bundle->log);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->nvm != NULL)
    {
        status = rte_osadapter_nvm_register(bundle->nvm);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->platform != NULL)
    {
        status = rte_osadapter_platform_register(bundle->platform);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->reboot != NULL)
    {
        status = rte_osadapter_reboot_register(bundle->reboot);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->ipc != NULL)
    {
        status = rte_osadapter_ipc_register(bundle->ipc);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->netlink != NULL)
    {
        status = rte_osadapter_netlink_register(bundle->netlink);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->flow != NULL)
    {
        status = rte_osadapter_flow_register(bundle->flow);
        if (status != RTE_STATUS_OK) { return status; }
    }
    if (bundle->sockets != NULL)
    {
        status = rte_osadapter_register_socket_ops(bundle->sockets);
        if (status != RTE_STATUS_OK) { return status; }
    }

    return RTE_STATUS_OK;
}


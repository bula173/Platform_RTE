/**
 * @file sapi_osadapter.c
 * @brief OS Abstraction Layer - Master OSAdapter and Socket Registration.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */

#include "safeapi_osadapter/sapi_osadapter.h"

static const sapi_os_socket_ops_t *s_socket_ops = NULL;

sapi_status_t sapi_osadapter_register_socket_ops(const sapi_os_socket_ops_t *ops)
{
    if (ops == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_socket_ops = ops;
    return SAPI_STATUS_OK;
}

const sapi_os_socket_ops_t *sapi_osadapter_get_socket_ops(void)
{
    return s_socket_ops;
}

sapi_status_t sapi_osadapter_register_all(const sapi_osadapter_bundle_t *bundle)
{
    sapi_status_t status = SAPI_STATUS_OK;

    if (bundle == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (bundle->memory != NULL)
    {
        status = sapi_mem_pool_register_backend(bundle->memory);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->clocksync != NULL)
    {
        status = sapi_clocksync_register_backend(bundle->clocksync);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->timer != NULL)
    {
        status = sapi_timer_register_backend(bundle->timer);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->mutex != NULL)
    {
        status = sapi_mutex_register_backend(bundle->mutex);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->task != NULL)
    {
        status = sapi_task_register_backend(bundle->task);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->log != NULL)
    {
        status = sapi_log_register_backend(bundle->log);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->nvm != NULL)
    {
        status = sapi_nvm_register_backend(bundle->nvm);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->platform != NULL)
    {
        status = sapi_platform_register_backend(bundle->platform);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->reboot != NULL)
    {
        status = sapi_reboot_register_backend(bundle->reboot);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->ipc != NULL)
    {
        status = sapi_ipc_register_backend(bundle->ipc);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->netlink != NULL)
    {
        status = sapi_netlink_register_backend(bundle->netlink);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->flow != NULL)
    {
        status = sapi_flow_register_backend(bundle->flow);
        if (status != SAPI_STATUS_OK) { return status; }
    }
    if (bundle->sockets != NULL)
    {
        status = sapi_osadapter_register_socket_ops(bundle->sockets);
        if (status != SAPI_STATUS_OK) { return status; }
    }

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_osadapter_memory_register(const sapi_osadapter_memory_t *adapter)
{
    return sapi_mem_pool_register_backend(adapter);
}

sapi_status_t sapi_osadapter_clocksync_register(const sapi_osadapter_clocksync_t *adapter)
{
    return sapi_clocksync_register_backend(adapter);
}

sapi_status_t sapi_osadapter_timer_register(const sapi_osadapter_timer_t *adapter)
{
    return sapi_timer_register_backend(adapter);
}

sapi_status_t sapi_osadapter_mutex_register(const sapi_osadapter_mutex_t *adapter)
{
    return sapi_mutex_register_backend(adapter);
}

sapi_status_t sapi_osadapter_task_register(const sapi_osadapter_task_t *adapter)
{
    return sapi_task_register_backend(adapter);
}

sapi_status_t sapi_osadapter_log_register(const sapi_osadapter_log_t *adapter)
{
    return sapi_log_register_backend(adapter);
}

sapi_status_t sapi_osadapter_nvm_register(const sapi_osadapter_nvm_t *adapter)
{
    return sapi_nvm_register_backend(adapter);
}

sapi_status_t sapi_osadapter_platform_register(const sapi_osadapter_platform_t *adapter)
{
    return sapi_platform_register_backend(adapter);
}

sapi_status_t sapi_osadapter_reboot_register(const sapi_osadapter_reboot_t *adapter)
{
    return sapi_reboot_register_backend(adapter);
}

sapi_status_t sapi_osadapter_ipc_register(const sapi_osadapter_ipc_t *adapter)
{
    return sapi_ipc_register_backend(adapter);
}

sapi_status_t sapi_osadapter_netlink_register(const sapi_osadapter_netlink_t *adapter)
{
    return sapi_netlink_register_backend(adapter);
}

sapi_status_t sapi_osadapter_flow_register(const sapi_osadapter_flow_t *adapter)
{
    return sapi_flow_register_backend(adapter);
}


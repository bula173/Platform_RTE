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
    if (bundle->sockets != NULL)
    {
        status = sapi_osadapter_register_socket_ops(bundle->sockets);
        if (status != SAPI_STATUS_OK) { return status; }
    }

    return SAPI_STATUS_OK;
}

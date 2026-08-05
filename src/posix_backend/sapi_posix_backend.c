/**
 * @file sapi_posix_backend.c
 * @brief Aggregator: registers the POSIX backend with every OAL service in
 *        one call (ADR-018 section 2.1).
 */
#include "safeapi/posix_backend/sapi_posix_backend.h"

sapi_status_t sapi_posix_backend_register_all(void)
{
    sapi_status_t status;

    status = sapi_timer_register_backend(sapi_posix_backend_timer());
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    status = sapi_ipc_register_backend(sapi_posix_backend_ipc());
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    status = sapi_task_register_backend(sapi_posix_backend_task());
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    status = sapi_log_register_backend(sapi_posix_backend_log());
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    status = sapi_nvm_register_backend(sapi_posix_backend_nvm());
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    status = sapi_reboot_register_backend(sapi_posix_backend_reboot());
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    status = sapi_mem_pool_register_backend(sapi_posix_backend_memory());
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }

    return SAPI_STATUS_OK;
}

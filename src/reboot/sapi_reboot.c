/**
 * @file sapi_reboot.c
 * @brief Reboot service: dispatches to the backend registered via
 *        sapi_reboot_register_backend() (ADR-005).
 */
#include "safeapi/reboot/sapi_reboot.h"

static const sapi_reboot_backend_t *s_backend = NULL;

sapi_status_t sapi_reboot_register_backend(const sapi_reboot_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_reboot_request(uint16_t reason_code)
{
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->request == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->request(reason_code);
}

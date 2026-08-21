/**
 * @file sapi_reboot.c
 * @ingroup REBOOT
 * @brief Reboot service: dispatches to the backend registered via
 *        sapi_reboot_register_backend() (ADR-005).
 */
#include "safeapi/reboot/sapi_reboot.h"
#include "safeapi/lifecycle/sapi_lifecycle.h"
#include "safeapi_backend/reboot/sapi_reboot_backend.h"

/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_reboot_backend_t *s_backend = NULL;

sapi_status_t sapi_reboot_register_backend(const sapi_reboot_backend_t *backend)
{
    sapi_status_t lifecycle_status;

    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
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

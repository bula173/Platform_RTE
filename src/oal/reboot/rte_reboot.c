/**
 * @file rte_reboot.c
 * @ingroup REBOOT
 * @brief Reboot service: dispatches to the backend registered via
 *        rte_reboot_register_backend() (ADR-005).
 */
#include "safeapi/oal/reboot/rte_reboot.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_backend/reboot/rte_reboot_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const rte_reboot_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_reboot_register_backend(const rte_reboot_backend_t *backend)
{
    rte_status_t lifecycle_status;

    if (backend == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_backend = backend;
    return RTE_STATUS_OK;
}

rte_status_t rte_reboot_request(uint16_t reason_code)
{
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->request == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->request(reason_code);
}

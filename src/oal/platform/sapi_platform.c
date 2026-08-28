/**
 * @file sapi_platform.c
 * @ingroup PLATFORM
 * @brief Real-time platform configuration service: validates parameters,
 *        then dispatches to the backend registered via
 *        sapi_platform_register_backend() (ADR-005, ADR-035).
 */
#include "safeapi/oal/platform/sapi_platform.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi_backend/platform/sapi_platform_backend.h"

/** Local makros */
/** @brief Highest real-time priority sapi_platform_realtime_init() accepts. */
#define SAPI_PLATFORM_RT_PRIORITY_MAX 99U

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_platform_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
sapi_status_t sapi_platform_register_backend(const sapi_platform_backend_t *backend)
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

sapi_status_t sapi_platform_realtime_init(uint32_t rt_priority)
{
    if (rt_priority > SAPI_PLATFORM_RT_PRIORITY_MAX)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->realtime_init == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->realtime_init(rt_priority);
}

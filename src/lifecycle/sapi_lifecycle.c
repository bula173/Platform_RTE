/**
 * @file sapi_lifecycle.c
 * @brief Implementation of the process-wide application setup-phase lock -
 *        see sapi_lifecycle.h.
 * @ingroup LIFECYCLE
 */
#include "safeapi/lifecycle/sapi_lifecycle.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Single, process-wide setup-phase flag (single instance; no
 *         dynamic allocation - same posture as sapi_appmanager.c's own
 *         g_app_state, see this module's own file header). */
static bool g_setup_locked = false;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
void sapi_lifecycle_lock(void)
{
    g_setup_locked = true;
}

void sapi_lifecycle_unlock(void)
{
    g_setup_locked = false;
}

bool sapi_lifecycle_is_locked(void)
{
    return g_setup_locked;
}

sapi_status_t sapi_lifecycle_check_setup_allowed(void)
{
    return g_setup_locked ? SAPI_STATUS_INVALID_STATE : SAPI_STATUS_OK;
}

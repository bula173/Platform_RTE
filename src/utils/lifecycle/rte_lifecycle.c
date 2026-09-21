/**
 * @file rte_lifecycle.c
 * @brief Implementation of the process-wide application setup-phase lock -
 *        see rte_lifecycle.h.
 * @ingroup LIFECYCLE
 */
#include "rte/utils/lifecycle/rte_lifecycle.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Single, process-wide setup-phase flag (single instance; no
 *         dynamic allocation - same posture as rte_appmanager.c's own
 *         g_app_state, see this module's own file header). */
static bool g_setup_locked = false;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
void rte_lifecycle_lock(void)
{
    g_setup_locked = true;
}

void rte_lifecycle_unlock(void)
{
    g_setup_locked = false;
}

bool rte_lifecycle_is_locked(void)
{
    return g_setup_locked;
}

rte_status_t rte_lifecycle_check_setup_allowed(void)
{
    return g_setup_locked ? RTE_STATUS_INVALID_STATE : RTE_STATUS_OK;
}

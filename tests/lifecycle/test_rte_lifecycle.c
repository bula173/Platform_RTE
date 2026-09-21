/* Verifies rte_lifecycle's own lock/unlock/is_locked/check_setup_allowed
 * behavior in isolation - see rte_lifecycle.h (ADR-026). Integration
 * with rte_appmanager_run() and the gated constructors is covered by
 * tests/appmanager/test_rte_appmanager.c and each gated module's own
 * test file. */
#include <assert.h>
#include "safeapi/utils/lifecycle/rte_lifecycle.h"

int main(void)
{
    /* Starts unlocked. */
    assert(!rte_lifecycle_is_locked());
    assert(rte_lifecycle_check_setup_allowed() == RTE_STATUS_OK);

    /* lock() locks. */
    rte_lifecycle_lock();
    assert(rte_lifecycle_is_locked());
    assert(rte_lifecycle_check_setup_allowed() == RTE_STATUS_INVALID_STATE);

    /* Idempotent: locking an already-locked application is a no-op. */
    rte_lifecycle_lock();
    assert(rte_lifecycle_is_locked());

    /* unlock() unlocks. */
    rte_lifecycle_unlock();
    assert(!rte_lifecycle_is_locked());
    assert(rte_lifecycle_check_setup_allowed() == RTE_STATUS_OK);

    /* Idempotent: unlocking an already-unlocked application is a no-op. */
    rte_lifecycle_unlock();
    assert(!rte_lifecycle_is_locked());

    /* Lock/unlock cycles repeatably. */
    rte_lifecycle_lock();
    assert(rte_lifecycle_is_locked());
    rte_lifecycle_unlock();
    assert(!rte_lifecycle_is_locked());
    rte_lifecycle_lock();
    assert(rte_lifecycle_is_locked());
    rte_lifecycle_unlock();
    assert(!rte_lifecycle_is_locked());

    return 0;
}

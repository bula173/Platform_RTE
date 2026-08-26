/* Verifies sapi_lifecycle's own lock/unlock/is_locked/check_setup_allowed
 * behavior in isolation - see sapi_lifecycle.h (ADR-026). Integration
 * with sapi_appmanager_run() and the gated constructors is covered by
 * tests/appmanager/test_sapi_appmanager.c and each gated module's own
 * test file. */
#include <assert.h>
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"

int main(void)
{
    /* Starts unlocked. */
    assert(!sapi_lifecycle_is_locked());
    assert(sapi_lifecycle_check_setup_allowed() == SAPI_STATUS_OK);

    /* lock() locks. */
    sapi_lifecycle_lock();
    assert(sapi_lifecycle_is_locked());
    assert(sapi_lifecycle_check_setup_allowed() == SAPI_STATUS_INVALID_STATE);

    /* Idempotent: locking an already-locked application is a no-op. */
    sapi_lifecycle_lock();
    assert(sapi_lifecycle_is_locked());

    /* unlock() unlocks. */
    sapi_lifecycle_unlock();
    assert(!sapi_lifecycle_is_locked());
    assert(sapi_lifecycle_check_setup_allowed() == SAPI_STATUS_OK);

    /* Idempotent: unlocking an already-unlocked application is a no-op. */
    sapi_lifecycle_unlock();
    assert(!sapi_lifecycle_is_locked());

    /* Lock/unlock cycles repeatably. */
    sapi_lifecycle_lock();
    assert(sapi_lifecycle_is_locked());
    sapi_lifecycle_unlock();
    assert(!sapi_lifecycle_is_locked());
    sapi_lifecycle_lock();
    assert(sapi_lifecycle_is_locked());
    sapi_lifecycle_unlock();
    assert(!sapi_lifecycle_is_locked());

    return 0;
}

/* Tests for the rte_mutex validate-then-dispatch API (ADR-033), rte_mutex_lock_timed() (REQ-OAL-MUTEX-014): argument
 * checks before the OSAdapter lookup, a NULL lock_timed slot, and the timeout and the OSAdapter's result passed through
 * unchanged. The real POSIX timed lock is tested in RBC_GP tests/posix_osadapter/test_rte_posix_osadapter_mutex.c. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "rte/oal/mutex/rte_mutex.h"
#include "rte_osadapter/mutex/rte_osadapter_mutex.h"

static int g_lock_timed_calls;
static rte_mutex_handle_t g_last_handle;
static rte_duration_ms_t g_last_timeout_ms;
static rte_status_t g_lock_timed_result = RTE_STATUS_OK;

static rte_status_t fake_lock_timed(rte_mutex_handle_t handle, rte_duration_ms_t timeout_ms)
{
    g_lock_timed_calls++;
    g_last_handle = handle;
    g_last_timeout_ms = timeout_ms;
    return g_lock_timed_result;
}

static const rte_osadapter_mutex_t g_fake_osadapter = { NULL, NULL, NULL, NULL, fake_lock_timed };
static const rte_osadapter_mutex_t g_fake_osadapter_no_timed = { NULL, NULL, NULL, NULL, NULL };

int main(void)
{
    rte_mutex_storage_t storage;
    rte_mutex_handle_t handle = (rte_mutex_handle_t)(void *)&storage;

    /* NULL handle is refused before the OSAdapter lookup. */
    assert(rte_mutex_lock_timed(NULL, 10U) == RTE_STATUS_INVALID_PARAM);

    /* No OSAdapter registered yet. */
    assert(rte_mutex_lock_timed(handle, 10U) == RTE_STATUS_NOT_INITIALIZED);

    /* OSAdapter registered but its lock_timed slot is NULL. */
    assert(rte_osadapter_mutex_register(&g_fake_osadapter_no_timed) == RTE_STATUS_OK);
    assert(rte_mutex_lock_timed(handle, 10U) == RTE_STATUS_NOT_SUPPORTED);

    /* Real dispatch: handle and timeout passed through unchanged, including 0 and the maximum. */
    assert(rte_osadapter_mutex_register(&g_fake_osadapter) == RTE_STATUS_OK);
    assert(rte_mutex_lock_timed(NULL, 10U) == RTE_STATUS_INVALID_PARAM);
    assert(g_lock_timed_calls == 0);
    assert(rte_mutex_lock_timed(handle, 250U) == RTE_STATUS_OK);
    assert((g_lock_timed_calls == 1) && (g_last_handle == handle) && (g_last_timeout_ms == 250U));
    assert(rte_mutex_lock_timed(handle, 0U) == RTE_STATUS_OK);
    assert((g_lock_timed_calls == 2) && (g_last_timeout_ms == 0U));
    assert(rte_mutex_lock_timed(handle, UINT32_MAX) == RTE_STATUS_OK);
    assert((g_lock_timed_calls == 3) && (g_last_timeout_ms == UINT32_MAX));

    /* The OSAdapter's verdict is passed through, never masked. */
    g_lock_timed_result = RTE_STATUS_TIMEOUT;
    assert(rte_mutex_lock_timed(handle, 5U) == RTE_STATUS_TIMEOUT);
    assert((g_lock_timed_calls == 4) && (g_last_timeout_ms == 5U));
    g_lock_timed_result = RTE_STATUS_INTERNAL_ERROR;
    assert(rte_mutex_lock_timed(handle, 5U) == RTE_STATUS_INTERNAL_ERROR);
    assert(g_lock_timed_calls == 5);

    return 0;
}

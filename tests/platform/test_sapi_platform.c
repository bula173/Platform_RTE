/* Tests for the sapi_platform validate-then-dispatch API (ADR-005/ADR-035). */
#include <assert.h>
#include "safeapi/oal/platform/sapi_platform.h"
#include "safeapi_backend/platform/sapi_platform_backend.h"

static int g_realtime_init_calls;
static uint32_t g_last_priority;

static sapi_status_t mock_realtime_init(uint32_t rt_priority)
{
    g_realtime_init_calls++;
    g_last_priority = rt_priority;
    return SAPI_STATUS_OK;
}

static const sapi_platform_backend_t g_mock_backend __attribute__((unused)) = { mock_realtime_init };
static const sapi_platform_backend_t g_mock_backend_empty __attribute__((unused)) = { NULL };

int main(void)
{
    /* Range check happens before the backend lookup. */
    assert(sapi_platform_realtime_init(100U) == SAPI_STATUS_INVALID_PARAM);

    /* No backend registered yet. */
    assert(sapi_platform_realtime_init(80U) == SAPI_STATUS_NOT_INITIALIZED);

    assert(sapi_platform_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);

    /* Backend registered but the vtable slot is NULL. */
    assert(sapi_platform_register_backend(&g_mock_backend_empty) == SAPI_STATUS_OK);
    assert(sapi_platform_realtime_init(80U) == SAPI_STATUS_NOT_SUPPORTED);

    /* Real dispatch. */
    assert(sapi_platform_register_backend(&g_mock_backend) == SAPI_STATUS_OK);
    assert(sapi_platform_realtime_init(0U) == SAPI_STATUS_OK);
    assert(sapi_platform_realtime_init(80U) == SAPI_STATUS_OK);
    assert(g_realtime_init_calls == 2);
    assert(g_last_priority == 80U);

    /* Boundary: 99 is in range, 100 is not (checked again after a backend
     * exists, to prove the order is range-check-then-dispatch). */
    assert(sapi_platform_realtime_init(99U) == SAPI_STATUS_OK);
    assert(sapi_platform_realtime_init(100U) == SAPI_STATUS_INVALID_PARAM);
    assert(g_realtime_init_calls == 3);

    return 0;
}

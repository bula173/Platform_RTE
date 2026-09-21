/* Tests for the rte_platform validate-then-dispatch API (ADR-005/ADR-035). */
#include <assert.h>
#include "safeapi/oal/platform/rte_platform.h"
#include "safeapi_osadapter/platform/rte_osadapter_platform.h"

static int g_realtime_init_calls;
static uint32_t g_last_priority;

static rte_status_t mock_realtime_init(uint32_t rt_priority)
{
    g_realtime_init_calls++;
    g_last_priority = rt_priority;
    return RTE_STATUS_OK;
}

static const rte_osadapter_platform_t g_mock_osadapter __attribute__((unused)) = { mock_realtime_init };
static const rte_osadapter_platform_t g_mock_osadapter_empty __attribute__((unused)) = { NULL };

int main(void)
{
    /* Range check happens before the OSAdapter lookup. */
    assert(rte_platform_realtime_init(100U) == RTE_STATUS_INVALID_PARAM);

    /* No OSAdapter registered yet. */
    assert(rte_platform_realtime_init(80U) == RTE_STATUS_NOT_INITIALIZED);

    assert(rte_osadapter_platform_register(NULL) == RTE_STATUS_INVALID_PARAM);

    /* OSAdapter registered but the vtable slot is NULL. */
    assert(rte_osadapter_platform_register(&g_mock_osadapter_empty) == RTE_STATUS_OK);
    assert(rte_platform_realtime_init(80U) == RTE_STATUS_NOT_SUPPORTED);

    /* Real dispatch. */
    assert(rte_osadapter_platform_register(&g_mock_osadapter) == RTE_STATUS_OK);
    assert(rte_platform_realtime_init(0U) == RTE_STATUS_OK);
    assert(rte_platform_realtime_init(80U) == RTE_STATUS_OK);
    assert(g_realtime_init_calls == 2);
    assert(g_last_priority == 80U);

    /* Boundary: 99 is in range, 100 is not (checked again after an OSAdapter
     * exists, to prove the order is range-check-then-dispatch). */
    assert(rte_platform_realtime_init(99U) == RTE_STATUS_OK);
    assert(rte_platform_realtime_init(100U) == RTE_STATUS_INVALID_PARAM);
    assert(g_realtime_init_calls == 3);

    return 0;
}

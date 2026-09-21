/* Tests for rte_clocksync (ADR-017). */
#include <assert.h>
#include "rte/oal/clocksync/rte_clocksync.h"
#include "rte_osadapter/clocksync/rte_osadapter_clocksync.h"

static int64_t g_mock_offset_ms;
static rte_clocksync_quality_t g_mock_quality;

static rte_status_t mock_get_offset_ms(int64_t *out_offset_ms)
{
    *out_offset_ms = g_mock_offset_ms;
    return RTE_STATUS_OK;
}

static rte_status_t mock_get_quality(rte_clocksync_quality_t *out_quality)
{
    *out_quality = g_mock_quality;
    return RTE_STATUS_OK;
}

static void test_register_osadapter_validation(void)
{
    assert(rte_osadapter_clocksync_register(NULL) == RTE_STATUS_INVALID_PARAM);
}

static void test_not_initialized_before_registration(void)
{
    /* Relies on test ordering: no OSAdapter has been registered yet in
     * this process. Kept as the first behavioral test for that reason. */
    int64_t offset;
    rte_clocksync_quality_t quality;

    assert(rte_clocksync_get_offset_ms(&offset) == RTE_STATUS_NOT_INITIALIZED);
    assert(rte_clocksync_get_quality(&quality) == RTE_STATUS_NOT_INITIALIZED);
}

static void test_null_out_params(void)
{
    rte_osadapter_clocksync_t osadapter;

    osadapter.get_offset_ms = mock_get_offset_ms;
    osadapter.get_quality = mock_get_quality;
    assert(rte_osadapter_clocksync_register(&osadapter) == RTE_STATUS_OK);

    assert(rte_clocksync_get_offset_ms(NULL) == RTE_STATUS_INVALID_PARAM);
    assert(rte_clocksync_get_quality(NULL) == RTE_STATUS_INVALID_PARAM);
}

static void test_full_osadapter_reports_values(void)
{
    rte_osadapter_clocksync_t osadapter;
    int64_t offset = 0;
    rte_clocksync_quality_t quality = RTE_CLOCKSYNC_UNSYNCHRONIZED;

    osadapter.get_offset_ms = mock_get_offset_ms;
    osadapter.get_quality = mock_get_quality;
    assert(rte_osadapter_clocksync_register(&osadapter) == RTE_STATUS_OK);

    g_mock_offset_ms = -12;
    g_mock_quality = RTE_CLOCKSYNC_SYNCHRONIZED;

    assert(rte_clocksync_get_offset_ms(&offset) == RTE_STATUS_OK);
    assert(offset == -12);
    assert(rte_clocksync_get_quality(&quality) == RTE_STATUS_OK);
    assert(quality == RTE_CLOCKSYNC_SYNCHRONIZED);
}

static void test_partial_osadapter_reports_not_supported(void)
{
    rte_osadapter_clocksync_t osadapter;
    int64_t offset;
    rte_clocksync_quality_t quality;

    osadapter.get_offset_ms = NULL;
    osadapter.get_quality = mock_get_quality;
    assert(rte_osadapter_clocksync_register(&osadapter) == RTE_STATUS_OK);

    assert(rte_clocksync_get_offset_ms(&offset) == RTE_STATUS_NOT_SUPPORTED);
    g_mock_quality = RTE_CLOCKSYNC_DEGRADED;
    assert(rte_clocksync_get_quality(&quality) == RTE_STATUS_OK);
    assert(quality == RTE_CLOCKSYNC_DEGRADED);
}

static void test_partial_osadapter_quality_not_supported(void)
{
    /* Mirror of test_partial_osadapter_reports_not_supported() but with the
     * NULL/non-NULL callbacks swapped, to cover get_quality()'s own
     * "OSAdapter registered but get_quality is NULL" branch (as opposed to
     * get_offset_ms()'s equivalent branch, already covered above). */
    rte_osadapter_clocksync_t osadapter;
    int64_t offset;
    rte_clocksync_quality_t quality;

    osadapter.get_offset_ms = mock_get_offset_ms;
    osadapter.get_quality = NULL;
    assert(rte_osadapter_clocksync_register(&osadapter) == RTE_STATUS_OK);

    g_mock_offset_ms = 5;
    assert(rte_clocksync_get_offset_ms(&offset) == RTE_STATUS_OK);
    assert(offset == 5);
    assert(rte_clocksync_get_quality(&quality) == RTE_STATUS_NOT_SUPPORTED);
}

int main(void)
{
    test_register_osadapter_validation();
    test_not_initialized_before_registration();
    test_null_out_params();
    test_full_osadapter_reports_values();
    test_partial_osadapter_reports_not_supported();
    test_partial_osadapter_quality_not_supported();
    return 0;
}

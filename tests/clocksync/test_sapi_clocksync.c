/* Tests for sapi_clocksync (ADR-017). */
#include <assert.h>
#include "safeapi/clocksync/sapi_clocksync.h"

static int64_t g_mock_offset_ms;
static sapi_clocksync_quality_t g_mock_quality;

static sapi_status_t mock_get_offset_ms(int64_t *out_offset_ms)
{
    *out_offset_ms = g_mock_offset_ms;
    return SAPI_STATUS_OK;
}

static sapi_status_t mock_get_quality(sapi_clocksync_quality_t *out_quality)
{
    *out_quality = g_mock_quality;
    return SAPI_STATUS_OK;
}

static void test_register_backend_validation(void)
{
    assert(sapi_clocksync_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_not_initialized_before_registration(void)
{
    /* Relies on test ordering: no backend has been registered yet in
     * this process. Kept as the first behavioral test for that reason. */
    int64_t offset;
    sapi_clocksync_quality_t quality;

    assert(sapi_clocksync_get_offset_ms(&offset) == SAPI_STATUS_NOT_INITIALIZED);
    assert(sapi_clocksync_get_quality(&quality) == SAPI_STATUS_NOT_INITIALIZED);
}

static void test_null_out_params(void)
{
    sapi_clocksync_backend_t backend;

    backend.get_offset_ms = mock_get_offset_ms;
    backend.get_quality = mock_get_quality;
    assert(sapi_clocksync_register_backend(&backend) == SAPI_STATUS_OK);

    assert(sapi_clocksync_get_offset_ms(NULL) == SAPI_STATUS_INVALID_PARAM);
    assert(sapi_clocksync_get_quality(NULL) == SAPI_STATUS_INVALID_PARAM);
}

static void test_full_backend_reports_values(void)
{
    sapi_clocksync_backend_t backend;
    int64_t offset = 0;
    sapi_clocksync_quality_t quality = SAPI_CLOCKSYNC_UNSYNCHRONIZED;

    backend.get_offset_ms = mock_get_offset_ms;
    backend.get_quality = mock_get_quality;
    assert(sapi_clocksync_register_backend(&backend) == SAPI_STATUS_OK);

    g_mock_offset_ms = -12;
    g_mock_quality = SAPI_CLOCKSYNC_SYNCHRONIZED;

    assert(sapi_clocksync_get_offset_ms(&offset) == SAPI_STATUS_OK);
    assert(offset == -12);
    assert(sapi_clocksync_get_quality(&quality) == SAPI_STATUS_OK);
    assert(quality == SAPI_CLOCKSYNC_SYNCHRONIZED);
}

static void test_partial_backend_reports_not_supported(void)
{
    sapi_clocksync_backend_t backend;
    int64_t offset;
    sapi_clocksync_quality_t quality;

    backend.get_offset_ms = NULL;
    backend.get_quality = mock_get_quality;
    assert(sapi_clocksync_register_backend(&backend) == SAPI_STATUS_OK);

    assert(sapi_clocksync_get_offset_ms(&offset) == SAPI_STATUS_NOT_SUPPORTED);
    g_mock_quality = SAPI_CLOCKSYNC_DEGRADED;
    assert(sapi_clocksync_get_quality(&quality) == SAPI_STATUS_OK);
    assert(quality == SAPI_CLOCKSYNC_DEGRADED);
}

int main(void)
{
    test_register_backend_validation();
    test_not_initialized_before_registration();
    test_null_out_params();
    test_full_backend_reports_values();
    test_partial_backend_reports_not_supported();
    return 0;
}

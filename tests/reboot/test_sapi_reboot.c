/* Tests for the sapi_reboot validate-then-dispatch API (ADR-004/ADR-005). */
#include <assert.h>
#include "safeapi/reboot/sapi_reboot.h"

static int g_request_calls;
static uint16_t g_last_reason;

static sapi_status_t mock_request(uint16_t reason_code)
{
    g_request_calls++;
    g_last_reason = reason_code;
    return SAPI_STATUS_OK;
}

static const sapi_reboot_backend_t g_mock_backend __attribute__((unused)) = { mock_request };
static const sapi_reboot_backend_t g_mock_backend_empty __attribute__((unused)) = { NULL };

int main(void)
{
    /* No backend registered yet. */
    assert(sapi_reboot_request(1U) == SAPI_STATUS_NOT_INITIALIZED);

    assert(sapi_reboot_register_backend(NULL) == SAPI_STATUS_INVALID_PARAM);

    assert(sapi_reboot_register_backend(&g_mock_backend_empty) == SAPI_STATUS_OK);
    assert(sapi_reboot_request(2U) == SAPI_STATUS_NOT_SUPPORTED);

    assert(sapi_reboot_register_backend(&g_mock_backend) == SAPI_STATUS_OK);
    assert(sapi_reboot_request(1234U) == SAPI_STATUS_OK);
    assert(g_request_calls == 1);
    assert(g_last_reason == 1234U);

    return 0;
}

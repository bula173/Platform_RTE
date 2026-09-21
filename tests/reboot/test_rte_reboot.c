/* Tests for the rte_reboot validate-then-dispatch API (ADR-004/ADR-005). */
#include <assert.h>
#include "safeapi/oal/reboot/rte_reboot.h"
#include "safeapi_osadapter/reboot/rte_osadapter_reboot.h"

static int g_request_calls;
static uint16_t g_last_reason;

static rte_status_t mock_request(uint16_t reason_code)
{
    g_request_calls++;
    g_last_reason = reason_code;
    return RTE_STATUS_OK;
}

static const rte_osadapter_reboot_t g_mock_osadapter __attribute__((unused)) = { mock_request };
static const rte_osadapter_reboot_t g_mock_osadapter_empty __attribute__((unused)) = { NULL };

int main(void)
{
    /* No OSAdapter registered yet. */
    assert(rte_reboot_request(1U) == RTE_STATUS_NOT_INITIALIZED);

    assert(rte_osadapter_reboot_register(NULL) == RTE_STATUS_INVALID_PARAM);

    assert(rte_osadapter_reboot_register(&g_mock_osadapter_empty) == RTE_STATUS_OK);
    assert(rte_reboot_request(2U) == RTE_STATUS_NOT_SUPPORTED);

    assert(rte_osadapter_reboot_register(&g_mock_osadapter) == RTE_STATUS_OK);
    assert(rte_reboot_request(1234U) == RTE_STATUS_OK);
    assert(g_request_calls == 1);
    assert(g_last_reason == 1234U);

    return 0;
}

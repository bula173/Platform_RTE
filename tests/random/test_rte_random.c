/* Tests for the rte_random validate-then-dispatch API (ADR-005). */
#include <assert.h>
#include <string.h>
#include "rte/oal/random/rte_random.h"
#include "rte_osadapter/random/rte_osadapter_random.h"

static int g_fill_calls;
static size_t g_last_len;
static rte_status_t g_fill_result = RTE_STATUS_OK;

static rte_status_t mock_fill(uint8_t *out, size_t len)
{
    g_fill_calls++;
    g_last_len = len;
    (void)memset(out, 0xA5, len);
    return g_fill_result;
}

static const rte_osadapter_random_t g_mock_osadapter = { mock_fill };
static const rte_osadapter_random_t g_mock_osadapter_empty = { NULL };

int main(void)
{
    uint8_t buf[RTE_RANDOM_MAX_REQUEST + 1U];

    /* Parameter checks happen before the OSAdapter lookup. */
    assert(rte_random_fill(NULL, 8U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_random_fill(buf, 0U) == RTE_STATUS_INVALID_PARAM);
    assert(rte_random_fill(buf, RTE_RANDOM_MAX_REQUEST + 1U) == RTE_STATUS_INVALID_PARAM);

    /* No OSAdapter registered yet. */
    assert(rte_random_fill(buf, 8U) == RTE_STATUS_NOT_INITIALIZED);

    assert(rte_osadapter_random_register(NULL) == RTE_STATUS_INVALID_PARAM);

    /* OSAdapter registered but the vtable slot is NULL. */
    assert(rte_osadapter_random_register(&g_mock_osadapter_empty) == RTE_STATUS_OK);
    assert(rte_random_fill(buf, 8U) == RTE_STATUS_NOT_SUPPORTED);

    /* Real dispatch, including the upper boundary. */
    assert(rte_osadapter_random_register(&g_mock_osadapter) == RTE_STATUS_OK);
    (void)memset(buf, 0, sizeof(buf));
    assert(rte_random_fill(buf, 8U) == RTE_STATUS_OK);
    assert((g_fill_calls == 1) && (g_last_len == 8U) && (buf[0] == 0xA5U) && (buf[8] == 0U));
    assert(rte_random_fill(buf, RTE_RANDOM_MAX_REQUEST) == RTE_STATUS_OK);
    assert(g_last_len == RTE_RANDOM_MAX_REQUEST);

    /* A source failure is passed through, never masked. */
    g_fill_result = RTE_STATUS_HARDWARE_FAULT;
    assert(rte_random_fill(buf, 8U) == RTE_STATUS_HARDWARE_FAULT);

    return 0;
}

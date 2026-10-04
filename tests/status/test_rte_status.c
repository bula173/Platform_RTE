/* Verifies every status code maps to a non-null, non-"UNKNOWN" string. */
#include <assert.h>
#include <string.h>
#include "rte/utils/status/rte_status.h"

int main(void)
{
    rte_status_t codes[] = {
        RTE_STATUS_OK, RTE_STATUS_INVALID_PARAM, RTE_STATUS_NOT_INITIALIZED,
        RTE_STATUS_ALREADY_INITIALIZED, RTE_STATUS_TIMEOUT,
        RTE_STATUS_RESOURCE_EXHAUSTED, RTE_STATUS_NOT_SUPPORTED,
        RTE_STATUS_NOT_IMPLEMENTED, RTE_STATUS_HARDWARE_FAULT,
        RTE_STATUS_DATA_CORRUPTION, RTE_STATUS_INTERNAL_ERROR,
        RTE_STATUS_VALUE_OUT_OF_RANGE, RTE_STATUS_INVALID_STATE,
        RTE_STATUS_NOT_CONNECTED
    };
    size_t i;
    size_t j;
    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
    {
        const char *s __attribute__((unused)) = rte_status_to_string(codes[i]);
        assert(s != NULL);
        assert(strcmp(s, "UNKNOWN_STATUS") != 0);
        /* Every code has its own value and its own string (no collision). */
        for (j = i + 1U; j < sizeof(codes) / sizeof(codes[0]); ++j)
        {
            assert(codes[i] != codes[j]);
            assert(strcmp(s, rte_status_to_string(codes[j])) != 0);
        }
    }

    /* REQ-CHANSVC-001: the listen-role "no peer yet" code is appended as 13 (the ABI only appends). */
    assert((int)RTE_STATUS_NOT_CONNECTED == 13);
    assert(strcmp(rte_status_to_string(RTE_STATUS_NOT_CONNECTED), "NOT_CONNECTED") == 0);

    /* Out-of-range value: exercises the default arm of the switch, per
     * the established convention (see tests/log/test_rte_log.c). */
    {
        const char *unknown = rte_status_to_string((rte_status_t)999);
        assert(unknown != NULL);
        assert(strcmp(unknown, "UNKNOWN_STATUS") == 0);
    }
    return 0;
}

/* Verifies every status code maps to a non-null, non-"UNKNOWN" string. */
#include <assert.h>
#include <string.h>
#include "safeapi/utils/status/sapi_status.h"

int main(void)
{
    sapi_status_t codes[] = {
        SAPI_STATUS_OK, SAPI_STATUS_INVALID_PARAM, SAPI_STATUS_NOT_INITIALIZED,
        SAPI_STATUS_ALREADY_INITIALIZED, SAPI_STATUS_TIMEOUT,
        SAPI_STATUS_RESOURCE_EXHAUSTED, SAPI_STATUS_NOT_SUPPORTED,
        SAPI_STATUS_NOT_IMPLEMENTED, SAPI_STATUS_HARDWARE_FAULT,
        SAPI_STATUS_DATA_CORRUPTION, SAPI_STATUS_INTERNAL_ERROR,
        SAPI_STATUS_VALUE_OUT_OF_RANGE, SAPI_STATUS_INVALID_STATE
    };
    size_t i;
    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
    {
        const char *s __attribute__((unused)) = sapi_status_to_string(codes[i]);
        assert(s != NULL);
        assert(strcmp(s, "UNKNOWN_STATUS") != 0);
    }

    /* Out-of-range value: exercises the default arm of the switch, per
     * the established convention (see tests/log/test_sapi_log.c). */
    {
        const char *unknown = sapi_status_to_string((sapi_status_t)999);
        assert(unknown != NULL);
        assert(strcmp(unknown, "UNKNOWN_STATUS") == 0);
    }
    return 0;
}

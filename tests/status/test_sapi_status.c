/* Verifies every status code maps to a non-null, non-"UNKNOWN" string. */
#include <assert.h>
#include <string.h>
#include "safeapi/status/sapi_status.h"

int main(void)
{
    sapi_status_t codes[] = {
        SAPI_STATUS_OK, SAPI_STATUS_INVALID_PARAM, SAPI_STATUS_NOT_INITIALIZED,
        SAPI_STATUS_ALREADY_INITIALIZED, SAPI_STATUS_TIMEOUT,
        SAPI_STATUS_RESOURCE_EXHAUSTED, SAPI_STATUS_NOT_SUPPORTED,
        SAPI_STATUS_NOT_IMPLEMENTED, SAPI_STATUS_HARDWARE_FAULT,
        SAPI_STATUS_DATA_CORRUPTION, SAPI_STATUS_INTERNAL_ERROR,
        SAPI_STATUS_VALUE_OUT_OF_RANGE
    };
    size_t i;
    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
    {
        const char *s __attribute__((unused)) = sapi_status_to_string(codes[i]);
        assert(s != NULL);
        assert(strcmp(s, "UNKNOWN_STATUS") != 0);
    }
    return 0;
}

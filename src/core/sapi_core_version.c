/**
 * @file sapi_core_version.c
 * @brief See safeapi/core/sapi_core_version.h.
 */
#include "safeapi/core/sapi_core_version.h"

const char *sapi_core_get_version(void)
{
    return SAFEAPI_FRAMEWORK_VERSION;
}

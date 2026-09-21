/**
 * @file rte_core_version.c
 * @brief See safeapi/core/rte_core_version.h.
 */
#include "safeapi/core/rte_core_version.h"

const char *rte_core_get_version(void)
{
    return SAFEAPI_FRAMEWORK_VERSION;
}

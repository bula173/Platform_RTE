/**
 * @file sapi_safety_violation.c
 * @ingroup SAFETYVIOLATION
 * @brief See sapi_safety_violation.h for behavior.
 */
#include "safeapi/utils/safestate/sapi_safety_violation.h"

static sapi_safety_violation_handler_t s_handler = NULL;

sapi_status_t sapi_safety_violation_register_handler(sapi_safety_violation_handler_t handler)
{
    if (handler == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_handler = handler;
    return SAPI_STATUS_OK;
}

void sapi_safety_violation_report(sapi_safety_violation_kind_t kind,
                                   const char *file,
                                   int32_t line,
                                   const char *message)
{
    if (s_handler == NULL)
    {
        return;
    }
    s_handler(kind, file, line, message);
}

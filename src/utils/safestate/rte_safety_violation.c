/**
 * @file rte_safety_violation.c
 * @ingroup SAFETYVIOLATION
 * @brief See rte_safety_violation.h for behavior.
 */
#include "safeapi/utils/safestate/rte_safety_violation.h"

static rte_safety_violation_handler_t s_handler = NULL;

rte_status_t rte_safety_violation_register_handler(rte_safety_violation_handler_t handler)
{
    if (handler == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    s_handler = handler;
    return RTE_STATUS_OK;
}

void rte_safety_violation_report(rte_safety_violation_kind_t kind,
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

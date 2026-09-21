/**
 * @file rte_safe_ptr.c
 * @ingroup SAFEPTR
 * @brief See rte_safe_ptr.h for behavior.
 */
#include "rte/oal/memory/rte_safe_ptr.h"

#include "rte/utils/cast/rte_cast.h"
#include "rte/utils/safestate/rte_safety_violation.h"

/** Fixed canary value written by rte_safe_ptr_init() and checked by
 *  every access function - an arbitrary, distinctive bit pattern (not
 *  0x00000000 or 0xFFFFFFFF, which a plain zeroed or all-ones-erased
 *  memory region could produce by coincidence). */
#define RTE_SAFE_PTR_CANARY ((uint32_t)0x5AFE9021U)

rte_status_t rte_safe_ptr_init(rte_safe_ptr_t *sp, void *ptr, size_t size)
{
    if (sp == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((ptr == NULL) && (size != 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    sp->ptr = ptr;
    sp->size = size;
    sp->canary = RTE_SAFE_PTR_CANARY;
    return RTE_STATUS_OK;
}

bool rte_safe_ptr_is_valid(const rte_safe_ptr_t *sp)
{
    if (sp == NULL)
    {
        return false;
    }
    if (sp->canary != RTE_SAFE_PTR_CANARY)
    {
        return false;
    }
    return sp->ptr != NULL;
}

rte_status_t rte_safe_ptr_get(const rte_safe_ptr_t *sp, void **out_ptr)
{
    if ((sp == NULL) || (out_ptr == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (sp->canary != RTE_SAFE_PTR_CANARY)
    {
        rte_safety_violation_report(RTE_SAFETY_VIOLATION_CORRUPTION, __FILE__, (int32_t)__LINE__,
                                      "rte_safe_ptr_get: canary mismatch");
        return RTE_STATUS_DATA_CORRUPTION;
    }
    if (sp->ptr == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_ptr = sp->ptr;
    return RTE_STATUS_OK;
}

rte_status_t rte_safe_ptr_offset(const rte_safe_ptr_t *sp, size_t offset, size_t length, void **out_ptr)
{
    size_t end;
    rte_status_t status;

    if ((sp == NULL) || (out_ptr == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (sp->canary != RTE_SAFE_PTR_CANARY)
    {
        rte_safety_violation_report(RTE_SAFETY_VIOLATION_CORRUPTION, __FILE__, (int32_t)__LINE__,
                                      "rte_safe_ptr_offset: canary mismatch");
        return RTE_STATUS_DATA_CORRUPTION;
    }
    if (sp->ptr == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* Checked, not raw, addition: offset + length must not itself
     * overflow size_t before it can even be compared against sp->size
     * (REQ-OAL-SAFEPTR-002). */
    status = rte_cast_checked_add_size(offset, length, &end);
    if (status != RTE_STATUS_OK)
    {
        rte_safety_violation_report(RTE_SAFETY_VIOLATION_OUT_OF_RANGE, __FILE__, (int32_t)__LINE__,
                                      "rte_safe_ptr_offset: offset + length overflowed size_t");
        return RTE_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (end > sp->size)
    {
        rte_safety_violation_report(RTE_SAFETY_VIOLATION_OUT_OF_RANGE, __FILE__, (int32_t)__LINE__,
                                      "rte_safe_ptr_offset: offset + length exceeds wrapped region size");
        return RTE_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out_ptr = (void *)(&((uint8_t *)sp->ptr)[offset]);
    return RTE_STATUS_OK;
}

void rte_safe_ptr_invalidate(rte_safe_ptr_t *sp)
{
    if (sp == NULL)
    {
        return;
    }
    sp->ptr = NULL;
    sp->size = 0U;
    sp->canary = 0U;
}

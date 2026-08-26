/**
 * @file sapi_safe_ptr.c
 * @ingroup SAFEPTR
 * @brief See sapi_safe_ptr.h for behavior.
 */
#include "safeapi/oal/memory/sapi_safe_ptr.h"

#include "safeapi/utils/cast/sapi_cast.h"
#include "safeapi/utils/safestate/sapi_safety_violation.h"

/** Fixed canary value written by sapi_safe_ptr_init() and checked by
 *  every access function - an arbitrary, distinctive bit pattern (not
 *  0x00000000 or 0xFFFFFFFF, which a plain zeroed or all-ones-erased
 *  memory region could produce by coincidence). */
#define SAPI_SAFE_PTR_CANARY ((uint32_t)0x5AFE9021U)

sapi_status_t sapi_safe_ptr_init(sapi_safe_ptr_t *sp, void *ptr, size_t size)
{
    if (sp == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((ptr == NULL) && (size != 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    sp->ptr = ptr;
    sp->size = size;
    sp->canary = SAPI_SAFE_PTR_CANARY;
    return SAPI_STATUS_OK;
}

bool sapi_safe_ptr_is_valid(const sapi_safe_ptr_t *sp)
{
    if (sp == NULL)
    {
        return false;
    }
    if (sp->canary != SAPI_SAFE_PTR_CANARY)
    {
        return false;
    }
    return sp->ptr != NULL;
}

sapi_status_t sapi_safe_ptr_get(const sapi_safe_ptr_t *sp, void **out_ptr)
{
    if ((sp == NULL) || (out_ptr == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (sp->canary != SAPI_SAFE_PTR_CANARY)
    {
        sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_CORRUPTION, __FILE__, (int32_t)__LINE__,
                                      "sapi_safe_ptr_get: canary mismatch");
        return SAPI_STATUS_DATA_CORRUPTION;
    }
    if (sp->ptr == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_ptr = sp->ptr;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_safe_ptr_offset(const sapi_safe_ptr_t *sp, size_t offset, size_t length, void **out_ptr)
{
    size_t end;
    sapi_status_t status;

    if ((sp == NULL) || (out_ptr == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (sp->canary != SAPI_SAFE_PTR_CANARY)
    {
        sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_CORRUPTION, __FILE__, (int32_t)__LINE__,
                                      "sapi_safe_ptr_offset: canary mismatch");
        return SAPI_STATUS_DATA_CORRUPTION;
    }
    if (sp->ptr == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* Checked, not raw, addition: offset + length must not itself
     * overflow size_t before it can even be compared against sp->size
     * (REQ-OAL-SAFEPTR-002). */
    status = sapi_cast_checked_add_size(offset, length, &end);
    if (status != SAPI_STATUS_OK)
    {
        sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_OUT_OF_RANGE, __FILE__, (int32_t)__LINE__,
                                      "sapi_safe_ptr_offset: offset + length overflowed size_t");
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (end > sp->size)
    {
        sapi_safety_violation_report(SAPI_SAFETY_VIOLATION_OUT_OF_RANGE, __FILE__, (int32_t)__LINE__,
                                      "sapi_safe_ptr_offset: offset + length exceeds wrapped region size");
        return SAPI_STATUS_VALUE_OUT_OF_RANGE;
    }
    *out_ptr = (void *)(&((uint8_t *)sp->ptr)[offset]);
    return SAPI_STATUS_OK;
}

void sapi_safe_ptr_invalidate(sapi_safe_ptr_t *sp)
{
    if (sp == NULL)
    {
        return;
    }
    sp->ptr = NULL;
    sp->size = 0U;
    sp->canary = 0U;
}

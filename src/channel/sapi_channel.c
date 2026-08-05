/**
 * @file sapi_channel.c
 * @brief Implementation of dual-channel identity and result comparison
 *        (ADR-008).
 */
#include "safeapi/channel/sapi_channel.h"
#include <string.h>

sapi_channel_id_t sapi_channel_local_id(void)
{
    sapi_channel_id_t id;

#if defined(SAPI_CHANNEL_BUILD_A)
    id = SAPI_CHANNEL_ID_A;
#else
    /* sapi_channel.h already #errors unless exactly one of
     * SAPI_CHANNEL_BUILD_A / SAPI_CHANNEL_BUILD_B is defined, so reaching
     * this branch means SAPI_CHANNEL_BUILD_B is the one that was set. */
    id = SAPI_CHANNEL_ID_B;
#endif

    return id;
}

const char *sapi_channel_id_to_string(sapi_channel_id_t id)
{
    const char *result;

    switch (id)
    {
        case SAPI_CHANNEL_ID_A:
            result = "CHANNEL_A";
            break;
        case SAPI_CHANNEL_ID_B:
            result = "CHANNEL_B";
            break;
        default:
            /* Defensive: id is caller-supplied and not guaranteed to be a
             * valid enumerator. Never return NULL (REQ-COMMON-CHANNEL). */
            result = "CHANNEL_UNKNOWN";
            break;
    }
    return result;
}

sapi_status_t sapi_channel_compare(sapi_const_buffer_t local_result,
                                    sapi_const_buffer_t peer_result,
                                    sapi_channel_compare_result_t *out_result)
{
    sapi_status_t status = SAPI_STATUS_OK;

    if (out_result == NULL)
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else if ((local_result.data == NULL) && (local_result.length > 0U))
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else if ((peer_result.data == NULL) && (peer_result.length > 0U))
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else if (local_result.length != peer_result.length)
    {
        /* REQ-COMMON-CHANNEL-002: a length difference is itself a
         * disagreement, not an error - report it as such. */
        *out_result = SAPI_CHANNEL_COMPARE_MISMATCH;
    }
    else if (local_result.length == 0U)
    {
        /* Both empty and same (zero) length: trivially identical.
         * Avoided calling memcmp() with a zero count on possibly-NULL
         * pointers, which is unspecified/undefined in the C standard even
         * though many implementations tolerate it - defensive coding
         * means not relying on that tolerance. */
        *out_result = SAPI_CHANNEL_COMPARE_MATCH;
    }
    else if (memcmp(local_result.data, peer_result.data, local_result.length) == 0)
    {
        *out_result = SAPI_CHANNEL_COMPARE_MATCH;
    }
    else
    {
        *out_result = SAPI_CHANNEL_COMPARE_MISMATCH;
    }

    return status;
}

sapi_status_t sapi_channel_compare_and_enter_safestate(sapi_const_buffer_t local_result,
                                                         sapi_const_buffer_t peer_result,
                                                         sapi_safestate_reason_t reason)
{
    sapi_channel_compare_result_t compare_result = SAPI_CHANNEL_COMPARE_MATCH;
    sapi_status_t status = sapi_channel_compare(local_result, peer_result, &compare_result);

    if ((status == SAPI_STATUS_OK) && (compare_result == SAPI_CHANNEL_COMPARE_MISMATCH))
    {
        sapi_safestate_reason_t effective_reason = reason;

        if (reason == SAPI_SAFESTATE_REASON_UNSPECIFIED)
        {
            effective_reason = SAPI_SAFESTATE_REASON_CHANNEL_MISMATCH;
        }

        /* Never returns (REQ-COMMON-SAFESTATE-002): the defensive
         * infinite-loop fallback lives inside sapi_safestate_enter()
         * itself, so nothing further is needed here. */
        sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE, effective_reason, __FILE__, (int32_t)__LINE__, NULL);
    }

    return status;
}

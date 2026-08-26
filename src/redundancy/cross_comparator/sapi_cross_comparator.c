/**
 * @file sapi_cross_comparator.c
 * @ingroup cross_comparator
 * @brief Pairwise channel comparison (ADR-025): validates parameters,
 *        receives from both registered channels, and compares them.
 */
#include <string.h>

#include "safeapi/redundancy/cross_comparator/sapi_cross_comparator.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi/oal/log/sapi_log.h"
#include "safeapi/utils/safestate/sapi_safestate.h"

static bool cross_comparator_data_equal(const sapi_cross_comparator_t *cmp,
                                         const void *a, const void *b, size_t size)
{
    bool equal;

    if (cmp->config.compare != NULL)
    {
        equal = cmp->config.compare(a, b, size, cmp->config.compare_context);
    }
    else
    {
        equal = (memcmp(a, b, size) == 0);
    }
    return equal;
}

sapi_status_t sapi_cross_comparator_init(sapi_cross_comparator_storage_t *storage,
                                          const sapi_cross_comparator_config_t *config)
{
    sapi_status_t lifecycle_status;

    if ((storage == NULL) || (config == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): a cross-comparator is a setup-only resource - refuse once
     * the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }

    storage->config = *config;
    storage->channel_a = NULL;
    storage->channel_b = NULL;
    storage->registered_count = 0U;
    storage->total_disagreements = 0U;
    storage->initialized = true;

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cross_comparator_register_channel(sapi_cross_comparator_t *cmp,
                                                      sapi_channel_t *channel)
{
    sapi_status_t lifecycle_status;

    if ((cmp == NULL) || (channel == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!cmp->initialized)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a channel into a cross-comparator is
     * setup-only - refuse once the application's setup phase has been
     * locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }

    if (cmp->registered_count == 0U)
    {
        cmp->channel_a = channel;
        cmp->registered_count = 1U;
    }
    else if (cmp->registered_count == 1U)
    {
        cmp->channel_b = channel;
        cmp->registered_count = 2U;
    }
    else
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cross_comparator_execute(sapi_cross_comparator_t *cmp, size_t data_size,
                                             sapi_voting_result_t *result,
                                             void *out_data, size_t *out_size)
{
    uint8_t buf_a[SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE];
    uint8_t buf_b[SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE];
    sapi_channel_health_t health_a;
    sapi_channel_health_t health_b;
    sapi_status_t st_a;
    sapi_status_t st_b;
    sapi_voting_result_t local_result;

    if ((cmp == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!cmp->initialized)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (data_size > SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    if (cmp->registered_count != 2U)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    (void)sapi_channel_get_health(cmp->channel_a, &health_a);
    (void)sapi_channel_get_health(cmp->channel_b, &health_b);

    if ((!health_a.is_healthy) || (!health_b.is_healthy))
    {
        local_result = SAPI_VOTING_INSUFFICIENT_QUORUM;
    }
    else
    {
        st_a = sapi_channel_receive(cmp->channel_a, buf_a, data_size, cmp->config.channel_timeout_ms);
        st_b = sapi_channel_receive(cmp->channel_b, buf_b, data_size, cmp->config.channel_timeout_ms);

        if ((st_a == SAPI_STATUS_TIMEOUT) || (st_b == SAPI_STATUS_TIMEOUT))
        {
            local_result = SAPI_VOTING_TIMEOUT;
        }
        else if ((st_a != SAPI_STATUS_OK) || (st_b != SAPI_STATUS_OK))
        {
            local_result = SAPI_VOTING_INSUFFICIENT_QUORUM;
        }
        else if (cross_comparator_data_equal(cmp, buf_a, buf_b, data_size))
        {
            local_result = SAPI_VOTING_AGREED;
        }
        else
        {
            local_result = SAPI_VOTING_DISAGREED;
        }
    }

    if (result != NULL)
    {
        *result = local_result;
    }

    if (local_result == SAPI_VOTING_AGREED)
    {
        if (out_data != NULL)
        {
            (void)memcpy(out_data, buf_a, data_size);
        }
        if (out_size != NULL)
        {
            *out_size = data_size;
        }
        return SAPI_STATUS_OK;
    }

    if (out_size != NULL)
    {
        *out_size = 0U;
    }

    if (local_result == SAPI_VOTING_DISAGREED)
    {
        cmp->total_disagreements++;
        if (cmp->config.log_disagreements)
        {
            sapi_log_write(SAPI_LOG_LEVEL_ERROR, "cross_comparator", "channels disagreed");
        }
        if (cmp->config.trigger_safestate_on_disagreement)
        {
            SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_SAFE, SAPI_SAFESTATE_REASON_UNSPECIFIED);
        }
    }

    if (cmp->config.on_disagreement != NULL)
    {
        cmp->config.on_disagreement(cmp->config.disagreement_context, local_result);
    }

    return SAPI_STATUS_HARDWARE_FAULT;
}

sapi_status_t sapi_cross_comparator_get_aggregated_health(const sapi_cross_comparator_t *cmp,
                                                           uint32_t *healthy_count,
                                                           uint32_t *total_disagreements)
{
    uint32_t healthy;
    sapi_channel_health_t h;

    if (cmp == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    healthy = 0U;
    if (cmp->registered_count >= 1U)
    {
        (void)sapi_channel_get_health(cmp->channel_a, &h);
        if (h.is_healthy)
        {
            healthy++;
        }
    }
    if (cmp->registered_count >= 2U)
    {
        (void)sapi_channel_get_health(cmp->channel_b, &h);
        if (h.is_healthy)
        {
            healthy++;
        }
    }

    if (healthy_count != NULL)
    {
        *healthy_count = healthy;
    }
    if (total_disagreements != NULL)
    {
        *total_disagreements = cmp->total_disagreements;
    }
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_cross_comparator_destroy(sapi_cross_comparator_t *cmp)
{
    if (cmp == NULL)
    {
        return SAPI_STATUS_OK;
    }
    cmp->initialized = false;
    return SAPI_STATUS_OK;
}

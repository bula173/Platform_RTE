/**
 * @file rte_cross_comparator.c
 * @ingroup cross_comparator
 * @brief Pairwise channel comparison (ADR-025): validates parameters,
 *        receives from both registered channels, and compares them.
 */
#include <string.h>

#include "safeapi/redundancy/cross_comparator/rte_cross_comparator.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi/oal/log/rte_log.h"
#include "safeapi/utils/safestate/rte_safestate.h"

static bool cross_comparator_data_equal(const rte_cross_comparator_t *cmp,
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

rte_status_t rte_cross_comparator_init(rte_cross_comparator_storage_t *storage,
                                          const rte_cross_comparator_config_t *config)
{
    rte_status_t lifecycle_status;

    if ((storage == NULL) || (config == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): a cross-comparator is a setup-only resource - refuse once
     * the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }

    storage->config = *config;
    storage->channel_a = NULL;
    storage->channel_b = NULL;
    storage->registered_count = 0U;
    storage->total_disagreements = 0U;
    storage->initialized = true;

    return RTE_STATUS_OK;
}

rte_status_t rte_cross_comparator_register_channel(rte_cross_comparator_t *cmp,
                                                      rte_channel_t *channel)
{
    rte_status_t lifecycle_status;

    if ((cmp == NULL) || (channel == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (!cmp->initialized)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a channel into a cross-comparator is
     * setup-only - refuse once the application's setup phase has been
     * locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
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
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }
    return RTE_STATUS_OK;
}

/** Shared tail for both rte_cross_comparator_execute() and
 *  rte_cross_comparator_execute_buffers(): given a computed
 *  local_result, applies the exact same AGREED/DISAGREED/other handling
 *  (callback, unconditional safestate on DISAGREED) both public entry
 *  points must have identically - see rte_cross_comparator.h's own
 *  RCA/OCORA PI-API compatibility note for why this exists. */
static rte_status_t cross_comparator_finish(rte_cross_comparator_t *cmp, rte_voting_result_t local_result,
                                              const void *agreed_data, size_t data_size,
                                              rte_voting_result_t *result, void *out_data, size_t *out_size)
{
    if (result != NULL)
    {
        *result = local_result;
    }

    if (local_result == RTE_VOTING_AGREED)
    {
        if (out_data != NULL)
        {
            (void)memcpy(out_data, agreed_data, data_size);
        }
        if (out_size != NULL)
        {
            *out_size = data_size;
        }
        return RTE_STATUS_OK;
    }

    if (out_size != NULL)
    {
        *out_size = 0U;
    }

    if (local_result == RTE_VOTING_DISAGREED)
    {
        cmp->total_disagreements++;
        if (cmp->config.log_disagreements)
        {
            rte_log_write(RTE_LOG_LEVEL_ERROR, "cross_comparator", "channels disagreed");
        }
        /* Application reacts (cleanup, alarms, closing its own links)
         * BEFORE the Platform enters safe state below - it gets no
         * "after". See this module's own RCA/OCORA PI-API compatibility
         * note (rte_cross_comparator.h) for why the Platform, not the
         * application, now unconditionally owns this transition. */
        if (cmp->config.on_disagreement != NULL)
        {
            cmp->config.on_disagreement(cmp->config.disagreement_context, local_result);
        }
        RTE_SAFESTATE(cmp->config.safestate_level, cmp->config.safestate_reason);
    }
    else if (cmp->config.on_disagreement != NULL)
    {
        /* Non-DISAGREED non-AGREED results (TIMEOUT/INSUFFICIENT_QUORUM)
         * still get the callback, same as before this change, but do
         * NOT trigger a safestate transition here - only a genuine
         * DISAGREE does. */
        cmp->config.on_disagreement(cmp->config.disagreement_context, local_result);
    }
    else
    {
        /* Nothing further to do for a non-DISAGREED result with no
         * callback registered. */
    }

    return RTE_STATUS_HARDWARE_FAULT;
}

rte_status_t rte_cross_comparator_execute(rte_cross_comparator_t *cmp, size_t data_size,
                                             rte_voting_result_t *result,
                                             void *out_data, size_t *out_size)
{
    uint8_t buf_a[RTE_CROSS_COMPARATOR_MAX_MESSAGE_SIZE] = { 0 };
    rte_channel_health_t health_a;
    rte_channel_health_t health_b;
    rte_voting_result_t local_result;

    if ((cmp == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (!cmp->initialized)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (data_size > RTE_CROSS_COMPARATOR_MAX_MESSAGE_SIZE)
    {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }
    if (cmp->registered_count != 2U)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    (void)rte_channel_get_health(cmp->channel_a, &health_a);
    (void)rte_channel_get_health(cmp->channel_b, &health_b);

    if ((!health_a.is_healthy) || (!health_b.is_healthy))
    {
        local_result = RTE_VOTING_INSUFFICIENT_QUORUM;
    }
    else
    {
        uint8_t buf_b[RTE_CROSS_COMPARATOR_MAX_MESSAGE_SIZE];
        rte_status_t st_a = rte_channel_receive(cmp->channel_a, buf_a, data_size, cmp->config.channel_timeout_ms);
        rte_status_t st_b = rte_channel_receive(cmp->channel_b, buf_b, data_size, cmp->config.channel_timeout_ms);

        if ((st_a == RTE_STATUS_TIMEOUT) || (st_b == RTE_STATUS_TIMEOUT))
        {
            local_result = RTE_VOTING_TIMEOUT;
        }
        else if ((st_a != RTE_STATUS_OK) || (st_b != RTE_STATUS_OK))
        {
            local_result = RTE_VOTING_INSUFFICIENT_QUORUM;
        }
        else if (cross_comparator_data_equal(cmp, buf_a, buf_b, data_size))
        {
            local_result = RTE_VOTING_AGREED;
        }
        else
        {
            local_result = RTE_VOTING_DISAGREED;
        }
    }

    return cross_comparator_finish(cmp, local_result, buf_a, data_size, result, out_data, out_size);
}

rte_status_t rte_cross_comparator_execute_buffers(rte_cross_comparator_t *cmp,
                                                      const void *local_data, const void *peer_data,
                                                      size_t data_size,
                                                      rte_voting_result_t *result,
                                                      void *out_data, size_t *out_size)
{
    rte_voting_result_t local_result;

    if ((cmp == NULL) || (local_data == NULL) || (peer_data == NULL) || (data_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (!cmp->initialized)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (data_size > RTE_CROSS_COMPARATOR_MAX_MESSAGE_SIZE)
    {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }

    local_result = cross_comparator_data_equal(cmp, local_data, peer_data, data_size) ? RTE_VOTING_AGREED
                                                                                       : RTE_VOTING_DISAGREED;

    return cross_comparator_finish(cmp, local_result, local_data, data_size, result, out_data, out_size);
}

rte_status_t rte_cross_comparator_get_aggregated_health(const rte_cross_comparator_t *cmp,
                                                           uint32_t *healthy_count,
                                                           uint32_t *total_disagreements)
{
    uint32_t healthy;
    rte_channel_health_t h;

    if (cmp == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    healthy = 0U;
    if (cmp->registered_count >= 1U)
    {
        (void)rte_channel_get_health(cmp->channel_a, &h);
        if (h.is_healthy)
        {
            healthy++;
        }
    }
    if (cmp->registered_count >= 2U)
    {
        (void)rte_channel_get_health(cmp->channel_b, &h);
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
    return RTE_STATUS_OK;
}

rte_status_t rte_cross_comparator_destroy(rte_cross_comparator_t *cmp)
{
    if (cmp == NULL)
    {
        return RTE_STATUS_OK;
    }
    cmp->initialized = false;
    return RTE_STATUS_OK;
}

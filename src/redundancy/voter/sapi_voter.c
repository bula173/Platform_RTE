/**
 * @file sapi_voter.c
 * @ingroup voter
 * @brief N-way voting engine (ADR-025): validates parameters, dispatches
 *        send/receive to every registered healthy channel, and - for
 *        receive - groups the responses by mutual agreement and picks
 *        the largest group meeting quorum.
 */
#include <string.h>

#include "safeapi/redundancy/voter/sapi_voter.h"
#include "safeapi/utils/lifecycle/sapi_lifecycle.h"
#include "safeapi/oal/log/sapi_log.h"
#include "safeapi/utils/safestate/sapi_safestate.h"

static bool voter_channel_count_matches_strategy(const sapi_voter_t *voter)
{
    bool ok;

    switch (voter->config.voting_strategy)
    {
        case SAPI_VOTING_2OO2:
            ok = (voter->channel_count == 2U);
            break;
        case SAPI_VOTING_2OO3:
            ok = (voter->channel_count == 3U);
            break;
        case SAPI_VOTING_NMR:
            ok = (voter->channel_count >= 1U) &&
                 (voter->config.quorum_size >= 1U) &&
                 (voter->config.quorum_size <= voter->channel_count);
            break;
        default:
            ok = false;
            break;
    }
    return ok;
}

static uint32_t voter_required_quorum(const sapi_voter_t *voter)
{
    uint32_t quorum;

    switch (voter->config.voting_strategy)
    {
        case SAPI_VOTING_2OO2:
            quorum = 2U;
            break;
        case SAPI_VOTING_2OO3:
            quorum = 2U;
            break;
        case SAPI_VOTING_NMR:
            quorum = voter->config.quorum_size;
            break;
        default:
            /* Defensive: sapi_voter_init() already rejects any other
             * value, so a live voter's strategy is always one of the
             * three above. */
            quorum = 0xFFFFFFFFU;
            break;
    }
    return quorum;
}

static bool voter_data_equal(const sapi_voter_t *voter, const void *a, const void *b, size_t size)
{
    bool equal;

    if (voter->config.compare != NULL)
    {
        equal = voter->config.compare(a, b, size, voter->config.compare_context);
    }
    else
    {
        equal = (memcmp(a, b, size) == 0);
    }
    return equal;
}

static uint32_t voter_count_healthy(const sapi_voter_t *voter)
{
    uint32_t i;
    uint32_t healthy = 0U;
    sapi_channel_health_t h;

    for (i = 0U; i < voter->channel_count; i++)
    {
        (void)sapi_channel_get_health(voter->channels[i], &h);
        if (h.is_healthy)
        {
            healthy++;
        }
    }
    return healthy;
}

sapi_status_t sapi_voter_init(sapi_voter_storage_t *storage, const sapi_voter_config_t *config)
{
    sapi_status_t lifecycle_status;

    if ((storage == NULL) || (config == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): a voter is a setup-only resource - refuse once the
     * application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }

    switch (config->voting_strategy)
    {
        case SAPI_VOTING_2OO2:
        case SAPI_VOTING_2OO3:
            break;
        case SAPI_VOTING_NMR:
            if (config->quorum_size == 0U)
            {
                return SAPI_STATUS_INVALID_PARAM;
            }
            break;
        default:
            return SAPI_STATUS_INVALID_PARAM;
    }

    storage->config = *config;
    storage->channel_count = 0U;
    storage->total_disagreements = 0U;
    storage->initialized = true;

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_voter_register_channel(sapi_voter_t *voter, sapi_channel_t *channel)
{
    sapi_status_t lifecycle_status;

    if ((voter == NULL) || (channel == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!voter->initialized)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a channel into a voter is setup-only - refuse
     * once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }
    if (voter->channel_count >= SAPI_VOTER_MAX_CHANNELS)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    voter->channels[voter->channel_count] = channel;
    voter->channel_count++;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_voter_send(sapi_voter_t *voter, const void *data, size_t data_size)
{
    uint32_t i;
    uint32_t healthy_count = 0U;
    bool any_failed = false;
    bool any_timeout = false;
    sapi_channel_health_t h;

    if ((voter == NULL) || (data == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!voter->initialized)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (data_size > SAPI_VOTER_MAX_MESSAGE_SIZE)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    if (!voter_channel_count_matches_strategy(voter))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < voter->channel_count; i++)
    {
        sapi_status_t st;

        (void)sapi_channel_get_health(voter->channels[i], &h);
        if (!h.is_healthy)
        {
            continue;
        }
        healthy_count++;

        st = sapi_channel_send(voter->channels[i], data, data_size);
        if (st != SAPI_STATUS_OK)
        {
            any_failed = true;
            if (st == SAPI_STATUS_TIMEOUT)
            {
                any_timeout = true;
            }
        }
    }

    if (healthy_count == 0U)
    {
        return SAPI_STATUS_HARDWARE_FAULT;
    }
    if (any_timeout)
    {
        return SAPI_STATUS_TIMEOUT;
    }
    if (any_failed)
    {
        return SAPI_STATUS_HARDWARE_FAULT;
    }
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_voter_receive(sapi_voter_t *voter, void *data, size_t data_size,
                                  sapi_voting_result_t *result, size_t *bytes_received)
{
    uint8_t buffers[SAPI_VOTER_MAX_CHANNELS][SAPI_VOTER_MAX_MESSAGE_SIZE];
    bool responded[SAPI_VOTER_MAX_CHANNELS];
    uint32_t group_id[SAPI_VOTER_MAX_CHANNELS];
    uint32_t group_count[SAPI_VOTER_MAX_CHANNELS];
    uint32_t num_groups = 0U;
    uint32_t successful = 0U;
    uint32_t required_quorum;
    uint32_t best_group = 0U;
    uint32_t best_count = 0U;
    bool saw_timeout = false;
    sapi_voting_result_t local_result;
    uint32_t i;
    uint32_t j;

    if ((voter == NULL) || (data == NULL) || (data_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (!voter->initialized)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (data_size > SAPI_VOTER_MAX_MESSAGE_SIZE)
    {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }
    if (!voter_channel_count_matches_strategy(voter))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    required_quorum = voter_required_quorum(voter);

    for (i = 0U; i < voter->channel_count; i++)
    {
        responded[i] = false;
        group_count[i] = 0U;
    }

    if (voter_count_healthy(voter) < required_quorum)
    {
        local_result = SAPI_VOTING_INSUFFICIENT_QUORUM;
    }
    else
    {
        for (i = 0U; i < voter->channel_count; i++)
        {
            sapi_channel_health_t h;
            sapi_status_t st;

            (void)sapi_channel_get_health(voter->channels[i], &h);
            if (!h.is_healthy)
            {
                continue;
            }

            st = sapi_channel_receive(voter->channels[i], buffers[i], data_size,
                                             voter->config.channel_timeout_ms);
            if (st == SAPI_STATUS_OK)
            {
                responded[i] = true;
                successful++;
            }
            else if (st == SAPI_STATUS_TIMEOUT)
            {
                saw_timeout = true;
            }
            else
            {
                /* Any other failure just doesn't count toward successful;
                 * saw_timeout stays as-is. */
            }
        }

        if (successful == 0U)
        {
            local_result = saw_timeout ? SAPI_VOTING_TIMEOUT : SAPI_VOTING_INSUFFICIENT_QUORUM;
        }
        else if (successful < required_quorum)
        {
            local_result = SAPI_VOTING_INSUFFICIENT_QUORUM;
        }
        else
        {
            for (i = 0U; i < voter->channel_count; i++)
            {
                bool placed = false;

                if (!responded[i])
                {
                    continue;
                }
                for (j = 0U; j < i; j++)
                {
                    if (!responded[j])
                    {
                        continue;
                    }
                    if (voter_data_equal(voter, buffers[i], buffers[j], data_size))
                    {
                        group_id[i] = group_id[j];
                        group_count[group_id[i]]++;
                        placed = true;
                        break;
                    }
                }
                if (!placed)
                {
                    group_id[i] = num_groups;
                    group_count[num_groups] = 1U;
                    num_groups++;
                }
            }

            for (i = 0U; i < num_groups; i++)
            {
                if (group_count[i] > best_count)
                {
                    best_count = group_count[i];
                    best_group = i;
                }
            }

            if (best_count >= required_quorum)
            {
                for (i = 0U; i < voter->channel_count; i++)
                {
                    if (responded[i] && (group_id[i] == best_group))
                    {
                        (void)memcpy(data, buffers[i], data_size);
                        break;
                    }
                }
                local_result = SAPI_VOTING_AGREED;
            }
            else
            {
                local_result = SAPI_VOTING_DISAGREED;
            }
        }
    }

    if (result != NULL)
    {
        *result = local_result;
    }

    if (local_result == SAPI_VOTING_AGREED)
    {
        if (bytes_received != NULL)
        {
            *bytes_received = data_size;
        }
        return SAPI_STATUS_OK;
    }

    if (bytes_received != NULL)
    {
        *bytes_received = 0U;
    }

    if (local_result == SAPI_VOTING_DISAGREED)
    {
        voter->total_disagreements++;
        if (voter->config.log_disagreements)
        {
            sapi_log_write(SAPI_LOG_LEVEL_ERROR, "voter", "channels disagreed");
        }
        /* Application reacts BEFORE the Platform enters safe state below -
         * see this module's own RCA/OCORA PI-API compatibility note
         * (sapi_voter.h) for why the Platform now unconditionally owns
         * this transition. */
        if (voter->config.on_disagreement != NULL)
        {
            voter->config.on_disagreement(voter->config.disagreement_context, local_result);
        }
        SAPI_SAFESTATE(voter->config.safestate_level, voter->config.safestate_reason);
    }
    else if (voter->config.on_disagreement != NULL)
    {
        /* Non-DISAGREED non-AGREED results (TIMEOUT/INSUFFICIENT_QUORUM)
         * still get the callback, same as before this change, but do
         * NOT trigger a safestate transition here - only a genuine
         * DISAGREE does. */
        voter->config.on_disagreement(voter->config.disagreement_context, local_result);
    }
    else
    {
        /* Nothing further to do for a non-DISAGREED result with no
         * callback registered. */
    }

    return SAPI_STATUS_HARDWARE_FAULT;
}

sapi_status_t sapi_voter_get_aggregated_health(const sapi_voter_t *voter,
                                                uint32_t *healthy_count,
                                                uint32_t *total_disagreements)
{
    if (voter == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (healthy_count != NULL)
    {
        *healthy_count = voter_count_healthy(voter);
    }
    if (total_disagreements != NULL)
    {
        *total_disagreements = voter->total_disagreements;
    }
    return SAPI_STATUS_OK;
}

uint32_t sapi_voter_get_channel_count(const sapi_voter_t *voter)
{
    return (voter != NULL) ? voter->channel_count : 0U;
}

sapi_channel_t *sapi_voter_get_channel(const sapi_voter_t *voter, uint32_t index)
{
    if ((voter == NULL) || (index >= voter->channel_count))
    {
        return NULL;
    }
    return voter->channels[index];
}

sapi_channel_t *sapi_voter_get_channel_by_name(const sapi_voter_t *voter, const char *name)
{
    uint32_t i;

    if ((voter == NULL) || (name == NULL))
    {
        return NULL;
    }
    for (i = 0U; i < voter->channel_count; i++)
    {
        const char *channel_name = sapi_channel_get_name(voter->channels[i]);

        if ((channel_name != NULL) && (strcmp(channel_name, name) == 0))
        {
            return voter->channels[i];
        }
    }
    return NULL;
}

sapi_status_t sapi_voter_destroy(sapi_voter_t *voter)
{
    if (voter == NULL)
    {
        return SAPI_STATUS_OK;
    }
    voter->initialized = false;
    return SAPI_STATUS_OK;
}

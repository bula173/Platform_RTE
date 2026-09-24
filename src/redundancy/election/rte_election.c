/**
 * @file rte_election.c
 * @brief See rte_election.h.
 */
#include "rte/redundancy/election/rte_election.h"

#include <stddef.h>

/** Local makros */

/** Local types declarations */

/** Local function declarations */
static bool find_own_index(const rte_election_candidate_t *candidates, uint32_t candidate_count, uint32_t own_id,
                            uint32_t *out_index);
static uint32_t find_winner_index(const rte_election_candidate_t *candidates, uint32_t candidate_count);

/** Local variables declarations */

/** Global functions */
rte_status_t rte_election_init(rte_election_t *election, const rte_election_config_t *config)
{
    if ((election == NULL) || (config == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    election->own_id = config->own_id;
    election->own_last_state = RTE_DUAL_STATE_IDLE;
    election->winner_id = 0U;
    election->have_winner = false;

    return RTE_STATUS_OK;
}

rte_status_t rte_election_execute(rte_election_t *election, const rte_election_candidate_t *candidates,
                                    uint32_t candidate_count)
{
    uint32_t own_index;
    uint32_t winner_index;

    if (election == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (candidate_count > (uint32_t)RTE_REDUNDANCY_CONFIG_MAX_REPLICAS)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((candidates == NULL) && (candidate_count > 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    if (!find_own_index(candidates, candidate_count, election->own_id, &own_index))
    {
        /* No quorum this round to decide anything from - report, do not react
         * (REQ-ELECTION-003), matching rte_dual_negotiator_execute()'s own lost-peer posture. */
        election->own_last_state = RTE_DUAL_STATE_UNKNOWN;
        return RTE_STATUS_OK;
    }

    winner_index = find_winner_index(candidates, candidate_count);
    election->winner_id = candidates[winner_index].id;
    election->have_winner = true;

    if (winner_index == own_index)
    {
        election->own_last_state = RTE_DUAL_STATE_ONLINE;
    }
    else if (candidates[winner_index].degraded)
    {
        election->own_last_state = RTE_DUAL_STATE_COLDSTANDBY;
    }
    else
    {
        election->own_last_state = RTE_DUAL_STATE_HOTSTANDBY;
    }

    return RTE_STATUS_OK;
}

rte_dual_state_t rte_election_get_own_state(const rte_election_t *election)
{
    if (election == NULL)
    {
        return RTE_DUAL_STATE_IDLE;
    }
    return election->own_last_state;
}

rte_status_t rte_election_get_winner_id(const rte_election_t *election, uint32_t *out_id)
{
    if ((election == NULL) || (out_id == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (!election->have_winner)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    *out_id = election->winner_id;
    return RTE_STATUS_OK;
}

/****Local functions ****/

static bool find_own_index(const rte_election_candidate_t *candidates, uint32_t candidate_count, uint32_t own_id,
                            uint32_t *out_index)
{
    uint32_t i;

    for (i = 0U; i < candidate_count; i++)
    {
        if (candidates[i].id == own_id)
        {
            *out_index = i;
            return true;
        }
    }
    return false;
}

/* REQ-ELECTION-002: oldest (smallest) startup_timestamp_ms wins; smallest id breaks an exact
 * tie. candidate_count is always > 0 here - the only caller (rte_election_execute()) already
 * confirmed the own-id candidate exists, so there is always at least one entry. */
static uint32_t find_winner_index(const rte_election_candidate_t *candidates, uint32_t candidate_count)
{
    uint32_t winner = 0U;
    uint32_t i;

    for (i = 1U; i < candidate_count; i++)
    {
        if ((candidates[i].startup_timestamp_ms < candidates[winner].startup_timestamp_ms) ||
            ((candidates[i].startup_timestamp_ms == candidates[winner].startup_timestamp_ms) &&
             (candidates[i].id < candidates[winner].id)))
        {
            winner = i;
        }
    }
    return winner;
}

/**
 * @file test_rte_election.c
 * @brief Unit tests for rte_election (ADR-039 N-way election primitive).
 */
#include <assert.h>
#include <stdio.h>

#include "rte/redundancy/election/rte_election.h"

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            (void)fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                              \
        }                                                                          \
    } while (0)

static rte_status_t init_election(rte_election_t *e, uint32_t own_id)
{
    rte_election_config_t cfg;

    cfg.own_id = own_id;
    return rte_election_init(e, &cfg);
}

static int test_init_rejects_null(void)
{
    rte_election_t e;
    rte_election_config_t cfg = {0U};

    CHECK(rte_election_init(NULL, &cfg) == RTE_STATUS_INVALID_PARAM);
    CHECK(rte_election_init(&e, NULL) == RTE_STATUS_INVALID_PARAM);
    return 0;
}

static int test_two_way_oldest_timestamp_wins(void)
{
    rte_election_t e;
    rte_election_candidate_t cands[2];
    uint32_t winner;

    CHECK(init_election(&e, 1U) == RTE_STATUS_OK);

    cands[0].id = 1U;
    cands[0].startup_timestamp_ms = 500U;
    cands[0].degraded = false;
    cands[1].id = 2U;
    cands[1].startup_timestamp_ms = 100U; /* older - wins */
    cands[1].degraded = false;

    CHECK(rte_election_execute(&e, cands, 2U) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e) == RTE_DUAL_STATE_HOTSTANDBY);
    CHECK(rte_election_get_winner_id(&e, &winner) == RTE_STATUS_OK);
    CHECK(winner == 2U);
    return 0;
}

static int test_this_instance_wins(void)
{
    rte_election_t e;
    rte_election_candidate_t cands[3];

    CHECK(init_election(&e, 7U) == RTE_STATUS_OK);

    cands[0].id = 7U; /* self */
    cands[0].startup_timestamp_ms = 10U;
    cands[0].degraded = false;
    cands[1].id = 8U;
    cands[1].startup_timestamp_ms = 20U;
    cands[1].degraded = false;
    cands[2].id = 9U;
    cands[2].startup_timestamp_ms = 30U;
    cands[2].degraded = false;

    CHECK(rte_election_execute(&e, cands, 3U) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e) == RTE_DUAL_STATE_ONLINE);
    return 0;
}

static int test_exact_tie_breaks_on_smallest_id(void)
{
    rte_election_t e_low;
    rte_election_t e_high;
    rte_election_candidate_t cands[2];

    cands[0].id = 5U;
    cands[0].startup_timestamp_ms = 1000U;
    cands[0].degraded = false;
    cands[1].id = 9U;
    cands[1].startup_timestamp_ms = 1000U; /* exact tie - smaller id (5) wins */
    cands[1].degraded = false;

    CHECK(init_election(&e_low, 5U) == RTE_STATUS_OK);
    CHECK(rte_election_execute(&e_low, cands, 2U) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e_low) == RTE_DUAL_STATE_ONLINE);

    CHECK(init_election(&e_high, 9U) == RTE_STATUS_OK);
    CHECK(rte_election_execute(&e_high, cands, 2U) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e_high) == RTE_DUAL_STATE_HOTSTANDBY);
    return 0;
}

static int test_winner_degraded_makes_standby_cold(void)
{
    rte_election_t e;
    rte_election_candidate_t cands[2];

    CHECK(init_election(&e, 2U) == RTE_STATUS_OK);

    cands[0].id = 1U;
    cands[0].startup_timestamp_ms = 10U; /* wins */
    cands[0].degraded = true;            /* winner's own degradation... */
    cands[1].id = 2U;                    /* self */
    cands[1].startup_timestamp_ms = 20U;
    cands[1].degraded = false; /* ...determines OUR hot/cold, not our own flag (REQ-ELECTION-005) */

    CHECK(rte_election_execute(&e, cands, 2U) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e) == RTE_DUAL_STATE_COLDSTANDBY);
    return 0;
}

static int test_self_absent_from_snapshot_is_unknown(void)
{
    rte_election_t e;
    rte_election_candidate_t cands[1];
    uint32_t winner;

    CHECK(init_election(&e, 99U) == RTE_STATUS_OK);

    cands[0].id = 1U;
    cands[0].startup_timestamp_ms = 10U;
    cands[0].degraded = false;

    CHECK(rte_election_execute(&e, cands, 1U) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e) == RTE_DUAL_STATE_UNKNOWN);
    /* No winner has ever been produced for this instance yet. */
    CHECK(rte_election_get_winner_id(&e, &winner) == RTE_STATUS_NOT_INITIALIZED);
    return 0;
}

static int test_empty_snapshot_is_unknown(void)
{
    rte_election_t e;

    CHECK(init_election(&e, 1U) == RTE_STATUS_OK);
    CHECK(rte_election_execute(&e, NULL, 0U) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e) == RTE_DUAL_STATE_UNKNOWN);
    return 0;
}

static int test_execute_rejects_invalid_args(void)
{
    rte_election_t e;
    rte_election_candidate_t one[1] = {{1U, 10U, false}};

    CHECK(init_election(&e, 1U) == RTE_STATUS_OK);
    CHECK(rte_election_execute(NULL, one, 1U) == RTE_STATUS_INVALID_PARAM);
    CHECK(rte_election_execute(&e, NULL, 1U) == RTE_STATUS_INVALID_PARAM);
    CHECK(rte_election_execute(&e, one, (uint32_t)RTE_REDUNDANCY_CONFIG_MAX_REPLICAS + 1U) ==
          RTE_STATUS_INVALID_PARAM);
    return 0;
}

static int test_getters_tolerate_null(void)
{
    uint32_t out;

    CHECK(rte_election_get_own_state(NULL) == RTE_DUAL_STATE_IDLE);
    CHECK(rte_election_get_winner_id(NULL, &out) == RTE_STATUS_INVALID_PARAM);
    return 0;
}

static int test_max_replica_count_boundary(void)
{
    rte_election_t e;
    rte_election_candidate_t cands[RTE_REDUNDANCY_CONFIG_MAX_REPLICAS];
    uint32_t i;

    CHECK(init_election(&e, 1U) == RTE_STATUS_OK);
    for (i = 0U; i < (uint32_t)RTE_REDUNDANCY_CONFIG_MAX_REPLICAS; i++)
    {
        cands[i].id = i + 1U;
        cands[i].startup_timestamp_ms = 1000U - i; /* last entry (highest i) is oldest -> wins */
        cands[i].degraded = false;
    }
    CHECK(rte_election_execute(&e, cands, (uint32_t)RTE_REDUNDANCY_CONFIG_MAX_REPLICAS) == RTE_STATUS_OK);
    CHECK(rte_election_get_own_state(&e) == RTE_DUAL_STATE_HOTSTANDBY);
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_init_rejects_null();
    failures += test_two_way_oldest_timestamp_wins();
    failures += test_this_instance_wins();
    failures += test_exact_tie_breaks_on_smallest_id();
    failures += test_winner_degraded_makes_standby_cold();
    failures += test_self_absent_from_snapshot_is_unknown();
    failures += test_empty_snapshot_is_unknown();
    failures += test_execute_rejects_invalid_args();
    failures += test_getters_tolerate_null();
    failures += test_max_replica_count_boundary();

    if (failures == 0)
    {
        (void)printf("test_rte_election: all tests passed\n");
    }
    return failures;
}

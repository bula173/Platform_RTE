/**
 * @file rte_election.h
 * @brief N-way election primitive (ADR-039): generalizes rte_dual_negotiator's pairwise
 *        older-startup-timestamp-wins tie-break to RTE_REDUNDANCY_CONFIG_MAX_REPLICAS
 *        candidates, for the 2oo3/NMR topologies rte_dual_negotiator cannot express.
 *
 * Deliberately transport-agnostic, unlike rte_dual_negotiator (which owns an
 * rte_dual_channel_t and drives its own beacon traffic): rte_dual_channel_t is inherently
 * point-to-point (ADR-020 section 2), so there is no ready-made N-way beacon transport to build
 * this on yet (ADR-039's own "shape TBD" note). Instead this module is a pure state-computation
 * primitive - same posture as rte_voter_t, which already operates on caller-collected buffers
 * rather than owning a channel of its own (ADR-025). The caller is responsible for gathering
 * each cycle's candidate snapshot however its own topology's transport works (N pairwise
 * rte_dual_channel_t instances, a broadcast link, etc. - out of this module's scope) and passing
 * it to rte_election_execute() as a plain array.
 *
 * Additive only: does not modify, replace, or get called by rte_dual_negotiator or
 * rte_cross_comparator. The 2oo2 path (both of those, and everything built on them in RBC_GP
 * today) is completely unaffected by this module's existence - it has no callers yet (ADR-039's
 * own step 3, wiring GP's 2oo3/NMR path to this, is separate, not-yet-started scope).
 *
 * REQ-ELECTION-001: no dynamic allocation; caller supplies storage and, each
 *                    rte_election_execute() call, a caller-gathered candidate snapshot.
 * REQ-ELECTION-002: the winner is the candidate with the OLDEST (smallest) startup_timestamp_ms
 *                    among all candidates currently in contact (see REQ-ELECTION-004); on an
 *                    exact timestamp tie, the smallest id wins - the same rule
 *                    rte_dual_negotiator uses (REQ-DUAL-NEGOTIATOR-003), generalized from 2 to N
 *                    candidates instead of special-cased for exactly 2.
 * REQ-ELECTION-003: rte_election_execute() shall never call rte_safestate_enter() itself - same
 *                    "no automatic safety reaction" posture as rte_dual_negotiator
 *                    (REQ-DUAL-NEGOTIATOR-002): deciding what a sustained lack of quorum means
 *                    for safety stays an application policy decision.
 * REQ-ELECTION-004: a candidate not present in the snapshot passed to the current
 *                    rte_election_execute() call (including this instance's own prior view of a
 *                    candidate that has since dropped out) is treated as not currently in
 *                    contact and cannot win that round - callers omit rather than mark timed-out
 *                    candidates.
 * REQ-ELECTION-005: every non-winning candidate that IS in contact is reported
 *                    RTE_DUAL_STATE_HOTSTANDBY if the winner's own `degraded` flag is false,
 *                    RTE_DUAL_STATE_COLDSTANDBY if true - the same "the ONLINE side's own
 *                    degradation determines every STANDBY side's HOT/COLD label, never a
 *                    STANDBY side's own self-report" rule rte_dual_negotiator uses
 *                    (REQ-DUAL-NEGOTIATOR-004), generalized from one standby to N-1.
 *
 * @defgroup ELECTION
 * @{
 */
#ifndef RTE_ELECTION_H
#define RTE_ELECTION_H

#include <stdbool.h>
#include <stdint.h>

#include "rte/redundancy/config/rte_redundancy_config.h"
#include "rte/redundancy/dual/rte_dual_types.h"
#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One candidate's info for one rte_election_execute() call. The caller builds this
 *        array fresh each call from whatever transport it uses to learn about the other
 *        replicas - this module does not retain it between calls except via the state
 *        recomputed into rte_election_t itself.
 */
typedef struct
{
    /** This candidate's tie-break identifier. Must be unique within one call's snapshot;
     *  duplicate ids within a single snapshot make that call's result unspecified (defensive
     *  behavior only - see rte_election_execute()'s own doc). */
    uint32_t id;
    /** This candidate's own startup timestamp, same clock domain/meaning as
     *  rte_dual_negotiator_config_t::own_id's own timestamp (older/smaller wins). */
    uint64_t startup_timestamp_ms;
    /** This candidate's own redundancy-degradation bit (e.g. an already-degraded
     *  rte_dual_channel_t sub-link count), consulted only if this candidate wins
     *  (REQ-ELECTION-005). Ignored for a losing candidate. */
    bool degraded;
} rte_election_candidate_t;

/**
 * @brief One rte_election_t instance's state. Caller-owned storage; every field is private -
 *        reach it only through the functions below.
 */
typedef struct rte_election_s
{
    uint32_t own_id;
    rte_dual_state_t own_last_state;

    uint32_t winner_id;
    bool     have_winner;
} rte_election_t;

/** @brief Configuration for rte_election_init(). */
typedef struct
{
    /** This instance's own tie-break identifier - must also appear in every
     *  rte_election_execute() call's own candidate snapshot for this instance to be able to
     *  win (an instance that never includes itself in its own snapshot can never be elected -
     *  a defensive design choice: the caller decides whether it is even a candidate this
     *  cycle, not this module). */
    uint32_t own_id;
} rte_election_config_t;

/**
 * @brief Initializes a rte_election_t. own_last_state starts at RTE_DUAL_STATE_IDLE (never yet
 *        evaluated).
 * @param election  Caller-owned storage to initialize. Must not be NULL.
 * @param config    Configuration. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM otherwise.
 */
rte_status_t rte_election_init(rte_election_t *election, const rte_election_config_t *config);

/**
 * @brief Evaluates one round of election against the caller-supplied candidate snapshot
 *        (REQ-ELECTION-002 through -005). Empty snapshot (candidate_count == 0, or this
 *        instance's own_id not present in it) yields RTE_DUAL_STATE_UNKNOWN - no quorum to
 *        decide anything from, same "report, do not react" posture as
 *        rte_dual_negotiator_execute() on a lost peer.
 * @param election         Initialized election. Must not be NULL.
 * @param candidates        This round's snapshot. May be NULL only if candidate_count is 0.
 * @param candidate_count   Number of entries in @p candidates; must not exceed
 *                          RTE_REDUNDANCY_CONFIG_MAX_REPLICAS (RTE_STATUS_INVALID_PARAM if it
 *                          does - a caller-side bug, not a runtime condition to degrade
 *                          gracefully from).
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a NULL election, an over-long
 *         candidate_count, or a NULL candidates with a nonzero candidate_count.
 */
rte_status_t rte_election_execute(rte_election_t *election, const rte_election_candidate_t *candidates,
                                    uint32_t candidate_count);

/**
 * @brief Returns this instance's own current rte_dual_state_t, as of the last
 *        rte_election_execute() call.
 * @param election  Election to query. May be NULL (returns RTE_DUAL_STATE_IDLE, defensive
 *                  default).
 * @return The current own state.
 */
rte_dual_state_t rte_election_get_own_state(const rte_election_t *election);

/**
 * @brief Returns the last-elected winner's id.
 * @param election  Election to query. May be NULL.
 * @param out_id     Receives the winner's id. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_NOT_INITIALIZED if no round has ever produced a winner yet
 *         (e.g. every rte_election_execute() call so far had an empty/self-absent snapshot);
 *         RTE_STATUS_INVALID_PARAM if election or out_id is NULL.
 */
rte_status_t rte_election_get_winner_id(const rte_election_t *election, uint32_t *out_id);

#ifdef __cplusplus
}
#endif

#endif /* RTE_ELECTION_H */

/** @} */

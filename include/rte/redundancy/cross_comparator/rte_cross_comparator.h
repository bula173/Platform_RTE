/**
 * @file rte_cross_comparator.h
 * @brief Pairwise comparison between exactly 2 registered
 *        rte_channel_t links (ADR-025).
 *
 * Distinct from rte_voter: a voter does N-modular-redundancy voting on
 * channels that are expected to carry the *same* computation (pick the
 * majority value). A cross-comparator checks *consistency* between two
 * independent channels/peers whose agreement is a diagnostic invariant,
 * not a value to pick a winner from - generalizes the pattern
 * `safeAPIRBC2oo2`'s `channel_ab_crosscompare.c` hand-rolls today.
 *
 * RCA/OCORA PI-API compatibility note (see
 * ../../../../../docs/rca/RCA-OCORA-SCP-Mapping.md at the workspace
 * root): per OCORA's Safe Computing Platform model, the Platform - not
 * the application - owns the decision to transition to a safe state on
 * disagreement between replicas/peers. rte_cross_comparator_execute()
 * therefore ALWAYS enters config->safestate_level on a DISAGREED
 * result; an application can no longer opt out of the transition
 * happening (only earlier, on_disagreement gave the application a
 * chance to react - e.g. clean up - before it does).
 *
 * @defgroup cross_comparator Cross-Comparator (2-way channel comparison)
 * @{
 */

#ifndef RTE_CROSS_COMPARATOR_H
#define RTE_CROSS_COMPARATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "rte/utils/status/rte_status.h"
#include "rte/utils/safestate/rte_safestate.h"
#include "rte/redundancy/channel_link/rte_channel.h"
#include "rte/redundancy/voter/rte_voter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for rte_cross_comparator_init().
 */
typedef struct {
    /** Timeout for each channel's receive operation (milliseconds). */
    uint32_t channel_timeout_ms;
    /** Optional custom comparison callback (same typedef as rte_voter);
     *  NULL -> memcmp default. */
    rte_voter_compare_fn compare;
    /** User context passed to compare(). */
    void *compare_context;
    /** Log a disagreement via rte_log_write() when it occurs. */
    bool log_disagreements;
    /** Safe-state level entered unconditionally on a DISAGREED result,
     *  after on_disagreement (below) has already run - see this
     *  header's own "RCA/OCORA PI-API compatibility" note above
     *  rte_cross_comparator_execute(). Replaces the old
     *  `bool trigger_safestate_on_disagreement` (which let a caller
     *  opt OUT of transitioning at all - the Platform, not the
     *  application, now always owns this decision). Use
     *  RTE_SAFESTATE_LEVEL_SAFE as the conservative default if the
     *  caller has no stronger reaction (e.g. REBOOT) of its own. */
    rte_safestate_level_t safestate_level;
    /** Diagnostic reason code passed to the safestate transition above. */
    rte_safestate_reason_t safestate_reason;
    /** Optional callback invoked whenever the result is not
     *  RTE_VOTING_AGREED, BEFORE the unconditional safestate
     *  transition above - the application's one chance to react (e.g.
     *  flush peer-negotiation state, raise an alarm, close its own
     *  links) before control does not return. Do this work
     *  synchronously within the callback; there is no "after" for a
     *  SAFE/REBOOT-level transition (REQ-COMMON-SAFESTATE-002). */
    void (*on_disagreement)(void *context, rte_voting_result_t result);
    /** User context passed to on_disagreement(). */
    void *disagreement_context;
} rte_cross_comparator_config_t;

/** @brief Maximum payload size rte_cross_comparator_execute() supports. */
#define RTE_CROSS_COMPARATOR_MAX_MESSAGE_SIZE 256U

/**
 * @brief Storage for one cross-comparator instance (opaque to caller).
 * No dynamic memory.
 */
typedef struct {
    rte_cross_comparator_config_t config;
    rte_channel_t *channel_a;
    rte_channel_t *channel_b;
    uint32_t registered_count;
    uint32_t total_disagreements;
    bool initialized;
} rte_cross_comparator_storage_t;

/** @brief Opaque handle to a cross-comparator instance. */
typedef rte_cross_comparator_storage_t rte_cross_comparator_t;

/**
 * @brief Initializes a cross-comparator with zero registered channels.
 *
 * @param[out] storage  Pre-allocated storage. Must not be NULL.
 * @param[in]  config   Configuration. Must not be NULL.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if storage or config is NULL
 *
 * REQ-CROSSCOMPARATOR-001: No dynamic allocation; exactly 2 channel slots.
 */
rte_status_t rte_cross_comparator_init(rte_cross_comparator_storage_t *storage,
                                          const rte_cross_comparator_config_t *config);

/**
 * @brief Registers one already-initialized channel (channel A, then B).
 *
 * @param[in] cmp      Cross-comparator handle. Must not be NULL and
 *                     must be initialized.
 * @param[in] channel  Already rte_channel_init()'d channel. Must
 *                     not be NULL.
 *
 * @return RTE_STATUS_OK on success (first call registers A, second
 *         registers B)
 * @return RTE_STATUS_INVALID_PARAM if cmp/channel is NULL
 * @return RTE_STATUS_NOT_INITIALIZED if cmp was never initialized
 * @return RTE_STATUS_RESOURCE_EXHAUSTED if both A and B are already
 *         registered (a 3rd call)
 *
 * REQ-CROSSCOMPARATOR-002: A 3rd registration attempt is rejected
 * without disturbing the 2 already-registered channels.
 */
rte_status_t rte_cross_comparator_register_channel(rte_cross_comparator_t *cmp,
                                                      rte_channel_t *channel);

/**
 * @brief Receives from both registered channels and compares them.
 *
 * @param[in]  cmp            Cross-comparator handle. Must not be NULL,
 *                            initialized, with exactly 2 channels
 *                            registered.
 * @param[in]  data_size      Bytes to receive/compare per channel; must
 *                            be > 0 and <=
 *                            RTE_CROSS_COMPARATOR_MAX_MESSAGE_SIZE.
 * @param[out] result         Comparison outcome. Can be NULL.
 * @param[out] out_data       Receives channel A's data on AGREED. Can be
 *                            NULL if the caller only cares about result.
 * @param[out] out_size       Bytes written to out_data. Can be NULL.
 *
 * @return RTE_STATUS_OK if result is RTE_VOTING_AGREED
 * @return RTE_STATUS_INVALID_PARAM for a bad argument, or if fewer
 *         than 2 channels are registered
 * @return RTE_STATUS_HARDWARE_FAULT otherwise (DISAGREED/TIMEOUT/
 *         INSUFFICIENT_QUORUM - the latter meaning one or both channels
 *         are marked unhealthy)
 *
 * @post On RTE_VOTING_DISAGREED, this function invokes
 *       config->on_disagreement (if set) and then unconditionally
 *       enters config->safestate_level - it does not return unless
 *       that level is RTE_SAFESTATE_LEVEL_DEGRADED (the only level
 *       rte_safestate_enter() may return from).
 *
 * REQ-CROSSCOMPARATOR-003: Both channels must be healthy and
 * successfully receive data_size bytes before comparison is attempted;
 * otherwise short-circuits to TIMEOUT/INSUFFICIENT_QUORUM.
 * REQ-CROSSCOMPARATOR-004: Uses config->compare if registered, else a
 * full memcmp() - CRC-64 transport integrity is a separate, already-
 * applied concern, never itself treated as the comparison.
 */
rte_status_t rte_cross_comparator_execute(rte_cross_comparator_t *cmp, size_t data_size,
                                             rte_voting_result_t *result,
                                             void *out_data, size_t *out_size);

/**
 * @brief Compares two already-in-hand data buffers directly, with the
 *        same on_disagreement-then-unconditional-safestate semantics as
 *        rte_cross_comparator_execute() - no rte_channel_t registration
 *        needed. Added per direct request as part of the RCA/OCORA
 *        compatibility initiative's Phase 2b (see docs/rca/ at the
 *        workspace root and TODO.md): an integrator whose local/peer data
 *        already arrived through its own transport (e.g.
 *        safeAPIRBC2oo2GP's kind-multiplexed peer_channel, which carries
 *        cross-compare traffic alongside checkpoint/site-state frames on
 *        one shared link - not something a generic comparator API can
 *        transparently subsume) no longer needs to hand-wire two pure
 *        in-memory rte_channel_t "already arrived" adapters (send() that
 *        is never called, recv() that just returns a buffer) purely to
 *        satisfy rte_cross_comparator_execute()'s channel-based
 *        interface - this collapses that boilerplate to one call.
 *
 * A cross-comparator used with this function does not need
 * rte_cross_comparator_register_channel() called on it at all - only
 * rte_cross_comparator_init(). Mixing the two calling styles on the same
 * rte_cross_comparator_t is allowed (this function ignores any
 * registered channels; rte_cross_comparator_execute() ignores this
 * function's own lack of them) but is not a pattern any current caller
 * uses.
 *
 * @param[in]  cmp        Cross-comparator handle (rte_cross_comparator_init()'d).
 *                        Must not be NULL.
 * @param[in]  local_data This side's own data. Must not be NULL.
 * @param[in]  peer_data  The counterpart's data, already received via
 *                        whatever transport the caller owns. Must not be
 *                        NULL.
 * @param[in]  data_size  Bytes to compare; must be > 0 and <=
 *                        RTE_CROSS_COMPARATOR_MAX_MESSAGE_SIZE.
 * @param[out] result     Comparison outcome (always RTE_VOTING_AGREED or
 *                        RTE_VOTING_DISAGREED - there is no I/O here, so
 *                        TIMEOUT/INSUFFICIENT_QUORUM never occur). Can be
 *                        NULL.
 * @param[out] out_data   Receives local_data on AGREED. Can be NULL.
 * @param[out] out_size   Bytes written to out_data. Can be NULL.
 *
 * @return RTE_STATUS_OK if result is RTE_VOTING_AGREED.
 * @return RTE_STATUS_INVALID_PARAM for a bad argument.
 * @return RTE_STATUS_HARDWARE_FAULT on RTE_VOTING_DISAGREED - but see
 *         @post: this function does not return in that case.
 *
 * @post On RTE_VOTING_DISAGREED, identical to rte_cross_comparator_execute():
 *       invokes config->on_disagreement (if set) and then unconditionally
 *       enters config->safestate_level.
 */
rte_status_t rte_cross_comparator_execute_buffers(rte_cross_comparator_t *cmp,
                                                      const void *local_data, const void *peer_data,
                                                      size_t data_size,
                                                      rte_voting_result_t *result,
                                                      void *out_data, size_t *out_size);

/**
 * @brief Aggregated health across both registered channels.
 *
 * @param[in]  cmp                 Cross-comparator handle. Must not be NULL.
 * @param[out] healthy_count       Number of currently-healthy registered
 *                                 channels (0, 1, or 2). Can be NULL.
 * @param[out] total_disagreements Running count of DISAGREED results.
 *                                 Can be NULL.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if cmp is NULL
 */
rte_status_t rte_cross_comparator_get_aggregated_health(const rte_cross_comparator_t *cmp,
                                                           uint32_t *healthy_count,
                                                           uint32_t *total_disagreements);

/**
 * @brief Destroys a cross-comparator instance.
 *
 * @param[in] cmp  Cross-comparator handle. May be NULL.
 * @return RTE_STATUS_OK always.
 * @safety Idempotent; safe to call with NULL.
 */
rte_status_t rte_cross_comparator_destroy(rte_cross_comparator_t *cmp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* RTE_CROSS_COMPARATOR_H */

/**
 * @file sapi_cross_comparator.h
 * @brief Pairwise comparison between exactly 2 registered
 *        sapi_channel_t links (ADR-025).
 *
 * Distinct from sapi_voter: a voter does N-modular-redundancy voting on
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
 * disagreement between replicas/peers. sapi_cross_comparator_execute()
 * therefore ALWAYS enters config->safestate_level on a DISAGREED
 * result; an application can no longer opt out of the transition
 * happening (only earlier, on_disagreement gave the application a
 * chance to react - e.g. clean up - before it does).
 *
 * @defgroup cross_comparator Cross-Comparator (2-way channel comparison)
 * @{
 */

#ifndef SAPI_CROSS_COMPARATOR_H
#define SAPI_CROSS_COMPARATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/safestate/sapi_safestate.h"
#include "safeapi/redundancy/channel_link/sapi_channel.h"
#include "safeapi/redundancy/voter/sapi_voter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for sapi_cross_comparator_init().
 */
typedef struct {
    /** Timeout for each channel's receive operation (milliseconds). */
    uint32_t channel_timeout_ms;
    /** Optional custom comparison callback (same typedef as sapi_voter);
     *  NULL -> memcmp default. */
    sapi_voter_compare_fn compare;
    /** User context passed to compare(). */
    void *compare_context;
    /** Log a disagreement via sapi_log_write() when it occurs. */
    bool log_disagreements;
    /** Safe-state level entered unconditionally on a DISAGREED result,
     *  after on_disagreement (below) has already run - see this
     *  header's own "RCA/OCORA PI-API compatibility" note above
     *  sapi_cross_comparator_execute(). Replaces the old
     *  `bool trigger_safestate_on_disagreement` (which let a caller
     *  opt OUT of transitioning at all - the Platform, not the
     *  application, now always owns this decision). Use
     *  SAPI_SAFESTATE_LEVEL_SAFE as the conservative default if the
     *  caller has no stronger reaction (e.g. REBOOT) of its own. */
    sapi_safestate_level_t safestate_level;
    /** Diagnostic reason code passed to the safestate transition above. */
    sapi_safestate_reason_t safestate_reason;
    /** Optional callback invoked whenever the result is not
     *  SAPI_VOTING_AGREED, BEFORE the unconditional safestate
     *  transition above - the application's one chance to react (e.g.
     *  flush peer-negotiation state, raise an alarm, close its own
     *  links) before control does not return. Do this work
     *  synchronously within the callback; there is no "after" for a
     *  SAFE/REBOOT-level transition (REQ-COMMON-SAFESTATE-002). */
    void (*on_disagreement)(void *context, sapi_voting_result_t result);
    /** User context passed to on_disagreement(). */
    void *disagreement_context;
} sapi_cross_comparator_config_t;

/** @brief Maximum payload size sapi_cross_comparator_execute() supports. */
#define SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE 256U

/**
 * @brief Storage for one cross-comparator instance (opaque to caller).
 * No dynamic memory.
 */
typedef struct {
    sapi_cross_comparator_config_t config;
    sapi_channel_t *channel_a;
    sapi_channel_t *channel_b;
    uint32_t registered_count;
    uint32_t total_disagreements;
    bool initialized;
} sapi_cross_comparator_storage_t;

/** @brief Opaque handle to a cross-comparator instance. */
typedef sapi_cross_comparator_storage_t sapi_cross_comparator_t;

/**
 * @brief Initializes a cross-comparator with zero registered channels.
 *
 * @param[out] storage  Pre-allocated storage. Must not be NULL.
 * @param[in]  config   Configuration. Must not be NULL.
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if storage or config is NULL
 *
 * REQ-CROSSCOMPARATOR-001: No dynamic allocation; exactly 2 channel slots.
 */
sapi_status_t sapi_cross_comparator_init(sapi_cross_comparator_storage_t *storage,
                                          const sapi_cross_comparator_config_t *config);

/**
 * @brief Registers one already-initialized channel (channel A, then B).
 *
 * @param[in] cmp      Cross-comparator handle. Must not be NULL and
 *                     must be initialized.
 * @param[in] channel  Already sapi_channel_init()'d channel. Must
 *                     not be NULL.
 *
 * @return SAPI_STATUS_OK on success (first call registers A, second
 *         registers B)
 * @return SAPI_STATUS_INVALID_PARAM if cmp/channel is NULL
 * @return SAPI_STATUS_NOT_INITIALIZED if cmp was never initialized
 * @return SAPI_STATUS_RESOURCE_EXHAUSTED if both A and B are already
 *         registered (a 3rd call)
 *
 * REQ-CROSSCOMPARATOR-002: A 3rd registration attempt is rejected
 * without disturbing the 2 already-registered channels.
 */
sapi_status_t sapi_cross_comparator_register_channel(sapi_cross_comparator_t *cmp,
                                                      sapi_channel_t *channel);

/**
 * @brief Receives from both registered channels and compares them.
 *
 * @param[in]  cmp            Cross-comparator handle. Must not be NULL,
 *                            initialized, with exactly 2 channels
 *                            registered.
 * @param[in]  data_size      Bytes to receive/compare per channel; must
 *                            be > 0 and <=
 *                            SAPI_CROSS_COMPARATOR_MAX_MESSAGE_SIZE.
 * @param[out] result         Comparison outcome. Can be NULL.
 * @param[out] out_data       Receives channel A's data on AGREED. Can be
 *                            NULL if the caller only cares about result.
 * @param[out] out_size       Bytes written to out_data. Can be NULL.
 *
 * @return SAPI_STATUS_OK if result is SAPI_VOTING_AGREED
 * @return SAPI_STATUS_INVALID_PARAM for a bad argument, or if fewer
 *         than 2 channels are registered
 * @return SAPI_STATUS_HARDWARE_FAULT otherwise (DISAGREED/TIMEOUT/
 *         INSUFFICIENT_QUORUM - the latter meaning one or both channels
 *         are marked unhealthy)
 *
 * @post On SAPI_VOTING_DISAGREED, this function invokes
 *       config->on_disagreement (if set) and then unconditionally
 *       enters config->safestate_level - it does not return unless
 *       that level is SAPI_SAFESTATE_LEVEL_DEGRADED (the only level
 *       sapi_safestate_enter() may return from).
 *
 * REQ-CROSSCOMPARATOR-003: Both channels must be healthy and
 * successfully receive data_size bytes before comparison is attempted;
 * otherwise short-circuits to TIMEOUT/INSUFFICIENT_QUORUM.
 * REQ-CROSSCOMPARATOR-004: Uses config->compare if registered, else a
 * full memcmp() - CRC-64 transport integrity is a separate, already-
 * applied concern, never itself treated as the comparison.
 */
sapi_status_t sapi_cross_comparator_execute(sapi_cross_comparator_t *cmp, size_t data_size,
                                             sapi_voting_result_t *result,
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
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if cmp is NULL
 */
sapi_status_t sapi_cross_comparator_get_aggregated_health(const sapi_cross_comparator_t *cmp,
                                                           uint32_t *healthy_count,
                                                           uint32_t *total_disagreements);

/**
 * @brief Destroys a cross-comparator instance.
 *
 * @param[in] cmp  Cross-comparator handle. May be NULL.
 * @return SAPI_STATUS_OK always.
 * @safety Idempotent; safe to call with NULL.
 */
sapi_status_t sapi_cross_comparator_destroy(sapi_cross_comparator_t *cmp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* SAPI_CROSS_COMPARATOR_H */

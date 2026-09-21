/**
 * @file rte_voter.h
 * @brief N-way voting across registered rte_channel_t links
 *        (2oo2/2oo3/NMR).
 *
 * ADR-025: this is the voting engine that used to live inside
 * `rte_channel` before that type was split into a single-link
 * primitive. Register 1..RTE_VOTER_MAX_CHANNELS already-initialized
 * `rte_channel_t` instances, then use rte_voter_send()/
 * _receive() the way rte_channel_send()/_receive() used to work.
 *
 * RCA/OCORA PI-API compatibility note (see
 * ../../../../../docs/rca/RCA-OCORA-SCP-Mapping.md at the workspace
 * root, and rte_cross_comparator.h's own identical note): per OCORA's
 * Safe Computing Platform model, the Platform - not the application -
 * owns the decision to transition to a safe state on disagreement.
 * rte_voter_receive() therefore ALWAYS enters config->safestate_level
 * on a DISAGREED result; an application can no longer opt out.
 *
 * @defgroup voter Voter (N-way channel voting)
 * @{
 */

#ifndef RTE_VOTER_H
#define RTE_VOTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/safestate/rte_safestate.h"
#include "safeapi/redundancy/channel_link/rte_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Voting strategy: how many registered channels must agree.
 */
typedef enum {
    /** 2 out of 2: both channels must agree. */
    RTE_VOTING_2OO2 = 1,
    /** 2 out of 3: majority among 3 channels, tolerates 1 fault. */
    RTE_VOTING_2OO3 = 2,
    /** N out of M: rte_voter_config_t::quorum_size of registered channels must agree. */
    RTE_VOTING_NMR = 3
} rte_voting_strategy_t;

/**
 * @brief Outcome of one rte_voter_receive() call.
 */
typedef enum {
    /** A quorum-sized group of channels agreed on the data. */
    RTE_VOTING_AGREED = 0,
    /** No group of channels large enough to meet quorum agreed. */
    RTE_VOTING_DISAGREED = 1,
    /** One or more channels timed out. */
    RTE_VOTING_TIMEOUT = 2,
    /** Fewer than the required number of healthy/responding channels. */
    RTE_VOTING_INSUFFICIENT_QUORUM = 3
} rte_voting_result_t;

/**
 * @brief Optional custom comparison callback.
 *
 * If not registered (rte_voter_config_t::compare == NULL), the default
 * is a full byte compare (`memcmp(a, b, size) == 0`) - the payloads
 * being compared have already had their transport-integrity CRC
 * verified by the channel layer (rte_checksum) if the OSAdapter uses
 * rte_checksum_vital_message_verify(); this callback is about
 * semantic/value comparison, not integrity checking.
 *
 * @param[in] a         First buffer.
 * @param[in] b         Second buffer.
 * @param[in] size      Number of bytes to compare.
 * @param[in] user_ctx  rte_voter_config_t::compare_context, passed
 *                      through verbatim.
 * @return true if a and b are considered equal for voting purposes.
 */
typedef bool (*rte_voter_compare_fn)(const void *a, const void *b, size_t size, void *user_ctx);

/**
 * @brief Configuration for rte_voter_init().
 */
typedef struct {
    /** Voting strategy. */
    rte_voting_strategy_t voting_strategy;
    /** Required agreeing-channel count for RTE_VOTING_NMR (ignored for
     *  2OO2/2OO3, which have a fixed quorum of 2). */
    uint32_t quorum_size;
    /** Timeout for each channel's receive operation (milliseconds). */
    uint32_t channel_timeout_ms;
    /** Optional custom comparison callback; NULL -> memcmp default. */
    rte_voter_compare_fn compare;
    /** User context passed to compare(). */
    void *compare_context;
    /** Log a disagreement via rte_log_write() when it occurs. */
    bool log_disagreements;
    /** Safe-state level entered unconditionally on a DISAGREED result,
     *  after on_disagreement (below) has already run. RCA/OCORA PI-API
     *  compatibility note (see docs/rca/RCA-OCORA-SCP-Mapping.md at the
     *  workspace root, and rte_cross_comparator.h's own identical
     *  note): the Platform, not the application, owns this decision -
     *  replaces the old `bool trigger_safestate_on_disagreement` (which
     *  let a caller opt OUT of transitioning at all). A voter instance
     *  that is provably never able to disagree (e.g. a single-channel
     *  RTE_VOTING_NMR quorum_size==1 registration, where the "group"
     *  of one channel trivially always meets quorum) can safely leave
     *  this zero-initialized (RTE_SAFESTATE_LEVEL_DEGRADED) since the
     *  DISAGREED branch can never actually be reached for it. */
    rte_safestate_level_t safestate_level;
    /** Diagnostic reason code passed to the safestate transition above. */
    rte_safestate_reason_t safestate_reason;
    /** Optional callback invoked whenever rte_voter_receive() does not
     *  return RTE_VOTING_AGREED (DISAGREED/TIMEOUT/INSUFFICIENT_QUORUM),
     *  BEFORE the unconditional safestate transition above for a
     *  DISAGREED result - the application's one chance to react (e.g.
     *  clean up its own state) before control does not return. Do this
     *  work synchronously; there is no "after" for a SAFE/REBOOT-level
     *  transition (REQ-COMMON-SAFESTATE-002). For TIMEOUT/
     *  INSUFFICIENT_QUORUM results (which never trigger the transition
     *  above), this callback simply fires and rte_voter_receive()
     *  returns normally afterward. */
    void (*on_disagreement)(void *context, rte_voting_result_t result);
    /** User context passed to on_disagreement(). */
    void *disagreement_context;
} rte_voter_config_t;

/** @brief Maximum number of channels a single voter can register. */
#define RTE_VOTER_MAX_CHANNELS 8U

/** @brief Maximum payload size rte_voter_send()/_receive() supports. */
#define RTE_VOTER_MAX_MESSAGE_SIZE 256U

/**
 * @brief Storage for one voter instance (opaque to caller). No dynamic memory.
 */
typedef struct {
    rte_voter_config_t config;
    rte_channel_t *channels[RTE_VOTER_MAX_CHANNELS];
    uint32_t channel_count;
    uint32_t total_disagreements;
    bool initialized;
} rte_voter_storage_t;

/** @brief Opaque handle to a voter instance. */
typedef rte_voter_storage_t rte_voter_t;

/**
 * @brief Initializes a voter with zero registered channels.
 *
 * @param[out] storage  Pre-allocated storage. Must not be NULL.
 * @param[in]  config   Voting configuration. Must not be NULL. For
 *                      RTE_VOTING_NMR, config->quorum_size must be >= 1.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if storage/config is NULL, or (NMR
 *         strategy and quorum_size == 0)
 *
 * REQ-VOTER-001: No dynamic allocation; fixed RTE_VOTER_MAX_CHANNELS array.
 * REQ-VOTER-002: Rejects an unrecognized voting_strategy, or NMR with
 * quorum_size == 0.
 */
rte_status_t rte_voter_init(rte_voter_storage_t *storage, const rte_voter_config_t *config);

/**
 * @brief Registers one already-initialized channel with this voter.
 *
 * @param[in] voter    Voter handle. Must not be NULL and must be initialized.
 * @param[in] channel  Already rte_channel_init()'d channel. Must
 *                     not be NULL.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if voter/channel is NULL
 * @return RTE_STATUS_NOT_INITIALIZED if voter was never initialized
 * @return RTE_STATUS_RESOURCE_EXHAUSTED if RTE_VOTER_MAX_CHANNELS are
 *         already registered
 *
 * @post For RTE_VOTING_2OO2/2OO3, rte_voter_send()/_receive() require
 *       exactly 2/3 channels to be registered respectively before use;
 *       calling them with the wrong count returns
 *       RTE_STATUS_INVALID_PARAM.
 */
rte_status_t rte_voter_register_channel(rte_voter_t *voter, rte_channel_t *channel);

/**
 * @brief Broadcasts data to every registered, healthy channel.
 *
 * @param[in] voter      Voter handle. Must not be NULL and initialized.
 * @param[in] data       Data to send. Must not be NULL.
 * @param[in] data_size  Size in bytes; must be > 0 and <=
 *                       RTE_VOTER_MAX_MESSAGE_SIZE.
 *
 * @return RTE_STATUS_OK if every healthy channel accepted the send
 * @return RTE_STATUS_INVALID_PARAM for a bad argument or wrong
 *         registered-channel count for a fixed strategy (2OO2/2OO3)
 * @return RTE_STATUS_RESOURCE_EXHAUSTED if data_size exceeds the max
 * @return RTE_STATUS_HARDWARE_FAULT if no channel is healthy, or any
 *         healthy channel's send failed
 *
 * REQ-VOTER-003: Requires the registered-channel count to match the
 * configured strategy before sending.
 */
rte_status_t rte_voter_send(rte_voter_t *voter, const void *data, size_t data_size);

/**
 * @brief Receives from every registered, healthy channel and votes.
 *
 * Groups the channels that responded successfully by mutual agreement
 * (per config->compare, default memcmp); the largest group that meets
 * the configured quorum wins - AGREED, with *data set to that group's
 * value. If no group is large enough, DISAGREED. If fewer than 2
 * channels responded at all, TIMEOUT (0 responded) or
 * INSUFFICIENT_QUORUM (1 responded, or not enough healthy channels to
 * possibly meet quorum before even trying).
 *
 * @param[in]  voter          Voter handle. Must not be NULL and initialized.
 * @param[out] data           Buffer to receive the winning group's data.
 *                            Must not be NULL.
 * @param[in]  data_size      Size of data buffer; must be > 0 and <=
 *                            RTE_VOTER_MAX_MESSAGE_SIZE.
 * @param[out] result         Voting outcome. Can be NULL.
 * @param[out] bytes_received Bytes written to data. Can be NULL.
 *
 * @return RTE_STATUS_OK if result is RTE_VOTING_AGREED
 * @return RTE_STATUS_INVALID_PARAM for a bad argument or wrong
 *         registered-channel count for a fixed strategy
 * @return RTE_STATUS_HARDWARE_FAULT otherwise (DISAGREED/TIMEOUT/
 *         INSUFFICIENT_QUORUM)
 *
 * @post On RTE_VOTING_DISAGREED, this function invokes
 *       config->on_disagreement (if set) and then unconditionally
 *       enters config->safestate_level - it does not return unless
 *       that level is RTE_SAFESTATE_LEVEL_DEGRADED (the only level
 *       rte_safestate_enter() may return from).
 *
 * REQ-VOTER-003: Requires the registered-channel count to match the
 * configured strategy before receiving/voting.
 * REQ-VOTER-004: Groups responses by mutual agreement and selects the
 * largest quorum-meeting group (majority vote), not a pairwise compare
 * against a single reference channel.
 * REQ-VOTER-005: On DISAGREED, always invokes on_disagreement (if
 * registered) and then unconditionally enters config->safestate_level.
 */
rte_status_t rte_voter_receive(rte_voter_t *voter, void *data, size_t data_size,
                                  rte_voting_result_t *result, size_t *bytes_received);

/**
 * @brief Aggregated health across every registered channel.
 *
 * @param[in]  voter               Voter handle. Must not be NULL.
 * @param[out] healthy_count       Number of currently-healthy registered
 *                                 channels. Can be NULL.
 * @param[out] total_disagreements Running count of DISAGREED results
 *                                 returned by this voter. Can be NULL.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if voter is NULL
 */
rte_status_t rte_voter_get_aggregated_health(const rte_voter_t *voter,
                                                uint32_t *healthy_count,
                                                uint32_t *total_disagreements);

/**
 * @brief Number of channels currently registered with this voter.
 *
 * Intended for callers (e.g. rte_channel_checkpoint()) that need to
 * iterate the voter's registered channels directly rather than through
 * send()/receive()'s own voting.
 *
 * @param[in] voter  Voter handle. Must not be NULL.
 * @return Registered channel count, or 0 if voter is NULL.
 */
uint32_t rte_voter_get_channel_count(const rte_voter_t *voter);

/**
 * @brief Direct access to one registered channel, by index.
 *
 * @param[in] voter  Voter handle. Must not be NULL.
 * @param[in] index  0-based index; must be < rte_voter_get_channel_count(voter).
 * @return The registered channel, or NULL if voter is NULL or index is
 *         out of range.
 */
rte_channel_t *rte_voter_get_channel(const rte_voter_t *voter, uint32_t index);

/**
 * @brief Direct access to one registered channel, by name (e.g. "ChannelAtoB") -
 *        the name each channel was given via rte_channel_config_t::name at
 *        its own rte_channel_init() time.
 *
 * Linear search over the registered channels (channel counts here are
 * small - RTE_VOTER_MAX_CHANNELS - so this is not a hot-path lookup by
 * design); compares with strcmp(), so an exact, case-sensitive match is
 * required.
 *
 * @param[in] voter  Voter handle. Must not be NULL.
 * @param[in] name   Name to search for. Must not be NULL.
 * @return The first registered channel whose own name matches (exact
 *         strcmp() equality); NULL if voter or name is NULL, or no
 *         registered channel has a matching (non-NULL) name.
 */
rte_channel_t *rte_voter_get_channel_by_name(const rte_voter_t *voter, const char *name);

/**
 * @brief Destroys a voter instance.
 *
 * No dynamic memory to free; registered channels are NOT destroyed
 * (caller retains ownership).
 *
 * @param[in] voter  Voter handle. May be NULL.
 * @return RTE_STATUS_OK always.
 * @safety Idempotent; safe to call with NULL.
 */
rte_status_t rte_voter_destroy(rte_voter_t *voter);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* RTE_VOTER_H */

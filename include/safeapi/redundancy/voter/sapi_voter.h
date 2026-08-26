/**
 * @file sapi_voter.h
 * @brief N-way voting across registered sapi_channel_t links
 *        (2oo2/2oo3/NMR).
 *
 * ADR-025: this is the voting engine that used to live inside
 * `sapi_channel` before that type was split into a single-link
 * primitive. Register 1..SAPI_VOTER_MAX_CHANNELS already-initialized
 * `sapi_channel_t` instances, then use sapi_voter_send()/
 * _receive() the way sapi_channel_send()/_receive() used to work.
 *
 * @defgroup voter Voter (N-way channel voting)
 * @{
 */

#ifndef SAPI_VOTER_H
#define SAPI_VOTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/redundancy/channel_link/sapi_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Voting strategy: how many registered channels must agree.
 */
typedef enum {
    /** 2 out of 2: both channels must agree. */
    SAPI_VOTING_2OO2 = 1,
    /** 2 out of 3: majority among 3 channels, tolerates 1 fault. */
    SAPI_VOTING_2OO3 = 2,
    /** N out of M: sapi_voter_config_t::quorum_size of registered channels must agree. */
    SAPI_VOTING_NMR = 3
} sapi_voting_strategy_t;

/**
 * @brief Outcome of one sapi_voter_receive() call.
 */
typedef enum {
    /** A quorum-sized group of channels agreed on the data. */
    SAPI_VOTING_AGREED = 0,
    /** No group of channels large enough to meet quorum agreed. */
    SAPI_VOTING_DISAGREED = 1,
    /** One or more channels timed out. */
    SAPI_VOTING_TIMEOUT = 2,
    /** Fewer than the required number of healthy/responding channels. */
    SAPI_VOTING_INSUFFICIENT_QUORUM = 3
} sapi_voting_result_t;

/**
 * @brief Optional custom comparison callback.
 *
 * If not registered (sapi_voter_config_t::compare == NULL), the default
 * is a full byte compare (`memcmp(a, b, size) == 0`) - the payloads
 * being compared have already had their transport-integrity CRC
 * verified by the channel layer (sapi_checksum) if the backend uses
 * sapi_checksum_vital_message_verify(); this callback is about
 * semantic/value comparison, not integrity checking.
 *
 * @param[in] a         First buffer.
 * @param[in] b         Second buffer.
 * @param[in] size      Number of bytes to compare.
 * @param[in] user_ctx  sapi_voter_config_t::compare_context, passed
 *                      through verbatim.
 * @return true if a and b are considered equal for voting purposes.
 */
typedef bool (*sapi_voter_compare_fn)(const void *a, const void *b, size_t size, void *user_ctx);

/**
 * @brief Configuration for sapi_voter_init().
 */
typedef struct {
    /** Voting strategy. */
    sapi_voting_strategy_t voting_strategy;
    /** Required agreeing-channel count for SAPI_VOTING_NMR (ignored for
     *  2OO2/2OO3, which have a fixed quorum of 2). */
    uint32_t quorum_size;
    /** Timeout for each channel's receive operation (milliseconds). */
    uint32_t channel_timeout_ms;
    /** Optional custom comparison callback; NULL -> memcmp default. */
    sapi_voter_compare_fn compare;
    /** User context passed to compare(). */
    void *compare_context;
    /** Log a disagreement via sapi_log_write() when it occurs. */
    bool log_disagreements;
    /** If true (default expected), a DISAGREED result also triggers
     *  SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_SAFE, ...) before
     *  sapi_voter_receive() returns - matching the pre-ADR-025
     *  sapi_channel behavior. Set false for a voter instance that
     *  is not itself on a safety-decision path and wants to handle
     *  disagreement entirely through on_disagreement below instead. */
    bool trigger_safestate_on_disagreement;
    /** Optional callback invoked whenever sapi_voter_receive() does not
     *  return SAPI_VOTING_AGREED (DISAGREED/TIMEOUT/INSUFFICIENT_QUORUM).
     *  Invoked after safestate (if triggered above) would already not
     *  have returned, so in practice this only fires when
     *  trigger_safestate_on_disagreement is false or the result is
     *  TIMEOUT/INSUFFICIENT_QUORUM (which never triggers safestate). */
    void (*on_disagreement)(void *context, sapi_voting_result_t result);
    /** User context passed to on_disagreement(). */
    void *disagreement_context;
} sapi_voter_config_t;

/** @brief Maximum number of channels a single voter can register. */
#define SAPI_VOTER_MAX_CHANNELS 8U

/** @brief Maximum payload size sapi_voter_send()/_receive() supports. */
#define SAPI_VOTER_MAX_MESSAGE_SIZE 256U

/**
 * @brief Storage for one voter instance (opaque to caller). No dynamic memory.
 */
typedef struct {
    sapi_voter_config_t config;
    sapi_channel_t *channels[SAPI_VOTER_MAX_CHANNELS];
    uint32_t channel_count;
    uint32_t total_disagreements;
    bool initialized;
} sapi_voter_storage_t;

/** @brief Opaque handle to a voter instance. */
typedef sapi_voter_storage_t sapi_voter_t;

/**
 * @brief Initializes a voter with zero registered channels.
 *
 * @param[out] storage  Pre-allocated storage. Must not be NULL.
 * @param[in]  config   Voting configuration. Must not be NULL. For
 *                      SAPI_VOTING_NMR, config->quorum_size must be >= 1.
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if storage/config is NULL, or (NMR
 *         strategy and quorum_size == 0)
 *
 * REQ-VOTER-001: No dynamic allocation; fixed SAPI_VOTER_MAX_CHANNELS array.
 * REQ-VOTER-002: Rejects an unrecognized voting_strategy, or NMR with
 * quorum_size == 0.
 */
sapi_status_t sapi_voter_init(sapi_voter_storage_t *storage, const sapi_voter_config_t *config);

/**
 * @brief Registers one already-initialized channel with this voter.
 *
 * @param[in] voter    Voter handle. Must not be NULL and must be initialized.
 * @param[in] channel  Already sapi_channel_init()'d channel. Must
 *                     not be NULL.
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if voter/channel is NULL
 * @return SAPI_STATUS_NOT_INITIALIZED if voter was never initialized
 * @return SAPI_STATUS_RESOURCE_EXHAUSTED if SAPI_VOTER_MAX_CHANNELS are
 *         already registered
 *
 * @post For SAPI_VOTING_2OO2/2OO3, sapi_voter_send()/_receive() require
 *       exactly 2/3 channels to be registered respectively before use;
 *       calling them with the wrong count returns
 *       SAPI_STATUS_INVALID_PARAM.
 */
sapi_status_t sapi_voter_register_channel(sapi_voter_t *voter, sapi_channel_t *channel);

/**
 * @brief Broadcasts data to every registered, healthy channel.
 *
 * @param[in] voter      Voter handle. Must not be NULL and initialized.
 * @param[in] data       Data to send. Must not be NULL.
 * @param[in] data_size  Size in bytes; must be > 0 and <=
 *                       SAPI_VOTER_MAX_MESSAGE_SIZE.
 *
 * @return SAPI_STATUS_OK if every healthy channel accepted the send
 * @return SAPI_STATUS_INVALID_PARAM for a bad argument or wrong
 *         registered-channel count for a fixed strategy (2OO2/2OO3)
 * @return SAPI_STATUS_RESOURCE_EXHAUSTED if data_size exceeds the max
 * @return SAPI_STATUS_HARDWARE_FAULT if no channel is healthy, or any
 *         healthy channel's send failed
 *
 * REQ-VOTER-003: Requires the registered-channel count to match the
 * configured strategy before sending.
 */
sapi_status_t sapi_voter_send(sapi_voter_t *voter, const void *data, size_t data_size);

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
 *                            SAPI_VOTER_MAX_MESSAGE_SIZE.
 * @param[out] result         Voting outcome. Can be NULL.
 * @param[out] bytes_received Bytes written to data. Can be NULL.
 *
 * @return SAPI_STATUS_OK if result is SAPI_VOTING_AGREED
 * @return SAPI_STATUS_INVALID_PARAM for a bad argument or wrong
 *         registered-channel count for a fixed strategy
 * @return SAPI_STATUS_HARDWARE_FAULT otherwise (DISAGREED/TIMEOUT/
 *         INSUFFICIENT_QUORUM)
 *
 * @post On SAPI_VOTING_DISAGREED, if config->trigger_safestate_on_disagreement
 *       is true (the default expectation), this function does not
 *       return - see SAPI_SAFESTATE_LEVEL_SAFE.
 *
 * REQ-VOTER-003: Requires the registered-channel count to match the
 * configured strategy before receiving/voting.
 * REQ-VOTER-004: Groups responses by mutual agreement and selects the
 * largest quorum-meeting group (majority vote), not a pairwise compare
 * against a single reference channel.
 * REQ-VOTER-005: On DISAGREED, enters SAPI_SAFESTATE_LEVEL_SAFE when
 * trigger_safestate_on_disagreement is true, and always invokes
 * on_disagreement (if registered) regardless of that flag.
 */
sapi_status_t sapi_voter_receive(sapi_voter_t *voter, void *data, size_t data_size,
                                  sapi_voting_result_t *result, size_t *bytes_received);

/**
 * @brief Aggregated health across every registered channel.
 *
 * @param[in]  voter               Voter handle. Must not be NULL.
 * @param[out] healthy_count       Number of currently-healthy registered
 *                                 channels. Can be NULL.
 * @param[out] total_disagreements Running count of DISAGREED results
 *                                 returned by this voter. Can be NULL.
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if voter is NULL
 */
sapi_status_t sapi_voter_get_aggregated_health(const sapi_voter_t *voter,
                                                uint32_t *healthy_count,
                                                uint32_t *total_disagreements);

/**
 * @brief Number of channels currently registered with this voter.
 *
 * Intended for callers (e.g. sapi_channel_checkpoint()) that need to
 * iterate the voter's registered channels directly rather than through
 * send()/receive()'s own voting.
 *
 * @param[in] voter  Voter handle. Must not be NULL.
 * @return Registered channel count, or 0 if voter is NULL.
 */
uint32_t sapi_voter_get_channel_count(const sapi_voter_t *voter);

/**
 * @brief Direct access to one registered channel, by index.
 *
 * @param[in] voter  Voter handle. Must not be NULL.
 * @param[in] index  0-based index; must be < sapi_voter_get_channel_count(voter).
 * @return The registered channel, or NULL if voter is NULL or index is
 *         out of range.
 */
sapi_channel_t *sapi_voter_get_channel(const sapi_voter_t *voter, uint32_t index);

/**
 * @brief Direct access to one registered channel, by name (e.g. "ChannelAtoB") -
 *        the name each channel was given via sapi_channel_config_t::name at
 *        its own sapi_channel_init() time.
 *
 * Linear search over the registered channels (channel counts here are
 * small - SAPI_VOTER_MAX_CHANNELS - so this is not a hot-path lookup by
 * design); compares with strcmp(), so an exact, case-sensitive match is
 * required.
 *
 * @param[in] voter  Voter handle. Must not be NULL.
 * @param[in] name   Name to search for. Must not be NULL.
 * @return The first registered channel whose own name matches (exact
 *         strcmp() equality); NULL if voter or name is NULL, or no
 *         registered channel has a matching (non-NULL) name.
 */
sapi_channel_t *sapi_voter_get_channel_by_name(const sapi_voter_t *voter, const char *name);

/**
 * @brief Destroys a voter instance.
 *
 * No dynamic memory to free; registered channels are NOT destroyed
 * (caller retains ownership).
 *
 * @param[in] voter  Voter handle. May be NULL.
 * @return SAPI_STATUS_OK always.
 * @safety Idempotent; safe to call with NULL.
 */
sapi_status_t sapi_voter_destroy(sapi_voter_t *voter);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* SAPI_VOTER_H */

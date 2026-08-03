/**
 * @file sapi_vital_channel.h
 * @brief Safety-critical redundant channel abstraction with voting logic
 *
 * Wraps multiple underlying IPC channels with voting/arbitration logic
 * for safety-critical (vital) channels. Implements 2oo2, 2oo3, and NMR
 * voting strategies with automatic fault detection and safe-state handling.
 *
 * @defgroup vital_channel Vital Channel Abstraction
 * @{
 */

#ifndef SAPI_VITAL_CHANNEL_H
#define SAPI_VITAL_CHANNEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "safeapi/status/sapi_status.h"
#include "safeapi/ipc/sapi_ipc_request_reply.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Voting strategy for redundant channels
 *
 * Determines how disagreements between redundant channels are handled.
 */
typedef enum {
    /** 2 out of 2: Both channels must agree */
    SAPI_VOTING_2OO2 = 1,
    /** 2 out of 3: Majority vote among 3 channels, tolerate 1 fault */
    SAPI_VOTING_2OO3 = 2,
    /** N out of M: Custom majority voting for arbitrary channel count */
    SAPI_VOTING_NMR = 3
} sapi_voting_strategy_t;

/**
 * @brief Voting result status
 *
 * Outcome of the voting process across redundant channels.
 */
typedef enum {
    /** All channels agreed on a valid result */
    SAPI_VOTING_AGREED = 0,
    /** Channels disagreed; safe-state has been triggered */
    SAPI_VOTING_DISAGREED = 1,
    /** One or more channels timed out */
    SAPI_VOTING_TIMEOUT = 2,
    /** Insufficient channels available (below quorum) */
    SAPI_VOTING_INSUFFICIENT_QUORUM = 3
} sapi_voting_result_t;

/**
 * @brief Per-channel health statistics
 *
 * Tracks the health and performance of individual redundant channels.
 */
typedef struct {
    /** Number of successful sends on this channel */
    uint32_t send_count;
    /** Number of failed sends on this channel */
    uint32_t send_error_count;
    /** Number of successful receives on this channel */
    uint32_t receive_count;
    /** Number of failed receives (timeout/error) on this channel */
    uint32_t receive_error_count;
    /** Number of times this channel disagreed with others */
    uint32_t disagreement_count;
    /** Is this channel currently considered healthy */
    bool is_healthy;
    /** Last error status on this channel */
    sapi_status_t last_error;
} sapi_vital_channel_health_t;

/**
 * @brief Vital channel configuration
 *
 * Specifies how the vital channel redundancy is configured.
 */
typedef struct {
    /** Voting strategy (2oo2, 2oo3, NMR) */
    sapi_voting_strategy_t voting_strategy;
    /** Number of redundant channels */
    uint32_t channel_count;
    /** Quorum size for NMR voting (ignored for 2oo2/2oo3) */
    uint32_t quorum_size;
    /** Timeout for each channel operation (milliseconds) */
    uint32_t channel_timeout_ms;
    /** Enable automatic disagreement logging */
    bool log_disagreements;
    /** User-defined disagreement callback (can be NULL) */
    void (*on_disagreement)(void *context, const sapi_voting_result_t *result);
    /** User context for disagreement callback */
    void *context;
} sapi_vital_channel_config_t;

/** @brief Maximum number of redundant channels supported */
#define SAPI_VITAL_CHANNEL_MAX_CHANNELS 8

/**
 * @brief Storage for vital channel instance (opaque to caller)
 *
 * Caller allocates this structure and passes it to init function.
 * Satisfies embedded C requirement of no dynamic memory.
 */
typedef struct {
    sapi_vital_channel_config_t config;
    void **channels;  /* Opaque channel handles (IPC or other transport) */
    uint32_t channel_count;
    sapi_vital_channel_health_t health[SAPI_VITAL_CHANNEL_MAX_CHANNELS];
} sapi_vital_channel_storage_t;

/**
 * @brief Opaque handle to a vital channel instance
 *
 * Created by sapi_vital_channel_init() and used in all subsequent operations.
 */
typedef sapi_vital_channel_storage_t sapi_vital_channel_t;

/**
 * @brief Initialize a vital (redundant) channel with voting logic
 *
 * Initializes a pre-allocated vital channel that wraps multiple underlying
 * channels (IPC or other transport) and performs voting-based arbitration
 * on all sends and receives.
 *
 * @param[out] storage          Pre-allocated storage for the vital channel (must not be NULL)
 * @param[in]  config           Channel configuration (must not be NULL)
 * @param[in]  channels         Array of opaque channel handles (must not be NULL)
 * @param[in]  channel_count    Number of channels in the array
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if parameters are invalid
 *
 * @pre storage != NULL
 * @pre config != NULL
 * @pre channels != NULL
 * @pre channel_count >= 2 (at minimum 2 redundant channels required)
 * @pre channel_count <= SAPI_VITAL_CHANNEL_MAX_CHANNELS
 * @post On success, storage contains initialized vital channel state
 *
 * @safety No dynamic memory allocation; caller provides storage.
 *         This function performs no I/O operations; all channels must be
 *         pre-configured and operational before calling this function.
 */
sapi_status_t sapi_vital_channel_init(sapi_vital_channel_t *storage,
                                      const sapi_vital_channel_config_t *config,
                                      void **channels,
                                      uint32_t channel_count);

/**
 * @brief Send data via vital channel (broadcast to all redundant channels)
 *
 * Broadcasts the message to all redundant channels. All channels must
 * complete successfully for the send to succeed (atomic semantics).
 *
 * If any channel fails, the entire send fails and no data is transmitted
 * to any channel (all-or-nothing).
 *
 * @param[in] handle        Vital channel handle (must not be NULL)
 * @param[in] data          Data to send (must not be NULL)
 * @param[in] data_size     Size of data in bytes
 *
 * @return SAPI_STATUS_OK if all channels accepted the send
 * @return SAPI_STATUS_INVALID_PARAM if handle or data is NULL
 * @return SAPI_STATUS_TIMEOUT if any channel times out
 * @return SAPI_STATUS_HARDWARE_FAULT if redundancy is compromised
 *
 * @pre handle != NULL
 * @pre data != NULL
 * @pre data_size > 0
 *
 * @safety Atomic: either all channels succeed or none do. Partial sends
 *         are not possible, preventing inconsistent system state.
 */
sapi_status_t sapi_vital_channel_send(sapi_vital_channel_t *handle,
                                      const void *data,
                                      size_t data_size);

/**
 * @brief Receive data via vital channel (voting-based reception)
 *
 * Receives from all redundant channels and applies voting logic to
 * determine the canonical result. Automatically detects disagreements
 * and triggers safe-state on voting failure.
 *
 * @param[in]  handle        Vital channel handle (must not be NULL)
 * @param[out] data          Buffer to receive data (must not be NULL)
 * @param[in]  data_size     Size of data buffer
 * @param[out] result        Voting result status (can be NULL)
 * @param[out] bytes_received Number of bytes actually received (can be NULL)
 *
 * @return SAPI_STATUS_OK if voting succeeded and data is valid
 * @return SAPI_STATUS_INVALID_PARAM if handle or data is NULL
 * @return SAPI_STATUS_TIMEOUT if voting timeout expires
 * @return SAPI_STATUS_HARDWARE_FAULT if channels disagree (triggers safe-state)
 *
 * @pre handle != NULL
 * @pre data != NULL
 * @pre data_size > 0
 *
 * @post On disagreement, safe-state is automatically triggered via
 *       sapi_safestate_enter() with SAPI_SAFESTATE_LEVEL_SAFE
 *
 * @safety Voting is atomic: all channels must complete or timeout.
 *         Disagreement triggers safe-state to prevent corrupted data
 *         propagation.
 */
sapi_status_t sapi_vital_channel_receive(sapi_vital_channel_t *handle,
                                         void *data,
                                         size_t data_size,
                                         sapi_voting_result_t *result,
                                         size_t *bytes_received);

/**
 * @brief Query per-channel health statistics
 *
 * Retrieves health metrics for a specific redundant channel to enable
 * early fault detection and preventive isolation.
 *
 * @param[in]  handle        Vital channel handle (must not be NULL)
 * @param[in]  channel_idx   Index of the channel (0-based)
 * @param[out] health        Health statistics (must not be NULL)
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if handle or health is NULL, or channel_idx is out of range
 *
 * @pre handle != NULL
 * @pre health != NULL
 * @pre channel_idx < channel_count
 *
 * @post *health contains current statistics for the specified channel
 *
 * @safety Read-only operation; does not modify channel state. Safe to call
 *         from any context.
 */
sapi_status_t sapi_vital_channel_get_health(sapi_vital_channel_t *handle,
                                            uint32_t channel_idx,
                                            sapi_vital_channel_health_t *health);

/**
 * @brief Get aggregated health across all redundant channels
 *
 * Computes a summary of health across all channels to quickly assess
 * system redundancy status.
 *
 * @param[in]  handle              Vital channel handle (must not be NULL)
 * @param[out] healthy_count       Number of healthy channels (can be NULL)
 * @param[out] total_disagreements Total disagreements across all channels (can be NULL)
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if handle is NULL
 *
 * @pre handle != NULL
 *
 * @post If healthy_count is not NULL, it contains the number of channels
 *       currently marked as healthy
 *
 * @safety Read-only operation. Safe for watchdog queries.
 */
sapi_status_t sapi_vital_channel_get_aggregated_health(
    sapi_vital_channel_t *handle,
    uint32_t *healthy_count,
    uint32_t *total_disagreements);

/**
 * @brief Destroy a vital channel and free all resources
 *
 * Closes the vital channel handle and releases all associated resources.
 * The underlying IPC channels are NOT closed (caller retains ownership).
 *
 * @param[in] handle        Vital channel handle (can be NULL)
 *
 * @return SAPI_STATUS_OK on success
 * @return SAPI_STATUS_INVALID_PARAM if handle is invalid
 *
 * @post handle is no longer valid for subsequent operations
 *
 * @safety Idempotent: safe to call with NULL handle.
 */
sapi_status_t sapi_vital_channel_destroy(sapi_vital_channel_t *handle);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* SAPI_VITAL_CHANNEL_H */

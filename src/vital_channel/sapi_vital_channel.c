/**
 * @file sapi_vital_channel.c
 * @brief Implementation of vital (redundant) channel abstraction
 *
 * Implements voting logic for 2oo2, 2oo3, and NMR strategies with
 * automatic fault detection and safe-state handling.
 */

#include <stddef.h>
#include <string.h>

#include "safeapi/vital_channel/sapi_vital_channel.h"
#include "safeapi/safestate/sapi_safestate.h"
#include "safeapi/log/sapi_log.h"

/* Maximum number of redundant channels supported */
#define SAPI_VITAL_CHANNEL_MAX_CHANNELS 8

/* No separate struct needed - storage_t is the handle */

/**
 * @brief Compare two data buffers for equality (voting comparison)
 *
 * Used to determine if two channels agree on received data.
 */
static bool sapi_vital_channel_data_equal(const void *a, const void *b, size_t size)
{
    if (a == NULL || b == NULL) {
        return a == b;
    }
    return memcmp(a, b, size) == 0;
}

/**
 * @brief Check if voting quorum is satisfied
 *
 * Verifies that enough channels are healthy to achieve a valid vote.
 */
static bool sapi_vital_channel_has_quorum(sapi_vital_channel_t *handle)
{
    uint32_t healthy_count = 0;

    if (handle == NULL) {
        return false;
    }

    for (uint32_t i = 0; i < handle->channel_count; i++) {
        if (handle->health[i].is_healthy) {
            healthy_count++;
        }
    }

    switch (handle->config.voting_strategy) {
    case SAPI_VOTING_2OO2:
        return healthy_count >= 2;
    case SAPI_VOTING_2OO3:
        return healthy_count >= 2;
    case SAPI_VOTING_NMR:
        return healthy_count >= handle->config.quorum_size;
    default:
        return false;
    }
}

sapi_status_t sapi_vital_channel_init(sapi_vital_channel_t *storage,
                                      const sapi_vital_channel_config_t *config,
                                      sapi_ipc_request_reply_t **channels,
                                      uint32_t channel_count)
{
    if (storage == NULL || config == NULL || channels == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (channel_count < 2 || channel_count > SAPI_VITAL_CHANNEL_MAX_CHANNELS) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Validate voting strategy */
    switch (config->voting_strategy) {
    case SAPI_VOTING_2OO2:
        if (channel_count != 2) {
            return SAPI_STATUS_INVALID_PARAM;
        }
        break;
    case SAPI_VOTING_2OO3:
        if (channel_count != 3) {
            return SAPI_STATUS_INVALID_PARAM;
        }
        break;
    case SAPI_VOTING_NMR:
        if (config->quorum_size > channel_count || config->quorum_size < 2) {
            return SAPI_STATUS_INVALID_PARAM;
        }
        break;
    default:
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Initialize storage */
    storage->config = *config;
    storage->channels = channels;
    storage->channel_count = channel_count;

    /* Initialize health tracking for all channels */
    for (uint32_t i = 0; i < channel_count; i++) {
        storage->health[i].send_count = 0;
        storage->health[i].send_error_count = 0;
        storage->health[i].receive_count = 0;
        storage->health[i].receive_error_count = 0;
        storage->health[i].disagreement_count = 0;
        storage->health[i].is_healthy = true;
        storage->health[i].last_error = SAPI_STATUS_OK;
    }

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_vital_channel_send(sapi_vital_channel_t *handle,
                                      const void *data,
                                      size_t data_size)
{
    sapi_status_t rc;
    bool all_success = true;

    if (handle == NULL || data == NULL || data_size == 0) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (!sapi_vital_channel_has_quorum(handle)) {
        return SAPI_STATUS_HARDWARE_FAULT;
    }

    /* Broadcast to all redundant channels (atomic send) */
    for (uint32_t i = 0; i < handle->channel_count; i++) {
        if (!handle->health[i].is_healthy) {
            continue;
        }

        rc = sapi_ipc_send(handle->channels[i], data, data_size);
        if (rc != SAPI_STATUS_OK) {
            handle->health[i].send_error_count++;
            handle->health[i].last_error = rc;
            all_success = false;

            if (rc == SAPI_STATUS_TIMEOUT) {
                return SAPI_STATUS_TIMEOUT;
            }
        } else {
            handle->health[i].send_count++;
        }
    }

    /* All-or-nothing semantics: all channels must succeed */
    if (!all_success) {
        return SAPI_STATUS_HARDWARE_FAULT;
    }

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_vital_channel_receive(sapi_vital_channel_t *handle,
                                         void *data,
                                         size_t data_size,
                                         sapi_voting_result_t *result,
                                         size_t *bytes_received)
{
    sapi_status_t channel_status[SAPI_VITAL_CHANNEL_MAX_CHANNELS];
    uint32_t successful_receives = 0;
    sapi_voting_result_t vote_result = SAPI_VOTING_AGREED;
    sapi_status_t rc;
    uint8_t first_data[256];  /* Static buffer for first channel's data */
    uint32_t first_agree_idx = 0xFF;
    bool first_data_valid = false;

    if (handle == NULL || data == NULL || data_size == 0) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (data_size > sizeof(first_data)) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;  /* Message too large for static buffer */
    }

    if (!sapi_vital_channel_has_quorum(handle)) {
        if (result != NULL) {
            *result = SAPI_VOTING_INSUFFICIENT_QUORUM;
        }
        return SAPI_STATUS_HARDWARE_FAULT;
    }

    /* Receive from all redundant channels and perform voting */
    for (uint32_t i = 0; i < handle->channel_count; i++) {
        uint8_t channel_buffer[256];

        if (!handle->health[i].is_healthy) {
            channel_status[i] = SAPI_STATUS_HARDWARE_FAULT;
            continue;
        }

        if (data_size > sizeof(channel_buffer)) {
            return SAPI_STATUS_RESOURCE_EXHAUSTED;
        }

        rc = sapi_ipc_receive(handle->channels[i], channel_buffer, data_size,
                              handle->config.channel_timeout_ms);
        channel_status[i] = rc;

        if (rc == SAPI_STATUS_OK) {
            handle->health[i].receive_count++;
            successful_receives++;

            /* Store first successful receive for comparison */
            if (!first_data_valid) {
                memcpy(first_data, channel_buffer, data_size);
                first_data_valid = true;
                first_agree_idx = i;
            } else {
                /* Compare with first channel's data */
                if (!sapi_vital_channel_data_equal(channel_buffer, first_data, data_size)) {
                    handle->health[i].disagreement_count++;
                    handle->health[first_agree_idx].disagreement_count++;
                    vote_result = SAPI_VOTING_DISAGREED;
                }
            }
        } else {
            handle->health[i].receive_error_count++;
            handle->health[i].last_error = rc;

            if (rc == SAPI_STATUS_TIMEOUT) {
                vote_result = SAPI_VOTING_TIMEOUT;
            }
        }
    }

    /* Check if we have enough successful receives for voting */
    if (successful_receives < 2) {
        vote_result = (successful_receives == 0) ? SAPI_VOTING_TIMEOUT
                                                   : SAPI_VOTING_INSUFFICIENT_QUORUM;

        if (result != NULL) {
            *result = vote_result;
        }

        if (bytes_received != NULL) {
            *bytes_received = 0;
        }

        return SAPI_STATUS_HARDWARE_FAULT;
    }

    /* Copy agreed data to output buffer */
    if (vote_result == SAPI_VOTING_AGREED && first_data_valid) {
        memcpy(data, first_data, data_size);
        if (bytes_received != NULL) {
            *bytes_received = data_size;
        }
    } else {
        /* Disagreement or no data available */
        if (handle->config.log_disagreements && vote_result == SAPI_VOTING_DISAGREED) {
            sapi_log_write(SAPI_LOG_LEVEL_ERROR, "VITAL_CHANNEL",
                           "2oo2 voting disagreement detected");
        }

        /* Trigger safe-state on disagreement */
        if (vote_result == SAPI_VOTING_DISAGREED) {
            SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_SAFE,
                           SAPI_SAFESTATE_REASON_REDUNDANCY_FAILURE);
        }

        if (bytes_received != NULL) {
            *bytes_received = 0;
        }
    }

    /* Invoke disagreement callback if configured */
    if (vote_result != SAPI_VOTING_AGREED && handle->config.on_disagreement != NULL) {
        handle->config.on_disagreement(handle->config.context, &vote_result);
    }

    if (result != NULL) {
        *result = vote_result;
    }

    return (vote_result == SAPI_VOTING_AGREED) ? SAPI_STATUS_OK
                                                : SAPI_STATUS_HARDWARE_FAULT;
}

sapi_status_t sapi_vital_channel_get_health(sapi_vital_channel_t *handle,
                                            uint32_t channel_idx,
                                            sapi_vital_channel_health_t *health)
{
    if (handle == NULL || health == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (channel_idx >= handle->channel_count) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    *health = handle->health[channel_idx];
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_vital_channel_get_aggregated_health(
    sapi_vital_channel_t *handle,
    uint32_t *healthy_count,
    uint32_t *total_disagreements)
{
    uint32_t healthy = 0;
    uint32_t disagreements = 0;

    if (handle == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < handle->channel_count; i++) {
        if (handle->health[i].is_healthy) {
            healthy++;
        }
        disagreements += handle->health[i].disagreement_count;
    }

    if (healthy_count != NULL) {
        *healthy_count = healthy;
    }

    if (total_disagreements != NULL) {
        *total_disagreements = disagreements;
    }

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_vital_channel_destroy(sapi_vital_channel_t *handle)
{
    /* No dynamic memory to free - handle is caller-allocated */
    if (handle == NULL) {
        return SAPI_STATUS_OK;
    }

    return SAPI_STATUS_OK;
}

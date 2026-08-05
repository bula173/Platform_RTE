/**
 * @file sapi_checkpoint.h
 * @brief Bounded checkpoint rendezvous for distributed vital channels (ADR-017)
 *
 * Implements sapi_channel_checkpoint(), already specified (but never built)
 * in docs/REDUNDANCY_ARCHITECTURE.md: a bounded-time rendezvous that lets
 * channels running on separate machines - possibly in separate geographic
 * locations - agree on "we are both at the same point" without depending
 * on wall-clock agreement. Two channels are considered synchronized not
 * because their clocks match, but because they both confirmed the same
 * checkpoint_id to each other within max_delay_ms; if that confirmation
 * doesn't happen in time, that is itself a fault and is handled exactly
 * like a voting disagreement is elsewhere in this framework - safe-state,
 * not silence.
 *
 * This module adds no new transport backend of its own (ADR-017 section
 * 2.2): it reuses the sapi_vital_channel_t instance's own already-
 * registered backend_send/backend_recv callbacks, so whatever transport
 * an integrator plugged into vital_channel (sapi_ipc over POSIX/RTOS, or
 * a real network link) is what checkpoint messages travel over too - this
 * is how "pluggable" is achieved here, consistent with ADR-005 rather
 * than inventing a second plug-in point.
 *
 * The EN 50159-style message envelope (sequence number, sender ID,
 * timestamp, CRC-64) is not reinvented either: every checkpoint-arrival
 * message is a sapi_vital_message_t (sapi_checksum.h), verified with
 * sapi_checksum_vital_message_verify() before it is allowed to count
 * toward quorum.
 *
 * @defgroup CHECKPOINT Checkpoint Rendezvous
 * @brief Bounded cross-channel synchronization for distributed vital channels
 * @{
 *
 * REQ-CHECKPOINT-001: sapi_channel_checkpoint() shall never block longer
 *                     than config->max_delay_ms.
 * REQ-CHECKPOINT-002: a checkpoint-arrival reply that fails CRC
 *                     verification or carries a different checkpoint_id
 *                     shall not count toward expected_node_count.
 * REQ-CHECKPOINT-003: if fewer than expected_node_count valid replies
 *                     arrive within max_delay_ms, sapi_channel_checkpoint()
 *                     shall call sapi_safestate_enter() at
 *                     SAPI_SAFESTATE_LEVEL_SAFE with
 *                     SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT before
 *                     returning SAPI_STATUS_TIMEOUT.
 */

#ifndef SAPI_CHECKPOINT_H
#define SAPI_CHECKPOINT_H

#include <stdint.h>

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"
#include "safeapi/vital_channel/sapi_vital_channel.h"
#include "safeapi/watchdog/sapi_watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checkpoint configuration.
 *
 * Field names match the illustrative usage already present in
 * docs/REDUNDANCY_ARCHITECTURE.md, so those examples describe real
 * behavior rather than an aspirational API.
 */
typedef struct
{
    /** Identifies this checkpoint (e.g. a per-cycle counter). Carried as
     *  both the sequence number and payload of the underlying
     *  sapi_vital_message_t, so a stale or wrong-cycle reply is rejected
     *  rather than silently accepted (REQ-CHECKPOINT-002). */
    uint32_t checkpoint_id;

    /** Maximum time to wait for peer confirmations before treating this
     *  checkpoint as failed (REQ-CHECKPOINT-001). Sized using the
     *  deployment's known network/clock jitter - see sapi_clocksync.h -
     *  never assumed to be zero-jitter. */
    sapi_duration_ms_t max_delay_ms;

    /** Minimum number of *other* channels whose valid confirmation must
     *  arrive within max_delay_ms for this checkpoint to succeed. */
    uint32_t expected_node_count;

    /** Optional SAPI_WATCHDOG_CHECKPOINT-type watchdog to kick on a
     *  successful checkpoint, for liveness monitoring across many
     *  checkpoints independent of any single checkpoint's own timeout
     *  reaction. May be NULL. */
    sapi_watchdog_t watchdog;
} sapi_checkpoint_config_t;

/**
 * @brief Performs one bounded checkpoint rendezvous across every channel
 *        of an already-initialized vital channel instance.
 *
 * Broadcasts a checkpoint-arrival message (config->checkpoint_id) to
 * every channel via the vital channel's own backend_send, then polls
 * each channel's backend_recv bounded by config->max_delay_ms, counting
 * only replies that pass sapi_checksum_vital_message_verify() and carry
 * the matching checkpoint_id.
 *
 * @param handle  Initialized vital channel instance (sapi_vital_channel_init()
 *                already called). Must not be NULL.
 * @param config  Checkpoint configuration. Must not be NULL.
 * @return SAPI_STATUS_OK if at least config->expected_node_count valid
 *         confirmations arrived within config->max_delay_ms (the optional
 *         watchdog, if any, has been kicked).
 * @return SAPI_STATUS_INVALID_PARAM if handle or config is NULL, or
 *         config->expected_node_count exceeds handle->channel_count.
 * @return SAPI_STATUS_TIMEOUT if fewer than expected_node_count valid
 *         confirmations arrived in time.
 *
 * @pre handle != NULL, config != NULL
 * @post On SAPI_STATUS_TIMEOUT, sapi_safestate_enter() has already been
 *       called at SAPI_SAFESTATE_LEVEL_SAFE with
 *       SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT before this function
 *       returns (REQ-CHECKPOINT-003) - the same "safe-state is triggered
 *       automatically by the sync/vote logic itself" pattern
 *       sapi_vital_channel_receive() already uses on a voting
 *       disagreement.
 *
 * @safety No dynamic memory allocation. Never blocks longer than
 *         config->max_delay_ms (REQ-CHECKPOINT-001).
 */
sapi_status_t sapi_channel_checkpoint(sapi_vital_channel_t *handle,
                                       const sapi_checkpoint_config_t *config);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* SAPI_CHECKPOINT_H */

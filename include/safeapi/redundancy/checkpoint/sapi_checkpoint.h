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
 * 2.2, updated by ADR-025): it reuses a sapi_voter_t's own registered
 * sapi_channel_t links - each already carrying whatever transport
 * an integrator plugged in (sapi_ipc over POSIX/RTOS, or a real network
 * link) - so checkpoint messages travel over the same channels a
 * disagreement vote already would. This is how "pluggable" is achieved
 * here, consistent with ADR-005 rather than inventing a second plug-in
 * point.
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

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"
#include "safeapi/redundancy/voter/sapi_voter.h"
#include "safeapi/redundancy/watchdog/sapi_watchdog.h"

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

    /** Optional watchdog to kick for liveness monitoring, independent of
     *  any single checkpoint's own pass/fail outcome. Kicked once per
     *  internal retry round (see sapi_channel_checkpoint()'s own doc) as
     *  well as on a fully successful checkpoint - a round that performed
     *  real send/receive I/O is genuine forward progress, not a fake
     *  kick. Intended for a caller whose own liveness watchdog is kicked
     *  once per outer cycle, AFTER this call returns: passing that same
     *  watchdog here lets it keep being kicked while this call is still
     *  legitimately retrying within its own longer max_delay_ms budget
     *  (e.g. a relaxed startup window), rather than firing out from under
     *  an in-progress-but-not-hung call. May be NULL. */
    sapi_watchdog_t watchdog;
} sapi_checkpoint_config_t;

/**
 * @brief Performs one bounded checkpoint rendezvous across every channel
 *        registered with a voter.
 *
 * Broadcasts a checkpoint-arrival message (config->checkpoint_id) to
 * every one of voter's registered channels (via each channel's own
 * sapi_channel_send()), then polls each channel's
 * sapi_channel_receive() bounded by config->max_delay_ms, counting
 * only replies that pass sapi_checksum_vital_message_verify() and carry
 * the matching checkpoint_id. This is direct per-channel I/O, not a
 * sapi_voter_send()/_receive() voting round - checkpoint messages are
 * rendezvous markers, not data to vote on.
 *
 * @param voter   Voter with 1+ channels already registered
 *                (sapi_voter_register_channel()). Must not be NULL.
 * @param config  Checkpoint configuration. Must not be NULL.
 * @return SAPI_STATUS_OK if at least config->expected_node_count valid
 *         confirmations arrived within config->max_delay_ms (the optional
 *         watchdog, if any, has been kicked at least once - once per
 *         internal retry round, plus once more on this success).
 * @return SAPI_STATUS_INVALID_PARAM if voter or config is NULL, or
 *         config->expected_node_count exceeds voter's registered channel count.
 * @return SAPI_STATUS_TIMEOUT if fewer than expected_node_count valid
 *         confirmations arrived in time.
 *
 * @pre voter != NULL, config != NULL
 * @post On SAPI_STATUS_TIMEOUT, sapi_safestate_enter() has already been
 *       called at SAPI_SAFESTATE_LEVEL_SAFE with
 *       SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT before this function
 *       returns (REQ-CHECKPOINT-003) - the same "safe-state is triggered
 *       automatically by the sync/vote logic itself" pattern
 *       sapi_voter_receive() already uses on a voting disagreement.
 *
 * @safety No dynamic memory allocation. Never blocks longer than
 *         config->max_delay_ms (REQ-CHECKPOINT-001).
 */
sapi_status_t sapi_channel_checkpoint(sapi_voter_t *voter,
                                       const sapi_checkpoint_config_t *config);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* SAPI_CHECKPOINT_H */

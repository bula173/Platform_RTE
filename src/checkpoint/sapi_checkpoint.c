/**
 * @file sapi_checkpoint.c
 * @brief Implementation of the bounded checkpoint rendezvous (ADR-017).
 * @ingroup CHECKPOINT
 */
#include "safeapi/checkpoint/sapi_checkpoint.h"

#include <stdbool.h>
#include <stdio.h>

#include "safeapi/buffer/sapi_buffer.h"
#include "safeapi/checksum/sapi_checksum.h"
#include "safeapi/safestate/sapi_safestate.h"
#include "safeapi/timer/sapi_timer.h"

/** Fixed sender id for checkpoint-arrival markers. sapi_checksum's verify
 *  path does not constrain this value; it only matters for diagnostics,
 *  so a single framework-reserved constant is sufficient rather than
 *  threading a real per-channel identity through this module. */
#define SAPI_CHECKPOINT_SENDER_ID ((uint32_t)0xC4EC0001U)

/**
 * @brief Builds the checkpoint-arrival marker message.
 *
 * checkpoint_id is carried both as the sapi_vital_message_t sequence
 * number (checked by sapi_checksum_vital_message_verify() for
 * continuity) and, explicit-endian-encoded, as the 4-byte payload -
 * belt and suspenders against a reply from a stale or wrong checkpoint
 * being mistaken for a current one (REQ-CHECKPOINT-002). The payload is
 * written little-endian rather than a raw struct copy specifically
 * because the two channels exchanging it may be different machines,
 * potentially different CPU architectures - a bare memcpy of a uint32_t
 * would silently corrupt the value across a byte-order mismatch.
 *
 * @param checkpoint_id  Checkpoint sequence number to encode.
 * @param out_msg        Receives the built message. Must not be NULL.
 * @return SAPI_STATUS_OK on success; a sapi_buffer_t/sapi_checksum error
 *         status otherwise.
 */
static sapi_status_t build_arrival_message(uint32_t checkpoint_id, sapi_vital_message_t *out_msg)
{
    uint8_t payload_bytes[4];
    sapi_buffer_t payload_buf;
    sapi_status_t status = sapi_buffer_init(&payload_buf, payload_bytes, sizeof(payload_bytes));

    if (status == SAPI_STATUS_OK)
    {
        status = sapi_buffer_write_u32_le(&payload_buf, checkpoint_id);
    }

    if (status == SAPI_STATUS_OK)
    {
        status = sapi_checksum_vital_message_create(out_msg,
                                                      SAPI_CHECKPOINT_SENDER_ID,
                                                      checkpoint_id,
                                                      payload_bytes,
                                                      sizeof(payload_bytes));
    }

    return status;
}

/**
 * @brief Verifies a candidate reply's CRC/sequence (via sapi_checksum) and
 *        that its decoded payload matches checkpoint_id.
 *
 * Returns false - not an error - for anything that fails either check: a
 * corrupted or stale/wrong-checkpoint reply must never count toward
 * quorum (REQ-CHECKPOINT-002), but it is not itself grounds to fail the
 * whole rendezvous - other channels may still confirm in time.
 *
 * @param reply          Candidate reply message. Must not be NULL.
 * @param checkpoint_id  Expected checkpoint sequence number.
 * @return true if reply is a valid, current confirmation; false otherwise.
 */
static bool reply_confirms_checkpoint(const sapi_vital_message_t *reply, uint32_t checkpoint_id)
{
    uint8_t payload_out[4];
    uint8_t payload_size_out = 0U;
    bool confirmed = false;
    sapi_status_t verify_status = sapi_checksum_vital_message_verify(
        reply, checkpoint_id, payload_out, sizeof(payload_out), &payload_size_out);

    if ((verify_status == SAPI_STATUS_OK) && (payload_size_out == sizeof(payload_out)))
    {
        sapi_buffer_t payload_buf;

        if (sapi_buffer_init(&payload_buf, payload_out, sizeof(payload_out)) == SAPI_STATUS_OK)
        {
            uint32_t decoded_id = 0U;

            /* Reads happen against buf->length, not just capacity; the
             * buffer was just fully written by verify() above, so its
             * length must be advanced to cover it before reading back. */
            (void)sapi_buffer_set_length(&payload_buf, sizeof(payload_out));

            if ((sapi_buffer_read_u32_le(&payload_buf, 0U, &decoded_id) == SAPI_STATUS_OK)
                && (decoded_id == checkpoint_id))
            {
                confirmed = true;
            }
        }
    }

    return confirmed;
}

/**
 * @brief Returns the time budget remaining until start_ms + max_delay_ms.
 *
 * Keeps the whole rendezvous bounded by max_delay_ms in total
 * (REQ-CHECKPOINT-001) rather than re-granting a fresh max_delay_ms
 * budget to every channel polled in sequence.
 *
 * @param start_ms      Monotonic time the rendezvous budget started.
 * @param now_ms        Current monotonic time.
 * @param max_delay_ms  Total budget allotted to the rendezvous.
 * @return Remaining budget in ms; 0 if the deadline has already passed.
 */
static sapi_duration_ms_t remaining_budget_ms(sapi_timestamp_ms_t start_ms,
                                               sapi_timestamp_ms_t now_ms,
                                               sapi_duration_ms_t max_delay_ms)
{
    sapi_duration_ms_t remaining;
    sapi_timestamp_ms_t elapsed_ms = now_ms - start_ms;

    if (elapsed_ms >= (sapi_timestamp_ms_t)max_delay_ms)
    {
        remaining = 0U;
    }
    else
    {
        remaining = max_delay_ms - (sapi_duration_ms_t)elapsed_ms;
    }

    return remaining;
}

sapi_status_t sapi_channel_checkpoint(sapi_voter_t *voter, const sapi_checkpoint_config_t *config)
{
    sapi_status_t status = SAPI_STATUS_OK;
    sapi_vital_message_t arrival_msg;
    uint32_t channel_count;

    channel_count = sapi_voter_get_channel_count(voter);

    if ((voter == NULL) || (config == NULL))
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else if ((config->expected_node_count == 0U) || (config->expected_node_count > channel_count))
    {
        status = SAPI_STATUS_INVALID_PARAM;
    }
    else
    {
        status = build_arrival_message(config->checkpoint_id, &arrival_msg);
    }

    if (status == SAPI_STATUS_OK)
    {
        sapi_timestamp_ms_t start_ms = 0U;
        uint32_t confirmed_count = 0U;
        uint32_t i;
        sapi_status_t last_send_status = SAPI_STATUS_OK;
        sapi_status_t last_recv_status = SAPI_STATUS_OK;

        (void)sapi_timer_now(&start_ms);

        for (i = 0U; i < channel_count; i++)
        {
            sapi_channel_t *channel = sapi_voter_get_channel(voter, i);
            sapi_status_t send_status = sapi_channel_send(channel, &arrival_msg, sizeof(arrival_msg));

            last_send_status = send_status;
            if (send_status == SAPI_STATUS_OK)
            {
                sapi_vital_message_t reply;
                sapi_timestamp_ms_t now_ms = start_ms;
                sapi_status_t recv_status;

                (void)sapi_timer_now(&now_ms);
                recv_status = sapi_channel_receive(
                    channel, &reply, sizeof(reply),
                    remaining_budget_ms(start_ms, now_ms, config->max_delay_ms));

                last_recv_status = recv_status;
                if ((recv_status == SAPI_STATUS_OK) && reply_confirms_checkpoint(&reply, config->checkpoint_id))
                {
                    confirmed_count++;
                }
            }
        }

        if (confirmed_count >= config->expected_node_count)
        {
            if (config->watchdog != NULL)
            {
                (void)sapi_watchdog_kick(config->watchdog);
            }
            status = SAPI_STATUS_OK;
        }
        else
        {
            /* Diagnostic-only, fixed-size buffer (no dynamic allocation,
             * CLAUDE.md): before this REQ-COMMON-SAFESTATE-002 permanent
             * halt, capture the LAST channel's own send/receive outcome so
             * a registered SAFE-level handler (see sapi_safestate.h) can
             * actually log WHY this rendezvous failed - confirmed_count
             * alone does not distinguish "peer never sent" (send_status
             * failure) from "peer sent but didn't reply in time"
             * (recv_status == SAPI_STATUS_TIMEOUT) from "replied with the
             * wrong checkpoint_id" (recv_status == SAPI_STATUS_OK but not
             * confirmed) - this was previously impossible to tell apart
             * from outside this function, since no handler was ever
             * registered for this level anywhere in this codebase and the
             * halt itself is otherwise completely silent. */
            char diag_message[96];

            (void)snprintf(diag_message, sizeof(diag_message),
                            "checkpoint confirmed=%u expected=%u channels=%u last_send=%d last_recv=%d",
                            (unsigned int)confirmed_count, (unsigned int)config->expected_node_count,
                            (unsigned int)channel_count, (int)last_send_status, (int)last_recv_status);
            /* Matches the pattern sapi_voter_receive() already uses on a
             * voting disagreement: safe-state is triggered directly by
             * the sync/vote logic itself, not left to the caller to
             * notice and react to (REQ-CHECKPOINT-003). */
            sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE, SAPI_SAFESTATE_REASON_CHECKPOINT_TIMEOUT, __FILE__,
                                  (int32_t)__LINE__, diag_message);
            status = SAPI_STATUS_TIMEOUT;
        }
    }

    return status;
}

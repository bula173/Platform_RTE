/**
 * @file rte_checkpoint.c
 * @brief Implementation of the bounded checkpoint rendezvous (ADR-017).
 * @ingroup CHECKPOINT
 */
#include "rte/redundancy/checkpoint/rte_checkpoint.h"

#include <stdbool.h>
#include <stdio.h>

#include "rte/utils/buffer/rte_buffer.h"
#include "rte/redundancy/checksum/rte_checksum.h"
#include "rte/utils/safestate/rte_safestate.h"
#include "rte/oal/timer/rte_timer.h"

/** Fixed sender id for checkpoint-arrival markers. rte_checksum's verify
 *  path does not constrain this value; it only matters for diagnostics,
 *  so a single framework-reserved constant is sufficient rather than
 *  threading a real per-channel identity through this module. */
#define RTE_CHECKPOINT_SENDER_ID ((uint32_t)0xC4EC0001U)

/**
 * Per-attempt receive sub-budget within one rte_channel_checkpoint()
 * call (see the retry loop below) - deliberately short relative to a
 * typical config->max_delay_ms so a single call gets several independent
 * send+receive rounds instead of committing its whole budget to one.
 * Real bug this fixes, found via live Docker testing: a LISTEN-role
 * OSAdapter channel can only reply once it has learned its peer's address
 * from an inbound packet (rte_posix_osadapter_channel_service.c's own
 * has_peer_addr) - on a genuinely cold two-way start neither side has
 * heard from the other yet, so a SINGLE round's send is a structural
 * no-op on the listening side regardless of how long the matching
 * receive is allowed to wait. One long blocking round-trip attempt can
 * therefore never succeed on a cold start; it always spends the entire
 * budget and then fails, triggering RTE_SAFESTATE_REASON_CHECKPOINT_TIMEOUT
 * on both peers - which each reboot resets, replaying the same
 * structurally-doomed single attempt forever (a real, reproduced
 * livelock, not a transport/network problem - confirmed via a raw UDP
 * probe succeeding instantly against the same listening socket). Several
 * short rounds instead give that one-time address-learning step room to
 * complete on an early round, so a later round within the SAME call can
 * actually exchange confirmations - still bounded by max_delay_ms in
 * total (REQ-CHECKPOINT-001 unchanged). */
#define RTE_CHECKPOINT_RETRY_ROUND_MS ((rte_duration_ms_t)250U)

/**
 * Minimum number of retry rounds a call's own config->max_delay_ms budget
 * is deliberately divided into, regardless of how small that budget is.
 * RTE_CHECKPOINT_RETRY_ROUND_MS alone only bounds a round's LENGTH from
 * above; for a short budget (a steady-state call, not the relaxed
 * post-reconnect window this retry loop was originally added for) that
 * still leaves as few as one or two rounds - not enough headroom for the
 * project's own documented real-world finding that a single round can
 * miss under genuine container-host scheduling jitter even once a peer
 * is already known and reachable (see
 * RTE_EXAMPLE_AB_CHECKPOINT_RELAXED_MAX_DELAY_MS's doc in
 * safeAPIRBC2oo2GP/src/application/AB/common/common_config.h). Confirmed
 * live: even with the round-based retry loop below, a steady-state
 * budget of 400ms (~1-2 rounds at the flat 250ms cap) still let a single
 * missed round trip escalate to REQ-CHECKPOINT-003's SAFE/REBOOT often
 * enough to keep A/WEST and B/WEST cycling reboots roughly every 40s
 * instead of settling. Dividing max_delay_ms by this constant (applied
 * only when that division yields something SMALLER than
 * RTE_CHECKPOINT_RETRY_ROUND_MS - a long relaxed budget is unaffected)
 * trades round length for round COUNT on a short budget, without raising
 * the budget itself. */
#define RTE_CHECKPOINT_MIN_ROUNDS_PER_BUDGET ((rte_duration_ms_t)3U)

/**
 * Hard cap on retry rounds within one rte_channel_checkpoint() call -
 * a backstop independent of config->max_delay_ms (REQ-CHECKPOINT-001
 * stays satisfied by the time-based bound alone; this exists only to
 * guarantee termination even if a receive callback returns near-
 * instantly instead of genuinely blocking for its requested timeout, as
 * this project's own mock-backed unit tests do). The largest legitimate
 * round count in real use is the relaxed startup budget divided by the
 * per-round sub-budget (10000ms / 250ms = 40); this leaves headroom
 * above that without being unbounded. */
#define RTE_CHECKPOINT_MAX_ROUNDS ((uint32_t)64U)

/**
 * @brief Builds the checkpoint-arrival marker message.
 *
 * checkpoint_id is carried both as the rte_vital_message_t sequence
 * number (checked by rte_checksum_vital_message_verify() for
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
 * @return RTE_STATUS_OK on success; a rte_buffer_t/rte_checksum error
 *         status otherwise.
 */
static rte_status_t build_arrival_message(uint32_t checkpoint_id, rte_vital_message_t *out_msg)
{
    uint8_t payload_bytes[4];
    rte_buffer_t payload_buf;
    rte_status_t status = rte_buffer_init(&payload_buf, payload_bytes, sizeof(payload_bytes));

    if (status == RTE_STATUS_OK)
    {
        status = rte_buffer_write_u32_le(&payload_buf, checkpoint_id);
    }

    if (status == RTE_STATUS_OK)
    {
        status = rte_checksum_vital_message_create(out_msg,
                                                      RTE_CHECKPOINT_SENDER_ID,
                                                      checkpoint_id,
                                                      payload_bytes,
                                                      sizeof(payload_bytes));
    }

    return status;
}

/**
 * @brief Verifies a candidate reply's CRC/sequence (via rte_checksum) and
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
static bool reply_confirms_checkpoint(const rte_vital_message_t *reply, uint32_t checkpoint_id)
{
    uint8_t payload_out[4];
    uint8_t payload_size_out = 0U;
    bool confirmed = false;
    rte_status_t verify_status = rte_checksum_vital_message_verify(
        reply, checkpoint_id, payload_out, sizeof(payload_out), &payload_size_out);

    if ((verify_status == RTE_STATUS_OK) && (payload_size_out == sizeof(payload_out)))
    {
        rte_buffer_t payload_buf;

        if (rte_buffer_init(&payload_buf, payload_out, sizeof(payload_out)) == RTE_STATUS_OK)
        {
            uint32_t decoded_id = 0U;

            /* Reads happen against buf->length, not just capacity; the
             * buffer was just fully written by verify() above, so its
             * length must be advanced to cover it before reading back. */
            (void)rte_buffer_set_length(&payload_buf, sizeof(payload_out));

            if ((rte_buffer_read_u32_le(&payload_buf, 0U, &decoded_id) == RTE_STATUS_OK)
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
static rte_duration_ms_t remaining_budget_ms(rte_timestamp_ms_t start_ms,
                                               rte_timestamp_ms_t now_ms,
                                               rte_duration_ms_t max_delay_ms)
{
    rte_duration_ms_t remaining;
    rte_timestamp_ms_t elapsed_ms = now_ms - start_ms;

    if (elapsed_ms >= (rte_timestamp_ms_t)max_delay_ms)
    {
        remaining = 0U;
    }
    else
    {
        remaining = max_delay_ms - (rte_duration_ms_t)elapsed_ms;
    }

    return remaining;
}

/* rte_channel_checkpoint()'s own per-channel send+receive+confirm attempt, extracted: sends the
 * arrival message to channel index i, and on a successful send, waits (bounded by round_cap_ms
 * and whatever is left of the overall max_delay_ms budget from start_ms) for a reply confirming
 * checkpoint_id. Reports the outcome only - updating channel_confirmed[]/confirmed_count stays
 * the caller's job, same "do the I/O, let the caller interpret it" split
 * rte_voter_receive()'s own extraction already established (see git history). *now_ms is
 * updated (rte_timer_now() is called again before the receive, to size its own sub-budget
 * accurately) - an out-parameter, not a return value, to match the loop variable it feeds back
 * into on every call. No logic changed from the block this replaces. */
static bool checkpoint_try_channel(rte_voter_t *voter, uint32_t i, const rte_vital_message_t *arrival_msg,
                                    uint32_t checkpoint_id, rte_timestamp_ms_t start_ms, rte_timestamp_ms_t *now_ms,
                                    rte_duration_ms_t max_delay_ms, rte_duration_ms_t round_cap_ms,
                                    rte_status_t *out_last_send_status, rte_status_t *out_last_recv_status)
{
    rte_channel_t *channel;
    rte_status_t send_status;
    bool confirmed = false;

    channel = rte_voter_get_channel(voter, i);
    send_status = rte_channel_send(channel, arrival_msg, sizeof(*arrival_msg));
    *out_last_send_status = send_status;
    if (send_status == RTE_STATUS_OK)
    {
        rte_vital_message_t reply;
        rte_duration_ms_t sub_budget_ms;
        rte_status_t recv_status;

        (void)rte_timer_now(now_ms);
        sub_budget_ms = remaining_budget_ms(start_ms, *now_ms, max_delay_ms);
        if (sub_budget_ms > round_cap_ms)
        {
            sub_budget_ms = round_cap_ms;
        }

        recv_status = rte_channel_receive(channel, &reply, sizeof(reply), sub_budget_ms);
        *out_last_recv_status = recv_status;
        confirmed = (recv_status == RTE_STATUS_OK) && reply_confirms_checkpoint(&reply, checkpoint_id);
    }
    return confirmed;
}

rte_status_t rte_channel_checkpoint(rte_voter_t *voter, const rte_checkpoint_config_t *config)
{
    rte_status_t status = RTE_STATUS_OK;
    rte_vital_message_t arrival_msg;
    uint32_t channel_count;

    channel_count = rte_voter_get_channel_count(voter);

    if ((voter == NULL) || (config == NULL))
    {
        status = RTE_STATUS_INVALID_PARAM;
    }
    else if ((config->expected_node_count == 0U) || (config->expected_node_count > channel_count))
    {
        status = RTE_STATUS_INVALID_PARAM;
    }
    else
    {
        status = build_arrival_message(config->checkpoint_id, &arrival_msg);
    }

    if (status == RTE_STATUS_OK)
    {
        rte_timestamp_ms_t start_ms = 0U;
        rte_timestamp_ms_t now_ms = 0U;
        uint32_t confirmed_count = 0U;
        uint32_t i;
        rte_status_t last_send_status = RTE_STATUS_OK;
        rte_status_t last_recv_status = RTE_STATUS_OK;
        bool channel_confirmed[RTE_VOTER_MAX_CHANNELS];
        uint32_t round_count = 0U;
        rte_duration_ms_t round_cap_ms = RTE_CHECKPOINT_RETRY_ROUND_MS;
        rte_duration_ms_t budget_based_round_cap_ms =
            (rte_duration_ms_t)(config->max_delay_ms / RTE_CHECKPOINT_MIN_ROUNDS_PER_BUDGET);

        if ((budget_based_round_cap_ms > 0U) && (budget_based_round_cap_ms < round_cap_ms))
        {
            round_cap_ms = budget_based_round_cap_ms;
        }

        for (i = 0U; i < RTE_VOTER_MAX_CHANNELS; i++)
        {
            channel_confirmed[i] = false;
        }

        (void)rte_timer_now(&start_ms);
        now_ms = start_ms;

        /* Retries the whole send+receive round, per not-yet-confirmed
         * channel, until either every expected channel has confirmed, the
         * overall max_delay_ms budget is exhausted, or RTE_CHECKPOINT_MAX_ROUNDS
         * rounds have run - see RTE_CHECKPOINT_RETRY_ROUND_MS's own doc for
         * why a single round cannot succeed on a cold two-way start. Still
         * bounded by max_delay_ms in total (REQ-CHECKPOINT-001): each
         * round's own receive is capped to whatever's left of that budget,
         * never more. The round-count cap is a separate, deliberate
         * backstop against a transport whose receive call returns near-
         * instantly instead of genuinely blocking for its requested
         * timeout (confirmed live: this project's own mock-backed unit
         * tests do exactly that, which turned this loop into a real
         * busy-spin consuming the full max_delay_ms in real wall-clock
         * time at 100% CPU before this cap was added) - real production
         * OSAdapters block for close to the requested duration, so this
         * cap is not expected to bind there, only to guarantee it can't
         * regardless of transport behavior. */
        while ((confirmed_count < config->expected_node_count)
               && (round_count < RTE_CHECKPOINT_MAX_ROUNDS)
               && (remaining_budget_ms(start_ms, now_ms, config->max_delay_ms) > 0U))
        {
            for (i = 0U; i < channel_count; i++)
            {
                if ((i < RTE_VOTER_MAX_CHANNELS) && channel_confirmed[i])
                {
                    continue;
                }

                if (checkpoint_try_channel(voter, i, &arrival_msg, config->checkpoint_id, start_ms, &now_ms,
                                            config->max_delay_ms, round_cap_ms, &last_send_status,
                                            &last_recv_status))
                {
                    if (i < RTE_VOTER_MAX_CHANNELS)
                    {
                        channel_confirmed[i] = true;
                    }
                    confirmed_count++;
                }
            }
            (void)rte_timer_now(&now_ms);
            round_count++;

            /* Kick config->watchdog (if any) once per round, not only on
             * final success below - a caller-supplied liveness watchdog
             * exists to catch a genuinely stuck cyclic executive, and a
             * round that just performed real send/receive I/O is genuine
             * forward progress, not a fake kick. Found live: a caller
             * whose own liveness watchdog has a short (sub-second) timeout
             * and is kicked only once per full cycle, AFTER this function
             * returns, would otherwise have that watchdog fire out from
             * under a call that is still legitimately retrying within its
             * own much longer config->max_delay_ms budget (e.g. the
             * relaxed startup window this retry loop exists for) - not a
             * hung caller, just a caller who has not been given a chance
             * to kick its own watchdog yet. */
            if (config->watchdog != NULL)
            {
                (void)rte_watchdog_kick(config->watchdog);
            }
        }

        if (confirmed_count >= config->expected_node_count)
        {
            if (config->watchdog != NULL)
            {
                (void)rte_watchdog_kick(config->watchdog);
            }
            status = RTE_STATUS_OK;
        }
        else
        {
            /* Diagnostic-only, fixed-size buffer (no dynamic allocation,
             * CLAUDE.md): before this REQ-COMMON-SAFESTATE-002 permanent
             * halt, capture the LAST channel's own send/receive outcome so
             * a registered SAFE-level handler (see rte_safestate.h) can
             * actually log WHY this rendezvous failed - confirmed_count
             * alone does not distinguish "peer never sent" (send_status
             * failure) from "peer sent but didn't reply in time"
             * (recv_status == RTE_STATUS_TIMEOUT) from "replied with the
             * wrong checkpoint_id" (recv_status == RTE_STATUS_OK but not
             * confirmed) - this was previously impossible to tell apart
             * from outside this function, since no handler was ever
             * registered for this level anywhere in this codebase and the
             * halt itself is otherwise completely silent. */
            char diag_message[96];

            (void)snprintf(diag_message, sizeof(diag_message),
                            "checkpoint confirmed=%u expected=%u channels=%u last_send=%d last_recv=%d",
                            (unsigned int)confirmed_count, (unsigned int)config->expected_node_count,
                            (unsigned int)channel_count, (int)last_send_status, (int)last_recv_status);
            /* Matches the pattern rte_voter_receive() already uses on a
             * voting disagreement: safe-state is triggered directly by
             * the sync/vote logic itself, not left to the caller to
             * notice and react to (REQ-CHECKPOINT-003). */
            rte_safestate_enter(RTE_SAFESTATE_LEVEL_SAFE, RTE_SAFESTATE_REASON_CHECKPOINT_TIMEOUT, __FILE__,
                                  (int32_t)__LINE__, diag_message);
            status = RTE_STATUS_TIMEOUT;
        }
    }

    return status;
}

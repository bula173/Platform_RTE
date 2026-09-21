/**
 * @file rte_dual_channel.c
 * @ingroup DUAL
 * @brief "DualChannel" layer of ADR-020 - see rte_dual_channel.h.
 */
#include "safeapi/redundancy/dual/rte_dual_channel.h"

#include <string.h>

#include "safeapi/oal/timer/rte_timer.h"

/** @brief Cap on consecutive rte_dual_channel_send() ACK-wait polls that
 *         may complete without rte_timer_now() showing any measurable
 *         progress, before that link's wait loop gives up on this round -
 *         see that loop's own comment for why more than one such
 *         iteration is legitimate but an unbounded number is not. */
#define RTE_DUAL_CHANNEL_STALL_POLL_LIMIT 32U

/** @brief Result of one dual_channel_poll_link_once() attempt. */
typedef struct
{
    bool                    matched;      /**< true if a recognized frame was received this attempt. */
    rte_dual_frame_kind_t  kind;         /**< Valid only if matched. */
    uint32_t                ack_sequence; /**< Valid only if matched && kind == RTE_DUAL_FRAME_KIND_ACK. */
} dual_poll_result_t;

static rte_dual_channel_status_t dual_channel_compute_status(const rte_dual_channel_t *channel)
{
    uint32_t up_count = 0U;
    uint32_t i;

    for (i = 0U; i < channel->link_count; i++)
    {
        if (channel->link_up[i])
        {
            up_count++;
        }
    }

    if (up_count == 0U)
    {
        return RTE_DUAL_CHANNEL_STATUS_DOWN;
    }
    if (up_count == channel->link_count)
    {
        return RTE_DUAL_CHANNEL_STATUS_FULL;
    }
    return RTE_DUAL_CHANNEL_STATUS_DEGRADED;
}

static void dual_channel_update_status(rte_dual_channel_t *channel)
{
    rte_dual_channel_status_t new_status = dual_channel_compute_status(channel);

    if (new_status != channel->last_status)
    {
        rte_dual_channel_status_t old_status = channel->last_status;

        channel->last_status = new_status;
        if (channel->status_callback != NULL)
        {
            channel->status_callback(new_status, old_status, channel->status_callback_ctx);
        }
    }
}

/**
 * @brief One receive attempt on one link, dispatched by frame kind:
 *        DATA is auto-ACKed and staged for rte_dual_channel_receive();
 *        STATE is staged for rte_dual_channel_receive_state_frame();
 *        ACK is reported back to the caller (rte_dual_channel_send()'s
 *        own wait loop) via *out_result, not staged anywhere.
 */
static rte_status_t dual_channel_poll_link_once(rte_dual_channel_t *channel, uint32_t link_index,
                                                  rte_duration_ms_t timeout_ms, dual_poll_result_t *out_result)
{
    uint8_t                  raw[RTE_DUAL_MSGCHANNEL_MAX_PAYLOAD];
    uint8_t                  raw_size = 0U;
    uint32_t                 sequence = 0U;
    rte_status_t             status;
    rte_dual_frame_header_t header;

    out_result->matched = false;

    status = rte_dual_msgchannel_receive(&channel->links[link_index], raw, (uint8_t)sizeof(raw), timeout_ms,
                                           &raw_size, &sequence);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    if (raw_size < (uint8_t)sizeof(header))
    {
        /* Too short to even hold this layer's own header - treat as a
         * defended-integrity failure, same family as a Layer-1 CRC/
         * sequence failure, not a silently-ignored malformed frame. */
        return RTE_STATUS_DATA_CORRUPTION;
    }
    (void)memcpy(&header, raw, sizeof(header));

    switch ((rte_dual_frame_kind_t)header.kind)
    {
        case RTE_DUAL_FRAME_KIND_DATA:
        {
            uint8_t              app_size = (uint8_t)(raw_size - (uint8_t)sizeof(header));
            rte_dual_ack_frame_t ack;

            if (app_size <= RTE_DUAL_CHANNEL_MAX_PAYLOAD)
            {
                (void)memcpy(channel->pending_data, &raw[sizeof(header)], app_size);
                channel->pending_data_size  = app_size;
                channel->pending_data_valid = true;
            }

            /* Auto-ACK, best-effort/fire-and-forget: a lost ACK is
             * observable to the sender as its own timeout on this link
             * (ADR-020 section 2), not silently hidden here. */
            ack.header.kind        = (uint8_t)RTE_DUAL_FRAME_KIND_ACK;
            ack.header.reserved[0] = 0U;
            ack.header.reserved[1] = 0U;
            ack.header.reserved[2] = 0U;
            ack.acked_sequence     = sequence;
            (void)rte_dual_msgchannel_send(&channel->links[link_index], (const uint8_t *)&ack, (uint8_t)sizeof(ack),
                                             channel->ack_timeout_ms, NULL);

            out_result->matched = true;
            out_result->kind    = RTE_DUAL_FRAME_KIND_DATA;
            break;
        }
        case RTE_DUAL_FRAME_KIND_ACK:
        {
            rte_dual_ack_frame_t ack;

            if (raw_size < (uint8_t)sizeof(ack))
            {
                return RTE_STATUS_DATA_CORRUPTION;
            }
            (void)memcpy(&ack, raw, sizeof(ack));

            out_result->matched      = true;
            out_result->kind         = RTE_DUAL_FRAME_KIND_ACK;
            out_result->ack_sequence = ack.acked_sequence;
            break;
        }
        case RTE_DUAL_FRAME_KIND_STATE:
        {
            if (raw_size < (uint8_t)sizeof(rte_dual_state_frame_t))
            {
                return RTE_STATUS_DATA_CORRUPTION;
            }
            (void)memcpy(&channel->pending_state, raw, sizeof(channel->pending_state));
            channel->pending_state_valid = true;

            out_result->matched = true;
            out_result->kind    = RTE_DUAL_FRAME_KIND_STATE;
            break;
        }
        case RTE_DUAL_FRAME_KIND_HEARTBEAT:
        {
            rte_dual_ack_frame_t ack;

            /* Auto-ACK, best-effort/fire-and-forget: acknowledges heartbeat */
            ack.header.kind        = (uint8_t)RTE_DUAL_FRAME_KIND_ACK;
            ack.header.reserved[0] = 0U;
            ack.header.reserved[1] = 0U;
            ack.header.reserved[2] = 0U;
            ack.acked_sequence     = sequence;
            (void)rte_dual_msgchannel_send(&channel->links[link_index], (const uint8_t *)&ack, (uint8_t)sizeof(ack),
                                             channel->ack_timeout_ms, NULL);

            out_result->matched = true;
            out_result->kind    = RTE_DUAL_FRAME_KIND_HEARTBEAT;
            break;
        }
        default:
            /* Unrecognized kind - defensive only (not reachable through
             * a conforming sender), ignore rather than fail. */
            break;
    }

    return RTE_STATUS_OK;
}

rte_status_t rte_dual_channel_init(rte_dual_channel_t *channel, const rte_dual_channel_config_t *config)
{
    uint32_t i;

    if ((channel == NULL) || (config == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->link_count == 0U) || (config->link_count > RTE_DUAL_CHANNEL_MAX_LINKS))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    for (i = 0U; i < config->link_count; i++)
    {
        if (config->links[i] == NULL)
        {
            return RTE_STATUS_INVALID_PARAM;
        }
    }

    channel->link_count = config->link_count;
    for (i = 0U; i < config->link_count; i++)
    {
        rte_dual_msgchannel_config_t msgcfg;

        msgcfg.link             = config->links[i];
        msgcfg.sender_id        = config->sender_id;
        msgcfg.expected_peer_id = config->expected_peer_id;
        (void)rte_dual_msgchannel_init(&channel->links[i], &msgcfg);
        channel->link_up[i] = false;
    }
    for (i = config->link_count; i < RTE_DUAL_CHANNEL_MAX_LINKS; i++)
    {
        channel->link_up[i] = false;
    }

    channel->ack_timeout_ms     = config->ack_timeout_ms;
    channel->status_callback    = config->status_callback;
    channel->status_callback_ctx = config->status_callback_ctx;
    channel->last_status        = RTE_DUAL_CHANNEL_STATUS_DOWN;

    channel->pending_data_valid = false;
    channel->pending_data_size  = 0U;
    channel->pending_state_valid = false;
    (void)memset(&channel->pending_state, 0, sizeof(channel->pending_state));

    return RTE_STATUS_OK;
}

rte_status_t rte_dual_channel_send(rte_dual_channel_t *channel, const uint8_t *payload, uint8_t payload_size,
                                      uint32_t *out_ack_link_count)
{
    uint8_t frame[(size_t)RTE_DUAL_CHANNEL_MAX_PAYLOAD + sizeof(rte_dual_frame_header_t)];
    uint8_t frame_size;
    uint32_t i;
    uint32_t ack_count = 0U;
    /* REQ-DUAL-CHANNEL-008: a link whose own send/receive reports
     * something other than RTE_STATUS_OK/RTE_STATUS_TIMEOUT (e.g.
     * RTE_STATUS_HARDWARE_FAULT from a closed/reset connection) is
     * genuinely broken, not just quiet - see this function's own header
     * for why collapsing that into the same generic TIMEOUT every other
     * "no ACK yet" case returns hid real transport failures from every
     * caller. Kept as the FIRST such status seen across all links this
     * call, not the last - an arbitrary but stable choice among possibly
     * several simultaneous failures. */
    bool          saw_hard_fault = false;
    rte_status_t hard_fault_status = RTE_STATUS_OK;

    if (channel == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((payload == NULL) && (payload_size != 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (payload_size > RTE_DUAL_CHANNEL_MAX_PAYLOAD)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    {
        rte_dual_frame_header_t header;

        header.kind        = (uint8_t)RTE_DUAL_FRAME_KIND_DATA;
        header.reserved[0] = 0U;
        header.reserved[1] = 0U;
        header.reserved[2] = 0U;
        (void)memcpy(frame, &header, sizeof(header));
        if (payload_size > 0U)
        {
            (void)memcpy(&frame[sizeof(header)], payload, payload_size);
        }
        frame_size = (uint8_t)(sizeof(header) + payload_size);
    }

    for (i = 0U; i < channel->link_count; i++)
    {
        uint32_t      sent_sequence = 0U;
        rte_status_t send_status;
        bool          link_now_up = false;

        send_status = rte_dual_msgchannel_send(&channel->links[i], frame, frame_size, channel->ack_timeout_ms,
                                                 &sent_sequence);
        if ((send_status == RTE_STATUS_HARDWARE_FAULT) && (!saw_hard_fault))
        {
            saw_hard_fault    = true;
            hard_fault_status = send_status;
        }
        if (send_status == RTE_STATUS_OK)
        {
            rte_duration_ms_t   remaining = channel->ack_timeout_ms;
            rte_timestamp_ms_t  start_ms = 0U;
            /* Bounds how many consecutive polls may complete without
             * rte_timer_now() showing any measurable progress since
             * start_ms, before this loop gives up on this link for this
             * round - see the "else" branch below for why this can
             * legitimately happen more than once and must not itself be
             * unbounded. RTE_DUAL_CHANNEL_STALL_POLL_LIMIT is a fixed,
             * generous cap (a real exchange needs at most a handful of
             * iterations - one per DATA/STATE/foreign-ACK frame handled
             * as a side effect before this link's own matching ACK
             * arrives), not a tuned timing value. */
            uint32_t stall_polls = 0U;

            (void)rte_timer_now(&start_ms);

            while ((remaining > 0U) && (stall_polls < RTE_DUAL_CHANNEL_STALL_POLL_LIMIT))
            {
                dual_poll_result_t result;
                rte_status_t       poll_status;
                rte_timestamp_ms_t now_ms = 0U;

                poll_status = dual_channel_poll_link_once(channel, i, remaining, &result);
                if ((poll_status == RTE_STATUS_OK) && result.matched && (result.kind == RTE_DUAL_FRAME_KIND_ACK)
                    && (result.ack_sequence == sent_sequence))
                {
                    link_now_up = true;
                    break;
                }
                if (poll_status == RTE_STATUS_HARDWARE_FAULT)
                {
                    /* Genuinely broken (not just "no ACK frame arrived
                     * yet this poll"), e.g. HARDWARE_FAULT from the peer
                     * having closed/reset the connection - see this
                     * function's own header. No point continuing to poll
                     * a dead link for the rest of its own ack_timeout_ms
                     * budget. */
                    if (!saw_hard_fault)
                    {
                        saw_hard_fault    = true;
                        hard_fault_status = poll_status;
                    }
                    break;
                }

                /* Recompute the remaining budget from wall-clock elapsed
                 * time, not a fixed per-iteration decrement - a DATA/STATE
                 * frame handled as a side effect above may have consumed
                 * an arbitrary fraction of this link's own timeout
                 * already. */
                (void)rte_timer_now(&now_ms);
                if (now_ms > start_ms)
                {
                    rte_timestamp_ms_t elapsed = now_ms - start_ms;

                    if (elapsed >= (rte_timestamp_ms_t)channel->ack_timeout_ms)
                    {
                        remaining = 0U;
                    }
                    else
                    {
                        remaining = channel->ack_timeout_ms - (rte_duration_ms_t)elapsed;
                    }
                    stall_polls = 0U;
                }
                else
                {
                    /* REQ-DUAL-CHANNEL-007: now_ms == start_ms - no
                     * measurable time has passed on rte_timer_now()'s own
                     * tick resolution since this round started. This used
                     * to be treated as "no timer
                     * backend available, give up after one attempt"
                     * (REQ-OAL-LOG-001-style "never spin on an
                     * unmeasurable interval"), but a real localhost round
                     * trip (connect, send DATA, receive the peer's own
                     * DATA, auto-ACK it, receive the peer's own ACK)
                     * routinely completes inside a single millisecond
                     * tick, which made that same guard misfire as a false
                     * "no timer" abort after exactly one poll - discovered
                     * live via SITE's migration to rte_safechannel
                     * (ADR-022), where both sites send their negotiation
                     * beacon at nearly the same instant over a real
                     * loopback TCP link. `remaining` is left unchanged so
                     * a fast exchange like that one gets the extra polls
                     * it needs; stall_polls bounds how many such
                     * no-progress iterations are allowed before this loop
                     * gives up anyway, so a link with a genuinely
                     * non-advancing or absent timer still cannot spin
                     * forever. */
                    stall_polls++;
                }
            }
        }

        channel->link_up[i] = link_now_up;
        if (link_now_up)
        {
            ack_count++;
        }
    }

    if (out_ack_link_count != NULL)
    {
        *out_ack_link_count = ack_count;
    }

    dual_channel_update_status(channel);

    if (ack_count > 0U)
    {
        return RTE_STATUS_OK;
    }
    return saw_hard_fault ? hard_fault_status : RTE_STATUS_TIMEOUT;
}

rte_status_t rte_dual_channel_receive(rte_dual_channel_t *channel, uint8_t *out_payload, uint8_t max_size,
                                         rte_duration_ms_t timeout_ms, uint8_t *out_size)
{
    if ((channel == NULL) || (out_payload == NULL) || (out_size == NULL) || (max_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    {
        bool          saw_hard_fault = false;
        rte_status_t hard_fault_status = RTE_STATUS_OK;

        if (!channel->pending_data_valid)
        {
            uint32_t           i;
            rte_duration_ms_t per_link_timeout = (channel->link_count > 0U) ? (timeout_ms / channel->link_count) : 0U;

            /* Sweeps every configured link every call, even after an
             * earlier link in this same sweep already staged a DATA frame -
             * stopping early here would let a link with a shorter path (or
             * one that always has traffic) starve every other redundant
             * link of its own auto-ACK indefinitely (ADR-020 section 2: a
             * redundant link should degrade to DOWN only from genuinely not
             * responding, never from this module never getting around to
             * polling it). Still swept to completion even after a hard
             * fault on an earlier link, for the same reason. */
            for (i = 0U; i < channel->link_count; i++)
            {
                dual_poll_result_t result;
                rte_status_t       poll_status;

                poll_status = dual_channel_poll_link_once(channel, i, per_link_timeout, &result);
                /* REQ-DUAL-CHANNEL-008: see rte_dual_channel_send()'s own
                 * doc on this same distinction - a link reporting
                 * something other than OK/TIMEOUT (e.g. HARDWARE_FAULT
                 * from a closed/reset connection) is broken, not just
                 * quiet, and that must not be silently collapsed into the
                 * same generic TIMEOUT "nothing staged yet" returns below. */
                if ((poll_status == RTE_STATUS_HARDWARE_FAULT) && (!saw_hard_fault))
                {
                    saw_hard_fault    = true;
                    hard_fault_status = poll_status;
                }
            }
        }

        if (!channel->pending_data_valid)
        {
            return saw_hard_fault ? hard_fault_status : RTE_STATUS_TIMEOUT;
        }
    }
    if (channel->pending_data_size > max_size)
    {
        /* Caller's buffer is too small for a staged frame - reported,
         * not silently truncated (safety-relevant data). */
        return RTE_STATUS_INVALID_PARAM;
    }

    (void)memcpy(out_payload, channel->pending_data, channel->pending_data_size);
    *out_size                  = channel->pending_data_size;
    channel->pending_data_valid = false;

    return RTE_STATUS_OK;
}

rte_status_t rte_dual_channel_send_heartbeat(rte_dual_channel_t *channel, uint32_t *out_ack_link_count)
{
    rte_dual_heartbeat_frame_t frame;
    uint32_t i;
    uint32_t ack_count = 0U;
    bool saw_hard_fault = false;
    rte_status_t hard_fault_status = RTE_STATUS_OK;
    rte_timestamp_ms_t now_ms = 0U;

    if (channel == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    /* rte_dual_heartbeat_frame_t has a 4-byte compiler-inserted
     * alignment gap between `header` (4 bytes) and the 8-byte-aligned
     * `timestamp_ms` that follows it - unlike every other frame type in
     * this file, whose fields happen to add up to an already-aligned
     * offset. Without this memset, that gap is indeterminate stack
     * content that still gets read (and checksummed/transmitted) by the
     * rte_dual_msgchannel_send() call below, since it hashes/sends
     * `sizeof(frame)` raw bytes, not just the named fields - found via
     * Valgrind (SAFEAPI_ENABLE_ASAN/Valgrind CTest memcheck target)
     * flagging a real "use of uninitialised value" inside
     * rte_checksum_crc64(), reached from here. */
    (void)memset(&frame, 0, sizeof(frame));
    (void)rte_timer_now(&now_ms);
    frame.header.kind        = (uint8_t)RTE_DUAL_FRAME_KIND_HEARTBEAT;
    frame.header.reserved[0] = 0U;
    frame.header.reserved[1] = 0U;
    frame.header.reserved[2] = 0U;
    frame.timestamp_ms       = (uint64_t)now_ms;

    for (i = 0U; i < channel->link_count; i++)
    {
        uint32_t sent_sequence = 0U;
        rte_status_t send_status;
        bool link_now_up = false;

        send_status = rte_dual_msgchannel_send(&channel->links[i], (const uint8_t *)&frame,
                                                 (uint8_t)sizeof(frame), channel->ack_timeout_ms,
                                                 &sent_sequence);
        if ((send_status == RTE_STATUS_HARDWARE_FAULT) && (!saw_hard_fault))
        {
            saw_hard_fault = true;
            hard_fault_status = send_status;
        }
        if (send_status == RTE_STATUS_OK)
        {
            rte_duration_ms_t remaining = channel->ack_timeout_ms;
            rte_timestamp_ms_t start_ms = 0U;
            uint32_t stall_polls = 0U;

            (void)rte_timer_now(&start_ms);

            while ((remaining > 0U) && (stall_polls < RTE_DUAL_CHANNEL_STALL_POLL_LIMIT))
            {
                dual_poll_result_t result;
                rte_status_t poll_status;
                rte_timestamp_ms_t poll_now_ms = 0U;

                poll_status = dual_channel_poll_link_once(channel, i, remaining, &result);
                if ((poll_status == RTE_STATUS_OK) && result.matched && (result.kind == RTE_DUAL_FRAME_KIND_ACK)
                    && (result.ack_sequence == sent_sequence))
                {
                    link_now_up = true;
                    break;
                }
                if (poll_status == RTE_STATUS_HARDWARE_FAULT)
                {
                    if (!saw_hard_fault)
                    {
                        saw_hard_fault = true;
                        hard_fault_status = poll_status;
                    }
                    break;
                }

                (void)rte_timer_now(&poll_now_ms);
                if (poll_now_ms > start_ms)
                {
                    rte_timestamp_ms_t elapsed = poll_now_ms - start_ms;
                    if (elapsed >= (rte_timestamp_ms_t)channel->ack_timeout_ms)
                    {
                        remaining = 0U;
                    }
                    else
                    {
                        remaining = channel->ack_timeout_ms - (rte_duration_ms_t)elapsed;
                    }
                    stall_polls = 0U;
                }
                else
                {
                    stall_polls++;
                }
            }
        }

        channel->link_up[i] = link_now_up;
        if (link_now_up)
        {
            ack_count++;
        }
    }

    if (out_ack_link_count != NULL)
    {
        *out_ack_link_count = ack_count;
    }

    dual_channel_update_status(channel);

    if (ack_count > 0U)
    {
        return RTE_STATUS_OK;
    }
    return saw_hard_fault ? hard_fault_status : RTE_STATUS_TIMEOUT;
}

rte_status_t rte_dual_channel_send_state_frame(rte_dual_channel_t *channel, rte_dual_state_t state,
                                                  bool channel_degraded, uint64_t timestamp_ms)
{
    rte_dual_state_frame_t frame;
    uint32_t                i;
    uint32_t                sent_count = 0U;

    if (channel == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    frame.header.kind        = (uint8_t)RTE_DUAL_FRAME_KIND_STATE;
    frame.header.reserved[0] = 0U;
    frame.header.reserved[1] = 0U;
    frame.header.reserved[2] = 0U;
    frame.state              = (uint8_t)state;
    frame.channel_degraded   = channel_degraded ? 1U : 0U;
    frame.reserved           = 0U;
    frame.timestamp_ms       = timestamp_ms;

    for (i = 0U; i < channel->link_count; i++)
    {
        rte_status_t status = rte_dual_msgchannel_send(&channel->links[i], (const uint8_t *)&frame,
                                                          (uint8_t)sizeof(frame), channel->ack_timeout_ms, NULL);
        if (status == RTE_STATUS_OK)
        {
            sent_count++;
        }
    }

    return (sent_count > 0U) ? RTE_STATUS_OK : RTE_STATUS_TIMEOUT;
}

rte_status_t rte_dual_channel_receive_state_frame(rte_dual_channel_t *channel, rte_duration_ms_t timeout_ms,
                                                     rte_dual_state_frame_t *out_frame)
{
    if ((channel == NULL) || (out_frame == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    if (!channel->pending_state_valid)
    {
        uint32_t           i;
        rte_duration_ms_t per_link_timeout = (channel->link_count > 0U) ? (timeout_ms / channel->link_count) : 0U;

        /* Sweeps every link every call - see rte_dual_channel_receive()'s
         * own comment on why stopping early would starve other links. */
        for (i = 0U; i < channel->link_count; i++)
        {
            dual_poll_result_t result;

            (void)dual_channel_poll_link_once(channel, i, per_link_timeout, &result);
        }
    }

    if (!channel->pending_state_valid)
    {
        return RTE_STATUS_TIMEOUT;
    }

    *out_frame                    = channel->pending_state;
    channel->pending_state_valid = false;

    return RTE_STATUS_OK;
}

rte_dual_channel_status_t rte_dual_channel_get_status(const rte_dual_channel_t *channel)
{
    if (channel == NULL)
    {
        return RTE_DUAL_CHANNEL_STATUS_DOWN;
    }
    return channel->last_status;
}

bool rte_dual_channel_is_link_up(const rte_dual_channel_t *channel, uint32_t link_index)
{
    if ((channel == NULL) || (link_index >= channel->link_count))
    {
        return false;
    }
    return channel->link_up[link_index];
}

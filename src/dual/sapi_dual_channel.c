/**
 * @file sapi_dual_channel.c
 * @ingroup DUAL
 * @brief "DualChannel" layer of ADR-020 - see sapi_dual_channel.h.
 */
#include "safeapi/dual/sapi_dual_channel.h"

#include <string.h>

#include "safeapi/timer/sapi_timer.h"

/** @brief Cap on consecutive sapi_dual_channel_send() ACK-wait polls that
 *         may complete without sapi_timer_now() showing any measurable
 *         progress, before that link's wait loop gives up on this round -
 *         see that loop's own comment for why more than one such
 *         iteration is legitimate but an unbounded number is not. */
#define SAPI_DUAL_CHANNEL_STALL_POLL_LIMIT 32U

/** @brief Result of one dual_channel_poll_link_once() attempt. */
typedef struct
{
    bool                    matched;      /**< true if a recognized frame was received this attempt. */
    sapi_dual_frame_kind_t  kind;         /**< Valid only if matched. */
    uint32_t                ack_sequence; /**< Valid only if matched && kind == SAPI_DUAL_FRAME_KIND_ACK. */
} dual_poll_result_t;

static sapi_dual_channel_status_t dual_channel_compute_status(const sapi_dual_channel_t *channel)
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
        return SAPI_DUAL_CHANNEL_STATUS_DOWN;
    }
    if (up_count == channel->link_count)
    {
        return SAPI_DUAL_CHANNEL_STATUS_FULL;
    }
    return SAPI_DUAL_CHANNEL_STATUS_DEGRADED;
}

static void dual_channel_update_status(sapi_dual_channel_t *channel)
{
    sapi_dual_channel_status_t new_status = dual_channel_compute_status(channel);

    if (new_status != channel->last_status)
    {
        sapi_dual_channel_status_t old_status = channel->last_status;

        channel->last_status = new_status;
        if (channel->status_callback != NULL)
        {
            channel->status_callback(new_status, old_status, channel->status_callback_ctx);
        }
    }
}

/**
 * @brief One receive attempt on one link, dispatched by frame kind:
 *        DATA is auto-ACKed and staged for sapi_dual_channel_receive();
 *        STATE is staged for sapi_dual_channel_receive_state_frame();
 *        ACK is reported back to the caller (sapi_dual_channel_send()'s
 *        own wait loop) via *out_result, not staged anywhere.
 */
static sapi_status_t dual_channel_poll_link_once(sapi_dual_channel_t *channel, uint32_t link_index,
                                                  sapi_duration_ms_t timeout_ms, dual_poll_result_t *out_result)
{
    uint8_t                  raw[SAPI_DUAL_MSGCHANNEL_MAX_PAYLOAD];
    uint8_t                  raw_size = 0U;
    uint32_t                 sequence = 0U;
    sapi_status_t             status;
    sapi_dual_frame_header_t header;

    out_result->matched = false;

    status = sapi_dual_msgchannel_receive(&channel->links[link_index], raw, (uint8_t)sizeof(raw), timeout_ms,
                                           &raw_size, &sequence);
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }
    if (raw_size < (uint8_t)sizeof(header))
    {
        /* Too short to even hold this layer's own header - treat as a
         * defended-integrity failure, same family as a Layer-1 CRC/
         * sequence failure, not a silently-ignored malformed frame. */
        return SAPI_STATUS_DATA_CORRUPTION;
    }
    (void)memcpy(&header, raw, sizeof(header));

    switch ((sapi_dual_frame_kind_t)header.kind)
    {
        case SAPI_DUAL_FRAME_KIND_DATA:
        {
            uint8_t              app_size = (uint8_t)(raw_size - (uint8_t)sizeof(header));
            sapi_dual_ack_frame_t ack;

            if (app_size <= SAPI_DUAL_CHANNEL_MAX_PAYLOAD)
            {
                (void)memcpy(channel->pending_data, &raw[sizeof(header)], app_size);
                channel->pending_data_size  = app_size;
                channel->pending_data_valid = true;
            }

            /* Auto-ACK, best-effort/fire-and-forget: a lost ACK is
             * observable to the sender as its own timeout on this link
             * (ADR-020 section 2), not silently hidden here. */
            ack.header.kind        = (uint8_t)SAPI_DUAL_FRAME_KIND_ACK;
            ack.header.reserved[0] = 0U;
            ack.header.reserved[1] = 0U;
            ack.header.reserved[2] = 0U;
            ack.acked_sequence     = sequence;
            (void)sapi_dual_msgchannel_send(&channel->links[link_index], (const uint8_t *)&ack, (uint8_t)sizeof(ack),
                                             channel->ack_timeout_ms, NULL);

            out_result->matched = true;
            out_result->kind    = SAPI_DUAL_FRAME_KIND_DATA;
            break;
        }
        case SAPI_DUAL_FRAME_KIND_ACK:
        {
            sapi_dual_ack_frame_t ack;

            if (raw_size < (uint8_t)sizeof(ack))
            {
                return SAPI_STATUS_DATA_CORRUPTION;
            }
            (void)memcpy(&ack, raw, sizeof(ack));

            out_result->matched      = true;
            out_result->kind         = SAPI_DUAL_FRAME_KIND_ACK;
            out_result->ack_sequence = ack.acked_sequence;
            break;
        }
        case SAPI_DUAL_FRAME_KIND_STATE:
        {
            if (raw_size < (uint8_t)sizeof(sapi_dual_state_frame_t))
            {
                return SAPI_STATUS_DATA_CORRUPTION;
            }
            (void)memcpy(&channel->pending_state, raw, sizeof(channel->pending_state));
            channel->pending_state_valid = true;

            out_result->matched = true;
            out_result->kind    = SAPI_DUAL_FRAME_KIND_STATE;
            break;
        }
        default:
            /* Unrecognized kind - defensive only (not reachable through
             * a conforming sender), ignore rather than fail. */
            break;
    }

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_dual_channel_init(sapi_dual_channel_t *channel, const sapi_dual_channel_config_t *config)
{
    uint32_t i;

    if ((channel == NULL) || (config == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->link_count == 0U) || (config->link_count > SAPI_DUAL_CHANNEL_MAX_LINKS))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    for (i = 0U; i < config->link_count; i++)
    {
        if (config->links[i] == NULL)
        {
            return SAPI_STATUS_INVALID_PARAM;
        }
    }

    channel->link_count = config->link_count;
    for (i = 0U; i < config->link_count; i++)
    {
        sapi_dual_msgchannel_config_t msgcfg;

        msgcfg.link             = config->links[i];
        msgcfg.sender_id        = config->sender_id;
        msgcfg.expected_peer_id = config->expected_peer_id;
        (void)sapi_dual_msgchannel_init(&channel->links[i], &msgcfg);
        channel->link_up[i] = false;
    }
    for (i = config->link_count; i < SAPI_DUAL_CHANNEL_MAX_LINKS; i++)
    {
        channel->link_up[i] = false;
    }

    channel->ack_timeout_ms     = config->ack_timeout_ms;
    channel->status_callback    = config->status_callback;
    channel->status_callback_ctx = config->status_callback_ctx;
    channel->last_status        = SAPI_DUAL_CHANNEL_STATUS_DOWN;

    channel->pending_data_valid = false;
    channel->pending_data_size  = 0U;
    channel->pending_state_valid = false;
    (void)memset(&channel->pending_state, 0, sizeof(channel->pending_state));

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_dual_channel_send(sapi_dual_channel_t *channel, const uint8_t *payload, uint8_t payload_size,
                                      uint32_t *out_ack_link_count)
{
    uint8_t frame[(size_t)SAPI_DUAL_CHANNEL_MAX_PAYLOAD + sizeof(sapi_dual_frame_header_t)];
    uint8_t frame_size;
    uint32_t i;
    uint32_t ack_count = 0U;

    if (channel == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((payload == NULL) && (payload_size != 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (payload_size > SAPI_DUAL_CHANNEL_MAX_PAYLOAD)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    {
        sapi_dual_frame_header_t header;

        header.kind        = (uint8_t)SAPI_DUAL_FRAME_KIND_DATA;
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
        sapi_status_t send_status;
        bool          link_now_up = false;

        send_status = sapi_dual_msgchannel_send(&channel->links[i], frame, frame_size, channel->ack_timeout_ms,
                                                 &sent_sequence);
        if (send_status == SAPI_STATUS_OK)
        {
            sapi_duration_ms_t   remaining = channel->ack_timeout_ms;
            sapi_timestamp_ms_t  start_ms = 0U;
            /* Bounds how many consecutive polls may complete without
             * sapi_timer_now() showing any measurable progress since
             * start_ms, before this loop gives up on this link for this
             * round - see the "else" branch below for why this can
             * legitimately happen more than once and must not itself be
             * unbounded. SAPI_DUAL_CHANNEL_STALL_POLL_LIMIT is a fixed,
             * generous cap (a real exchange needs at most a handful of
             * iterations - one per DATA/STATE/foreign-ACK frame handled
             * as a side effect before this link's own matching ACK
             * arrives), not a tuned timing value. */
            uint32_t stall_polls = 0U;

            (void)sapi_timer_now(&start_ms);

            while ((remaining > 0U) && (stall_polls < SAPI_DUAL_CHANNEL_STALL_POLL_LIMIT))
            {
                dual_poll_result_t result;
                sapi_status_t       poll_status;
                sapi_timestamp_ms_t now_ms = 0U;

                poll_status = dual_channel_poll_link_once(channel, i, remaining, &result);
                if ((poll_status == SAPI_STATUS_OK) && result.matched && (result.kind == SAPI_DUAL_FRAME_KIND_ACK)
                    && (result.ack_sequence == sent_sequence))
                {
                    link_now_up = true;
                    break;
                }

                /* Recompute the remaining budget from wall-clock elapsed
                 * time, not a fixed per-iteration decrement - a DATA/STATE
                 * frame handled as a side effect above may have consumed
                 * an arbitrary fraction of this link's own timeout
                 * already. */
                (void)sapi_timer_now(&now_ms);
                if (now_ms > start_ms)
                {
                    sapi_timestamp_ms_t elapsed = now_ms - start_ms;

                    if (elapsed >= (sapi_timestamp_ms_t)channel->ack_timeout_ms)
                    {
                        remaining = 0U;
                    }
                    else
                    {
                        remaining = channel->ack_timeout_ms - (sapi_duration_ms_t)elapsed;
                    }
                    stall_polls = 0U;
                }
                else
                {
                    /* REQ-DUAL-CHANNEL-007: now_ms == start_ms - no
                     * measurable time has passed on sapi_timer_now()'s own
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
                     * live via SITE's migration to sapi_safechannel
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

    return (ack_count > 0U) ? SAPI_STATUS_OK : SAPI_STATUS_TIMEOUT;
}

sapi_status_t sapi_dual_channel_receive(sapi_dual_channel_t *channel, uint8_t *out_payload, uint8_t max_size,
                                         sapi_duration_ms_t timeout_ms, uint8_t *out_size)
{
    if ((channel == NULL) || (out_payload == NULL) || (out_size == NULL) || (max_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (!channel->pending_data_valid)
    {
        uint32_t           i;
        sapi_duration_ms_t per_link_timeout = (channel->link_count > 0U) ? (timeout_ms / channel->link_count) : 0U;

        /* Sweeps every configured link every call, even after an
         * earlier link in this same sweep already staged a DATA frame -
         * stopping early here would let a link with a shorter path (or
         * one that always has traffic) starve every other redundant
         * link of its own auto-ACK indefinitely (ADR-020 section 2: a
         * redundant link should degrade to DOWN only from genuinely not
         * responding, never from this module never getting around to
         * polling it). */
        for (i = 0U; i < channel->link_count; i++)
        {
            dual_poll_result_t result;

            (void)dual_channel_poll_link_once(channel, i, per_link_timeout, &result);
        }
    }

    if (!channel->pending_data_valid)
    {
        return SAPI_STATUS_TIMEOUT;
    }
    if (channel->pending_data_size > max_size)
    {
        /* Caller's buffer is too small for a staged frame - reported,
         * not silently truncated (safety-relevant data). */
        return SAPI_STATUS_INVALID_PARAM;
    }

    (void)memcpy(out_payload, channel->pending_data, channel->pending_data_size);
    *out_size                  = channel->pending_data_size;
    channel->pending_data_valid = false;

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_dual_channel_send_state_frame(sapi_dual_channel_t *channel, sapi_dual_state_t state,
                                                  bool channel_degraded, uint64_t timestamp_ms)
{
    sapi_dual_state_frame_t frame;
    uint32_t                i;
    uint32_t                sent_count = 0U;

    if (channel == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    frame.header.kind        = (uint8_t)SAPI_DUAL_FRAME_KIND_STATE;
    frame.header.reserved[0] = 0U;
    frame.header.reserved[1] = 0U;
    frame.header.reserved[2] = 0U;
    frame.state              = (uint8_t)state;
    frame.channel_degraded   = channel_degraded ? 1U : 0U;
    frame.reserved           = 0U;
    frame.timestamp_ms       = timestamp_ms;

    for (i = 0U; i < channel->link_count; i++)
    {
        sapi_status_t status = sapi_dual_msgchannel_send(&channel->links[i], (const uint8_t *)&frame,
                                                          (uint8_t)sizeof(frame), channel->ack_timeout_ms, NULL);
        if (status == SAPI_STATUS_OK)
        {
            sent_count++;
        }
    }

    return (sent_count > 0U) ? SAPI_STATUS_OK : SAPI_STATUS_TIMEOUT;
}

sapi_status_t sapi_dual_channel_receive_state_frame(sapi_dual_channel_t *channel, sapi_duration_ms_t timeout_ms,
                                                     sapi_dual_state_frame_t *out_frame)
{
    if ((channel == NULL) || (out_frame == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (!channel->pending_state_valid)
    {
        uint32_t           i;
        sapi_duration_ms_t per_link_timeout = (channel->link_count > 0U) ? (timeout_ms / channel->link_count) : 0U;

        /* Sweeps every link every call - see sapi_dual_channel_receive()'s
         * own comment on why stopping early would starve other links. */
        for (i = 0U; i < channel->link_count; i++)
        {
            dual_poll_result_t result;

            (void)dual_channel_poll_link_once(channel, i, per_link_timeout, &result);
        }
    }

    if (!channel->pending_state_valid)
    {
        return SAPI_STATUS_TIMEOUT;
    }

    *out_frame                    = channel->pending_state;
    channel->pending_state_valid = false;

    return SAPI_STATUS_OK;
}

sapi_dual_channel_status_t sapi_dual_channel_get_status(const sapi_dual_channel_t *channel)
{
    if (channel == NULL)
    {
        return SAPI_DUAL_CHANNEL_STATUS_DOWN;
    }
    return channel->last_status;
}

bool sapi_dual_channel_is_link_up(const sapi_dual_channel_t *channel, uint32_t link_index)
{
    if ((channel == NULL) || (link_index >= channel->link_count))
    {
        return false;
    }
    return channel->link_up[link_index];
}

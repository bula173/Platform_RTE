/**
 * @file rte_dual_negotiator.c
 * @ingroup DUAL
 * @brief Dual state negotiator of ADR-020 - see rte_dual_negotiator.h.
 */
#include "rte/redundancy/dual/rte_dual_negotiator.h"

#include "rte/oal/timer/rte_timer.h"

/**
 * @brief Older-timestamp-wins tie-break, own_id/peer_id as the
 *        deterministic fallback on an exact tie - same rule
 *        safeAPIRBC2oo2's site.c decide_online() uses today.
 */
static bool negotiator_decide_online(uint32_t own_id, uint64_t own_ts, uint32_t peer_id, uint64_t peer_ts)
{
    if (own_ts != peer_ts)
    {
        return own_ts < peer_ts;
    }
    return own_id < peer_id;
}

static void negotiator_process_peer_frame(rte_dual_negotiator_t *negotiator, const rte_dual_state_frame_t *frame)
{
    rte_timestamp_ms_t now_ms = 0U;

    negotiator->peer_channel_degraded         = (frame->channel_degraded != 0U);
    negotiator->peer_startup_timestamp_ms     = frame->timestamp_ms;
    negotiator->have_peer_startup_timestamp   = true;

    (void)rte_timer_now(&now_ms);
    negotiator->last_peer_seen_ms  = now_ms;
    negotiator->have_last_peer_seen = true;
}

static void negotiator_update_states(rte_dual_negotiator_t *negotiator)
{
    rte_dual_state_t old_own  = negotiator->own_state;
    rte_dual_state_t old_peer = negotiator->peer_state;
    bool               peer_contact_recent = false;

    if (negotiator->have_last_peer_seen)
    {
        rte_timestamp_ms_t now_ms = 0U;

        (void)rte_timer_now(&now_ms);
        if (now_ms >= negotiator->last_peer_seen_ms)
        {
            rte_timestamp_ms_t elapsed = now_ms - negotiator->last_peer_seen_ms;

            peer_contact_recent = (elapsed < (rte_timestamp_ms_t)negotiator->peer_lost_timeout_ms);
        }
        /* else: clock moved backwards - no monotonic guarantee assumed
         * (see rte_clocksync.h's own file-level note) - stay
         * conservative (peer_contact_recent already false) rather than
         * trust a negative interval. */
    }

    if (!negotiator->have_last_peer_seen)
    {
        /* Never yet heard from the peer at all. */
        negotiator->peer_state = RTE_DUAL_STATE_IDLE;
        /* own_state stays whatever it already was (IDLE, normally). */
    }
    else if (!peer_contact_recent)
    {
        negotiator->peer_state = RTE_DUAL_STATE_UNKNOWN;
        if (negotiator->own_state != RTE_DUAL_STATE_ONLINE)
        {
            /* IDLE, HOTSTANDBY, or COLDSTANDBY all degrade to UNKNOWN on
             * lost peer contact - see this file's header doc for why
             * ONLINE is the one exception. */
            negotiator->own_state = RTE_DUAL_STATE_UNKNOWN;
        }
    }
    else if ((negotiator->own_state == RTE_DUAL_STATE_IDLE) || (negotiator->own_state == RTE_DUAL_STATE_UNKNOWN))
    {
        bool decided_online = negotiator_decide_online(negotiator->own_id, negotiator->own_startup_timestamp_ms,
                                                         negotiator->peer_id, negotiator->peer_startup_timestamp_ms);

        if (decided_online)
        {
            /* We are ONLINE, they are STANDBY - their HOT/COLD depends
             * on OUR OWN degradation (they are backing US up), not on
             * whatever they themselves last reported about their own
             * channel - see rte_dual_negotiator_execute()'s own doc. */
            negotiator->own_state  = RTE_DUAL_STATE_ONLINE;
            negotiator->peer_state =
                negotiator->own_channel_degraded ? RTE_DUAL_STATE_COLDSTANDBY : RTE_DUAL_STATE_HOTSTANDBY;
        }
        else
        {
            /* We are STANDBY, they are ONLINE - OUR OWN HOT/COLD depends
             * on THEIR degradation (we are backing THEM up). */
            negotiator->own_state = negotiator->peer_channel_degraded ? RTE_DUAL_STATE_COLDSTANDBY
                                                                       : RTE_DUAL_STATE_HOTSTANDBY;
            negotiator->peer_state = RTE_DUAL_STATE_ONLINE;
        }
    }
    else if (negotiator->own_state == RTE_DUAL_STATE_ONLINE)
    {
        /* Same "their HOT/COLD depends on OUR degradation" rule as the
         * initial decision above, refreshed each round. */
        negotiator->peer_state =
            negotiator->own_channel_degraded ? RTE_DUAL_STATE_COLDSTANDBY : RTE_DUAL_STATE_HOTSTANDBY;
        /* own_state stays ONLINE. */
    }
    else
    {
        /* own_state is already HOTSTANDBY or COLDSTANDBY: refine from
         * the peer's latest channel_degraded bit; peer_state stays
         * ONLINE (already decided). */
        negotiator->own_state =
            negotiator->peer_channel_degraded ? RTE_DUAL_STATE_COLDSTANDBY : RTE_DUAL_STATE_HOTSTANDBY;
        negotiator->peer_state = RTE_DUAL_STATE_ONLINE;
    }

    if (((negotiator->own_state != old_own) || (negotiator->peer_state != old_peer))
        && (negotiator->state_change_callback != NULL))
    {
        negotiator->state_change_callback(negotiator->own_state, old_own, negotiator->peer_state, old_peer,
                                           negotiator->state_change_callback_ctx);
    }
}

rte_status_t rte_dual_negotiator_init(rte_dual_negotiator_t *negotiator,
                                         const rte_dual_negotiator_config_t *config)
{
    rte_timestamp_ms_t now_ms = 0U;

    if ((negotiator == NULL) || (config == NULL) || (config->channel == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    negotiator->channel               = config->channel;
    negotiator->own_id                = config->own_id;
    negotiator->peer_id               = config->peer_id;
    negotiator->peer_lost_timeout_ms  = config->peer_lost_timeout_ms;
    negotiator->state_change_callback = config->state_change_callback;
    negotiator->state_change_callback_ctx = config->state_change_callback_ctx;

    (void)rte_timer_now(&now_ms);
    negotiator->own_startup_timestamp_ms =
        (config->resume_own_startup_timestamp_ms != 0U) ? config->resume_own_startup_timestamp_ms : now_ms;
    negotiator->own_channel_degraded     = false;

    negotiator->have_peer_startup_timestamp = false;
    negotiator->peer_startup_timestamp_ms   = 0U;
    negotiator->peer_channel_degraded       = false;

    negotiator->own_state  = config->resume_own_state; /* RTE_DUAL_STATE_IDLE (0) unless a resume was requested */
    negotiator->peer_state = RTE_DUAL_STATE_IDLE; /* always fresh - see this field's own config doc */

    negotiator->have_last_peer_seen = false;
    negotiator->last_peer_seen_ms   = 0U;

    return RTE_STATUS_OK;
}

rte_status_t rte_dual_negotiator_execute(rte_dual_negotiator_t *negotiator, rte_duration_ms_t receive_timeout_ms)
{
    rte_dual_state_frame_t frame;
    rte_status_t             status;

    if (negotiator == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    negotiator->own_channel_degraded =
        (rte_dual_channel_get_status(negotiator->channel) != RTE_DUAL_CHANNEL_STATUS_FULL);

    (void)rte_dual_channel_send_state_frame(negotiator->channel, negotiator->own_state,
                                              negotiator->own_channel_degraded,
                                              negotiator->own_startup_timestamp_ms);

    status = rte_dual_channel_receive_state_frame(negotiator->channel, receive_timeout_ms, &frame);
    if (status == RTE_STATUS_OK)
    {
        negotiator_process_peer_frame(negotiator, &frame);
    }
    /* Drain any additional already-buffered frames non-blockingly, so a
     * burst arriving in one round doesn't leave stale ones unread past
     * this call. */
    while (rte_dual_channel_receive_state_frame(negotiator->channel, 0U, &frame) == RTE_STATUS_OK)
    {
        negotiator_process_peer_frame(negotiator, &frame);
    }

    negotiator_update_states(negotiator);

    return RTE_STATUS_OK;
}

rte_dual_state_t rte_dual_negotiator_get_own_state(const rte_dual_negotiator_t *negotiator)
{
    if (negotiator == NULL)
    {
        return RTE_DUAL_STATE_IDLE;
    }
    return negotiator->own_state;
}

rte_dual_state_t rte_dual_negotiator_get_peer_state(const rte_dual_negotiator_t *negotiator)
{
    if (negotiator == NULL)
    {
        return RTE_DUAL_STATE_IDLE;
    }
    return negotiator->peer_state;
}

uint64_t rte_dual_negotiator_get_own_startup_timestamp_ms(const rte_dual_negotiator_t *negotiator)
{
    if (negotiator == NULL)
    {
        return 0U;
    }
    return negotiator->own_startup_timestamp_ms;
}

uint64_t rte_dual_negotiator_get_peer_startup_timestamp_ms(const rte_dual_negotiator_t *negotiator)
{
    if (negotiator == NULL)
    {
        return 0U;
    }
    return negotiator->peer_startup_timestamp_ms;
}

uint32_t rte_dual_negotiator_get_own_id(const rte_dual_negotiator_t *negotiator)
{
    if (negotiator == NULL)
    {
        return 0U;
    }
    return negotiator->own_id;
}

uint32_t rte_dual_negotiator_get_peer_id(const rte_dual_negotiator_t *negotiator)
{
    if (negotiator == NULL)
    {
        return 0U;
    }
    return negotiator->peer_id;
}

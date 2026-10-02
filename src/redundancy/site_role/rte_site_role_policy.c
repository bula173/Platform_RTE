/**
 * @file rte_site_role_policy.c
 * @brief See rte_site_role_policy.h. Moved unchanged from RBC_GP's ab_gp_site_role.c and the generic part of
 *        ab_gp_standby_policy.c (ADR-040 step 1); only the names changed.
 */
#include "rte/redundancy/site_role/rte_site_role_policy.h"

/** The counterpart answered within the last 2 cycles. */
static bool peer_answering(const rte_site_role_input_t *in)
{
    return (in->cycle >= in->peer_last_rx_cycle) && ((in->cycle - in->peer_last_rx_cycle) <= 2U);
}

/** The A/B pair may promote: this channel is ready and its sibling is ONLINE, ready, or not there. */
static bool pair_ready_to_promote(const rte_site_role_input_t *in)
{
    return in->own_ready && (in->sibling_online || in->sibling_ready || !in->sibling_reachable);
}

rte_site_role_decision_t rte_site_role_decide(const rte_site_role_input_t *in)
{
    rte_site_role_decision_t d;

    d.action = RTE_SITE_ROLE_ACTION_NONE;
    d.reason = RTE_SITE_ROLE_REASON_NONE;
    d.both_standby_cycles = 0U;

    if (in->own_faulted && in->peer_faulted)
    {
        d.action = RTE_SITE_ROLE_ACTION_RENEGOTIATE;
        d.reason = RTE_SITE_ROLE_REASON_BOTH_FAULTED;
    }
    else if (!in->own_faulted && !in->is_online && in->peer_faulted)
    {
        d.action = RTE_SITE_ROLE_ACTION_PROMOTE;
        d.reason = RTE_SITE_ROLE_REASON_PEER_FAULTED;
    }
    else if (in->own_faulted && in->peer_snapshot_valid && !in->peer_faulted)
    {
        d.action = RTE_SITE_ROLE_ACTION_DEMOTE;
        d.reason = RTE_SITE_ROLE_REASON_RECOVERED;
    }
    else if (rte_site_role_takeover_allowed(in->is_online, in->own_faulted, in->peer_unresponsive, in->takeover_confirmed,
                                    in->cycle, in->takeover_confirm_cycle))
    {
        d.action = RTE_SITE_ROLE_ACTION_PROMOTE;
        d.reason = RTE_SITE_ROLE_REASON_DISPATCHER_CONFIRMED;
    }
    else if (in->is_online && !in->own_faulted && !in->peer_faulted && in->peer_online_now && peer_answering(in))
    {
        if (in->lost_peer_while_online)
        {
            d.action = RTE_SITE_ROLE_ACTION_DEMOTE;
            d.reason = RTE_SITE_ROLE_REASON_YIELD_TO_TAKEOVER;
        }
        else
        {
            d.action = RTE_SITE_ROLE_ACTION_ALARM;
            d.reason = RTE_SITE_ROLE_REASON_DUAL_ONLINE;
        }
    }
    else if (!in->is_online && !in->own_faulted && in->link_up && !in->peer_unresponsive && peer_answering(in) &&
             !in->peer_online_now && !in->peer_faulted)
    {
        d.both_standby_cycles = in->both_standby_cycles + 1U;
        if (rte_site_role_both_standby_tiebreak(d.both_standby_cycles, in->is_west, in->sibling_online))
        {
            d.action = RTE_SITE_ROLE_ACTION_PROMOTE;
            d.reason = in->sibling_online ? RTE_SITE_ROLE_REASON_BOTH_STANDBY_SIBLING : RTE_SITE_ROLE_REASON_BOTH_STANDBY_WEST;
            d.both_standby_cycles = 0U;
        }
    }
    else if (!in->is_online && !in->own_faulted && in->sibling_online && !in->peer_faulted &&
             !(in->peer_online_now && peer_answering(in)))
    {
        /* The A/B sibling is ONLINE but this channel's own link view is unusable (link flapping, counterpart silent):
         * the two channels of a site must agree, so follow the sibling once it has been ONLINE for 2 cycles, unless
         * the counterpart is verifiably ONLINE right now (then the disagreement is real and the pair must reboot). */
        d.both_standby_cycles = in->both_standby_cycles + 1U;
        if (d.both_standby_cycles >= 2U)
        {
            d.action = RTE_SITE_ROLE_ACTION_PROMOTE;
            d.reason = RTE_SITE_ROLE_REASON_BOTH_STANDBY_SIBLING;
            d.both_standby_cycles = 0U;
        }
    }
    else
    {
        /* nothing applies: the counter stays at 0 (it counts consecutive cycles only) */
    }

    if ((d.action == RTE_SITE_ROLE_ACTION_PROMOTE) && !pair_ready_to_promote(in))
    {
        d.action = RTE_SITE_ROLE_ACTION_WAIT;
        d.reason = RTE_SITE_ROLE_REASON_WAITING_READY;
        d.both_standby_cycles = in->both_standby_cycles + 1U; /* keep the tie-break condition counting */
    }
    return d;
}

const char *rte_site_role_reason_text(rte_site_role_reason_t reason)
{
    const char *text = "none";

    switch (reason)
    {
    case RTE_SITE_ROLE_REASON_BOTH_FAULTED:
        text = "both sites FAULTED, WEST wins";
        break;
    case RTE_SITE_ROLE_REASON_PEER_FAULTED:
        text = "counterpart FAULTED";
        break;
    case RTE_SITE_ROLE_REASON_RECOVERED:
        text = "was FAULTED, counterpart healthy ONLINE";
        break;
    case RTE_SITE_ROLE_REASON_DISPATCHER_CONFIRMED:
        text = "dispatcher confirmed the other site is not ONLINE";
        break;
    case RTE_SITE_ROLE_REASON_BOTH_STANDBY_WEST:
        text = "negotiation - both sites STANDBY, WEST wins the tie";
        break;
    case RTE_SITE_ROLE_REASON_BOTH_STANDBY_SIBLING:
        text = "negotiation - both sites STANDBY, joining the ONLINE A/B sibling";
        break;
    case RTE_SITE_ROLE_REASON_YIELD_TO_TAKEOVER:
        text = "counterpart took over while this channel was cut off";
        break;
    case RTE_SITE_ROLE_REASON_DUAL_ONLINE:
        text = "both sites ONLINE without a lost-contact history";
        break;
    case RTE_SITE_ROLE_REASON_WAITING_READY:
        text = "promotion held back until both A/B channels are ready";
        break;
    case RTE_SITE_ROLE_REASON_NONE:
    default:
        break;
    }
    return text;
}

const char *rte_site_role_reason_token(rte_site_role_reason_t reason)
{
    const char *token = "none";

    switch (reason)
    {
    case RTE_SITE_ROLE_REASON_BOTH_FAULTED:
        token = "both_faulted";
        break;
    case RTE_SITE_ROLE_REASON_PEER_FAULTED:
        token = "peer_faulted";
        break;
    case RTE_SITE_ROLE_REASON_RECOVERED:
        token = "recovered";
        break;
    case RTE_SITE_ROLE_REASON_DISPATCHER_CONFIRMED:
        token = "dispatcher";
        break;
    case RTE_SITE_ROLE_REASON_BOTH_STANDBY_WEST:
        token = "both_standby_west";
        break;
    case RTE_SITE_ROLE_REASON_BOTH_STANDBY_SIBLING:
        token = "both_standby_sibling";
        break;
    case RTE_SITE_ROLE_REASON_YIELD_TO_TAKEOVER:
        token = "yield";
        break;
    case RTE_SITE_ROLE_REASON_DUAL_ONLINE:
        token = "dual_online";
        break;
    case RTE_SITE_ROLE_REASON_WAITING_READY:
        token = "waiting_ready";
        break;
    case RTE_SITE_ROLE_REASON_NONE:
    default:
        break;
    }
    return token;
}

bool rte_site_role_standby_sync_due(bool sync_enabled, bool is_online, bool link_up, bool healthy, uint32_t cycle,
                                    uint32_t last_try)
{
    bool due = false;

    if (sync_enabled && is_online && link_up)
    {
        if (healthy)
        {
            due = true;
        }
        else
        {
            /* Unsigned subtraction is wrap-safe as long as cycle >= last_try; a cycle counter that
             * restarted (resync) makes last_try > cycle, which retries immediately - the safe side. */
            due = (cycle < last_try) || ((cycle - last_try) >= RTE_SITE_ROLE_STANDBY_SYNC_RETRY_CYCLES);
        }
    }
    return due;
}

bool rte_site_role_standby_data_stale(bool sync_enabled, bool is_online, bool have_snapshot, uint32_t cycle,
                                      uint32_t last_rx_cycle)
{
    bool stale = false;

    if (sync_enabled && !is_online && have_snapshot)
    {
        /* A restarted cycle counter (cycle < last_rx_cycle) is treated as stale: the numbering the
         * snapshot was stamped against is gone. */
        stale = (cycle < last_rx_cycle) || ((cycle - last_rx_cycle) > RTE_SITE_ROLE_STANDBY_DATA_TIMEOUT_CYCLES);
    }
    return stale;
}

bool rte_site_role_takeover_allowed(bool is_online, bool own_faulted, bool peer_unresponsive, bool confirmed,
                                    uint32_t cycle, uint32_t confirm_cycle)
{
    bool allowed = false;

    if (!is_online && !own_faulted && peer_unresponsive && confirmed)
    {
        /* A restarted cycle counter (cycle < confirm_cycle) makes the stamp meaningless: expired. */
        allowed = (cycle >= confirm_cycle) && ((cycle - confirm_cycle) <= RTE_SITE_ROLE_TAKEOVER_CONFIRM_VALID_CYCLES);
    }
    return allowed;
}

bool rte_site_role_both_standby_tiebreak(uint32_t both_standby_cycles, bool is_west, bool sibling_online)
{
    return (sibling_online && (both_standby_cycles >= 1U)) || (is_west && (both_standby_cycles >= RTE_SITE_ROLE_BOTH_STANDBY_CYCLES));
}

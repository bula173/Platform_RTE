/**
 * @file rte_site_role.c
 * @brief See rte_site_role.h. Moved from RBC_GP's ab_gp_channel_negotiate.c (ADR-040 step 2): same link handling,
 *        same frame layout, same decisions, same log text; the application-specific parts (state codec, what a
 *        promotion takes over, readiness) are the Site Role Client's callbacks and local facts.
 *
 * Design note kept from the original: the negotiator (rte_dual_negotiator, ADR-020) settles the startup role and is
 * logged every cycle as a diagnostic of the link's health. After startup the role is driven only by
 * rte_site_role_decide() over the facts gathered here; the negotiator's own state can legitimately differ from the
 * role after a FAULTED-driven promotion, because it has no view of that override.
 */
#include "rte/redundancy/site_role/rte_site_role.h"

#include <stdio.h>

#include "rte/oal/log/rte_log.h"
#include "rte/oal/memory/rte_mem_util.h"
#include "rte/oal/random/rte_random.h"
#include "rte/oal/timer/rte_timer.h"
#include "rte/utils/buffer/rte_buffer.h"

#define SITE_ROLE_TRACE_LINE_MAX 256U
#define SITE_ROLE_SERVICE_DRAIN 4U

/* ---- logging -------------------------------------------------------------------------------------------------- */

static void trace_line(const rte_site_role_t *rs, const char *line)
{
    if (rs->cfg.trace != NULL)
    {
        rs->cfg.trace(rs->cfg.user, line);
    }
}

static void log_event(const rte_site_role_t *rs, rte_log_level_t level, uint32_t cycle, const char *source,
                      const char *destination, const char *type, const char *info, const char *extra)
{
    rte_log_write_event(level, rs->cfg.site_name, cycle, source, destination, type, info, extra);
}

/* ---- frame codec ------------------------------------------------------------------------------------------------ */

static size_t state_size(const rte_site_role_t *rs)
{
    return rs->cfg.frame_size - RTE_SITE_ROLE_FRAME_OVERHEAD;
}

static void encode_frame(rte_site_role_t *rs, bool faulted, bool single_mode, uint32_t cycle, bool live)
{
    uint8_t *out = rs->cfg.tx_buf;
    rte_buffer_t buf;

    (void)rte_buffer_init(&buf, out, rs->cfg.frame_size);
    out[0] = faulted ? 1U : 0U;
    out[1] = single_mode ? (uint8_t)RTE_SITE_ROLE_SINGLE_MODE_SINGLE : (uint8_t)RTE_SITE_ROLE_SINGLE_MODE_DUAL;
    (void)rte_buffer_set_length(&buf, 2U);
    (void)rte_buffer_write_u32_le(&buf, live ? cycle : 0U);
    /* Zeroed before encoding, so the region never carries stale bytes. */
    rte_mem_set(&out[RTE_SITE_ROLE_FRAME_STATE_OFFSET], 0, state_size(rs));
    (void)rs->cfg.encode_state(rs->cfg.user, &out[RTE_SITE_ROLE_FRAME_STATE_OFFSET], state_size(rs), live);
    out[rs->cfg.frame_size - 1U] = live ? (uint8_t)RTE_SITE_ROLE_VALID_MARKER : 0U;
}

/* ---- role changes ----------------------------------------------------------------------------------------------- */

static void report_change(rte_site_role_t *rs, rte_site_role_change_kind_t kind, rte_site_role_reason_t reason,
                          bool apply_state)
{
    rte_site_role_change_t change;

    change.kind = kind;
    change.reason = reason;
    change.online = rs->online;
    change.apply_state = apply_state;
    change.snapshot_valid = rs->peer_snapshot_valid;
    change.snapshot_cycle = rs->peer_snapshot_cycle;
    rs->cfg.on_change(rs->cfg.user, &change);
    if (apply_state && rs->peer_snapshot_valid)
    {
        /* The application renumbers its cycle from the snapshot: every "last seen at cycle N" stamp refers to the old
         * numbering. Left as it is, a stamp from just before the renumbering looked like a fresh one against the new,
         * lower cycle number: a silent counterpart then "answered" and a dispatcher-confirmed takeover raised a
         * dual-ONLINE alarm two cycles later (Docker takeover test). */
        rs->peer_last_rx_cycle = 0U;
        rs->standby_rx_cycle = 0U;
        rs->standby_last_try_cycle = 0U;
    }
}

static void promote_to_online(rte_site_role_t *rs, const char *message, rte_site_role_reason_t reason)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    rs->online = true;
    rs->peer_unresponsive = false;
    rs->takeover_confirmed = false;
    (void)snprintf(line, sizeof(line), "[%s] *** TAKEOVER: %s, this channel promotes itself to ONLINE ***\n",
                   rs->cfg.role_tag, message);
    trace_line(rs, line);
    log_event(rs, RTE_LOG_LEVEL_WARNING, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "ROLE", message, NULL);
    report_change(rs, RTE_SITE_ROLE_CHANGE_PROMOTED, reason, true);
}

static void demote_to_standby(rte_site_role_t *rs, const char *message, rte_site_role_reason_t reason)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    rs->online = false;
    rs->lost_peer_while_online = false;
    (void)snprintf(line, sizeof(line), "[%s] *** %s - settling into STANDBY ***\n", rs->cfg.role_tag, message);
    trace_line(rs, line);
    log_event(rs, RTE_LOG_LEVEL_WARNING, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "ROLE", message, NULL);
    report_change(rs, RTE_SITE_ROLE_CHANGE_DEMOTED, reason, false);
}

/** Both sites FAULTED: the same deterministic, symmetric rule on both sides - WEST ONLINE, EAST STANDBY. */
static void renegotiate_from_deadlock(rte_site_role_t *rs, rte_site_role_reason_t reason)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    rs->online = rs->cfg.is_west;
    (void)snprintf(line, sizeof(line), "[%s] *** RE-NEGOTIATING: both sides FAULTED -> %s ***\n", rs->cfg.role_tag,
                   rs->online ? "ONLINE" : "STANDBY");
    trace_line(rs, line);
    log_event(rs, RTE_LOG_LEVEL_WARNING, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "NEGOTIATION",
              rs->online ? "RE-NEGOTIATED (both FAULTED): ONLINE" : "RE-NEGOTIATED (both FAULTED): STANDBY", NULL);
    report_change(rs, RTE_SITE_ROLE_CHANGE_RENEGOTIATED, reason, rs->online);
}

/* ---- link ------------------------------------------------------------------------------------------------------- */

static void on_negotiator_state_change(rte_dual_state_t new_own_state, rte_dual_state_t old_own_state,
                                       rte_dual_state_t new_peer_state, rte_dual_state_t old_peer_state,
                                       void *user_ctx)
{
    const rte_site_role_t *rs = (const rte_site_role_t *)user_ctx;
    char line[SITE_ROLE_TRACE_LINE_MAX];
    char extra[96];
    const char *old_own_str = rte_dual_state_to_string(old_own_state);
    const char *new_own_str = rte_dual_state_to_string(new_own_state);
    const char *old_peer_str = rte_dual_state_to_string(old_peer_state);
    const char *new_peer_str = rte_dual_state_to_string(new_peer_state);

    (void)snprintf(line, sizeof(line), "[%s] negotiator state change: own %s->%s, peer %s->%s (own_ts=%llu, peer_ts=%llu)\n",
                   rs->cfg.role_tag, old_own_str, new_own_str, old_peer_str, new_peer_str,
                   (unsigned long long)rte_dual_negotiator_get_own_startup_timestamp_ms(&rs->negotiator),
                   (unsigned long long)rte_dual_negotiator_get_peer_startup_timestamp_ms(&rs->negotiator));
    trace_line(rs, line);
    (void)snprintf(extra, sizeof(extra), "own:%s->%s peer:%s->%s", old_own_str, new_own_str, old_peer_str, new_peer_str);
    log_event(rs, RTE_LOG_LEVEL_INFO, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "NEGOTIATOR", "state change", extra);
}

static rte_status_t open_link_once(rte_site_role_t *rs, rte_duration_ms_t timeout_ms)
{
    rte_netlink_config_t link_cfg;

    rte_mem_set(&link_cfg, 0, sizeof(link_cfg));
    link_cfg.role = rs->cfg.listen ? RTE_NETLINK_ROLE_LISTEN : RTE_NETLINK_ROLE_CONNECT;
    link_cfg.host = rs->cfg.listen ? NULL : rs->cfg.peer_host;
    link_cfg.port = rs->cfg.port;
    link_cfg.message_size = rs->cfg.link_message_size; /* required by rte_dual_channel_init() */
    link_cfg.connect_timeout_ms = timeout_ms;
    return rte_netlink_open(&rs->link_storage, &link_cfg, &rs->link);
}

/** Startup: the listen side opens once; the connect side retries until startup_timeout_ms (container start order is
 *  not guaranteed). */
static rte_status_t open_link_blocking(rte_site_role_t *rs)
{
    rte_status_t status;
    uint32_t waited_ms = 0U;

    if (rs->cfg.listen)
    {
        return open_link_once(rs, rs->cfg.startup_timeout_ms);
    }
    status = RTE_STATUS_TIMEOUT;
    while (waited_ms < (uint32_t)rs->cfg.startup_timeout_ms)
    {
        status = open_link_once(rs, rs->cfg.connect_timeout_ms);
        if (status == RTE_STATUS_OK)
        {
            break;
        }
        if (rs->cfg.delay != NULL)
        {
            rs->cfg.delay(rs->cfg.user, rs->cfg.connect_retry_ms);
        }
        waited_ms += rs->cfg.connect_retry_ms;
        status = RTE_STATUS_TIMEOUT;
    }
    return status;
}

/** (Re)initializes the dual channel and the negotiator over the open link. A reconnect resumes this channel's own
 *  identity (startup timestamp, and ONLINE if it is ONLINE) instead of re-litigating the startup tie-break. */
static rte_status_t init_channel_and_negotiator(rte_site_role_t *rs)
{
    rte_dual_channel_config_t dual_cfg;
    rte_dual_negotiator_config_t negotiator_cfg;
    rte_status_t status;

    rte_mem_set(&dual_cfg, 0, sizeof(dual_cfg));
    dual_cfg.links[0] = rs->link;
    dual_cfg.link_count = 1U;
    dual_cfg.sender_id = rs->cfg.own_id;
    dual_cfg.expected_peer_id = rs->cfg.peer_id;
    dual_cfg.ack_timeout_ms = rs->cfg.ack_timeout_ms;
    status = rte_dual_channel_init(&rs->dual_channel, &dual_cfg);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    /* The counterpart restarts on its own: its frames start at sequence 0 again while this end keeps expecting the
     * old number; without a resync both ends rejected each other for good. */
    (void)rte_dual_channel_set_resync_on_sequence_error(&rs->dual_channel, true);
    rs->send_miss = 0U;

    rte_mem_set(&negotiator_cfg, 0, sizeof(negotiator_cfg));
    negotiator_cfg.channel = &rs->dual_channel;
    negotiator_cfg.own_id = rs->cfg.own_id;
    negotiator_cfg.peer_id = rs->cfg.peer_id;
    negotiator_cfg.peer_lost_timeout_ms = rs->cfg.peer_lost_timeout_ms;
    negotiator_cfg.state_change_callback = on_negotiator_state_change;
    negotiator_cfg.state_change_callback_ctx = rs;
    negotiator_cfg.resume_own_startup_timestamp_ms = rs->startup_timestamp_ms;
    if (rs->online)
    {
        negotiator_cfg.resume_own_state = RTE_DUAL_STATE_ONLINE;
    }
    status = rte_dual_negotiator_init(&rs->negotiator, &negotiator_cfg);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    if (rs->startup_timestamp_ms == 0U)
    {
        rs->startup_timestamp_ms = rte_dual_negotiator_get_own_startup_timestamp_ms(&rs->negotiator);
    }
    return RTE_STATUS_OK;
}

static void close_link(rte_site_role_t *rs)
{
    if (rs->link != NULL)
    {
        (void)rte_netlink_close(rs->link);
        rs->link = NULL;
    }
}

/** One bounded reconnect attempt per cycle on the cyclic executive's own thread (REQ-SITEROLE-012): with a silent
 *  counterpart a longer bound stretched every cycle of the site, also the ONLINE one. */
static rte_status_t reconnect(rte_site_role_t *rs)
{
    rte_status_t status = open_link_once(rs, rs->cfg.reconnect_attempt_ms);
    char line[SITE_ROLE_TRACE_LINE_MAX];

    if (status != RTE_STATUS_OK)
    {
        return status;
    }
    status = init_channel_and_negotiator(rs);
    if (status != RTE_STATUS_OK)
    {
        close_link(rs);
        return status;
    }
    (void)snprintf(line, sizeof(line), "[%s] inter-site negotiation link (re)established\n", rs->cfg.role_tag);
    trace_line(rs, line);
    log_event(rs, RTE_LOG_LEVEL_INFO, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "NEGOTIATION", "link (re)established",
              NULL);
    return RTE_STATUS_OK;
}

/* ---- per cycle -------------------------------------------------------------------------------------------------- */

static bool send_frame(rte_site_role_t *rs, uint32_t cycle)
{
    rte_status_t status;
    char line[SITE_ROLE_TRACE_LINE_MAX];

    encode_frame(rs, rs->local_faulted, rs->local_single_mode, rs->cycle, rs->online);
    status = rte_dual_channel_send(&rs->dual_channel, rs->cfg.tx_buf, (uint8_t)rs->cfg.frame_size, NULL);
    if (status != RTE_STATUS_OK)
    {
        /* Not fatal: the counterpart may still have a frame queued for us (e.g. its own final FAULTED flush), and
         * that frame can be exactly the promotion signal this channel is waiting for. */
        (void)snprintf(line, sizeof(line), "[%s] cycle %u: negotiation extra payload send failed (%s)\n",
                       rs->cfg.role_tag, (unsigned int)cycle, rte_status_to_string(status));
        trace_line(rs, line);
    }
    return status == RTE_STATUS_OK;
}

static rte_status_t receive_frame(rte_site_role_t *rs, uint32_t cycle, bool quiet)
{
    uint8_t received_size = 0U;
    rte_status_t status;
    char line[SITE_ROLE_TRACE_LINE_MAX];

    status = rte_dual_channel_receive(&rs->dual_channel, rs->cfg.rx_buf, (uint8_t)rs->cfg.frame_size, 0U,
                                      &received_size);
    if (status == RTE_STATUS_OK)
    {
        const uint8_t *in = rs->cfg.rx_buf;
        rte_buffer_t buf;
        uint32_t peer_cycle = 0U;
        const bool valid = (in[rs->cfg.frame_size - 1U] == (uint8_t)RTE_SITE_ROLE_VALID_MARKER);
        bool decoded = false;

        if (rte_buffer_init(&buf, rs->cfg.rx_buf, rs->cfg.frame_size) == RTE_STATUS_OK)
        {
            (void)rte_buffer_set_length(&buf, rs->cfg.frame_size);
            decoded = (rte_buffer_read_u32_le(&buf, 2U, &peer_cycle) == RTE_STATUS_OK) &&
                      rs->cfg.receive_state(rs->cfg.user, &in[RTE_SITE_ROLE_FRAME_STATE_OFFSET], state_size(rs),
                                            valid);
        }
        if (decoded)
        {
            const bool peer_faulted = (in[0] != 0U);

            rs->peer_faulted = peer_faulted;
            rs->peer_single_mode = in[1];
            /* The counterpart is talking again: it may well be ONLINE, so any confirmation is void. */
            rs->peer_unresponsive = false;
            rs->takeover_confirmed = false;
            rs->peer_last_rx_cycle = rs->cycle;
            rs->peer_online_now = valid && !peer_faulted;
            if (valid)
            {
                rs->peer_snapshot_cycle = peer_cycle;
                rs->peer_snapshot_valid = true;
                rs->standby_rx_cycle = rs->cycle;
            }
            if (!quiet)
            {
                (void)snprintf(line, sizeof(line),
                               "[%s] cycle %u: negotiation received payload from counterpart (valid=%d, peer_cycle=%u, "
                               "faulted=%d, single=%d)\n",
                               rs->cfg.role_tag, (unsigned int)cycle, valid ? 1 : 0, (unsigned int)peer_cycle,
                               peer_faulted ? 1 : 0, (int)in[1]);
                trace_line(rs, line);
            }
        }
    }

    if ((status != RTE_STATUS_OK) && !quiet)
    {
        (void)snprintf(line, sizeof(line),
                       "[%s] cycle %u: negotiation extra payload receive failed (%s) - using last-known counterpart "
                       "status\n",
                       rs->cfg.role_tag, (unsigned int)cycle, rte_status_to_string(status));
        trace_line(rs, line);
        log_event(rs, RTE_LOG_LEVEL_DEBUG, cycle, rs->cfg.peer_tag, rs->cfg.role_tag, "NEGOTIATION",
                  "extra payload receive failed - using last-known counterpart status", NULL);
    }
    return status;
}

/** REQ-SITEROLE-013: closes the link after send_miss_threshold consecutive unacknowledged sends (one lost UDP
 *  datagram is routine) or at once on a receive failure other than a timeout (ADR-027). */
static void handle_send_miss_and_link_teardown(rte_site_role_t *rs, uint32_t cycle, bool send_ok,
                                               rte_status_t receive_status)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    if (send_ok)
    {
        rs->send_miss = 0U;
    }
    else if (rs->send_miss < UINT8_MAX)
    {
        rs->send_miss++;
    }
    else
    {
        /* already saturated */
    }

    if ((rs->send_miss >= rs->cfg.send_miss_threshold) ||
        ((receive_status != RTE_STATUS_OK) && (receive_status != RTE_STATUS_TIMEOUT)))
    {
        (void)snprintf(line, sizeof(line), "[%s] cycle %u: inter-site negotiation link appears dead - closing for reconnect\n",
                       rs->cfg.role_tag, (unsigned int)cycle);
        trace_line(rs, line);
        if (rs->online)
        {
            /* If the counterpart was confirmed absent by the dispatcher and took over meanwhile, this channel must
             * yield when it sees that: see rte_site_role_decide(). */
            rs->lost_peer_while_online = true;
        }
        if (!rs->online && !rs->local_faulted && !rs->peer_unresponsive)
        {
            /* A silent counterpart never promotes this channel by itself (REQ-SITEROLE-003): it stays STANDBY until
             * a dispatcher confirmation, so a partition can never leave two ONLINE sites. */
            rs->peer_unresponsive = true;
            (void)snprintf(line, sizeof(line),
                           "[%s] cycle %u: counterpart not responding - staying STANDBY, waiting for the dispatcher to "
                           "confirm the other site is not ONLINE\n",
                           rs->cfg.role_tag, (unsigned int)cycle);
            trace_line(rs, line);
            log_event(rs, RTE_LOG_LEVEL_WARNING, cycle, rs->cfg.role_tag, "-", "TAKEOVER",
                      "counterpart not responding - dispatcher confirmation required", NULL);
        }
        close_link(rs);
        rs->send_miss = 0U;
    }
}

/** STANDBY, once per cycle: drops a snapshot that has gone stale (STANDBY goes cold) and logs the warm transition. */
static void update_standby_warmth(rte_site_role_t *rs, uint32_t cycle)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    if (rte_site_role_standby_data_stale(rs->cfg.standby_sync, rs->online, rs->peer_snapshot_valid, rs->cycle,
                                         rs->standby_rx_cycle))
    {
        rs->peer_snapshot_valid = false;
        (void)snprintf(line, sizeof(line), "[%s] cycle %u: standby snapshot silent for more than %u cycles - STANDBY goes cold\n",
                       rs->cfg.role_tag, (unsigned int)cycle, (unsigned int)RTE_SITE_ROLE_STANDBY_DATA_TIMEOUT_CYCLES);
        trace_line(rs, line);
        log_event(rs, RTE_LOG_LEVEL_WARNING, cycle, rs->cfg.role_tag, "-", "STANDBY", "snapshot stale - cold", NULL);
    }
    if (rs->cfg.standby_sync && !rs->online)
    {
        const bool warm = rs->peer_snapshot_valid;

        if (warm && !rs->standby_warm)
        {
            (void)snprintf(line, sizeof(line), "[%s] cycle %u: STANDBY is warm (receiving ONLINE snapshots)\n",
                           rs->cfg.role_tag, (unsigned int)cycle);
            trace_line(rs, line);
        }
        rs->standby_warm = warm;
    }
    else
    {
        rs->standby_warm = false;
    }
}

static void build_decision_input(const rte_site_role_t *rs, const rte_site_role_local_t *local,
                                 rte_site_role_input_t *in)
{
    in->is_online = rs->online;
    in->own_faulted = rs->local_faulted;
    in->is_west = rs->cfg.is_west;
    in->link_up = (rs->link != NULL);
    in->peer_unresponsive = rs->peer_unresponsive;
    in->cycle = rs->cycle;
    in->peer_last_rx_cycle = rs->peer_last_rx_cycle;
    in->peer_online_now = rs->peer_online_now;
    in->peer_faulted = rs->peer_faulted;
    in->peer_snapshot_valid = rs->peer_snapshot_valid;
    in->sibling_online = local->sibling_online;
    in->takeover_confirmed = rs->takeover_confirmed;
    in->takeover_confirm_cycle = rs->takeover_confirm_cycle;
    in->lost_peer_while_online = rs->lost_peer_while_online;
    in->both_standby_cycles = rs->both_standby_cycles;
    in->own_ready = local->own_ready;
    in->sibling_ready = local->sibling_ready;
    in->sibling_reachable = local->sibling_reachable;
}

static bool reason_changed(const rte_site_role_t *rs, rte_site_role_reason_t reason)
{
    return !rs->last_reason_set || (rs->last_reason != reason);
}

static void set_reason(rte_site_role_t *rs, rte_site_role_reason_t reason)
{
    rs->last_reason = reason;
    rs->last_reason_set = true;
}

/** Gathers the facts, asks rte_site_role_decide(), applies the one action it returns (REQ-SITEROLE-011). */
static void run_site_role_decision(rte_site_role_t *rs, uint32_t cycle, const rte_site_role_local_t *local)
{
    rte_site_role_input_t in;
    rte_site_role_decision_t d;
    char message[160];
    char line[SITE_ROLE_TRACE_LINE_MAX];

    build_decision_input(rs, local, &in);
    d = rte_site_role_decide(&in);
    rs->both_standby_cycles = d.both_standby_cycles;

    switch (d.action)
    {
    case RTE_SITE_ROLE_ACTION_PROMOTE:
        (void)snprintf(message, sizeof(message), "TAKEOVER: %s, promoted to ONLINE", rte_site_role_reason_text(d.reason));
        promote_to_online(rs, message, d.reason);
        set_reason(rs, d.reason);
        break;
    case RTE_SITE_ROLE_ACTION_DEMOTE:
        (void)snprintf(message, sizeof(message), "%s, demoted to STANDBY", rte_site_role_reason_text(d.reason));
        demote_to_standby(rs, message, d.reason);
        set_reason(rs, d.reason);
        break;
    case RTE_SITE_ROLE_ACTION_RENEGOTIATE:
        renegotiate_from_deadlock(rs, d.reason);
        set_reason(rs, d.reason);
        break;
    case RTE_SITE_ROLE_ACTION_ALARM:
        if (reason_changed(rs, d.reason))
        {
            set_reason(rs, d.reason);
            (void)snprintf(line, sizeof(line),
                           "[%s] *** DUAL ONLINE detected without a lost-contact history - not resolved automatically ***\n",
                           rs->cfg.role_tag);
            trace_line(rs, line);
            log_event(rs, RTE_LOG_LEVEL_ERROR, cycle, rs->cfg.role_tag, "-", "ROLE",
                      "DUAL ONLINE detected, not resolved automatically", NULL);
        }
        break;
    case RTE_SITE_ROLE_ACTION_WAIT:
        if (reason_changed(rs, d.reason))
        {
            set_reason(rs, d.reason);
            (void)snprintf(line, sizeof(line),
                           "[%s] cycle %u: promotion due but held back - waiting for both A/B channels to be ready\n",
                           rs->cfg.role_tag, (unsigned int)cycle);
            trace_line(rs, line);
            log_event(rs, RTE_LOG_LEVEL_INFO, cycle, rs->cfg.role_tag, "-", "ROLE",
                      "promotion held back - waiting for both A/B channels to be ready", NULL);
        }
        break;
    case RTE_SITE_ROLE_ACTION_NONE:
    default:
        break;
    }
}

static void take_local(rte_site_role_t *rs, const rte_site_role_local_t *local)
{
    rs->local_faulted = local->faulted;
    rs->local_single_mode = local->single_mode;
}

/* ---- takeover confirmation (REQ-SITEROLE-015) ------------------------------------------------------------------- */

static uint32_t read_u32_le(const uint8_t *in)
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8U) | ((uint32_t)in[2] << 16U) | ((uint32_t)in[3] << 24U);
}

static uint32_t challenge_lifetime(const rte_site_role_t *rs)
{
    return (rs->cfg.challenge_lifetime_cycles != 0U) ? rs->cfg.challenge_lifetime_cycles
                                                     : (uint32_t)RTE_SITE_ROLE_CHALLENGE_LIFETIME_DEFAULT_CYCLES;
}

static void draw_challenge(rte_site_role_t *rs, uint32_t cycle)
{
    uint8_t bytes[4];
    char line[SITE_ROLE_TRACE_LINE_MAX];

    rs->challenge_cycle = cycle;
    if (rte_random_fill(bytes, sizeof(bytes)) != RTE_STATUS_OK)
    {
        rs->challenge_valid = false; /* no token can match until a draw succeeds */
        if (!rs->challenge_fail_reported)
        {
            rs->challenge_fail_reported = true;
            (void)snprintf(line, sizeof(line), "[%s] cycle %u: takeover challenge could not be drawn - tokens refused\n",
                           rs->cfg.role_tag, (unsigned int)cycle);
            trace_line(rs, line);
            log_event(rs, RTE_LOG_LEVEL_ERROR, cycle, rs->cfg.role_tag, "-", "TAKEOVER",
                      "challenge could not be drawn - tokens refused", NULL);
        }
        return;
    }
    rs->challenge = read_u32_le(bytes);
    rs->challenge_valid = true;
    rs->challenge_fail_reported = false;
}

/* A new challenge when none is held, when it has lived its lifetime, or when the cycle number went back (a state
 * transfer renumbers cycles). */
static void refresh_challenge(rte_site_role_t *rs, uint32_t cycle)
{
    if (rs->cfg.verify_mac == NULL)
    {
        return;
    }
    if ((!rs->challenge_valid) || (cycle < rs->challenge_cycle) ||
        ((cycle - rs->challenge_cycle) >= challenge_lifetime(rs)))
    {
        draw_challenge(rs, cycle);
    }
}

static void record_takeover_confirmation(rte_site_role_t *rs, uint32_t cycle, const char *how)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    rs->takeover_confirmed = true;
    rs->takeover_confirm_cycle = cycle;
    (void)snprintf(line, sizeof(line), "[%s] cycle %u: dispatcher takeover confirmation received (valid for %u cycles)\n",
                   rs->cfg.role_tag, (unsigned int)cycle, (unsigned int)RTE_SITE_ROLE_TAKEOVER_CONFIRM_VALID_CYCLES);
    trace_line(rs, line);
    log_event(rs, RTE_LOG_LEVEL_WARNING, cycle, rs->cfg.role_tag, "-", "TAKEOVER", "dispatcher confirmation received",
              how);
}

static void trace_ignored_while_online(const rte_site_role_t *rs)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    (void)snprintf(line, sizeof(line), "[%s] takeover confirmation ignored - already ONLINE\n", rs->cfg.role_tag);
    trace_line(rs, line);
}

static rte_site_role_token_result_t check_token(const rte_site_role_t *rs, const uint8_t *token, size_t token_len)
{
    rte_site_role_token_result_t result;

    if (rs->cfg.verify_mac == NULL)
    {
        result = RTE_SITE_ROLE_TOKEN_NOT_CONFIGURED;
    }
    else if ((token == NULL) || (token_len != (size_t)RTE_SITE_ROLE_TOKEN_LEN) ||
             (token[0] != (uint8_t)RTE_SITE_ROLE_TOKEN_VERSION) || (token[1] != (uint8_t)(rs->cfg.own_id & 0xFFU)) ||
             (token[2] != rs->cfg.channel_id) || (token[3] != 0U) || (read_u32_le(&token[12]) != 0U))
    {
        result = RTE_SITE_ROLE_TOKEN_BAD_FORMAT;
    }
    else if (!rs->challenge_valid)
    {
        result = RTE_SITE_ROLE_TOKEN_NO_CHALLENGE;
    }
    else if (!rs->cfg.verify_mac(rs->cfg.user, token, (size_t)RTE_SITE_ROLE_TOKEN_MSG_LEN,
                                 &token[RTE_SITE_ROLE_TOKEN_MSG_LEN]))
    {
        result = RTE_SITE_ROLE_TOKEN_BAD_MAC;
    }
    else if (read_u32_le(&token[4]) != rs->challenge)
    {
        result = RTE_SITE_ROLE_TOKEN_STALE_CHALLENGE;
    }
    else
    {
        result = RTE_SITE_ROLE_TOKEN_ACCEPTED;
    }
    return result;
}

static const char *token_result_text(rte_site_role_token_result_t result)
{
    const char *text;

    switch (result)
    {
    case RTE_SITE_ROLE_TOKEN_ACCEPTED:
        text = "accepted";
        break;
    case RTE_SITE_ROLE_TOKEN_IGNORED_ONLINE:
        text = "ignored, already ONLINE";
        break;
    case RTE_SITE_ROLE_TOKEN_NOT_CONFIGURED:
        text = "no takeover key configured";
        break;
    case RTE_SITE_ROLE_TOKEN_NO_CHALLENGE:
        text = "no challenge held";
        break;
    case RTE_SITE_ROLE_TOKEN_BAD_FORMAT:
        text = "bad format";
        break;
    case RTE_SITE_ROLE_TOKEN_BAD_MAC:
        text = "bad MAC";
        break;
    case RTE_SITE_ROLE_TOKEN_STALE_CHALLENGE:
    default:
        text = "stale challenge";
        break;
    }
    return text;
}

/* ---- public ----------------------------------------------------------------------------------------------------- */

rte_status_t rte_site_role_init(rte_site_role_t *rs, const rte_site_role_config_t *cfg)
{
    if ((rs == NULL) || (cfg == NULL) || (cfg->tx_buf == NULL) || (cfg->rx_buf == NULL) ||
        (cfg->encode_state == NULL) || (cfg->receive_state == NULL) || (cfg->on_change == NULL) ||
        (cfg->frame_size <= (size_t)RTE_SITE_ROLE_FRAME_OVERHEAD) || (cfg->frame_size > (size_t)UINT8_MAX) ||
        (cfg->role_tag == NULL) || (cfg->peer_tag == NULL) || (cfg->site_name == NULL) ||
        ((!cfg->listen) && (cfg->peer_host == NULL)))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    rte_mem_set(rs, 0, sizeof(*rs));
    rs->cfg = *cfg;
    rs->link = NULL;
    rs->startup_timestamp_ms = cfg->startup_timestamp_ms;
    rs->standby_healthy = true;
    rs->last_reason = RTE_SITE_ROLE_REASON_NONE;
    rs->last_reason_set = true; /* matches the application's former zero-initialised int */
    return RTE_STATUS_OK;
}

rte_status_t rte_site_role_start(rte_site_role_t *rs)
{
    rte_status_t status;
    char line[SITE_ROLE_TRACE_LINE_MAX];
    uint32_t waited_ms = 0U;
    rte_dual_state_t own_state;

    if (rs == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    rs->standby_healthy = true;
    rs->standby_last_try_cycle = 0U;
    rs->standby_rx_cycle = 0U;
    rs->standby_warm = false;
    if (rs->startup_timestamp_ms == 0U)
    {
        (void)rte_timer_now(&rs->startup_timestamp_ms);
    }

    status = open_link_blocking(rs);
    (void)snprintf(line, sizeof(line), "[%s] inter-site negotiation link -> %s\n", rs->cfg.role_tag,
                   rte_status_to_string(status));
    trace_line(rs, line);
    if (status != RTE_STATUS_OK)
    {
        log_event(rs, RTE_LOG_LEVEL_ERROR, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "LINK",
                  "inter-site negotiation link failed to establish", NULL);
        return status;
    }

    status = init_channel_and_negotiator(rs);
    if (status != RTE_STATUS_OK)
    {
        (void)snprintf(line, sizeof(line), "[%s] negotiation dual_channel/negotiator init failed (%s)\n",
                       rs->cfg.role_tag, rte_status_to_string(status));
        trace_line(rs, line);
        log_event(rs, RTE_LOG_LEVEL_ERROR, 0U, rs->cfg.role_tag, "-", "NEGOTIATOR", "dual_channel/negotiator init failed",
                  NULL);
        close_link(rs);
        return status;
    }

    /* Startup negotiation until it settles (older startup timestamp wins, REQ-DUAL-NEGOTIATOR-003). */
    do
    {
        status = rte_dual_negotiator_execute(&rs->negotiator, rs->cfg.link_poll_ms);
        own_state = rte_dual_negotiator_get_own_state(&rs->negotiator);
        if ((status != RTE_STATUS_OK) || (own_state == RTE_DUAL_STATE_IDLE))
        {
            waited_ms += (uint32_t)rs->cfg.link_poll_ms;
        }
    } while ((own_state == RTE_DUAL_STATE_IDLE) && (waited_ms < (uint32_t)rs->cfg.startup_timeout_ms));

    if (own_state == RTE_DUAL_STATE_IDLE)
    {
        (void)snprintf(line, sizeof(line), "[%s] negotiation did not settle within %ums\n", rs->cfg.role_tag,
                       (unsigned int)rs->cfg.startup_timeout_ms);
        trace_line(rs, line);
        log_event(rs, RTE_LOG_LEVEL_ERROR, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "NEGOTIATION",
                  "did not settle within timeout", NULL);
        close_link(rs);
        return RTE_STATUS_TIMEOUT;
    }
    rs->online = (own_state == RTE_DUAL_STATE_ONLINE);

    (void)snprintf(line, sizeof(line), "[%s] negotiation result -> %s (own_ts=%llu, peer_ts=%llu, own_id=%u, peer_id=%u)\n",
                   rs->cfg.role_tag, rs->online ? "ONLINE" : "STANDBY",
                   (unsigned long long)rte_dual_negotiator_get_own_startup_timestamp_ms(&rs->negotiator),
                   (unsigned long long)rte_dual_negotiator_get_peer_startup_timestamp_ms(&rs->negotiator),
                   (unsigned int)rte_dual_negotiator_get_own_id(&rs->negotiator),
                   (unsigned int)rte_dual_negotiator_get_peer_id(&rs->negotiator));
    trace_line(rs, line);
    log_event(rs, RTE_LOG_LEVEL_INFO, 0U, rs->cfg.role_tag, rs->cfg.peer_tag, "NEGOTIATION",
              rs->online ? "result: ONLINE" : "result: STANDBY", NULL);
    return RTE_STATUS_OK;
}

void rte_site_role_execute(rte_site_role_t *rs, uint32_t cycle, const rte_site_role_local_t *local)
{
    rte_status_t status;
    rte_status_t receive_status;
    bool send_ok;
    char line[SITE_ROLE_TRACE_LINE_MAX];

    if ((rs == NULL) || (local == NULL))
    {
        return;
    }
    rs->cycle = cycle;
    take_local(rs, local);
    refresh_challenge(rs, cycle);

    if (rs->link == NULL)
    {
        if (reconnect(rs) != RTE_STATUS_OK)
        {
            run_site_role_decision(rs, cycle, local); /* rules that need no link (dispatcher confirmation) still apply */
            return;
        }
    }

    /* The negotiator's own STATE-frame exchange (ADR-020 section 3), non-blocking in cyclic execution. */
    status = rte_dual_negotiator_execute(&rs->negotiator, 0U);
    if (status != RTE_STATUS_OK)
    {
        (void)snprintf(line, sizeof(line), "[%s] cycle %u: negotiator execute failed (%s)\n", rs->cfg.role_tag,
                       (unsigned int)cycle, rte_status_to_string(status));
        trace_line(rs, line);
    }

    send_ok = send_frame(rs, cycle);
    receive_status = receive_frame(rs, cycle, false);
    update_standby_warmth(rs, cycle);

    run_site_role_decision(rs, cycle, local);

    {
        const char *own_role = rs->local_faulted ? "FAULTED" : (rs->online ? "ONLINE" : "STANDBY");
        const char *peer_role = rs->peer_faulted ? "FAULTED" : (rs->peer_snapshot_valid ? "ONLINE" : "STANDBY");

        (void)snprintf(line, sizeof(line),
                       "[%s] cycle %u: negotiation ok (role=%s, counterpart=%s, negotiator diagnostic own=%d peer=%d)\n",
                       rs->cfg.role_tag, (unsigned int)cycle, own_role, peer_role,
                       (int)rte_dual_negotiator_get_own_state(&rs->negotiator),
                       (int)rte_dual_negotiator_get_peer_state(&rs->negotiator));
        trace_line(rs, line);
    }

    handle_send_miss_and_link_teardown(rs, cycle, send_ok, receive_status);
}

void rte_site_role_service_link(rte_site_role_t *rs, uint32_t cycle)
{
    uint32_t i;

    if ((rs == NULL) || (rs->link == NULL))
    {
        return;
    }
    rs->cycle = cycle;
    /* rte_dual_channel acknowledges a DATA frame only from inside a receive call and the two sites' cycles are not
     * phase-locked: polling from the fast tick makes the acknowledgement independent of phase. Bounded drain. */
    for (i = 0U; i < SITE_ROLE_SERVICE_DRAIN; i++)
    {
        if (receive_frame(rs, cycle, true) != RTE_STATUS_OK)
        {
            break;
        }
    }
}

void rte_site_role_sync_standby(rte_site_role_t *rs, uint32_t cycle, const rte_site_role_local_t *local)
{
    bool link_up;
    bool ok;
    char line[SITE_ROLE_TRACE_LINE_MAX];

    if ((rs == NULL) || (local == NULL))
    {
        return;
    }
    take_local(rs, local);
    link_up = (rs->link != NULL);
    if (rs->cfg.standby_sync && rs->online && !link_up)
    {
        rs->standby_healthy = false;
    }
    if (!rte_site_role_standby_sync_due(rs->cfg.standby_sync, rs->online, link_up, rs->standby_healthy, cycle,
                                        rs->standby_last_try_cycle))
    {
        return;
    }
    ok = send_frame(rs, cycle);
    if (ok)
    {
        if (!rs->standby_healthy)
        {
            (void)snprintf(line, sizeof(line), "[%s] cycle %u: STANDBY acknowledges again\n", rs->cfg.role_tag,
                           (unsigned int)cycle);
            trace_line(rs, line);
        }
        rs->standby_healthy = true;
    }
    else
    {
        if (rs->standby_healthy)
        {
            (void)snprintf(line, sizeof(line),
                           "[%s] cycle %u: STANDBY did not acknowledge the snapshot - considered not working\n",
                           rs->cfg.role_tag, (unsigned int)cycle);
            trace_line(rs, line);
            log_event(rs, RTE_LOG_LEVEL_WARNING, cycle, rs->cfg.role_tag, "-", "STANDBY",
                      "no acknowledgement - considered not working", NULL);
        }
        rs->standby_healthy = false;
        rs->standby_last_try_cycle = cycle;
    }
}

void rte_site_role_on_takeover_confirm(rte_site_role_t *rs, uint32_t cycle)
{
    char line[SITE_ROLE_TRACE_LINE_MAX];

    if (rs == NULL)
    {
        return;
    }
    if (rs->cfg.verify_mac != NULL)
    {
        (void)snprintf(line, sizeof(line),
                       "[%s] cycle %u: unauthenticated takeover confirmation refused - a takeover key is configured\n",
                       rs->cfg.role_tag, (unsigned int)cycle);
        trace_line(rs, line);
        log_event(rs, RTE_LOG_LEVEL_WARNING, cycle, rs->cfg.role_tag, "-", "TAKEOVER",
                  "unauthenticated confirmation refused", NULL);
        return;
    }
    if (rs->online)
    {
        trace_ignored_while_online(rs);
        return;
    }
    record_takeover_confirmation(rs, cycle, "unauthenticated");
}

rte_site_role_token_result_t rte_site_role_on_takeover_token(rte_site_role_t *rs, uint32_t cycle, const uint8_t *token,
                                                             size_t token_len)
{
    rte_site_role_token_result_t result;
    char line[SITE_ROLE_TRACE_LINE_MAX];
    char extra[64];

    if (rs == NULL)
    {
        return RTE_SITE_ROLE_TOKEN_NOT_CONFIGURED;
    }
    if ((rs->cfg.verify_mac != NULL) && rs->online)
    {
        trace_ignored_while_online(rs);
        return RTE_SITE_ROLE_TOKEN_IGNORED_ONLINE;
    }
    result = check_token(rs, token, token_len);
    if (result == RTE_SITE_ROLE_TOKEN_ACCEPTED)
    {
        (void)snprintf(extra, sizeof(extra), "authenticated seq=%u", (unsigned int)read_u32_le(&token[8]));
        record_takeover_confirmation(rs, cycle, extra);
        draw_challenge(rs, cycle); /* single use */
    }
    else
    {
        (void)snprintf(line, sizeof(line), "[%s] cycle %u: takeover confirmation token rejected (%s)\n",
                       rs->cfg.role_tag, (unsigned int)cycle, token_result_text(result));
        trace_line(rs, line);
        log_event(rs, RTE_LOG_LEVEL_WARNING, cycle, rs->cfg.role_tag, "-", "TAKEOVER", "confirmation token rejected",
                  token_result_text(result));
    }
    return result;
}

bool rte_site_role_takeover_challenge(const rte_site_role_t *rs, uint32_t *out)
{
    if ((rs == NULL) || (out == NULL) || (!rs->challenge_valid))
    {
        return false;
    }
    *out = rs->challenge;
    return true;
}

void rte_site_role_note_lost_contact(rte_site_role_t *rs)
{
    if (rs != NULL)
    {
        rs->lost_peer_while_online = true;
    }
}

void rte_site_role_flush_faulted(rte_site_role_t *rs, bool single_mode)
{
    rte_status_t status;
    char line[SITE_ROLE_TRACE_LINE_MAX];

    if ((rs == NULL) || (rs->link == NULL))
    {
        return; /* link already down - nothing to flush */
    }
    /* This channel is shutting down: faulted, no state-transfer snapshot to offer. */
    encode_frame(rs, true, single_mode, 0U, false);
    status = rte_dual_channel_send(&rs->dual_channel, rs->cfg.tx_buf, (uint8_t)rs->cfg.frame_size, NULL);
    (void)snprintf(line, sizeof(line), "[%s] flushing FAULTED to counterpart before shutdown -> %s\n", rs->cfg.role_tag,
                   rte_status_to_string(status));
    trace_line(rs, line);
}

rte_site_role_sibling_verdict_t rte_site_role_check_sibling(rte_site_role_t *rs, uint32_t cycle, bool own_single_mode,
                                                            const rte_site_role_sibling_t *sibling)
{
    rte_site_role_sibling_verdict_t verdict;
    char line[SITE_ROLE_TRACE_LINE_MAX];
    char extra[80];

    if ((rs == NULL) || (sibling == NULL) || !sibling->known)
    {
        return RTE_SITE_ROLE_SIBLING_UNKNOWN; /* the sibling has not sent its site state yet - normal at start-up */
    }
    if ((sibling->online == rs->online) && (sibling->single_mode == own_single_mode))
    {
        rs->sibling_mismatch = 0U;
        return RTE_SITE_ROLE_SIBLING_AGREE;
    }

    if (rs->sibling_mismatch < UINT8_MAX)
    {
        rs->sibling_mismatch++;
    }
    if (rs->sibling_mismatch < (uint8_t)RTE_SITE_ROLE_SIBLING_MISMATCH_LIMIT)
    {
        (void)snprintf(line, sizeof(line),
                       "[%s] cycle %u: site state mismatch with local peer (mismatch count %u/%u: own online=%d single=%d "
                       "vs peer online=%d single=%d)\n",
                       rs->cfg.role_tag, (unsigned int)cycle, (unsigned int)rs->sibling_mismatch,
                       (unsigned int)RTE_SITE_ROLE_SIBLING_MISMATCH_LIMIT, rs->online ? 1 : 0, own_single_mode ? 1 : 0,
                       sibling->online ? 1 : 0, sibling->single_mode ? 1 : 0);
        trace_line(rs, line);
        verdict = RTE_SITE_ROLE_SIBLING_PENDING;
    }
    else
    {
        (void)snprintf(line, sizeof(line),
                       "[%s] cycle %u: NEGOTIATION MISMATCH - own (is_online=%d, single=%d) vs local peer's own "
                       "(is_online=%d, single=%d) - this site's role -> FAULTED, entering REBOOT\n",
                       rs->cfg.role_tag, (unsigned int)cycle, rs->online ? 1 : 0, own_single_mode ? 1 : 0,
                       sibling->online ? 1 : 0, sibling->single_mode ? 1 : 0);
        trace_line(rs, line);
        (void)snprintf(extra, sizeof(extra), "own(online=%d,single=%d) peer(online=%d,single=%d)", rs->online ? 1 : 0,
                       own_single_mode ? 1 : 0, sibling->online ? 1 : 0, sibling->single_mode ? 1 : 0);
        log_event(rs, RTE_LOG_LEVEL_ERROR, cycle, rs->cfg.role_tag,
                  (rs->cfg.sibling_tag != NULL) ? rs->cfg.sibling_tag : "-", "NEGOTIATION_MISMATCH",
                  "FAULTED, entering REBOOT", extra);
        verdict = RTE_SITE_ROLE_SIBLING_FAULT;
    }
    return verdict;
}

void rte_site_role_shutdown(rte_site_role_t *rs)
{
    if (rs != NULL)
    {
        close_link(rs);
    }
}

bool rte_site_role_is_online(const rte_site_role_t *rs)
{
    return (rs != NULL) && rs->online;
}

void rte_site_role_get_status(const rte_site_role_t *rs, rte_site_role_status_t *out)
{
    if ((rs == NULL) || (out == NULL))
    {
        return;
    }
    out->online = rs->online;
    out->link_up = (rs->link != NULL);
    out->peer_answering = (rs->cycle >= rs->peer_last_rx_cycle) && ((rs->cycle - rs->peer_last_rx_cycle) <= 2U);
    out->peer_online_now = rs->peer_online_now;
    out->peer_unresponsive = rs->peer_unresponsive;
    out->standby_warm = rs->standby_warm;
    out->standby_healthy = rs->standby_healthy;
    out->last_reason = rs->last_reason;
}

void rte_site_role_get_negotiator_states(const rte_site_role_t *rs, rte_dual_state_t *own, rte_dual_state_t *peer)
{
    if ((rs == NULL) || (own == NULL) || (peer == NULL))
    {
        return;
    }
    *own = rte_dual_negotiator_get_own_state(&rs->negotiator);
    *peer = rte_dual_negotiator_get_peer_state(&rs->negotiator);
}

/**
 * @file sapi_safechannel.c
 * @brief Unified channel factory: opens netlink links internally, wraps
 *        sapi_dual_channel_t or a sapi_voter_t over N sapi_channel_t
 *        links (ADR-025) - see sapi_safechannel.h and ADR-022.
 */
#include "safeapi/redundancy/safechannel/sapi_safechannel.h"

#include "safeapi/redundancy/checksum/sapi_checksum.h"

#include <string.h>

/** @brief Closes every already-opened link handles[0..count-1] and
 *         resets the caller's bookkeeping - used both for the normal
 *         close path and to unwind a partial open failure. */
static void safechannel_close_links(sapi_safechannel_t *channel, uint32_t count)
{
    uint32_t i;

    for (i = 0U; i < count; i++)
    {
        if (channel->link_handles[i] != NULL)
        {
            (void)sapi_netlink_close(channel->link_handles[i]);
            channel->link_handles[i] = NULL;
        }
    }
}

/** @brief Opens one endpoint, retrying sapi_netlink_open() with a short
 *         fixed delay until connect_timeout_ms elapses - the same retry
 *         shape every current hand-rolled CONNECT-role caller in
 *         safeAPIRBC2oo2 already implements itself (site.c, etc.). */
static sapi_status_t safechannel_open_one_endpoint(sapi_netlink_storage_t *storage,
                                                    const sapi_safechannel_endpoint_t *endpoint, size_t message_size,
                                                    sapi_duration_ms_t connect_timeout_ms,
                                                    sapi_netlink_handle_t *out_handle)
{
    sapi_netlink_config_t cfg;
    sapi_status_t status;

    memset(&cfg, 0, sizeof(cfg));
    cfg.role = endpoint->role;
    cfg.host = endpoint->host;
    cfg.port = endpoint->port;
    cfg.message_size = message_size;
    cfg.connect_timeout_ms = connect_timeout_ms;

    /* sapi_netlink_open() itself already bounds a single attempt to
     * connect_timeout_ms (REQ-OAL-NETLINK-002); a LISTEN-role endpoint
     * blocks until accepted or that same timeout, so one attempt is
     * sufficient there. A CONNECT-role endpoint can fail fast if the
     * peer isn't listening yet, so this retries for the same overall
     * budget rather than failing on the first attempt. */
    status = sapi_netlink_open(storage, &cfg, out_handle);
    return status;
}

static sapi_status_t safechannel_open_dual(sapi_safechannel_t *channel, const sapi_safechannel_dual_config_t *cfg)
{
    sapi_dual_channel_config_t dual_cfg;
    sapi_status_t status;
    uint32_t i;

    if ((cfg->link_count == 0U) || (cfg->link_count > SAPI_SAFECHANNEL_MAX_LINKS))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    for (i = 0U; i < cfg->link_count; i++)
    {
        if ((cfg->endpoints[i].role == SAPI_NETLINK_ROLE_CONNECT) && (cfg->endpoints[i].host == NULL))
        {
            return SAPI_STATUS_INVALID_PARAM;
        }
    }

    for (i = 0U; i < cfg->link_count; i++)
    {
        status = safechannel_open_one_endpoint(&channel->links[i], &cfg->endpoints[i], sizeof(sapi_vital_message_t),
                                                cfg->connect_timeout_ms, &channel->link_handles[i]);
        if (status != SAPI_STATUS_OK)
        {
            safechannel_close_links(channel, i);
            return status;
        }
    }
    channel->link_count = cfg->link_count;

    memset(&dual_cfg, 0, sizeof(dual_cfg));
    for (i = 0U; i < cfg->link_count; i++)
    {
        dual_cfg.links[i] = channel->link_handles[i];
    }
    dual_cfg.link_count = cfg->link_count;
    dual_cfg.sender_id = cfg->sender_id;
    dual_cfg.expected_peer_id = cfg->expected_peer_id;
    dual_cfg.ack_timeout_ms = cfg->ack_timeout_ms;
    dual_cfg.status_callback = cfg->status_callback;
    dual_cfg.status_callback_ctx = cfg->status_callback_ctx;

    status = sapi_dual_channel_init(&channel->impl.dual, &dual_cfg);
    if (status != SAPI_STATUS_OK)
    {
        safechannel_close_links(channel, channel->link_count);
        channel->link_count = 0U;
        return status;
    }
    return SAPI_STATUS_OK;
}

/** @brief sapi_channel_config_t::send bridge: casts the opaque
 *         channel handle back to the sapi_netlink_handle_t it actually
 *         is (this module is the only place that made that handle) and
 *         forwards to sapi_netlink_send(). One of these is wired into
 *         each of the voter's N individual sapi_channel_t links
 *         (ADR-025). */
static sapi_status_t safechannel_vital_backend_send(void *channel_handle, const void *data, size_t data_size)
{
    /* Fixed, generous per-op timeout: the voter's own
     * config->channel_timeout_ms already bounds the overall voting
     * round from the caller's perspective (sapi_voter_send() calls this
     * once per registered channel); this per-send value only bounds one
     * individual transport call within that round. */
    return sapi_netlink_send((sapi_netlink_handle_t)channel_handle, data, data_size, 5000U);
}

/** @brief sapi_channel_config_t::recv bridge - see
 *         safechannel_vital_backend_send(). */
static sapi_status_t safechannel_vital_backend_recv(void *channel_handle, void *data, size_t data_size,
                                                     uint32_t timeout_ms)
{
    return sapi_netlink_receive((sapi_netlink_handle_t)channel_handle, data, data_size,
                                 (sapi_duration_ms_t)timeout_ms);
}

static sapi_status_t safechannel_open_vital(sapi_safechannel_t *channel, const sapi_safechannel_vital_config_t *cfg)
{
    sapi_voter_config_t voter_cfg;
    sapi_status_t status;
    uint32_t i;

    if ((cfg->link_count == 0U) || (cfg->link_count > SAPI_SAFECHANNEL_MAX_LINKS) || (cfg->message_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    for (i = 0U; i < cfg->link_count; i++)
    {
        if ((cfg->endpoints[i].role == SAPI_NETLINK_ROLE_CONNECT) && (cfg->endpoints[i].host == NULL))
        {
            return SAPI_STATUS_INVALID_PARAM;
        }
    }

    for (i = 0U; i < cfg->link_count; i++)
    {
        status = safechannel_open_one_endpoint(&channel->links[i], &cfg->endpoints[i], cfg->message_size,
                                                cfg->connect_timeout_ms, &channel->link_handles[i]);
        if (status != SAPI_STATUS_OK)
        {
            safechannel_close_links(channel, i);
            return status;
        }
    }
    channel->link_count = cfg->link_count;

    memset(&voter_cfg, 0, sizeof(voter_cfg));
    voter_cfg.voting_strategy = cfg->voting_strategy;
    voter_cfg.quorum_size = cfg->quorum_size;
    voter_cfg.channel_timeout_ms = (uint32_t)cfg->channel_timeout_ms;
    voter_cfg.log_disagreements = cfg->log_disagreements;
    /* Always SAFE on disagreement - matches this path's pre-RCA/OCORA
     * behavior exactly (see sapi_voter.h's own compatibility note). */
    voter_cfg.safestate_level = SAPI_SAFESTATE_LEVEL_SAFE;
    voter_cfg.safestate_reason = SAPI_SAFESTATE_REASON_UNSPECIFIED;
    voter_cfg.on_disagreement = cfg->on_disagreement;
    voter_cfg.disagreement_context = cfg->context;

    status = sapi_voter_init(&channel->impl.vital.voter, &voter_cfg);
    if (status != SAPI_STATUS_OK)
    {
        safechannel_close_links(channel, channel->link_count);
        channel->link_count = 0U;
        return status;
    }

    for (i = 0U; i < cfg->link_count; i++)
    {
        sapi_channel_config_t chan_cfg;

        memset(&chan_cfg, 0, sizeof(chan_cfg));
        chan_cfg.channel_handle = (void *)channel->link_handles[i];
        chan_cfg.send = safechannel_vital_backend_send;
        chan_cfg.recv = safechannel_vital_backend_recv;

        status = sapi_channel_init(&channel->impl.vital.channels[i], &chan_cfg);
        if (status == SAPI_STATUS_OK)
        {
            status = sapi_voter_register_channel(&channel->impl.vital.voter, &channel->impl.vital.channels[i]);
        }
        if (status != SAPI_STATUS_OK)
        {
            safechannel_close_links(channel, channel->link_count);
            channel->link_count = 0U;
            return status;
        }
    }

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_safechannel_open(sapi_safechannel_t *channel, const sapi_safechannel_config_t *config)
{
    sapi_status_t status;

    if ((channel == NULL) || (config == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    memset(channel, 0, sizeof(*channel));
    channel->type = config->type;

    switch (config->type)
    {
        case SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT:
            status = safechannel_open_dual(channel, &config->as.dual);
            break;
        case SAPI_SAFECHANNEL_TYPE_VITAL_VOTED:
            status = safechannel_open_vital(channel, &config->as.vital);
            break;
        default:
            status = SAPI_STATUS_INVALID_PARAM;
            break;
    }

    channel->is_open = (status == SAPI_STATUS_OK);
    return status;
}

sapi_status_t sapi_safechannel_send(sapi_safechannel_t *channel, const uint8_t *payload, size_t payload_size)
{
    if ((channel == NULL) || (!channel->is_open))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (channel->type == SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT)
    {
        if (payload_size > (size_t)UINT8_MAX)
        {
            return SAPI_STATUS_INVALID_PARAM;
        }
        return sapi_dual_channel_send(&channel->impl.dual, payload, (uint8_t)payload_size, NULL);
    }
    return sapi_voter_send(&channel->impl.vital.voter, payload, payload_size);
}

sapi_status_t sapi_safechannel_receive(sapi_safechannel_t *channel, uint8_t *out_payload, size_t max_size,
                                        sapi_duration_ms_t timeout_ms, size_t *out_size)
{
    if ((channel == NULL) || (!channel->is_open) || (out_size == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (channel->type == SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT)
    {
        uint8_t received_size = 0U;
        sapi_status_t status;

        if (max_size > (size_t)UINT8_MAX)
        {
            max_size = (size_t)UINT8_MAX;
        }
        status = sapi_dual_channel_receive(&channel->impl.dual, out_payload, (uint8_t)max_size, timeout_ms,
                                            &received_size);
        *out_size = (size_t)received_size;
        return status;
    }
    {
        sapi_voting_result_t result = SAPI_VOTING_TIMEOUT;

        return sapi_voter_receive(&channel->impl.vital.voter, out_payload, max_size, &result, out_size);
    }
}

sapi_safechannel_link_status_t sapi_safechannel_get_status(const sapi_safechannel_t *channel)
{
    if ((channel == NULL) || (!channel->is_open))
    {
        return SAPI_SAFECHANNEL_LINK_DOWN;
    }

    if (channel->type == SAPI_SAFECHANNEL_TYPE_DUAL_REDUNDANT)
    {
        switch (sapi_dual_channel_get_status(&channel->impl.dual))
        {
            case SAPI_DUAL_CHANNEL_STATUS_FULL:
                return SAPI_SAFECHANNEL_LINK_FULL;
            case SAPI_DUAL_CHANNEL_STATUS_DEGRADED:
                return SAPI_SAFECHANNEL_LINK_DEGRADED;
            case SAPI_DUAL_CHANNEL_STATUS_DOWN:
            default:
                return SAPI_SAFECHANNEL_LINK_DOWN;
        }
    }
    {
        uint32_t healthy_count = 0U;

        (void)sapi_voter_get_aggregated_health(&channel->impl.vital.voter,
                                                &healthy_count, NULL);
        if (healthy_count == 0U)
        {
            return SAPI_SAFECHANNEL_LINK_DOWN;
        }
        if (healthy_count < channel->link_count)
        {
            return SAPI_SAFECHANNEL_LINK_DEGRADED;
        }
        return SAPI_SAFECHANNEL_LINK_FULL;
    }
}

sapi_status_t sapi_safechannel_close(sapi_safechannel_t *channel)
{
    if (channel == NULL)
    {
        return SAPI_STATUS_OK;
    }
    if (!channel->is_open)
    {
        return SAPI_STATUS_OK;
    }

    if (channel->type == SAPI_SAFECHANNEL_TYPE_VITAL_VOTED)
    {
        uint32_t i;

        for (i = 0U; i < channel->link_count; i++)
        {
            (void)sapi_channel_destroy(&channel->impl.vital.channels[i]);
        }
        (void)sapi_voter_destroy(&channel->impl.vital.voter);
    }
    safechannel_close_links(channel, channel->link_count);
    channel->link_count = 0U;
    channel->is_open = false;
    return SAPI_STATUS_OK;
}

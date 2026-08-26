/**
 * @file sapi_dual_msgchannel.c
 * @ingroup DUAL
 * @brief "Channel" layer of ADR-020 - see sapi_dual_msgchannel.h.
 */
#include "safeapi/redundancy/dual/sapi_dual_msgchannel.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
sapi_status_t sapi_dual_msgchannel_init(sapi_dual_msgchannel_t *channel,
                                         const sapi_dual_msgchannel_config_t *config)
{
    if ((channel == NULL) || (config == NULL) || (config->link == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    channel->link              = config->link;
    channel->sender_id         = config->sender_id;
    channel->expected_peer_id  = config->expected_peer_id;
    channel->next_sequence     = 0U;
    channel->expected_sequence = 0U;

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_dual_msgchannel_send(sapi_dual_msgchannel_t *channel,
                                         const uint8_t *payload,
                                         uint8_t payload_size,
                                         sapi_duration_ms_t timeout_ms,
                                         uint32_t *out_sequence)
{
    sapi_vital_message_t frame;
    sapi_status_t status;

    if (channel == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((payload == NULL) && (payload_size != 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    status = sapi_checksum_vital_message_create(&frame, channel->sender_id, channel->next_sequence, payload,
                                                 (size_t)payload_size);
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }

    status = sapi_netlink_send(channel->link, &frame, sizeof(frame), timeout_ms);
    if (status != SAPI_STATUS_OK)
    {
        /* channel->next_sequence intentionally NOT advanced on failure -
         * see this function's own doc: a retry reuses the same
         * sequence_number rather than silently skipping ahead. */
        return status;
    }

    if (out_sequence != NULL)
    {
        *out_sequence = channel->next_sequence;
    }
    channel->next_sequence++;

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_dual_msgchannel_receive(sapi_dual_msgchannel_t *channel,
                                            uint8_t *out_payload,
                                            uint8_t payload_max_size,
                                            sapi_duration_ms_t timeout_ms,
                                            uint8_t *out_payload_size,
                                            uint32_t *out_sequence)
{
    sapi_vital_message_t frame;
    sapi_status_t status;

    if ((channel == NULL) || (out_payload == NULL) || (out_payload_size == NULL) || (payload_max_size == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    status = sapi_netlink_receive(channel->link, &frame, sizeof(frame), timeout_ms);
    if (status != SAPI_STATUS_OK)
    {
        return status;
    }

    /* Masquerade defense (ADR-020 section 1): sapi_checksum_vital_message_
     * verify() below checks CRC-64 and sequence continuity but has no
     * concept of "which peer is this supposed to be" - that check is this
     * layer's own responsibility. Checked before the CRC/sequence
     * verify() call so a wrong-sender frame is reported distinctly
     * (SAPI_STATUS_HARDWARE_FAULT) rather than folded into the generic
     * SAPI_STATUS_DATA_CORRUPTION a corrupted-but-correctly-addressed
     * frame gets. */
    if (frame.sender_id != channel->expected_peer_id)
    {
        return SAPI_STATUS_HARDWARE_FAULT;
    }

    status = sapi_checksum_vital_message_verify(&frame, channel->expected_sequence, out_payload, (size_t)payload_max_size,
                                                 out_payload_size);
    if (status != SAPI_STATUS_OK)
    {
        /* channel->expected_sequence intentionally left unchanged on
         * failure - see sapi_dual_msgchannel_reset_sequence()'s own doc
         * for the deliberate recovery path after a link (re)establishes,
         * rather than silently resyncing here and weakening the
         * anti-replay/anti-reorder defense this check exists for. */
        return status;
    }

    if (out_sequence != NULL)
    {
        *out_sequence = frame.sequence_number;
    }
    channel->expected_sequence = frame.sequence_number + 1U;

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_dual_msgchannel_reset_sequence(sapi_dual_msgchannel_t *channel)
{
    if (channel == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    channel->next_sequence     = 0U;
    channel->expected_sequence = 0U;

    return SAPI_STATUS_OK;
}

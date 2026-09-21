/**
 * @file rte_dual_msgchannel.c
 * @ingroup DUAL
 * @brief "Channel" layer of ADR-020 - see rte_dual_msgchannel.h.
 */
#include "safeapi/redundancy/dual/rte_dual_msgchannel.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_dual_msgchannel_init(rte_dual_msgchannel_t *channel,
                                         const rte_dual_msgchannel_config_t *config)
{
    if ((channel == NULL) || (config == NULL) || (config->link == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    channel->link              = config->link;
    channel->sender_id         = config->sender_id;
    channel->expected_peer_id  = config->expected_peer_id;
    channel->next_sequence     = 0U;
    channel->expected_sequence = 0U;

    return RTE_STATUS_OK;
}

rte_status_t rte_dual_msgchannel_send(rte_dual_msgchannel_t *channel,
                                         const uint8_t *payload,
                                         uint8_t payload_size,
                                         rte_duration_ms_t timeout_ms,
                                         uint32_t *out_sequence)
{
    rte_vital_message_t frame;
    rte_status_t status;

    if (channel == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((payload == NULL) && (payload_size != 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    status = rte_checksum_vital_message_create(&frame, channel->sender_id, channel->next_sequence, payload,
                                                 (size_t)payload_size);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }

    status = rte_netlink_send(channel->link, &frame, sizeof(frame), timeout_ms);
    if (status != RTE_STATUS_OK)
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

    return RTE_STATUS_OK;
}

rte_status_t rte_dual_msgchannel_receive(rte_dual_msgchannel_t *channel,
                                            uint8_t *out_payload,
                                            uint8_t payload_max_size,
                                            rte_duration_ms_t timeout_ms,
                                            uint8_t *out_payload_size,
                                            uint32_t *out_sequence)
{
    rte_vital_message_t frame;
    rte_status_t status;

    if ((channel == NULL) || (out_payload == NULL) || (out_payload_size == NULL) || (payload_max_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    status = rte_netlink_receive(channel->link, &frame, sizeof(frame), timeout_ms);
    if (status != RTE_STATUS_OK)
    {
        return status;
    }

    /* Masquerade defense (ADR-020 section 1): rte_checksum_vital_message_
     * verify() below checks CRC-64 and sequence continuity but has no
     * concept of "which peer is this supposed to be" - that check is this
     * layer's own responsibility. Checked before the CRC/sequence
     * verify() call so a wrong-sender frame is reported distinctly
     * (RTE_STATUS_HARDWARE_FAULT) rather than folded into the generic
     * RTE_STATUS_DATA_CORRUPTION a corrupted-but-correctly-addressed
     * frame gets. */
    if (frame.sender_id != channel->expected_peer_id)
    {
        return RTE_STATUS_HARDWARE_FAULT;
    }

    status = rte_checksum_vital_message_verify(&frame, channel->expected_sequence, out_payload, (size_t)payload_max_size,
                                                 out_payload_size);
    if (status != RTE_STATUS_OK)
    {
        /* channel->expected_sequence intentionally left unchanged on
         * failure - see rte_dual_msgchannel_reset_sequence()'s own doc
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

    return RTE_STATUS_OK;
}

rte_status_t rte_dual_msgchannel_reset_sequence(rte_dual_msgchannel_t *channel)
{
    if (channel == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    channel->next_sequence     = 0U;
    channel->expected_sequence = 0U;

    return RTE_STATUS_OK;
}

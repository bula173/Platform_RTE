/**
 * @file rte_dual_msgchannel.h
 * @brief "Channel" layer of ADR-020: one EN 50159-defended message
 *        channel over one already-open rte_netlink_handle_t.
 *
 * Every frame sent/received is a rte_vital_message_t (rte_checksum.h -
 * already the framework's established EN 50159-style envelope, reused
 * here rather than reinvented, the same way rte_channel_checkpoint()
 * (ADR-017) already reuses it): sequence_number defends against
 * repetition/deletion/resequencing, sender_id (checked against
 * config->expected_peer_id on receive) defends against masquerade,
 * timestamp_ms supports delay/staleness detection by the caller, and
 * crc64 defends against corruption. See ADR-020 section 1.
 *
 * This is the lower of the two protocol layers ADR-020 defines -
 * rte_dual_channel_t (rte_dual_channel.h) is the upper layer, adding
 * its own DATA/ACK/STATE frame-kind header inside this layer's payload
 * and redundant-link fan-out on top of a single rte_dual_msgchannel_t
 * like this one.
 *
 * REQ-DUAL-MSGCHANNEL-001: no dynamic allocation; caller supplies storage
 *                          and an already-open rte_netlink_handle_t.
 * REQ-DUAL-MSGCHANNEL-002: send/receive never block longer than the
 *                          caller-supplied timeout_ms.
 * REQ-DUAL-MSGCHANNEL-003: rte_checksum_crc64_init() must already have
 *                          been called by the application before any
 *                          rte_dual_msgchannel_* call (module-global
 *                          singleton state, ADR-020 does not re-init it).
 * REQ-DUAL-MSGCHANNEL-004: sender_id is checked against
 *                          config->expected_peer_id before CRC/sequence
 *                          verification; a mismatch is rejected with
 *                          RTE_STATUS_HARDWARE_FAULT without advancing
 *                          expected_sequence (masquerade defense).
 * REQ-DUAL-MSGCHANNEL-005: a CRC or sequence-continuity failure yields
 *                          RTE_STATUS_DATA_CORRUPTION and leaves
 *                          expected_sequence unchanged.
 *
 * @defgroup DUAL
 * @{
 */
#ifndef RTE_DUAL_MSGCHANNEL_H
#define RTE_DUAL_MSGCHANNEL_H

#include <stdint.h>

#include "rte/redundancy/checksum/rte_checksum.h"
#include "rte/oal/netlink/rte_netlink.h"
#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Max payload bytes usable via rte_dual_msgchannel_send()/
 *         _receive() - bounded by rte_vital_message_t's own fixed
 *         248-byte payload field (rte_checksum.h). */
#define RTE_DUAL_MSGCHANNEL_MAX_PAYLOAD 248U

/**
 * @brief One rte_dual_msgchannel_t instance's state. Caller-owned
 *        storage (REQ-DUAL-MSGCHANNEL-001); opaque in practice, exposed
 *        here (not via RTE_DECLARE_STORAGE) only because its size is
 *        already small and fixed - callers must still treat every field
 *        as private and only reach it through the functions below.
 */
typedef struct rte_dual_msgchannel_s
{
    rte_netlink_handle_t link;      /**< Caller-opened; NOT closed by rte_dual_msgchannel_close(). */
    uint32_t              sender_id;      /**< This instance's own EN 50159 sender_id, stamped on every outbound frame. */
    uint32_t              expected_peer_id; /**< Inbound frames whose sender_id differs are rejected (masquerade defense). */
    uint32_t              next_sequence;   /**< Next outbound sequence_number. */
    uint32_t              expected_sequence; /**< Next expected inbound sequence_number (continuity check). */
    bool                  resync_on_sequence_error; /**< See rte_dual_msgchannel_config_t::resync_on_sequence_error. */
} rte_dual_msgchannel_t;

/** @brief Configuration for rte_dual_msgchannel_init(). */
typedef struct rte_dual_msgchannel_config_s
{
    /** Already-open link this channel sends/receives fixed-size
     *  rte_vital_message_t frames over. rte_netlink_open() must have
     *  been called with config->message_size == sizeof(rte_vital_message_t)
     *  for that link. Must not be NULL; ownership (closing it) stays
     *  with the caller. */
    rte_netlink_handle_t link;
    /** This instance's own EN 50159 sender identifier (e.g. a site or
     *  role ID), stamped on every frame this channel sends. */
    uint32_t sender_id;
    /** Expected sender_id of the peer at the other end of link; any
     *  inbound frame whose sender_id differs is rejected as a possible
     *  masquerade rather than delivered (ADR-020 section 1). */
    uint32_t expected_peer_id;
} rte_dual_msgchannel_config_t;

/**
 * @brief Turns sequence resynchronisation on or off (default off after init).
 *
 * When on, an inbound frame with a valid CRC and the right sender_id but the wrong sequence number is still rejected
 * (never delivered), but the expected sequence is set to that frame's number plus one, so the next frame from the same
 * peer is accepted. For a link whose peer can restart on its own (the inter-site negotiation link): each end restarts
 * at sequence 0, the surviving end keeps expecting the old number, and without this both ends reject each other for
 * good. CRC and sender checks stay in force.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a NULL channel.
 */
rte_status_t rte_dual_msgchannel_set_resync_on_sequence_error(rte_dual_msgchannel_t *channel, bool enable);

/**
 * @brief Initializes a rte_dual_msgchannel_t: starts both sequence
 *        counters at 0.
 * @param channel  Caller-owned storage to initialize. Must not be NULL.
 * @param config   Configuration. Must not be NULL; config->link must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a NULL argument.
 */
rte_status_t rte_dual_msgchannel_init(rte_dual_msgchannel_t *channel,
                                         const rte_dual_msgchannel_config_t *config);

/**
 * @brief Wraps payload in a rte_vital_message_t (this channel's own
 *        sender_id and next sequence_number) and sends it over
 *        config->link, blocking at most timeout_ms.
 * @param channel         Initialized channel. Must not be NULL.
 * @param payload         Payload to send. May be NULL only if payload_size is 0.
 * @param payload_size    Payload size in bytes; must be <=
 *                        RTE_DUAL_MSGCHANNEL_MAX_PAYLOAD.
 * @param timeout_ms      Maximum time to wait for the underlying
 *                        rte_netlink_send() to complete (REQ-DUAL-MSGCHANNEL-002).
 * @param out_sequence    Optional; if not NULL, receives the
 *                        sequence_number this frame was sent with (the
 *                        caller needs this to correlate a later ACK -
 *                        see rte_dual_channel.h). May be NULL.
 * @return RTE_STATUS_OK (channel->next_sequence has been incremented);
 *         RTE_STATUS_INVALID_PARAM; whatever rte_netlink_send() or
 *         rte_checksum_vital_message_create() returned on failure -
 *         channel->next_sequence is NOT incremented on failure, so a
 *         retried send reuses the same sequence_number.
 */
rte_status_t rte_dual_msgchannel_send(rte_dual_msgchannel_t *channel,
                                         const uint8_t *payload,
                                         uint8_t payload_size,
                                         rte_duration_ms_t timeout_ms,
                                         uint32_t *out_sequence);

/**
 * @brief Receives one frame over config->link, blocking at most
 *        timeout_ms, verifies its CRC-64 and sequence continuity
 *        (rte_checksum_vital_message_verify() against
 *        channel->expected_sequence) and its sender_id against
 *        config->expected_peer_id, and extracts the payload.
 * @param channel          Initialized channel. Must not be NULL.
 * @param out_payload      Destination buffer. Must not be NULL.
 * @param payload_max_size Usable size of out_payload; must be > 0.
 * @param timeout_ms       Maximum time to wait for a frame to arrive.
 * @param out_payload_size Receives the actual payload size. Must not be NULL.
 * @param out_sequence     Optional; receives the frame's own
 *                         sequence_number (e.g. so a DATA frame's
 *                         sequence can be echoed back in an ACK - see
 *                         rte_dual_channel.h). May be NULL.
 * @return RTE_STATUS_OK (channel->expected_sequence has been advanced
 *         to this frame's sequence_number + 1); RTE_STATUS_INVALID_PARAM;
 *         RTE_STATUS_TIMEOUT if no frame arrives in time;
 *         RTE_STATUS_DATA_CORRUPTION if the frame's CRC-64 or sequence
 *         continuity check fails (channel->expected_sequence is left
 *         unchanged - see rte_dual_msgchannel_reset_sequence() for how
 *         to recover after a link (re)establishment rather than staying
 *         permanently out of sync); RTE_STATUS_HARDWARE_FAULT if a
 *         frame with an unexpected sender_id arrives (possible
 *         masquerade - treated the same as a link fault, not silently
 *         dropped-and-retried, since this is a defended-integrity
 *         violation rather than ordinary transient loss).
 */
rte_status_t rte_dual_msgchannel_receive(rte_dual_msgchannel_t *channel,
                                            uint8_t *out_payload,
                                            uint8_t payload_max_size,
                                            rte_duration_ms_t timeout_ms,
                                            uint8_t *out_payload_size,
                                            uint32_t *out_sequence);

/**
 * @brief Resets both sequence counters to 0. Intended to be called by
 *        the caller (typically rte_dual_channel_t) exactly once,
 *        immediately after config->link has been freshly (re)established
 *        with the peer - mirrors safeAPIRBC2oo2's own precedent
 *        (channel_ab.c's cycle_resync_requested handling after a peer
 *        link reconnects) for why a fresh link needs a fresh, mutually
 *        agreed starting sequence rather than fighting over whatever
 *        counters were left over from before the disconnect.
 * @param channel  Channel to reset. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if channel is NULL.
 */
rte_status_t rte_dual_msgchannel_reset_sequence(rte_dual_msgchannel_t *channel);

#ifdef __cplusplus
}
#endif

#endif /* RTE_DUAL_MSGCHANNEL_H */

/** @} */

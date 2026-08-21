/**
 * @file sapi_dual_msgchannel.h
 * @brief "Channel" layer of ADR-020: one EN 50159-defended message
 *        channel over one already-open sapi_netlink_handle_t.
 *
 * Every frame sent/received is a sapi_vital_message_t (sapi_checksum.h -
 * already the framework's established EN 50159-style envelope, reused
 * here rather than reinvented, the same way sapi_channel_checkpoint()
 * (ADR-017) already reuses it): sequence_number defends against
 * repetition/deletion/resequencing, sender_id (checked against
 * config->expected_peer_id on receive) defends against masquerade,
 * timestamp_ms supports delay/staleness detection by the caller, and
 * crc64 defends against corruption. See ADR-020 section 1.
 *
 * This is the lower of the two protocol layers ADR-020 defines -
 * sapi_dual_channel_t (sapi_dual_channel.h) is the upper layer, adding
 * its own DATA/ACK/STATE frame-kind header inside this layer's payload
 * and redundant-link fan-out on top of a single sapi_dual_msgchannel_t
 * like this one.
 *
 * REQ-DUAL-MSGCHANNEL-001: no dynamic allocation; caller supplies storage
 *                          and an already-open sapi_netlink_handle_t.
 * REQ-DUAL-MSGCHANNEL-002: send/receive never block longer than the
 *                          caller-supplied timeout_ms.
 * REQ-DUAL-MSGCHANNEL-003: sapi_checksum_crc64_init() must already have
 *                          been called by the application before any
 *                          sapi_dual_msgchannel_* call (module-global
 *                          singleton state, ADR-020 does not re-init it).
 * REQ-DUAL-MSGCHANNEL-004: sender_id is checked against
 *                          config->expected_peer_id before CRC/sequence
 *                          verification; a mismatch is rejected with
 *                          SAPI_STATUS_HARDWARE_FAULT without advancing
 *                          expected_sequence (masquerade defense).
 * REQ-DUAL-MSGCHANNEL-005: a CRC or sequence-continuity failure yields
 *                          SAPI_STATUS_DATA_CORRUPTION and leaves
 *                          expected_sequence unchanged.
 *
 * @defgroup DUAL
 * @{
 */
#ifndef SAPI_DUAL_MSGCHANNEL_H
#define SAPI_DUAL_MSGCHANNEL_H

#include <stdint.h>

#include "safeapi/checksum/sapi_checksum.h"
#include "safeapi/netlink/sapi_netlink.h"
#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Max payload bytes usable via sapi_dual_msgchannel_send()/
 *         _receive() - bounded by sapi_vital_message_t's own fixed
 *         248-byte payload field (sapi_checksum.h). */
#define SAPI_DUAL_MSGCHANNEL_MAX_PAYLOAD 248U

/**
 * @brief One sapi_dual_msgchannel_t instance's state. Caller-owned
 *        storage (REQ-DUAL-MSGCHANNEL-001); opaque in practice, exposed
 *        here (not via SAFEAPI_DECLARE_STORAGE) only because its size is
 *        already small and fixed - callers must still treat every field
 *        as private and only reach it through the functions below.
 */
typedef struct sapi_dual_msgchannel_s
{
    sapi_netlink_handle_t link;      /**< Caller-opened; NOT closed by sapi_dual_msgchannel_close(). */
    uint32_t              sender_id;      /**< This instance's own EN 50159 sender_id, stamped on every outbound frame. */
    uint32_t              expected_peer_id; /**< Inbound frames whose sender_id differs are rejected (masquerade defense). */
    uint32_t              next_sequence;   /**< Next outbound sequence_number. */
    uint32_t              expected_sequence; /**< Next expected inbound sequence_number (continuity check). */
} sapi_dual_msgchannel_t;

/** @brief Configuration for sapi_dual_msgchannel_init(). */
typedef struct sapi_dual_msgchannel_config_s
{
    /** Already-open link this channel sends/receives fixed-size
     *  sapi_vital_message_t frames over. sapi_netlink_open() must have
     *  been called with config->message_size == sizeof(sapi_vital_message_t)
     *  for that link. Must not be NULL; ownership (closing it) stays
     *  with the caller. */
    sapi_netlink_handle_t link;
    /** This instance's own EN 50159 sender identifier (e.g. a site or
     *  role ID), stamped on every frame this channel sends. */
    uint32_t sender_id;
    /** Expected sender_id of the peer at the other end of link; any
     *  inbound frame whose sender_id differs is rejected as a possible
     *  masquerade rather than delivered (ADR-020 section 1). */
    uint32_t expected_peer_id;
} sapi_dual_msgchannel_config_t;

/**
 * @brief Initializes a sapi_dual_msgchannel_t: starts both sequence
 *        counters at 0.
 * @param channel  Caller-owned storage to initialize. Must not be NULL.
 * @param config   Configuration. Must not be NULL; config->link must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM for a NULL argument.
 */
sapi_status_t sapi_dual_msgchannel_init(sapi_dual_msgchannel_t *channel,
                                         const sapi_dual_msgchannel_config_t *config);

/**
 * @brief Wraps payload in a sapi_vital_message_t (this channel's own
 *        sender_id and next sequence_number) and sends it over
 *        config->link, blocking at most timeout_ms.
 * @param channel         Initialized channel. Must not be NULL.
 * @param payload         Payload to send. May be NULL only if payload_size is 0.
 * @param payload_size    Payload size in bytes; must be <=
 *                        SAPI_DUAL_MSGCHANNEL_MAX_PAYLOAD.
 * @param timeout_ms      Maximum time to wait for the underlying
 *                        sapi_netlink_send() to complete (REQ-DUAL-MSGCHANNEL-002).
 * @param out_sequence    Optional; if not NULL, receives the
 *                        sequence_number this frame was sent with (the
 *                        caller needs this to correlate a later ACK -
 *                        see sapi_dual_channel.h). May be NULL.
 * @return SAPI_STATUS_OK (channel->next_sequence has been incremented);
 *         SAPI_STATUS_INVALID_PARAM; whatever sapi_netlink_send() or
 *         sapi_checksum_vital_message_create() returned on failure -
 *         channel->next_sequence is NOT incremented on failure, so a
 *         retried send reuses the same sequence_number.
 */
sapi_status_t sapi_dual_msgchannel_send(sapi_dual_msgchannel_t *channel,
                                         const uint8_t *payload,
                                         uint8_t payload_size,
                                         sapi_duration_ms_t timeout_ms,
                                         uint32_t *out_sequence);

/**
 * @brief Receives one frame over config->link, blocking at most
 *        timeout_ms, verifies its CRC-64 and sequence continuity
 *        (sapi_checksum_vital_message_verify() against
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
 *                         sapi_dual_channel.h). May be NULL.
 * @return SAPI_STATUS_OK (channel->expected_sequence has been advanced
 *         to this frame's sequence_number + 1); SAPI_STATUS_INVALID_PARAM;
 *         SAPI_STATUS_TIMEOUT if no frame arrives in time;
 *         SAPI_STATUS_DATA_CORRUPTION if the frame's CRC-64 or sequence
 *         continuity check fails (channel->expected_sequence is left
 *         unchanged - see sapi_dual_msgchannel_reset_sequence() for how
 *         to recover after a link (re)establishment rather than staying
 *         permanently out of sync); SAPI_STATUS_HARDWARE_FAULT if a
 *         frame with an unexpected sender_id arrives (possible
 *         masquerade - treated the same as a link fault, not silently
 *         dropped-and-retried, since this is a defended-integrity
 *         violation rather than ordinary transient loss).
 */
sapi_status_t sapi_dual_msgchannel_receive(sapi_dual_msgchannel_t *channel,
                                            uint8_t *out_payload,
                                            uint8_t payload_max_size,
                                            sapi_duration_ms_t timeout_ms,
                                            uint8_t *out_payload_size,
                                            uint32_t *out_sequence);

/**
 * @brief Resets both sequence counters to 0. Intended to be called by
 *        the caller (typically sapi_dual_channel_t) exactly once,
 *        immediately after config->link has been freshly (re)established
 *        with the peer - mirrors safeAPIRBC2oo2's own precedent
 *        (channel_ab.c's cycle_resync_requested handling after a peer
 *        link reconnects) for why a fresh link needs a fresh, mutually
 *        agreed starting sequence rather than fighting over whatever
 *        counters were left over from before the disconnect.
 * @param channel  Channel to reset. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM if channel is NULL.
 */
sapi_status_t sapi_dual_msgchannel_reset_sequence(sapi_dual_msgchannel_t *channel);

#ifdef __cplusplus
}
#endif

#endif /* SAPI_DUAL_MSGCHANNEL_H */

/** @} */

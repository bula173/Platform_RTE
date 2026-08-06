/**
 * @file sapi_dual_channel.h
 * @brief "DualChannel" layer of ADR-020: wraps 1..N redundant
 *        sapi_dual_msgchannel_t links for fault-tolerant, always-send +
 *        bounded-ACK-wait delivery.
 *
 * sapi_dual_channel_t has no knowledge of sapi_dual_negotiator_t (ADR-020
 * section 3's ownership direction: the negotiator depends on this
 * module, not the other way around) - it exposes two independent frame
 * flows over the same redundant links:
 *
 *  - sapi_dual_channel_send()/_receive(): application DATA, always sent
 *    on every configured link, each link's send waits up to
 *    config->ack_timeout_ms for that specific link's ACK before that
 *    link is counted as down for this round (ADR-020 section 2) -
 *    inbound DATA frames are auto-ACKed and staged for the next
 *    _receive() call.
 *  - sapi_dual_channel_send_state_frame()/_receive_state_frame(): a
 *    narrower, negotiator-facing pair for fire-and-forget
 *    sapi_dual_state_frame_t beacons (sapi_dual_frames.h), sharing the
 *    same redundant links and the same DOWN/DEGRADED/FULL bookkeeping,
 *    but never counted toward or against DATA's own ACK accounting.
 *
 * REQ-DUAL-CHANNEL-001: no dynamic allocation; fixed array of at most
 *                       SAPI_DUAL_CHANNEL_MAX_LINKS redundant links.
 * REQ-DUAL-CHANNEL-002: sapi_dual_channel_send() always transmits DATA
 *                       on every configured link, never gated by any
 *                       negotiated sapi_dual_state_t.
 * REQ-DUAL-CHANNEL-003: aggregate status is recomputed after every send
 *                       and status_callback fires only on an actual
 *                       change.
 * REQ-DUAL-CHANNEL-004: DATA and STATE frame flows share links and
 *                       status bookkeeping; STATE traffic is
 *                       fire-and-forget and never counted toward or
 *                       against DATA's own ACK accounting.
 * REQ-DUAL-CHANNEL-005: sapi_dual_channel_receive()/_receive_state_frame()
 *                       poll every configured link on every call, never
 *                       stopping early, so one link cannot starve
 *                       another of its own auto-ACK.
 * REQ-DUAL-CHANNEL-006: an inbound frame shorter than this layer's own
 *                       header, or shorter than its kind's full fixed
 *                       size, is reported as SAPI_STATUS_DATA_CORRUPTION.
 *
 * @defgroup DUAL
 * @{
 */
#ifndef SAPI_DUAL_CHANNEL_H
#define SAPI_DUAL_CHANNEL_H

#include <stdint.h>
#include <stdbool.h>

#include "safeapi/dual/sapi_dual_frames.h"
#include "safeapi/dual/sapi_dual_msgchannel.h"
#include "safeapi/dual/sapi_dual_types.h"
#include "safeapi/netlink/sapi_netlink.h"
#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of redundant links one sapi_dual_channel_t may
 *         be configured with. Fixed, not dynamic (REQ-OAL-COMMON-010). */
#define SAPI_DUAL_CHANNEL_MAX_LINKS 4U

/** @brief Max application payload bytes usable via
 *         sapi_dual_channel_send()/_receive() - Layer-1's own
 *         SAPI_DUAL_MSGCHANNEL_MAX_PAYLOAD minus this layer's 4-byte
 *         sapi_dual_frame_header_t. */
#define SAPI_DUAL_CHANNEL_MAX_PAYLOAD (SAPI_DUAL_MSGCHANNEL_MAX_PAYLOAD - 4U)

/**
 * @brief Optional callback invoked whenever sapi_dual_channel_get_status()'s
 *        value changes (ADR-020 section 2's "indicate to the user via
 *        callback if registered" requirement). Called synchronously from
 *        inside sapi_dual_channel_send() (the only place aggregate status
 *        is recomputed) - keep this fast; it is on the same call path as
 *        the send it was triggered by.
 * @param new_status  Status just transitioned to.
 * @param old_status  Status just transitioned from.
 * @param user_ctx    Caller-supplied context from sapi_dual_channel_config_t.
 */
typedef void (*sapi_dual_channel_status_callback_t)(sapi_dual_channel_status_t new_status,
                                                      sapi_dual_channel_status_t old_status,
                                                      void *user_ctx);

/**
 * @brief One sapi_dual_channel_t instance's state. Caller-owned storage;
 *        every field is private - reach it only through the functions
 *        below.
 */
typedef struct sapi_dual_channel_s
{
    sapi_dual_msgchannel_t links[SAPI_DUAL_CHANNEL_MAX_LINKS];
    uint32_t                link_count;
    bool                    link_up[SAPI_DUAL_CHANNEL_MAX_LINKS];
    sapi_dual_channel_status_t last_status;
    sapi_dual_channel_status_callback_t status_callback;
    void                   *status_callback_ctx;
    sapi_duration_ms_t      ack_timeout_ms;

    bool    pending_data_valid;
    uint8_t pending_data[SAPI_DUAL_CHANNEL_MAX_PAYLOAD];
    uint8_t pending_data_size;

    bool                    pending_state_valid;
    sapi_dual_state_frame_t pending_state;
} sapi_dual_channel_t;

/** @brief Configuration for sapi_dual_channel_init(). */
typedef struct sapi_dual_channel_config_s
{
    /** Already-open links, config->link_count of them, each with
     *  config->link_count == the number of entries actually populated
     *  here (1..SAPI_DUAL_CHANNEL_MAX_LINKS). Every link must have been
     *  opened with message_size == sizeof(sapi_vital_message_t)
     *  (sapi_dual_msgchannel.h). Ownership (closing them) stays with the
     *  caller. */
    sapi_netlink_handle_t links[SAPI_DUAL_CHANNEL_MAX_LINKS];
    /** Number of entries populated in links[]; 1..SAPI_DUAL_CHANNEL_MAX_LINKS. */
    uint32_t link_count;
    /** This instance's own EN 50159 sender identifier, shared by every
     *  redundant link (they connect the same two logical endpoints). */
    uint32_t sender_id;
    /** Expected sender_id of the peer at the other end of every link. */
    uint32_t expected_peer_id;
    /** Per-link budget sapi_dual_channel_send() waits for that link's
     *  own ACK before counting it down for this round (ADR-020 section 2). */
    sapi_duration_ms_t ack_timeout_ms;
    /** Optional; NULL = no callback. */
    sapi_dual_channel_status_callback_t status_callback;
    /** Opaque context passed back to status_callback. Ignored if
     *  status_callback is NULL. */
    void *status_callback_ctx;
} sapi_dual_channel_config_t;

/**
 * @brief Initializes a sapi_dual_channel_t: initializes every configured
 *        redundant link (sapi_dual_msgchannel_init()) and starts
 *        sapi_dual_channel_get_status() at SAPI_DUAL_CHANNEL_STATUS_DOWN
 *        (no traffic has been sent/received yet).
 * @param channel  Caller-owned storage to initialize. Must not be NULL.
 * @param config   Configuration. Must not be NULL; config->link_count
 *                 must be in [1, SAPI_DUAL_CHANNEL_MAX_LINKS]; every
 *                 config->links[0..link_count-1] must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM otherwise.
 */
sapi_status_t sapi_dual_channel_init(sapi_dual_channel_t *channel, const sapi_dual_channel_config_t *config);

/**
 * @brief Sends payload as a DATA frame on every configured redundant
 *        link - never gated by any negotiated state (ADR-020 section 2:
 *        the real payload traffic itself is the liveness check). For
 *        each link, waits up to config->ack_timeout_ms for that link's
 *        own ACK (matched by sequence number, not just "an ACK arrived")
 *        before moving on; a DATA or STATE frame received from the peer
 *        while waiting is still auto-ACKed/staged as a side effect, not
 *        dropped.
 *
 * After every configured link has been tried, recomputes
 * sapi_dual_channel_get_status() and invokes config->status_callback if
 * it changed (ADR-020 section 2).
 *
 * @param channel           Initialized channel. Must not be NULL.
 * @param payload           Payload to send. May be NULL only if payload_size is 0.
 * @param payload_size      Payload size in bytes; must be <=
 *                          SAPI_DUAL_CHANNEL_MAX_PAYLOAD.
 * @param out_ack_link_count Optional; if not NULL, receives how many of
 *                          the configured links ACKed this send. May be NULL.
 * @return SAPI_STATUS_OK if at least one link ACKed; SAPI_STATUS_TIMEOUT
 *         if none did (data was still transmitted-attempted on every
 *         link; this reports delivery confirmation, not transmission
 *         attempt); SAPI_STATUS_INVALID_PARAM for a bad argument.
 */
sapi_status_t sapi_dual_channel_send(sapi_dual_channel_t *channel,
                                      const uint8_t *payload,
                                      uint8_t payload_size,
                                      uint32_t *out_ack_link_count);

/**
 * @brief Returns the most recently staged inbound DATA frame, actively
 *        polling the configured links (each auto-ACked on arrival - see
 *        sapi_dual_channel_send()'s own doc) if none was already staged
 *        from a previous sapi_dual_channel_send() call's own incidental
 *        polling.
 * @param channel      Initialized channel. Must not be NULL.
 * @param out_payload  Destination buffer. Must not be NULL.
 * @param max_size     Usable size of out_payload; must be > 0.
 * @param timeout_ms   Maximum total time to actively poll the configured
 *                      links if nothing was already staged (split evenly
 *                      across links); 0 = check only what is already staged.
 * @param out_size     Receives the actual payload size. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_TIMEOUT if nothing arrived in time;
 *         SAPI_STATUS_INVALID_PARAM if a staged frame is larger than
 *         max_size (not silently truncated) or for a bad argument.
 */
sapi_status_t sapi_dual_channel_receive(sapi_dual_channel_t *channel,
                                         uint8_t *out_payload,
                                         uint8_t max_size,
                                         sapi_duration_ms_t timeout_ms,
                                         uint8_t *out_size);

/**
 * @brief Sends a sapi_dual_state_frame_t (sapi_dual_frames.h) on every
 *        configured redundant link - fire-and-forget, no ACK wait
 *        (unlike sapi_dual_channel_send()'s DATA frames), matching the
 *        periodic-beacon nature of state negotiation. Intended to be
 *        called by a sapi_dual_negotiator_t, not directly by application
 *        code - see ADR-020 section 3.
 * @param channel          Initialized channel. Must not be NULL.
 * @param state             This instance's own sapi_dual_state_t to advertise.
 * @param channel_degraded  This instance's own current
 *                          sapi_dual_channel_get_status() != FULL, as a
 *                          single bit (so the peer can tell HOTSTANDBY
 *                          from COLDSTANDBY for itself).
 * @param timestamp_ms       Caller-supplied value for the frame's own
 *                          timestamp_ms field. This function does not
 *                          source it from sapi_timer_now() itself: a
 *                          sapi_dual_negotiator_t needs the *same fixed*
 *                          value on every beacon it ever sends (captured
 *                          once at its own init) for its startup
 *                          tie-break to remain stable call to call - see
 *                          sapi_dual_state_frame_t's own doc.
 * @return SAPI_STATUS_OK if the frame was transmitted on at least one
 *         link; SAPI_STATUS_TIMEOUT if every link's send failed/timed
 *         out; SAPI_STATUS_INVALID_PARAM if channel is NULL.
 */
sapi_status_t sapi_dual_channel_send_state_frame(sapi_dual_channel_t *channel, sapi_dual_state_t state,
                                                   bool channel_degraded, uint64_t timestamp_ms);

/**
 * @brief Returns the most recently staged inbound STATE frame, actively
 *        polling the configured links if none was already staged.
 *        Intended to be called by a sapi_dual_negotiator_t - see
 *        ADR-020 section 3.
 * @param channel     Initialized channel. Must not be NULL.
 * @param timeout_ms  Maximum total time to actively poll if nothing was
 *                    already staged (split evenly across links); 0 =
 *                    check only what is already staged.
 * @param out_frame   Receives the frame. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_TIMEOUT if none arrived in time;
 *         SAPI_STATUS_INVALID_PARAM for a bad argument.
 */
sapi_status_t sapi_dual_channel_receive_state_frame(sapi_dual_channel_t *channel, sapi_duration_ms_t timeout_ms,
                                                      sapi_dual_state_frame_t *out_frame);

/**
 * @brief Returns the aggregate connection status across every configured
 *        redundant link, as of the most recent sapi_dual_channel_send()
 *        call (ADR-020 section 2). SAPI_DUAL_CHANNEL_STATUS_DOWN before
 *        the first send.
 * @param channel  Channel to query. May be NULL (returns DOWN, defensive default).
 * @return The current aggregate status.
 */
sapi_dual_channel_status_t sapi_dual_channel_get_status(const sapi_dual_channel_t *channel);

/**
 * @brief Returns whether one specific configured link is currently
 *        considered up, as of the most recent sapi_dual_channel_send()
 *        call - for diagnostics/logging (e.g. which specific redundant
 *        path is the one that's down), not a safety-decision input on
 *        its own (see sapi_dual_channel_get_status() for the aggregate).
 * @param channel      Channel to query. May be NULL (returns false).
 * @param link_index   Index into the configured links, 0..link_count-1.
 * @return true if that link's most recent send was ACKed in time; false
 *         if link_index is out of range or channel is NULL.
 */
bool sapi_dual_channel_is_link_up(const sapi_dual_channel_t *channel, uint32_t link_index);

#ifdef __cplusplus
}
#endif

#endif /* SAPI_DUAL_CHANNEL_H */

/** @} */

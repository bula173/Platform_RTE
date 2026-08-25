/**
 * @file sapi_dual_frames.h
 * @brief Layer-2 (DualChannel-owned) frame kind/header definitions for
 *        ADR-020 - shared between sapi_dual_channel.h and
 *        sapi_dual_negotiator.h so both can refer to the same wire
 *        structs without either depending on the other (sapi_dual_channel_t
 *        has no knowledge of sapi_dual_negotiator_t at all; the
 *        negotiator is the one-directional dependent - see ADR-020
 *        section 3 and this module's own README note on ownership
 *        direction).
 *
 * These structs travel inside a Layer-1 sapi_vital_message_t's own
 * 248-byte payload (sapi_dual_msgchannel.h) - i.e. every field here is
 * additional, application/protocol-meaning defense layered on top of
 * Layer 1's transport-integrity defense (sequence/sender/timestamp/CRC),
 * not a replacement for it. See ADR-020 section 1.
 *
 * @defgroup DUAL
 * @{
 */
#ifndef SAPI_DUAL_FRAMES_H
#define SAPI_DUAL_FRAMES_H

#include <stdint.h>

#include "safeapi/dual/sapi_dual_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Which kind of Layer-2 frame a given Layer-1 payload holds.
 *        Stored as the first byte of every sapi_dual_channel_t frame so
 *        DATA, ACK, and STATE traffic sharing the same redundant links
 *        can never be misinterpreted as each other.
 *
 * Explicit numeric values are fixed and part of the wire format: do not
 * renumber existing entries, only append.
 */
typedef enum sapi_dual_frame_kind_e
{
    /** Application payload (sapi_dual_channel_send()/_receive()); expects
     *  a SAPI_DUAL_FRAME_KIND_ACK frame back. */
    SAPI_DUAL_FRAME_KIND_DATA = 0,
    /** Acknowledges one DATA frame's sequence_number (acked_sequence
     *  below) - checked, not just "some ACK arrived", against the
     *  specific send it corresponds to (ADR-020 section 1). */
    SAPI_DUAL_FRAME_KIND_ACK = 1,
    /** sapi_dual_negotiator_t's own state beacon - fire-and-forget,
     *  periodic, no ACK expected (unlike DATA). */
    SAPI_DUAL_FRAME_KIND_STATE = 2,
    /** Liveness heartbeat frame - sent periodically to maintain connection,
     *  expects a SAPI_DUAL_FRAME_KIND_ACK frame back. */
    SAPI_DUAL_FRAME_KIND_HEARTBEAT = 3
} sapi_dual_frame_kind_t;

/** @brief Common 4-byte header prefixing every Layer-2 frame; kept a
 *         fixed 4 bytes (not just 1) so the fields that follow in
 *         sapi_dual_ack_frame_t/sapi_dual_state_frame_t stay naturally
 *         aligned. */
typedef struct sapi_dual_frame_header_s
{
    /** A sapi_dual_frame_kind_t value, stored as uint8_t for a stable
     *  wire size regardless of the enum's underlying compiler type. */
    uint8_t kind;
    /** Reserved, always 0 on send; ignored (not rejected) on receive so
     *  a future minor revision can use these bits without breaking this
     *  one's receivers. */
    uint8_t reserved[3];
} sapi_dual_frame_header_t;

/** @brief SAPI_DUAL_FRAME_KIND_ACK payload. */
typedef struct sapi_dual_ack_frame_s
{
    sapi_dual_frame_header_t header;
    /** The DATA frame's own Layer-1 sequence_number this ACK confirms -
     *  checked by the sender against the sequence it actually sent with,
     *  not merely "an ACK arrived" (ADR-020 section 1). */
    uint32_t acked_sequence;
} sapi_dual_ack_frame_t;

/** @brief SAPI_DUAL_FRAME_KIND_STATE payload - sapi_dual_negotiator_t's
 *         own periodic beacon. */
typedef struct sapi_dual_state_frame_s
{
    sapi_dual_frame_header_t header;
    /** Sender's own sapi_dual_state_t, stored as uint8_t for a stable
     *  wire size. */
    uint8_t state;
    /** Sender's own DualChannel aggregate status at beacon time: 0 =
     *  SAPI_DUAL_CHANNEL_STATUS_FULL, 1 = SAPI_DUAL_CHANNEL_STATUS_DEGRADED
     *  or SAPI_DUAL_CHANNEL_STATUS_DOWN. This is what lets the *peer*
     *  distinguish HOTSTANDBY from COLDSTANDBY for itself - see ADR-020
     *  section 3. */
    uint8_t channel_degraded;
    uint16_t reserved;
    /** Sender's own fixed value, captured ONCE (typically at
     *  sapi_dual_negotiator_init() time) and repeated unchanged on every
     *  beacon this instance ever sends - NOT a live "now" refreshed each
     *  send. Used for the same older-timestamp-wins startup tie-break
     *  safeAPIRBC2oo2's site.c uses today (decide_online()): both sides
     *  must keep sending the *same* value across repeated beacons for
     *  that comparison to stay stable and reproducible rather than
     *  racing by whatever margin two live clocks happened to differ by
     *  on a given cycle. Not a cross-site clock synchronization claim
     *  either way (see sapi_clocksync.h's own file-level note: never
     *  assume zero jitter). */
    uint64_t timestamp_ms;
} sapi_dual_state_frame_t;

/** @brief SAPI_DUAL_FRAME_KIND_HEARTBEAT payload - connection maintenance heartbeat. */
typedef struct sapi_dual_heartbeat_frame_s
{
    sapi_dual_frame_header_t header;
    /** Monotonic sender timestamp */
    uint64_t timestamp_ms;
} sapi_dual_heartbeat_frame_t;

#ifdef __cplusplus
}
#endif

#endif /* SAPI_DUAL_FRAMES_H */

/** @} */

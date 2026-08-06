/**
 * @file sapi_dual_negotiator.h
 * @brief Dual state negotiator of ADR-020: decides sapi_dual_state_t for
 *        both this instance and its peer, driven over an attached
 *        sapi_dual_channel_t's STATE-frame flow.
 *
 * One-directional dependency (ADR-020 section 3): sapi_dual_negotiator_t
 * depends on an already-initialized sapi_dual_channel_t (given to it at
 * config time) and drives its own periodic beacon traffic through that
 * channel's sapi_dual_channel_send_state_frame()/_receive_state_frame()
 * pair; sapi_dual_channel_t itself has no knowledge of this module.
 *
 * Intended call pattern: sapi_dual_negotiator_execute() once per
 * application cycle (the same way sapi_channel_checkpoint() is driven
 * today - ADR-017), after the application has already driven
 * sapi_dual_channel_send()/_receive() for its own real payload traffic
 * that cycle if it has any.
 *
 * REQ-DUAL-NEGOTIATOR-001: no dynamic allocation; caller supplies
 *                          storage and an already-initialized
 *                          sapi_dual_channel_t.
 * REQ-DUAL-NEGOTIATOR-002: sapi_dual_negotiator_execute() shall never
 *                          call sapi_safestate_enter() itself - deciding
 *                          what a sustained SAPI_DUAL_STATE_UNKNOWN means
 *                          for safety stays an application policy
 *                          decision (ADR-020 section 4's "no automatic
 *                          safety reaction" non-goal).
 * REQ-DUAL-NEGOTIATOR-003: the initial ONLINE-vs-STANDBY decision uses
 *                          an older-startup-timestamp-wins rule, with
 *                          own_id/peer_id as a deterministic fallback
 *                          only on an exact timestamp tie.
 * REQ-DUAL-NEGOTIATOR-004: whichever side is currently STANDBY has its
 *                          HOT/COLD label derived from the ONLINE side's
 *                          own channel-degradation bit - never from the
 *                          STANDBY side's own self-report, and never
 *                          from the ONLINE side's opinion of itself.
 * REQ-DUAL-NEGOTIATOR-005: loss of peer contact past
 *                          config->peer_lost_timeout_ms sets peer_state
 *                          to UNKNOWN; own_state degrades to UNKNOWN too
 *                          unless it was already ONLINE, which stays
 *                          ONLINE.
 *
 * @defgroup DUAL
 * @{
 */
#ifndef SAPI_DUAL_NEGOTIATOR_H
#define SAPI_DUAL_NEGOTIATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "safeapi/dual/sapi_dual_channel.h"
#include "safeapi/dual/sapi_dual_types.h"
#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Optional callback invoked whenever either the own or the peer
 *        sapi_dual_state_t changes, from inside sapi_dual_negotiator_execute().
 * @param new_own_state   Own state just transitioned to.
 * @param old_own_state   Own state just transitioned from.
 * @param new_peer_state  Peer state just transitioned to.
 * @param old_peer_state  Peer state just transitioned from.
 * @param user_ctx        Caller-supplied context from
 *                        sapi_dual_negotiator_config_t.
 */
typedef void (*sapi_dual_negotiator_state_change_callback_t)(sapi_dual_state_t new_own_state,
                                                               sapi_dual_state_t old_own_state,
                                                               sapi_dual_state_t new_peer_state,
                                                               sapi_dual_state_t old_peer_state, void *user_ctx);

/**
 * @brief One sapi_dual_negotiator_t instance's state. Caller-owned
 *        storage; every field is private - reach it only through the
 *        functions below.
 */
typedef struct sapi_dual_negotiator_s
{
    sapi_dual_channel_t *channel;
    uint32_t              own_id;
    uint32_t              peer_id;
    sapi_duration_ms_t     peer_lost_timeout_ms;
    sapi_dual_negotiator_state_change_callback_t state_change_callback;
    void                  *state_change_callback_ctx;

    uint64_t own_startup_timestamp_ms;
    bool     own_channel_degraded;

    bool     have_peer_startup_timestamp;
    uint64_t peer_startup_timestamp_ms;
    bool     peer_channel_degraded;

    sapi_dual_state_t own_state;
    sapi_dual_state_t peer_state;

    bool                have_last_peer_seen;
    sapi_timestamp_ms_t last_peer_seen_ms;
} sapi_dual_negotiator_t;

/** @brief Configuration for sapi_dual_negotiator_init(). */
typedef struct sapi_dual_negotiator_config_s
{
    /** Already-initialized DualChannel this negotiator drives its own
     *  STATE beacon traffic through. Ownership stays with the caller -
     *  not closed/deinitialized by this module. Must not be NULL. */
    sapi_dual_channel_t *channel;
    /** This instance's own tie-break identifier for the startup
     *  ONLINE/STANDBY decision (older timestamp wins; this ID is only
     *  consulted if both sides' startup timestamps are exactly equal). */
    uint32_t own_id;
    /** Expected peer's own tie-break identifier, for the same comparison. */
    uint32_t peer_id;
    /** How long without a valid peer STATE frame before
     *  sapi_dual_negotiator_get_peer_state() degrades to
     *  SAPI_DUAL_STATE_UNKNOWN (and, if this instance's own state was
     *  HOTSTANDBY/COLDSTANDBY/IDLE, its own state degrades too - an
     *  already-ONLINE instance's own state does not - see
     *  sapi_dual_negotiator_execute()'s own doc). */
    sapi_duration_ms_t peer_lost_timeout_ms;
    /** Optional; NULL = no callback. */
    sapi_dual_negotiator_state_change_callback_t state_change_callback;
    /** Opaque context passed back to state_change_callback. Ignored if
     *  state_change_callback is NULL. */
    void *state_change_callback_ctx;
} sapi_dual_negotiator_config_t;

/**
 * @brief Initializes a sapi_dual_negotiator_t. Captures this instance's
 *        own startup timestamp once (sapi_timer_now(), degrading to 0 if
 *        no timer backend is registered - same best-effort posture as
 *        sapi_log_write_event()'s Timestamp field) for use on every
 *        beacon this negotiator ever sends - see
 *        sapi_dual_state_frame_t's own doc on why this must stay fixed.
 *        Both own and peer state start at SAPI_DUAL_STATE_IDLE.
 * @param negotiator  Caller-owned storage to initialize. Must not be NULL.
 * @param config      Configuration. Must not be NULL; config->channel must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM otherwise.
 */
sapi_status_t sapi_dual_negotiator_init(sapi_dual_negotiator_t *negotiator,
                                         const sapi_dual_negotiator_config_t *config);

/**
 * @brief Drives one round of state negotiation: refreshes
 *        own_channel_degraded from the attached channel's current
 *        sapi_dual_channel_get_status(), sends this instance's own
 *        beacon, drains and processes every currently-pending inbound
 *        STATE frame, then recomputes both own and peer
 *        sapi_dual_state_t:
 *
 *        - Never yet heard from the peer: both states are
 *          SAPI_DUAL_STATE_IDLE.
 *        - Heard from the peer before but not within
 *          config->peer_lost_timeout_ms: peer_state becomes
 *          SAPI_DUAL_STATE_UNKNOWN; own_state becomes
 *          SAPI_DUAL_STATE_UNKNOWN too UNLESS it was already
 *          SAPI_DUAL_STATE_ONLINE (an active instance keeps acting
 *          ONLINE without needing continuous peer confirmation - see
 *          ADR-020 section 4's "no automatic safety reaction": this
 *          negotiator reports UNKNOWN, it does not itself decide to stop).
 *        - Peer contact current, own_state still IDLE or UNKNOWN: decides
 *          ONLINE vs STANDBY via the older-startup-timestamp-wins /
 *          own_id-vs-peer_id tie-break (ADR-020 section 3). Losing the
 *          tie-break (this instance is STANDBY) sets own_state to
 *          HOTSTANDBY/COLDSTANDBY from the *peer's* own last-reported
 *          channel_degraded bit (they are ONLINE - their degradation is
 *          what determines how well-backed this standby instance is).
 *          Winning it (this instance is ONLINE) sets peer_state to
 *          HOTSTANDBY/COLDSTANDBY from *this instance's own*
 *          sapi_dual_channel_get_status() instead - the peer is standing
 *          by for *this* instance, so it is this instance's own
 *          degradation that determines the peer's HOT/COLD, not
 *          whatever the peer last reported about itself.
 *        - Peer contact current, own_state already decided: stays
 *          decided; whichever side's HOTSTANDBY/COLDSTANDBY value is
 *          meaningful (the STANDBY side's own_state, or the ONLINE
 *          side's peer_state) refines each call using the same
 *          own-degradation-determines-the-*other*-side's-HOT/COLD rule
 *          above.
 *
 *        Invokes config->state_change_callback if either state changed.
 * @param negotiator          Initialized negotiator. Must not be NULL.
 * @param receive_timeout_ms  Maximum time to wait for at least one
 *                            inbound STATE frame if none is already
 *                            pending on the attached channel (passed
 *                            through to sapi_dual_channel_receive_state_frame());
 *                            0 = do not block, only process what is
 *                            already pending.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM if negotiator is NULL.
 */
sapi_status_t sapi_dual_negotiator_execute(sapi_dual_negotiator_t *negotiator, sapi_duration_ms_t receive_timeout_ms);

/**
 * @brief Returns this instance's own current sapi_dual_state_t.
 * @param negotiator  Negotiator to query. May be NULL (returns
 *                    SAPI_DUAL_STATE_IDLE, defensive default).
 * @return The current own state.
 */
sapi_dual_state_t sapi_dual_negotiator_get_own_state(const sapi_dual_negotiator_t *negotiator);

/**
 * @brief Returns this instance's last-known view of the peer's
 *        sapi_dual_state_t.
 * @param negotiator  Negotiator to query. May be NULL (returns
 *                    SAPI_DUAL_STATE_IDLE, defensive default).
 * @return The current peer state.
 */
sapi_dual_state_t sapi_dual_negotiator_get_peer_state(const sapi_dual_negotiator_t *negotiator);

#ifdef __cplusplus
}
#endif

#endif /* SAPI_DUAL_NEGOTIATOR_H */

/** @} */

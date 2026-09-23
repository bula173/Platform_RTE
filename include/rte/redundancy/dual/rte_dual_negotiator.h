/**
 * @file rte_dual_negotiator.h
 * @brief Dual state negotiator of ADR-020: decides rte_dual_state_t for
 *        both this instance and its peer, driven over an attached
 *        rte_dual_channel_t's STATE-frame flow.
 *
 * One-directional dependency (ADR-020 section 3): rte_dual_negotiator_t
 * depends on an already-initialized rte_dual_channel_t (given to it at
 * config time) and drives its own periodic beacon traffic through that
 * channel's rte_dual_channel_send_state_frame()/_receive_state_frame()
 * pair; rte_dual_channel_t itself has no knowledge of this module.
 *
 * Intended call pattern: rte_dual_negotiator_execute() once per
 * application cycle (the same way rte_channel_checkpoint() is driven
 * today - ADR-017), after the application has already driven
 * rte_dual_channel_send()/_receive() for its own real payload traffic
 * that cycle if it has any.
 *
 * REQ-DUAL-NEGOTIATOR-001: no dynamic allocation; caller supplies
 *                          storage and an already-initialized
 *                          rte_dual_channel_t.
 * REQ-DUAL-NEGOTIATOR-002: rte_dual_negotiator_execute() shall never
 *                          call rte_safestate_enter() itself - deciding
 *                          what a sustained RTE_DUAL_STATE_UNKNOWN means
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
#ifndef RTE_DUAL_NEGOTIATOR_H
#define RTE_DUAL_NEGOTIATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "rte/redundancy/dual/rte_dual_channel.h"
#include "rte/redundancy/dual/rte_dual_types.h"
#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Optional callback invoked whenever either the own or the peer
 *        rte_dual_state_t changes, from inside rte_dual_negotiator_execute().
 * @param new_own_state   Own state just transitioned to.
 * @param old_own_state   Own state just transitioned from.
 * @param new_peer_state  Peer state just transitioned to.
 * @param old_peer_state  Peer state just transitioned from.
 * @param user_ctx        Caller-supplied context from
 *                        rte_dual_negotiator_config_t.
 */
typedef void (*rte_dual_negotiator_state_change_callback_t)(rte_dual_state_t new_own_state,
                                                               rte_dual_state_t old_own_state,
                                                               rte_dual_state_t new_peer_state,
                                                               rte_dual_state_t old_peer_state, void *user_ctx);

/**
 * @brief One rte_dual_negotiator_t instance's state. Caller-owned
 *        storage; every field is private - reach it only through the
 *        functions below.
 */
typedef struct rte_dual_negotiator_s
{
    rte_dual_channel_t *channel;
    uint32_t              own_id;
    uint32_t              peer_id;
    rte_duration_ms_t     peer_lost_timeout_ms;
    rte_dual_negotiator_state_change_callback_t state_change_callback;
    void                  *state_change_callback_ctx;

    uint64_t own_startup_timestamp_ms;
    bool     own_channel_degraded;

    bool     have_peer_startup_timestamp;
    uint64_t peer_startup_timestamp_ms;
    bool     peer_channel_degraded;

    rte_dual_state_t own_state;
    rte_dual_state_t peer_state;

    bool                have_last_peer_seen;
    rte_timestamp_ms_t last_peer_seen_ms;
} rte_dual_negotiator_t;

/** @brief Configuration for rte_dual_negotiator_init(). */
typedef struct rte_dual_negotiator_config_s
{
    /** Already-initialized DualChannel this negotiator drives its own
     *  STATE beacon traffic through. Ownership stays with the caller -
     *  not closed/deinitialized by this module. Must not be NULL. */
    rte_dual_channel_t *channel;
    /** This instance's own tie-break identifier for the startup
     *  ONLINE/STANDBY decision (older timestamp wins; this ID is only
     *  consulted if both sides' startup timestamps are exactly equal). */
    uint32_t own_id;
    /** Expected peer's own tie-break identifier, for the same comparison. */
    uint32_t peer_id;
    /** How long without a valid peer STATE frame before
     *  rte_dual_negotiator_get_peer_state() degrades to
     *  RTE_DUAL_STATE_UNKNOWN (and, if this instance's own state was
     *  HOTSTANDBY/COLDSTANDBY/IDLE, its own state degrades too - an
     *  already-ONLINE instance's own state does not - see
     *  rte_dual_negotiator_execute()'s own doc). */
    rte_duration_ms_t peer_lost_timeout_ms;
    /** Optional; NULL = no callback. */
    rte_dual_negotiator_state_change_callback_t state_change_callback;
    /** Opaque context passed back to state_change_callback. Ignored if
     *  state_change_callback is NULL. */
    void *state_change_callback_ctx;
    /** Optional: seeds own_state instead of defaulting to RTE_DUAL_STATE_IDLE - for a caller
     *  resuming an already-established identity across a transient reconnect (a fresh
     *  channel/link, but not a fresh instance), where re-litigating the startup tie-break from
     *  scratch would risk a spurious promotion/demotion flip-flop. RTE_DUAL_STATE_IDLE (the
     *  zero value) means "no resume, use the default" - an existing, zero-initialized config
     *  gets exactly today's behavior. peer_state always starts fresh at IDLE regardless (the
     *  peer's own state is not something this instance can safely assume across a reconnect). */
    rte_dual_state_t resume_own_state;
    /** Optional: seeds own_startup_timestamp_ms instead of capturing a fresh rte_timer_now() -
     *  same "resume an identity across reconnect" use case as resume_own_state above, but an
     *  independent choice: a caller that is still negotiating (own_state not yet decided) may
     *  still want its ORIGINAL startup timestamp preserved across a reconnect, so a fresh one
     *  captured mid-negotiation cannot flip the tie-break outcome against its peer - resuming
     *  the timestamp does not require also resuming own_state. 0 (the zero value, never a
     *  legitimate timestamp) means "no resume, capture fresh" - an existing, zero-initialized
     *  config gets exactly today's behavior. */
    uint64_t resume_own_startup_timestamp_ms;
} rte_dual_negotiator_config_t;

/**
 * @brief Initializes a rte_dual_negotiator_t. Captures this instance's
 *        own startup timestamp once (rte_timer_now(), degrading to 0 if
 *        no timer OSAdapter is registered - same best-effort posture as
 *        rte_log_write_event()'s Timestamp field) for use on every
 *        beacon this negotiator ever sends - see
 *        rte_dual_state_frame_t's own doc on why this must stay fixed -
 *        UNLESS config->resume_own_state/resume_own_startup_timestamp_ms
 *        opt into resuming a prior identity instead (see their own doc).
 *        peer_state always starts at RTE_DUAL_STATE_IDLE; own_state does
 *        too unless a resume was requested.
 * @param negotiator  Caller-owned storage to initialize. Must not be NULL.
 * @param config      Configuration. Must not be NULL; config->channel must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM otherwise.
 */
rte_status_t rte_dual_negotiator_init(rte_dual_negotiator_t *negotiator,
                                         const rte_dual_negotiator_config_t *config);

/**
 * @brief Drives one round of state negotiation: refreshes
 *        own_channel_degraded from the attached channel's current
 *        rte_dual_channel_get_status(), sends this instance's own
 *        beacon, drains and processes every currently-pending inbound
 *        STATE frame, then recomputes both own and peer
 *        rte_dual_state_t:
 *
 *        - Never yet heard from the peer: both states are
 *          RTE_DUAL_STATE_IDLE.
 *        - Heard from the peer before but not within
 *          config->peer_lost_timeout_ms: peer_state becomes
 *          RTE_DUAL_STATE_UNKNOWN; own_state becomes
 *          RTE_DUAL_STATE_UNKNOWN too UNLESS it was already
 *          RTE_DUAL_STATE_ONLINE (an active instance keeps acting
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
 *          rte_dual_channel_get_status() instead - the peer is standing
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
 *                            through to rte_dual_channel_receive_state_frame());
 *                            0 = do not block, only process what is
 *                            already pending.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if negotiator is NULL.
 */
rte_status_t rte_dual_negotiator_execute(rte_dual_negotiator_t *negotiator, rte_duration_ms_t receive_timeout_ms);

/**
 * @brief Returns this instance's own current rte_dual_state_t.
 * @param negotiator  Negotiator to query. May be NULL (returns
 *                    RTE_DUAL_STATE_IDLE, defensive default).
 * @return The current own state.
 */
rte_dual_state_t rte_dual_negotiator_get_own_state(const rte_dual_negotiator_t *negotiator);

/**
 * @brief Returns this instance's last-known view of the peer's
 *        rte_dual_state_t.
 * @param negotiator  Negotiator to query. May be NULL (returns
 *                    RTE_DUAL_STATE_IDLE, defensive default).
 * @return The current peer state.
 */
rte_dual_state_t rte_dual_negotiator_get_peer_state(const rte_dual_negotiator_t *negotiator);

/**
 * @brief Returns this instance's own startup timestamp (captured at
 *        rte_dual_negotiator_init() time, or resumed from config - see
 *        rte_dual_negotiator_config_t::resume_own_startup_timestamp_ms).
 *        Diagnostic use (e.g. logging alongside the peer's own, below) -
 *        never itself consulted by this module's own tie-break logic
 *        after init.
 * @param negotiator  Negotiator to query. May be NULL (returns 0).
 * @return The own startup timestamp, in milliseconds.
 */
uint64_t rte_dual_negotiator_get_own_startup_timestamp_ms(const rte_dual_negotiator_t *negotiator);

/**
 * @brief Returns this instance's last-known view of the peer's own
 *        startup timestamp (0/unset until the first valid STATE frame
 *        arrives). Diagnostic use only, same posture as
 *        rte_dual_negotiator_get_own_startup_timestamp_ms() above.
 * @param negotiator  Negotiator to query. May be NULL (returns 0).
 * @return The peer's own last-reported startup timestamp, in milliseconds.
 */
uint64_t rte_dual_negotiator_get_peer_startup_timestamp_ms(const rte_dual_negotiator_t *negotiator);

/**
 * @brief Returns this instance's own tie-break identifier
 *        (rte_dual_negotiator_config_t::own_id at init time). Diagnostic
 *        use only.
 * @param negotiator  Negotiator to query. May be NULL (returns 0).
 * @return The own tie-break identifier.
 */
uint32_t rte_dual_negotiator_get_own_id(const rte_dual_negotiator_t *negotiator);

/**
 * @brief Returns the expected peer's tie-break identifier
 *        (rte_dual_negotiator_config_t::peer_id at init time). Diagnostic
 *        use only.
 * @param negotiator  Negotiator to query. May be NULL (returns 0).
 * @return The expected peer's tie-break identifier.
 */
uint32_t rte_dual_negotiator_get_peer_id(const rte_dual_negotiator_t *negotiator);

#ifdef __cplusplus
}
#endif

#endif /* RTE_DUAL_NEGOTIATOR_H */

/** @} */

/**
 * @file rte_site_role_policy.h
 * @brief Site-role policy: one pure decision function for "should this channel promote, demote or stay?" and the
 *        timing predicates of standby sync, takeover and the both-STANDBY tie-break.
 *
 * ADR-040 step 1: the Capella model places site-role negotiation in RTE Redundancy Services; the application keeps a
 * declarative Site Role Client. This header is the pure part of that service, moved unchanged from RBC_GP
 * (ab_gp_site_role, ab_gp_standby_policy). No I/O, no globals: facts come from the caller, the answer is one action
 * per cycle, and the both-STANDBY counter is passed in and returned.
 *
 * A channel becomes ONLINE only through:
 *  1. negotiation - startup, the both-STANDBY tie-break, or joining an ONLINE A/B sibling;
 *  2. the counterpart's FAULTED (about-to-reboot) notice;
 *  3. a dispatcher confirmation that the other site is not ONLINE, while the counterpart is silent.
 * Silence alone never promotes.
 *
 * REQ-SITEROLE-001: rte_site_role_decide() evaluates the rules in the documented order; the first that applies wins.
 * REQ-SITEROLE-002: a promotion is carried out only when the pair is ready (else WAIT).
 * REQ-SITEROLE-003: silence alone never promotes (rte_site_role_takeover_allowed needs a valid confirmation).
 * REQ-SITEROLE-004: a restarted cycle counter makes a takeover confirmation expired and a held snapshot stale.
 */
#ifndef RTE_SITE_ROLE_POLICY_H
#define RTE_SITE_ROLE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Cycles STANDBY may go without a valid snapshot before it is treated as cold. */
#define RTE_SITE_ROLE_STANDBY_DATA_TIMEOUT_CYCLES 3U

/** While STANDBY is considered not working, ONLINE retries the sync every this many cycles. */
#define RTE_SITE_ROLE_STANDBY_SYNC_RETRY_CYCLES 10U

/** How long (cycles) a dispatcher takeover confirmation stays valid. */
#define RTE_SITE_ROLE_TAKEOVER_CONFIRM_VALID_CYCLES 120U

/** Consecutive cycles both sites must be STANDBY (counterpart responding, neither ONLINE nor FAULTED) before the tie-break. */
#define RTE_SITE_ROLE_BOTH_STANDBY_CYCLES 6U

/** What the channel should do this cycle. */
typedef enum
{
    RTE_SITE_ROLE_ACTION_NONE = 0,    /**< stay as is */
    RTE_SITE_ROLE_ACTION_PROMOTE,     /**< become ONLINE */
    RTE_SITE_ROLE_ACTION_DEMOTE,      /**< become STANDBY */
    RTE_SITE_ROLE_ACTION_RENEGOTIATE, /**< both sites FAULTED: deterministic re-negotiation (WEST wins) */
    RTE_SITE_ROLE_ACTION_ALARM,       /**< a state that is not resolved automatically: report it loudly */
    RTE_SITE_ROLE_ACTION_WAIT         /**< a promotion is due but not yet allowed: waiting for this channel and its A/B sibling to be ready */
} rte_site_role_action_t;

/** Why (the log line, the status frame and the tests use it). */
typedef enum
{
    RTE_SITE_ROLE_REASON_NONE = 0,
    RTE_SITE_ROLE_REASON_BOTH_FAULTED,         /**< own and counterpart FAULTED */
    RTE_SITE_ROLE_REASON_PEER_FAULTED,         /**< counterpart announced FAULTED / reboot */
    RTE_SITE_ROLE_REASON_RECOVERED,            /**< was FAULTED, counterpart healthy ONLINE */
    RTE_SITE_ROLE_REASON_DISPATCHER_CONFIRMED, /**< silent counterpart, dispatcher confirmed it is not ONLINE */
    RTE_SITE_ROLE_REASON_BOTH_STANDBY_WEST,    /**< negotiation tie-break: both STANDBY, WEST wins */
    RTE_SITE_ROLE_REASON_BOTH_STANDBY_SIBLING, /**< joined the ONLINE A/B sibling of this site */
    RTE_SITE_ROLE_REASON_YIELD_TO_TAKEOVER,    /**< was cut off while ONLINE, counterpart took over meanwhile */
    RTE_SITE_ROLE_REASON_DUAL_ONLINE,          /**< both ONLINE without a lost-contact history (alarm) */
    RTE_SITE_ROLE_REASON_WAITING_READY         /**< promotion held back until both A/B channels confirm they are ready */
} rte_site_role_reason_t;

/** Facts the decision needs. */
typedef struct
{
    bool     is_online;              /**< this channel is ONLINE */
    bool     own_faulted;            /**< this channel is FAULTED */
    bool     is_west;                /**< this channel belongs to the WEST site */
    bool     link_up;                /**< the inter-site negotiation link is open */
    bool     peer_unresponsive;      /**< the link was declared dead while this channel was STANDBY */
    uint32_t cycle;                  /**< current cycle */
    uint32_t peer_last_rx_cycle;     /**< cycle of the newest payload from the counterpart */
    bool     peer_online_now;        /**< the newest payload says the counterpart is ONLINE */
    bool     peer_faulted;           /**< the newest payload says the counterpart is FAULTED */
    bool     peer_snapshot_valid;    /**< a snapshot from an ONLINE counterpart is held (may be stale) */
    bool     sibling_online;         /**< the A/B sibling channel of this site reports ONLINE */
    bool     takeover_confirmed;     /**< dispatcher confirmation received */
    uint32_t takeover_confirm_cycle; /**< cycle at which it was received */
    bool     lost_peer_while_online; /**< this channel lost its counterpart while ONLINE */
    uint32_t both_standby_cycles;    /**< consecutive cycles both sites were STANDBY, as returned last time */
    bool     own_ready;              /**< this channel confirms it is ready to be ONLINE (application readiness) */
    bool     sibling_ready;          /**< the A/B sibling confirms the same (its site-state frame) */
    bool     sibling_reachable;      /**< the A/B sibling's site-state frame arrived within the last 3 cycles */
} rte_site_role_input_t;

/** The answer. */
typedef struct
{
    rte_site_role_action_t action;
    rte_site_role_reason_t reason;
    uint32_t               both_standby_cycles; /**< the counter to keep for the next call */
} rte_site_role_decision_t;

/**
 * @brief Decides this cycle's role action. Rules are evaluated in this order and the first that applies wins:
 *  1. own FAULTED and counterpart FAULTED: RENEGOTIATE.
 *  2. this channel is STANDBY, healthy, counterpart FAULTED: PROMOTE (PEER_FAULTED).
 *  3. own FAULTED, counterpart healthy and holding an ONLINE snapshot: DEMOTE (RECOVERED).
 *  4. STANDBY, counterpart silent, dispatcher confirmation valid: PROMOTE (DISPATCHER_CONFIRMED).
 *  5. ONLINE and the counterpart, answering within 2 cycles, is ONLINE too: DEMOTE (YIELD_TO_TAKEOVER) if this channel
 *     was cut off while ONLINE, otherwise ALARM (DUAL_ONLINE).
 *  6. STANDBY, link up, counterpart answering within 2 cycles and not ONLINE for RTE_SITE_ROLE_BOTH_STANDBY_CYCLES
 *     cycles (or 1 cycle when the A/B sibling is already ONLINE): PROMOTE (BOTH_STANDBY_WEST / _SIBLING).
 *  7. STANDBY, A/B sibling ONLINE, own link view unusable and counterpart not verifiably ONLINE: follow the sibling
 *     after 2 cycles, PROMOTE (BOTH_STANDBY_SIBLING).
 *
 * A promotion is only carried out when this channel is ready AND its A/B sibling is ONLINE already, or confirms it is
 * ready, or cannot be reached (single channel). Otherwise the answer is WAIT.
 * @param in facts; must not be NULL
 * @return the decision; both_standby_cycles must be stored by the caller
 */
rte_site_role_decision_t rte_site_role_decide(const rte_site_role_input_t *in);

/** @return a short text for a reason, for logs. */
const char *rte_site_role_reason_text(rte_site_role_reason_t reason);

/** @return a one-word token (no spaces) for a reason, for the status frame. */
const char *rte_site_role_reason_token(rte_site_role_reason_t reason);

/**
 * @brief Whether ONLINE should attempt the pre-commit standby sync this cycle.
 * @param sync_enabled  true when standby_mode is warm or hot
 * @param is_online     this channel is ONLINE
 * @param link_up       the inter-site link is open
 * @param healthy       STANDBY answered the last sync
 * @param cycle         current cycle number
 * @param last_try      cycle of the last sync attempt made while unhealthy
 * @return true if a sync should be sent now
 */
bool rte_site_role_standby_sync_due(bool sync_enabled, bool is_online, bool link_up, bool healthy, uint32_t cycle,
                                    uint32_t last_try);

/**
 * @brief Whether STANDBY's held snapshot is stale and must be dropped (STANDBY goes cold).
 * @return true if the snapshot has been silent for more than RTE_SITE_ROLE_STANDBY_DATA_TIMEOUT_CYCLES, or the cycle
 *         counter restarted since it arrived
 */
bool rte_site_role_standby_data_stale(bool sync_enabled, bool is_online, bool have_snapshot, uint32_t cycle,
                                      uint32_t last_rx_cycle);

/**
 * @brief Whether a STANDBY channel may promote itself because its counterpart stopped responding. Silence alone never
 *        promotes: the dispatcher must have confirmed, within RTE_SITE_ROLE_TAKEOVER_CONFIRM_VALID_CYCLES, that the
 *        other site is not ONLINE.
 */
bool rte_site_role_takeover_allowed(bool is_online, bool own_faulted, bool peer_unresponsive, bool confirmed,
                                    uint32_t cycle, uint32_t confirm_cycle);

/**
 * @brief Negotiation tie-break for "both sites STANDBY": a channel whose A/B sibling is already ONLINE joins it at once;
 *        otherwise WEST promotes after RTE_SITE_ROLE_BOTH_STANDBY_CYCLES. Both sites compute the same answer from the
 *        same link, so this is a negotiation outcome, not a unilateral promotion.
 */
bool rte_site_role_both_standby_tiebreak(uint32_t both_standby_cycles, bool is_west, bool sibling_online);

#ifdef __cplusplus
}
#endif

#endif /* RTE_SITE_ROLE_POLICY_H */

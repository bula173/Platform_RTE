/**
 * @file rte_dual_types.h
 * @brief Shared enums for the rte_dual module (ADR-020): dual-instance
 *        state negotiation and the redundant, EN 50159-defended
 *        messaging channel it can run over.
 *
 * REQ-DUAL-TYPES-001: rte_dual_state_to_string()/_channel_status_to_string()
 *                     return a non-NULL, static string for every defined
 *                     enum value and a defensive "UNKNOWN_STATE"/
 *                     "UNKNOWN_STATUS" for an unrecognized one.
 *
 * @defgroup DUAL Dual-Transfer State Negotiation
 * @brief Which of two redundant instances is active, and how well-backed
 *        is the standby one (ADR-020)
 * @{
 */
#ifndef RTE_DUAL_TYPES_H
#define RTE_DUAL_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief State of one instance in a dual (two-redundant-instance)
 *        relationship, as decided by rte_dual_negotiator_t (ADR-020
 *        section 3). Both this instance's own state and its last-known
 *        view of the peer's state use this same enum - see
 *        rte_dual_negotiator_get_own_state()/_get_peer_state().
 *
 * Explicit numeric values are fixed and part of the ABI (this value
 * travels on the wire inside rte_dual_state_frame_t::state): do not
 * renumber existing entries, only append.
 */
typedef enum rte_dual_state_e
{
    /** Never yet negotiated with a peer - startup transient, before the
     *  first valid STATE frame has been processed either way. */
    RTE_DUAL_STATE_IDLE = 0,
    /** Had a negotiated state before but contact with the peer has been
     *  lost long enough (config.peer_lost_timeout_ms) that the state can
     *  no longer be trusted. Not itself a safety reaction - see
     *  ADR-020 section 4's "no automatic safety reaction" non-goal. */
    RTE_DUAL_STATE_UNKNOWN = 1,
    /** This instance is the active one. */
    RTE_DUAL_STATE_ONLINE = 2,
    /** This instance is standby, and the peer's own redundancy is
     *  currently full (peer's last-reported channel_degraded == 0). */
    RTE_DUAL_STATE_HOTSTANDBY = 3,
    /** This instance is standby, and the peer's own redundancy is
     *  currently degraded (peer's last-reported channel_degraded == 1). */
    RTE_DUAL_STATE_COLDSTANDBY = 4
} rte_dual_state_t;

/**
 * @brief Aggregate connection status of a rte_dual_channel_t across all
 *        of its configured redundant links (ADR-020 section 2).
 *        Independent of rte_dual_state_t: losing some (not all) of the
 *        redundant links degrades this status without by itself forcing
 *        any rte_dual_state_t transition - only total loss (DOWN) is a
 *        negotiator-relevant liveness event.
 */
typedef enum rte_dual_channel_status_e
{
    /** No configured redundant link is currently up (every link's most
     *  recent send timed out waiting for its ACK). */
    RTE_DUAL_CHANNEL_STATUS_DOWN = 0,
    /** Some, but not all, configured redundant links are up. */
    RTE_DUAL_CHANNEL_STATUS_DEGRADED = 1,
    /** Every configured redundant link is up. */
    RTE_DUAL_CHANNEL_STATUS_FULL = 2
} rte_dual_channel_status_t;

/**
 * @brief Returns a short, static, human-readable string for a
 *        rte_dual_state_t. Intended for diagnostics/logging only
 *        (e.g. as the Info= field of a rte_log_write_event() call);
 *        never on a safety-decision path.
 * @param state  Value to render; an unrecognized value (defensive only -
 *               not reachable through the public enum) renders as
 *               "UNKNOWN_STATE".
 * @return A NUL-terminated string literal - static storage, never NULL;
 *         caller must not modify or free it.
 */
const char *rte_dual_state_to_string(rte_dual_state_t state);

/**
 * @brief Returns a short, static, human-readable string for a
 *        rte_dual_channel_status_t. Same diagnostics-only contract as
 *        rte_dual_state_to_string().
 * @param status  Value to render; an unrecognized value renders as
 *                "UNKNOWN_STATUS".
 * @return A NUL-terminated string literal - static storage, never NULL.
 */
const char *rte_dual_channel_status_to_string(rte_dual_channel_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* RTE_DUAL_TYPES_H */

/** @} */ /* DUAL */

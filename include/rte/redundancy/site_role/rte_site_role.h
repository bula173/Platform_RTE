/**
 * @file rte_site_role.h
 * @brief Site role service (ADR-040 step 2): the inter-site link, ONLINE/STANDBY negotiation, partner supervision,
 *        dispatcher takeover handling, warm-standby snapshot sync and the FAULTED flush - the mechanism the Capella
 *        model places in RTE Redundancy Services. The application is a declarative Site Role Client: it supplies its
 *        local facts (faulted, single-channel, readiness, A/B sibling) and its state codec, and is told when its role
 *        changes. The decision rules are rte_site_role_policy.h.
 *
 * Moved from RBC_GP's ab_gp_channel_negotiate.c with the behaviour, the wire layout and the log text unchanged.
 * Driven by the application at its own cycle points (ADR-040 decision 2, option A): rte_site_role_execute() once per
 * cycle, rte_site_role_service_link() from the fast tick, rte_site_role_sync_standby() before the output commit.
 *
 * Frame on the link (one rte_dual_channel DATA frame, frame_size bytes):
 *   faulted(1) | single_mode(1) | cycle(4, little endian) | application state(frame_size - 7) | valid(1)
 * The state region carries the application's live state only while this channel is ONLINE (an empty state otherwise);
 * valid marks a snapshot the receiving side may adopt on promotion.
 *
 * REQ-SITEROLE-010: no dynamic allocation; the caller supplies the state storage and both frame buffers.
 * REQ-SITEROLE-011: the role changes only through rte_site_role_decide() (or the startup negotiation); every change is
 *                   reported to the application's change callback before the next call returns.
 * REQ-SITEROLE-012: a per-cycle reconnect attempt is bounded by reconnect_attempt_ms; only rte_site_role_start()
 *                   blocks, bounded by startup_timeout_ms.
 * REQ-SITEROLE-014: the role of this channel and of its A/B sibling must agree (2oo2): a disagreement in
 *                   {online, single_mode} for RTE_SITE_ROLE_SIBLING_MISMATCH_LIMIT consecutive checks is a fault
 *                   (rte_site_role_check_sibling() returns RTE_SITE_ROLE_SIBLING_FAULT); one agreeing check resets
 *                   the count. The application reacts (safe-state reboot) until the fault reaction moves too.
 * REQ-SITEROLE-013: the link is closed for reconnect after send_miss_threshold consecutive unacknowledged sends or on
 *                   any receive failure other than a timeout; a STANDBY channel then marks the counterpart
 *                   unresponsive and stays STANDBY (silence alone never promotes, REQ-SITEROLE-003).
 */
#ifndef RTE_SITE_ROLE_H
#define RTE_SITE_ROLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rte/oal/netlink/rte_netlink.h"
#include "rte/redundancy/dual/rte_dual_channel.h"
#include "rte/redundancy/dual/rte_dual_negotiator.h"
#include "rte/redundancy/site_role/rte_site_role_policy.h"
#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes of framing around the application state: faulted, single_mode, cycle (4) before it, valid after it. */
#define RTE_SITE_ROLE_FRAME_OVERHEAD 7U
/** Offset of the application state region in the frame. */
#define RTE_SITE_ROLE_FRAME_STATE_OFFSET 6U
/** Value of the trailing byte of a frame that carries a valid ONLINE snapshot. */
#define RTE_SITE_ROLE_VALID_MARKER 0xA5U
/** single_mode byte values on the wire. */
#define RTE_SITE_ROLE_SINGLE_MODE_DUAL 0U
#define RTE_SITE_ROLE_SINGLE_MODE_SINGLE 1U

/** Local facts the application supplies each call (its own redundancy state and readiness). */
typedef struct
{
    bool faulted;           /**< this channel is FAULTED (about to reboot) */
    bool single_mode;       /**< this channel runs without its A/B sibling */
    bool own_ready;         /**< this channel is ready to serve (application readiness) */
    bool sibling_online;    /**< the A/B sibling reports ONLINE */
    bool sibling_ready;     /**< the A/B sibling reports ready */
    bool sibling_reachable; /**< the A/B sibling's state arrived recently */
} rte_site_role_local_t;

/** What kind of role change the change callback reports. */
typedef enum
{
    RTE_SITE_ROLE_CHANGE_PROMOTED = 0, /**< STANDBY -> ONLINE (rte_site_role_decide PROMOTE) */
    RTE_SITE_ROLE_CHANGE_DEMOTED,      /**< -> STANDBY (rte_site_role_decide DEMOTE) */
    RTE_SITE_ROLE_CHANGE_RENEGOTIATED  /**< both FAULTED: re-negotiated, WEST ONLINE, EAST STANDBY */
} rte_site_role_change_kind_t;

/** A role change, as reported to the application. */
typedef struct
{
    rte_site_role_change_kind_t kind;
    rte_site_role_reason_t      reason;
    bool                        online;         /**< the new role */
    bool                        apply_state;    /**< the application shall take over state now (promotion) */
    bool                        snapshot_valid; /**< a valid counterpart snapshot is held (received via receive_state) */
    uint32_t                    snapshot_cycle; /**< the counterpart's cycle number in that snapshot */
} rte_site_role_change_t;

/** Fills the state region (size bytes, zeroed before the call) for every outgoing frame: from the application's live
 *  state when live is true (this channel is ONLINE), otherwise from an empty state (the receiver decodes every frame,
 *  so the region always carries a well-formed encoding). */
typedef rte_status_t (*rte_site_role_encode_state_fn)(void *user, uint8_t *out, size_t size, bool live);
/** Decodes a received state region; when valid, the application keeps it as the snapshot to adopt on promotion.
 *  Returning false drops the whole frame (nothing about the counterpart is updated). */
typedef bool (*rte_site_role_receive_state_fn)(void *user, const uint8_t *in, size_t size, bool valid);
/** Reports a role change. With apply_state the application takes over its snapshot (or starts cold without one). */
typedef void (*rte_site_role_change_fn)(void *user, const rte_site_role_change_t *change);
/** One diagnostic console line (already formatted, ends with a newline). */
typedef void (*rte_site_role_trace_fn)(void *user, const char *line);
/** Waits ms milliseconds; used only by the blocking startup connect retry. */
typedef void (*rte_site_role_delay_fn)(void *user, uint32_t ms);

/** Configuration (copied by rte_site_role_init(); the strings and buffers must outlive the service). */
typedef struct
{
    uint32_t           own_id;               /**< this site's id (dual channel sender id, negotiator own id) */
    uint32_t           peer_id;              /**< the counterpart site's id */
    bool               is_west;              /**< the WEST site wins the tie-breaks */
    bool               listen;               /**< true: listen for the counterpart; false: connect to peer_host */
    const char        *peer_host;            /**< counterpart host (connect side) */
    uint16_t           port;                 /**< link port */
    size_t             link_message_size;    /**< netlink message size the dual channel needs */
    rte_duration_ms_t  ack_timeout_ms;       /**< dual channel ack timeout */
    rte_duration_ms_t  peer_lost_timeout_ms; /**< negotiator peer-lost timeout */
    rte_duration_ms_t  reconnect_attempt_ms; /**< bound of one per-cycle reconnect attempt */
    rte_duration_ms_t  startup_timeout_ms;   /**< bound of the blocking startup connect and negotiation */
    rte_duration_ms_t  link_poll_ms;         /**< negotiator execute timeout during startup */
    uint32_t           connect_retry_ms;     /**< delay between startup connect attempts */
    rte_duration_ms_t  connect_timeout_ms;   /**< per-attempt connect timeout during startup */
    uint8_t            send_miss_threshold;  /**< consecutive unacknowledged sends that close the link */
    bool               standby_sync;         /**< warm/hot standby: snapshot sync before each commit */
    rte_timestamp_ms_t startup_timestamp_ms; /**< 0: taken at start; otherwise resumed (shared by A and B) */
    size_t             frame_size;           /**< whole frame, RTE_SITE_ROLE_FRAME_OVERHEAD + state size */
    uint8_t           *tx_buf;               /**< frame_size bytes, owned by the caller */
    uint8_t           *rx_buf;               /**< frame_size bytes, owned by the caller */
    const char        *role_tag;             /**< e.g. "A/WEST", for logs */
    const char        *peer_tag;             /**< e.g. "A/EAST", for logs */
    const char        *site_name;            /**< e.g. "WEST", for log events */
    const char        *sibling_tag;          /**< the A/B sibling of this channel, e.g. "B", for log events; NULL = "-" */
    rte_site_role_encode_state_fn  encode_state;
    rte_site_role_receive_state_fn receive_state;
    rte_site_role_change_fn        on_change;
    rte_site_role_trace_fn         trace;    /**< may be NULL */
    rte_site_role_delay_fn         delay;    /**< may be NULL (startup retries without a pause) */
    void                          *user;
} rte_site_role_config_t;

/** Consecutive disagreeing A/B checks that make a fault (REQ-SITEROLE-014). */
#define RTE_SITE_ROLE_SIBLING_MISMATCH_LIMIT 3U

/** The A/B sibling's site state as this channel last received it (ADR-040 step 3: forwarded by the application until
 *  the platform owns the peer link). */
typedef struct
{
    bool known;       /**< a site-state frame has arrived this run */
    bool online;      /**< the sibling reports ONLINE */
    bool single_mode; /**< the sibling reports single-channel mode */
} rte_site_role_sibling_t;

/** Result of rte_site_role_check_sibling(). */
typedef enum
{
    RTE_SITE_ROLE_SIBLING_UNKNOWN = 0, /**< no site-state frame from the sibling yet (normal at start-up) */
    RTE_SITE_ROLE_SIBLING_AGREE,       /**< same role and mode */
    RTE_SITE_ROLE_SIBLING_PENDING,     /**< disagreement below the limit (logged) */
    RTE_SITE_ROLE_SIBLING_FAULT        /**< disagreement reached the limit: the pair must not continue */
} rte_site_role_sibling_verdict_t;

/** Status snapshot for monitors (the application's status frame). */
typedef struct
{
    bool                   online;
    bool                   link_up;
    bool                   peer_answering;   /**< counterpart frame within the last 2 cycles */
    bool                   peer_online_now;
    bool                   peer_unresponsive;
    bool                   standby_warm;
    bool                   standby_healthy;
    rte_site_role_reason_t last_reason;
} rte_site_role_status_t;

/** Service state; storage owned by the caller, all fields private. */
typedef struct
{
    rte_site_role_config_t cfg;
    rte_netlink_storage_t  link_storage;
    rte_netlink_handle_t   link;
    rte_dual_channel_t     dual_channel;
    rte_dual_negotiator_t  negotiator;
    rte_timestamp_ms_t     startup_timestamp_ms;
    uint8_t                send_miss;
    bool                   online;
    bool                   local_faulted;
    bool                   local_single_mode;
    bool                   peer_faulted;
    uint8_t                peer_single_mode;
    bool                   peer_online_now;
    uint32_t               peer_last_rx_cycle;
    bool                   peer_snapshot_valid;
    uint32_t               peer_snapshot_cycle;
    bool                   peer_unresponsive;
    bool                   lost_peer_while_online;
    bool                   takeover_confirmed;
    uint32_t               takeover_confirm_cycle;
    uint32_t               both_standby_cycles;
    rte_site_role_reason_t last_reason;
    bool                   last_reason_set;
    bool                   standby_healthy;
    uint32_t               standby_last_try_cycle;
    uint32_t               standby_rx_cycle;
    bool                   standby_warm;
    uint32_t               cycle;
    uint8_t                sibling_mismatch;
} rte_site_role_t;

/**
 * @brief Validates and stores the configuration. Opens nothing.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a NULL argument, a missing callback or buffer, or a frame_size
 *         not larger than RTE_SITE_ROLE_FRAME_OVERHEAD.
 */
rte_status_t rte_site_role_init(rte_site_role_t *rs, const rte_site_role_config_t *cfg);

/**
 * @brief Opens the link (blocking, bounded by startup_timeout_ms) and runs the startup negotiation until it settles
 *        (older startup timestamp wins). Setup phase only.
 * @return RTE_STATUS_OK with the startup role known (rte_site_role_is_online()); the link failure status, or
 *         RTE_STATUS_TIMEOUT if the negotiation did not settle (the link is closed again).
 */
rte_status_t rte_site_role_start(rte_site_role_t *rs);

/** @brief One cycle: reconnect if needed, exchange frames, update the standby state, decide and apply the role. */
void rte_site_role_execute(rte_site_role_t *rs, uint32_t cycle, const rte_site_role_local_t *local);

/** @brief Fast tick: drains up to 4 frames so acknowledgements do not depend on the two sites' cycle phase. */
void rte_site_role_service_link(rte_site_role_t *rs, uint32_t cycle);

/** @brief ONLINE, warm/hot standby: sends this cycle's snapshot before the output commit and tracks the ack. */
void rte_site_role_sync_standby(rte_site_role_t *rs, uint32_t cycle, const rte_site_role_local_t *local);

/** @brief Records a dispatcher confirmation that the other site is not ONLINE (ignored while ONLINE). */
void rte_site_role_on_takeover_confirm(rte_site_role_t *rs, uint32_t cycle);

/** @brief The application stalled while ONLINE long enough for the counterpart to have taken over: yield if it is. */
void rte_site_role_note_lost_contact(rte_site_role_t *rs);

/** @brief Best-effort final frame with faulted=1 before the process reboots, so the counterpart can promote. */
void rte_site_role_flush_faulted(rte_site_role_t *rs, bool single_mode);

/**
 * @brief A/B role agreement (REQ-SITEROLE-014): compares this channel's role and single_mode with its sibling's.
 * @param own_single_mode  this channel's single-channel mode at the time of the check
 * @return the verdict; on RTE_SITE_ROLE_SIBLING_FAULT the application shall enter its fault reaction (the
 *         disagreement has been logged as NEGOTIATION_MISMATCH).
 */
rte_site_role_sibling_verdict_t rte_site_role_check_sibling(rte_site_role_t *rs, uint32_t cycle, bool own_single_mode,
                                                            const rte_site_role_sibling_t *sibling);

/** @brief Closes the link. Idempotent. */
void rte_site_role_shutdown(rte_site_role_t *rs);

/** @return true while this channel is ONLINE. */
bool rte_site_role_is_online(const rte_site_role_t *rs);

/** @brief Fills a status snapshot for monitors. */
void rte_site_role_get_status(const rte_site_role_t *rs, rte_site_role_status_t *out);

/** @brief The underlying negotiator's own and peer states (diagnostic only; the role is rte_site_role_is_online()). */
void rte_site_role_get_negotiator_states(const rte_site_role_t *rs, rte_dual_state_t *own, rte_dual_state_t *peer);

#ifdef __cplusplus
}
#endif

#endif /* RTE_SITE_ROLE_H */

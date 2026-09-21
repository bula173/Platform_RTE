/**
 * @file rte_safechannel.h
 * @brief Unified, transport-hiding channel factory (ADR-022).
 *
 * The single entry point an application uses to open a communication
 * channel: pick a @ref rte_safechannel_type_t, describe the remote
 * endpoint(s) as host/port/role, and get back one handle with uniform
 * open/send/receive/close/status operations - regardless of whether it
 * is backed by a redundant-link @ref rte_dual_channel_t or a
 * voting @ref rte_voter_t (over N @ref rte_channel_t links,
 * ADR-025) underneath.
 *
 * This header never requires the caller to include
 * `safeapi/netlink/rte_netlink.h` or `safeapi/ipc/rte_ipc.h`, or to
 * call `rte_netlink_open()`/`rte_ipc_create()` itself:
 * rte_safechannel_open() opens every configured endpoint internally via
 * the already-registered rte_netlink backend (ADR-005). Direct use of
 * rte_netlink/rte_ipc is a backend-and-channel-layer-only concern from
 * here on (ADR-022 section 2.3) - an application should not need those
 * headers at all.
 *
 * REQ-SAFECHANNEL-001: rte_safechannel_open() shall open every
 *                      configured endpoint itself; the caller shall never
 *                      need to call rte_netlink_open() or hold a
 *                      rte_netlink_handle_t.
 * REQ-SAFECHANNEL-002: no dynamic allocation; all storage is caller-owned
 *                      and fixed-size.
 * REQ-SAFECHANNEL-003: rte_safechannel_send()/_receive() behave
 *                      identically to the caller regardless of
 *                      config.type (uniform facade over
 *                      rte_dual_channel_t / rte_channel_t).
 *
 * @defgroup SAFECHANNEL Unified Channel Factory
 * @brief App-facing channel open/send/receive/close, transport hidden (ADR-022)
 * @{
 */
#ifndef RTE_SAFECHANNEL_H
#define RTE_SAFECHANNEL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "safeapi/redundancy/dual/rte_dual_channel.h"
#include "safeapi/oal/netlink/rte_netlink.h"
#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"
#include "safeapi/redundancy/channel_link/rte_channel.h"
#include "safeapi/redundancy/voter/rte_voter.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of redundant endpoints one rte_safechannel_t
 *         may be configured with, for either type. Fixed, not dynamic. */
#define RTE_SAFECHANNEL_MAX_LINKS 4U

/** @brief Which underlying channel implementation a rte_safechannel_t wraps. */
typedef enum rte_safechannel_type_e
{
    /** Wraps rte_dual_channel_t: 1..N redundant links, always-send +
     *  bounded-ACK-wait delivery, EN 50159-framed (ADR-020). */
    RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT = 0,
    /** Wraps a rte_voter_t over N rte_channel_t links: 2oo2/2oo3/NMR
     *  voting arbitration (ADR-025). */
    RTE_SAFECHANNEL_TYPE_VITAL_VOTED = 1
} rte_safechannel_type_t;

/** @brief Aggregate link status, uniform across both wrapped types. */
typedef enum rte_safechannel_link_status_e
{
    RTE_SAFECHANNEL_LINK_DOWN     = 0, /**< No configured endpoint is usable. */
    RTE_SAFECHANNEL_LINK_DEGRADED = 1, /**< Some, but not all, endpoints usable. */
    RTE_SAFECHANNEL_LINK_FULL     = 2  /**< Every configured endpoint usable. */
} rte_safechannel_link_status_t;

/**
 * @brief One remote endpoint to open internally - replaces a caller-
 *        supplied rte_netlink_handle_t (ADR-022 section 2.2).
 */
typedef struct rte_safechannel_endpoint_s
{
    /** LISTEN (bind+accept) or CONNECT (dial). */
    rte_netlink_role_t role;
    /** CONNECT: target host, must not be NULL. LISTEN: bind address,
     *  NULL = any (same convention as rte_netlink_config_t::host). */
    const char *host;
    /** Port to bind (LISTEN) or dial (CONNECT). */
    uint16_t port;
} rte_safechannel_endpoint_t;

/** @brief Configuration specific to RTE_SAFECHANNEL_TYPE_DUAL_REDUNDANT. */
typedef struct rte_safechannel_dual_config_s
{
    /** Endpoints to open, 1..RTE_SAFECHANNEL_MAX_LINKS of them. */
    rte_safechannel_endpoint_t endpoints[RTE_SAFECHANNEL_MAX_LINKS];
    /** Number of entries populated in endpoints[]. */
    uint32_t link_count;
    /** This instance's own EN 50159 sender identifier. */
    uint32_t sender_id;
    /** Expected sender_id of the peer at the other end of every link. */
    uint32_t expected_peer_id;
    /** Max time rte_safechannel_open() may block per endpoint establishing it. */
    rte_duration_ms_t connect_timeout_ms;
    /** Per-link budget rte_safechannel_send() waits for that link's own ACK. */
    rte_duration_ms_t ack_timeout_ms;
    /** Optional; NULL = no callback. Fires on aggregate status change. */
    rte_dual_channel_status_callback_t status_callback;
    /** Opaque context passed back to status_callback. Ignored if NULL. */
    void *status_callback_ctx;
} rte_safechannel_dual_config_t;

/** @brief Configuration specific to RTE_SAFECHANNEL_TYPE_VITAL_VOTED. */
typedef struct rte_safechannel_vital_config_s
{
    /** Endpoints to open, one per redundant voted channel. */
    rte_safechannel_endpoint_t endpoints[RTE_SAFECHANNEL_MAX_LINKS];
    /** Number of entries populated in endpoints[]. */
    uint32_t link_count;
    /** Fixed size in bytes of every message exchanged - every endpoint
     *  is opened with this as its rte_netlink_config_t::message_size. */
    size_t message_size;
    /** Voting strategy (2oo2, 2oo3, NMR). */
    rte_voting_strategy_t voting_strategy;
    /** Quorum size for NMR voting (ignored for 2oo2/2oo3). */
    uint32_t quorum_size;
    /** Max time rte_safechannel_open() may block per endpoint establishing it. */
    rte_duration_ms_t connect_timeout_ms;
    /** Timeout for each channel operation (milliseconds), forwarded to
     *  rte_voter_config_t::channel_timeout_ms. */
    rte_duration_ms_t channel_timeout_ms;
    /** Enable automatic disagreement logging (forwarded as-is). */
    bool log_disagreements;
    /** Optional; NULL = no callback. Forwarded to rte_voter_config_t::on_disagreement. */
    void (*on_disagreement)(void *context, rte_voting_result_t result);
    /** Opaque context passed back to on_disagreement. Ignored if NULL. */
    void *context;
} rte_safechannel_vital_config_t;

/** @brief Configuration for rte_safechannel_open(). */
typedef struct rte_safechannel_config_s
{
    /** Selects which member of the union below is read. */
    rte_safechannel_type_t type;
    union
    {
        rte_safechannel_dual_config_t  dual;
        rte_safechannel_vital_config_t vital;
    } as;
} rte_safechannel_config_t;

/**
 * @brief Caller-owned storage for one rte_safechannel_t instance. Every
 *        field is private - reach it only through the functions below.
 *        No dynamic allocation (REQ-SAFECHANNEL-002): sized to hold up to
 *        RTE_SAFECHANNEL_MAX_LINKS opened netlink links plus whichever
 *        of rte_dual_channel_t/rte_channel_t is in use.
 */
typedef struct rte_safechannel_s
{
    rte_safechannel_type_t type;
    bool                    is_open;

    /** Endpoints opened by this instance; ownership (closing them) stays
     *  with this module, not the caller (unlike rte_dual_channel_t,
     *  which takes already-opened handles it does not own). */
    rte_netlink_storage_t links[RTE_SAFECHANNEL_MAX_LINKS];
    rte_netlink_handle_t  link_handles[RTE_SAFECHANNEL_MAX_LINKS];
    uint32_t                link_count;

    union
    {
        rte_dual_channel_t dual;
        struct
        {
            /** ADR-025: N individual channels registered into a voter,
             *  replacing the old single N-channel-bundle rte_channel_t. */
            rte_channel_t channels[RTE_SAFECHANNEL_MAX_LINKS];
            rte_voter_t         voter;
        } vital;
    } impl;
} rte_safechannel_t;

/**
 * @brief Opens a channel: opens every configured endpoint via the
 *        registered rte_netlink backend, then initializes the wrapped
 *        rte_dual_channel_t or rte_channel_t on top of the
 *        resulting links (ADR-022 section 2.2). Retries each endpoint's
 *        rte_netlink_open() internally up to config's own
 *        connect_timeout_ms, matching the retry pattern every current
 *        hand-rolled caller already implemented itself.
 * @param channel  Caller-owned storage to initialize. Must not be NULL.
 * @param config   Configuration. Must not be NULL; config->type selects
 *                 which union member is read; that member's link_count
 *                 must be in [1, its type's max] and every
 *                 endpoints[0..link_count-1].host must not be NULL for
 *                 a CONNECT-role endpoint.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_TIMEOUT if any configured endpoint fails to
 *         establish within its connect_timeout_ms (every endpoint opened
 *         so far is closed again before returning - all-or-nothing);
 *         RTE_STATUS_NOT_INITIALIZED if no rte_netlink backend is
 *         registered.
 */
rte_status_t rte_safechannel_open(rte_safechannel_t *channel, const rte_safechannel_config_t *config);

/**
 * @brief Sends payload on the underlying channel - broadcast-with-ACK-
 *        wait for DUAL_REDUNDANT (rte_dual_channel_send()), atomic
 *        all-or-nothing broadcast to every registered channel for
 *        VITAL_VOTED (rte_voter_send()).
 * @param channel       Opened channel. Must not be NULL.
 * @param payload       Payload to send. May be NULL only if payload_size is 0.
 * @param payload_size  Payload size in bytes; must be <=
 *                      RTE_DUAL_CHANNEL_MAX_PAYLOAD (DUAL_REDUNDANT) or
 *                      the configured message_size (VITAL_VOTED).
 * @return RTE_STATUS_OK; RTE_STATUS_TIMEOUT; RTE_STATUS_INVALID_PARAM;
 *         RTE_STATUS_HARDWARE_FAULT (VITAL_VOTED disagreement/fault) -
 *         see rte_dual_channel_send()/rte_voter_send() for the exact
 *         per-type semantics.
 */
rte_status_t rte_safechannel_send(rte_safechannel_t *channel, const uint8_t *payload, size_t payload_size);

/**
 * @brief Returns the most recent inbound payload, actively polling the
 *        underlying channel if nothing was already staged.
 * @param channel      Opened channel. Must not be NULL.
 * @param out_payload  Destination buffer. Must not be NULL.
 * @param max_size     Usable size of out_payload; must be > 0.
 * @param timeout_ms   Maximum time to actively poll if nothing was
 *                     already staged; 0 = check only what is already staged.
 * @param out_size     Receives the actual payload size. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_TIMEOUT if nothing arrived in time;
 *         RTE_STATUS_INVALID_PARAM for a bad argument or an
 *         already-staged frame larger than max_size.
 */
rte_status_t rte_safechannel_receive(rte_safechannel_t *channel, uint8_t *out_payload, size_t max_size,
                                        rte_duration_ms_t timeout_ms, size_t *out_size);

/**
 * @brief Aggregate status across every configured endpoint - DOWN before
 *        the first send/receive.
 * @param channel  Channel to query. May be NULL (returns DOWN, defensive default).
 * @return The current aggregate status.
 */
rte_safechannel_link_status_t rte_safechannel_get_status(const rte_safechannel_t *channel);

/**
 * @brief Closes every endpoint this instance opened and releases the
 *        wrapped channel. Safe to call on an already-closed/never-opened
 *        instance (no-op).
 * @param channel  Channel to close. May be NULL (no-op).
 * @return RTE_STATUS_OK.
 */
rte_status_t rte_safechannel_close(rte_safechannel_t *channel);

#ifdef __cplusplus
}
#endif

#endif /* RTE_SAFECHANNEL_H */

/** @} */ /* SAFECHANNEL */

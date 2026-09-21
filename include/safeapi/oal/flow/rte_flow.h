/**
 * @file rte_flow.h
 * @brief OS Abstraction Layer - OCORA PI-API-compatible Flow service.
 *
 * A name-based, publish/subscribe (or request/response) message
 * endpoint, matching the shape OCORA's "Generic Safe Computing Platform
 * - Specification of the PI API between Application and Platform"
 * (v2.0, July 2022) Annex A.1 sketches in `flows.h`: `fl_open`/
 * `fl_close`/`fl_send`/`fl_receive`/`fl_getattr`/`fl_setattr`, an
 * `e_fl_oflags` open-mode bitmask (O_REQUESTER/O_RESPONDER/O_PUBLISHER/
 * O_SUBSCRIBER/O_NONBLOCK), and a USER/CTRL channel discriminator on
 * send/receive. Function names here use this project's own `rte_`
 * prefix convention instead of shadowing OCORA's bare `fl_*` names
 * verbatim - the intent is shape/semantic compatibility with the PI
 * API, not literal symbol-name compatibility.
 *
 * Deliberately NOT a thin wrapper over rte_netlink (oal/netlink/
 * rte_netlink.h): netlink is connection-oriented point-to-point
 * (LISTEN/CONNECT, host:port) - a Flow is name/topic-addressed
 * publish-subscribe, the same shape a real DDS OSAdapter provides
 * natively. Forcing Flow through netlink's host:port model would waste
 * a DDS OSAdapter's actual pub/sub capability (topic discovery, N
 * subscribers) later. This service has its own OSAdapter seam instead
 * (safeapi_osadapter/flow/rte_osadapter_flow.h) so a POSIX/TCP OSAdapter can
 * emulate pub/sub over point-to-point links today, and a DDS OSAdapter
 * can implement it natively later, with zero change to this header or
 * any Functional-Actor code written against it.
 *
 * Per the PI API's own model (ADR note: see this project's ISSUES.md/
 * session history for the OCORA research this is based on), a Flow is
 * the ONLY I/O primitive a replicated Functional Actor uses - N-way
 * voting/replication of what is sent/received over a Flow is meant to
 * happen transparently, beneath this API, inside the Platform/RTE
 * layer. This header defines the actor-facing surface only; the
 * replication/voting layer underneath it (built from this framework's
 * existing rte_voter/rte_cross_comparator primitives) is tracked as
 * separate, follow-up work - see ISSUES.md.
 *
 * REQ-OAL-FLOW-001: no dynamic allocation; caller supplies storage.
 * REQ-OAL-FLOW-002: rte_flow_open() shall never block longer than
 *                   config->open_timeout_ms.
 * REQ-OAL-FLOW-003: send/receive shall accept an explicit timeout and
 *                   shall never block indefinitely by default - a
 *                   deliberate strengthening beyond the PI API sketch
 *                   (which relies on O_NONBLOCK alone), matching every
 *                   other blocking call in this framework (e.g.
 *                   REQ-OAL-NETLINK-003).
 * REQ-OAL-FLOW-004: this service provides no message ordering,
 *                   deduplication, or delivery guarantee of its own,
 *                   same posture as rte_netlink (REQ-OAL-NETLINK-014).
 *
 * @defgroup FLOW OCORA PI-API-compatible Flow Service
 * @brief Name-addressed publish/subscribe message endpoint (OCORA PI API Annex A.1)
 * @{
 */
#ifndef SAFEAPI_OAL_FLOW_H
#define SAFEAPI_OAL_FLOW_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caller-owned, fixed-size storage backing one rte_flow_handle_t. */
SAFEAPI_DECLARE_STORAGE(rte_flow_storage_t, 64U);

/** @brief Opaque handle to an opened Flow, returned by rte_flow_open(). */
typedef struct rte_flow_impl_s *rte_flow_handle_t;

/**
 * @brief Open-mode bitmask, matching OCORA PI API Annex A.1's
 *        `e_fl_oflags` values exactly (so an integrator porting real
 *        OCORA-authored configuration data does not need to remap
 *        numeric flag values).
 */
typedef enum rte_flow_oflag_e
{
    RTE_FLOW_O_REQUESTER = 1, /**< Sends a request, expects a matching response. */
    RTE_FLOW_O_RESPONDER = 2, /**< Receives requests, sends matching responses. */
    RTE_FLOW_O_PUBLISHER = 4, /**< Sends messages to any current/future subscriber. */
    RTE_FLOW_O_SUBSCRIBER = 8, /**< Receives messages from any publisher on this name. */
    RTE_FLOW_O_NONBLOCK  = 16  /**< Backend-defined non-blocking hint; callers should
                                  *   still pass an explicit timeout_ms (REQ-OAL-FLOW-003)
                                  *   rather than relying on this flag alone. */
} rte_flow_oflag_t;

/**
 * @brief Message-channel discriminator, matching OCORA PI API Annex
 *        A.1's `e_fl_channels`: a Flow can carry ordinary application
 *        (user) traffic and platform control traffic on the same
 *        opened endpoint, distinguished per send/receive call.
 */
typedef enum rte_flow_channel_e
{
    RTE_FLOW_CHANNEL_USER = 1, /**< Application/user message. */
    RTE_FLOW_CHANNEL_CTRL = 2  /**< Platform control message. */
} rte_flow_channel_t;

/**
 * @brief Flow attributes (OCORA PI API Annex A.1 leaves `fl_attr`'s
 *        content as "to be defined" - this is this framework's own
 *        concrete, deliberately minimal choice of static+dynamic
 *        attributes, not a claim of OCORA-authored content).
 */
typedef struct rte_flow_attr_s
{
    size_t   message_size; /**< Fixed size in bytes of every message on this Flow. */
    uint32_t oflags;       /**< The rte_flow_oflag_t bitmask this Flow was opened with. */
    bool     is_connected; /**< Dynamic: whether the OSAdapter currently has at least
                             *   one live peer (publisher<->subscriber pairing,
                             *   or an established request/response pairing). */
} rte_flow_attr_t;

/** @brief Configuration for rte_flow_open(). */
typedef struct rte_flow_config_s
{
    /** Flow name (topic). Backend-defined interpretation; a POSIX/TCP
     *  OSAdapter maps this to a configured host:port pair, a DDS OSAdapter
     *  maps it directly to a DDS topic name. Caller-owned; only read
     *  during rte_flow_open(). Must not be NULL. */
    const char *name;
    /** Bitmask of rte_flow_oflag_t values. Exactly one of
     *  (O_PUBLISHER|O_SUBSCRIBER|O_REQUESTER|O_RESPONDER) must be set. */
    uint32_t    oflags;
    /** Fixed size in bytes of every message exchanged on this Flow. */
    size_t      message_size;
    /** Maximum time rte_flow_open() may block (REQ-OAL-FLOW-002). */
    rte_duration_ms_t open_timeout_ms;
} rte_flow_config_t;

/**
 * @brief Opens (joins/registers/subscribes to, per OCORA's own
 *        `fl_open()` doc) a named Flow.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Flow configuration. Must not be NULL; config->name must not
 *                    be NULL; config->message_size must be > 0; config->oflags
 *                    must set exactly one of PUBLISHER/SUBSCRIBER/REQUESTER/RESPONDER.
 * @param out_handle  Receives the opened Flow's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_TIMEOUT if not opened within config->open_timeout_ms;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (rte_osadapter_flow_register()); RTE_STATUS_NOT_SUPPORTED if the
 *         registered OSAdapter does not implement open.
 * REQ-OAL-FLOW-010
 */
rte_status_t rte_flow_open(rte_flow_storage_t *storage,
                              const rte_flow_config_t *config,
                              rte_flow_handle_t *out_handle);

/**
 * @brief Closes (leaves/unregisters/unsubscribes from) a Flow.
 * @param handle  Handle to close. Must not be NULL. Invalid to use after this call.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_flow_open().
 * REQ-OAL-FLOW-013
 */
rte_status_t rte_flow_close(rte_flow_handle_t handle);

/**
 * @brief Sends one fixed-size message on the given channel, blocking at
 *        most timeout_ms.
 * @param handle       Flow handle. Must not be NULL.
 * @param data         Message data to send. Must not be NULL.
 * @param data_size    Size of data in bytes; must be > 0.
 * @param channel      Which logical channel (user/control) this message is on.
 * @param timeout_ms   Maximum time to wait for the send to complete.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_TIMEOUT
 *         if the send does not complete in time; RTE_STATUS_HARDWARE_FAULT
 *         if the OSAdapter can positively confirm no peer is reachable (not
 *         guaranteed on every OSAdapter, see REQ-OAL-FLOW-004);
 *         RTE_STATUS_NOT_INITIALIZED/RTE_STATUS_NOT_SUPPORTED as in
 *         rte_flow_open().
 * REQ-OAL-FLOW-011
 */
rte_status_t rte_flow_send(rte_flow_handle_t handle,
                              const void *data,
                              size_t data_size,
                              rte_flow_channel_t channel,
                              rte_duration_ms_t timeout_ms);

/**
 * @brief Receives one fixed-size message, blocking at most timeout_ms.
 * @param handle       Flow handle. Must not be NULL.
 * @param out_data     Destination buffer. Must not be NULL.
 * @param buffer_size  Usable size of out_data in bytes; must be > 0.
 * @param out_channel  Receives which logical channel the message arrived on.
 *                     May be NULL if the caller does not need it.
 * @param timeout_ms   Maximum time to wait for a message to arrive.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_TIMEOUT
 *         if no message arrives in time; RTE_STATUS_HARDWARE_FAULT/
 *         RTE_STATUS_DATA_CORRUPTION per rte_flow_send()'s own note;
 *         RTE_STATUS_NOT_INITIALIZED/RTE_STATUS_NOT_SUPPORTED as in
 *         rte_flow_open().
 * REQ-OAL-FLOW-012
 */
rte_status_t rte_flow_receive(rte_flow_handle_t handle,
                                 void *out_data,
                                 size_t buffer_size,
                                 rte_flow_channel_t *out_channel,
                                 rte_duration_ms_t timeout_ms);

/**
 * @brief Reads this Flow's current (static and dynamic) attributes.
 * @param handle    Flow handle. Must not be NULL.
 * @param out_attr  Receives the current attributes. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_flow_open().
 * REQ-OAL-FLOW-014
 */
rte_status_t rte_flow_getattr(rte_flow_handle_t handle, rte_flow_attr_t *out_attr);

/**
 * @brief Sets this Flow's mutable (dynamic) attributes.
 * @param handle       Flow handle. Must not be NULL.
 * @param new_attr     Attributes to apply. Must not be NULL. Backend-defined
 *                     which fields are actually mutable; immutable fields
 *                     (e.g. message_size) are ignored, not rejected.
 * @param out_old_attr Receives the attributes as they were before this call.
 *                     May be NULL if the caller does not need it.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_flow_open().
 * REQ-OAL-FLOW-015
 */
rte_status_t rte_flow_setattr(rte_flow_handle_t handle,
                                 const rte_flow_attr_t *new_attr,
                                 rte_flow_attr_t *out_old_attr);

/*
 * The OSAdapter vtable (rte_osadapter_flow_t) and
 * rte_osadapter_flow_register() live in
 * safeapi_osadapter/flow/rte_osadapter_flow.h, not here (ADR-021 seam,
 * same posture as rte_netlink). This header is the consumer-facing
 * surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OAL_FLOW_H */

/** @} */ /* FLOW */

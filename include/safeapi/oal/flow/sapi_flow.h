/**
 * @file sapi_flow.h
 * @brief OS Abstraction Layer - OCORA PI-API-compatible Flow service.
 *
 * A name-based, publish/subscribe (or request/response) message
 * endpoint, matching the shape OCORA's "Generic Safe Computing Platform
 * - Specification of the PI API between Application and Platform"
 * (v2.0, July 2022) Annex A.1 sketches in `flows.h`: `fl_open`/
 * `fl_close`/`fl_send`/`fl_receive`/`fl_getattr`/`fl_setattr`, an
 * `e_fl_oflags` open-mode bitmask (O_REQUESTER/O_RESPONDER/O_PUBLISHER/
 * O_SUBSCRIBER/O_NONBLOCK), and a USER/CTRL channel discriminator on
 * send/receive. Function names here use this project's own `sapi_`
 * prefix convention instead of shadowing OCORA's bare `fl_*` names
 * verbatim - the intent is shape/semantic compatibility with the PI
 * API, not literal symbol-name compatibility.
 *
 * Deliberately NOT a thin wrapper over sapi_netlink (oal/netlink/
 * sapi_netlink.h): netlink is connection-oriented point-to-point
 * (LISTEN/CONNECT, host:port) - a Flow is name/topic-addressed
 * publish-subscribe, the same shape a real DDS backend provides
 * natively. Forcing Flow through netlink's host:port model would waste
 * a DDS backend's actual pub/sub capability (topic discovery, N
 * subscribers) later. This service has its own backend seam instead
 * (safeapi_backend/flow/sapi_flow_backend.h) so a POSIX/TCP backend can
 * emulate pub/sub over point-to-point links today, and a DDS backend
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
 * existing sapi_voter/sapi_cross_comparator primitives) is tracked as
 * separate, follow-up work - see ISSUES.md.
 *
 * REQ-OAL-FLOW-001: no dynamic allocation; caller supplies storage.
 * REQ-OAL-FLOW-002: sapi_flow_open() shall never block longer than
 *                   config->open_timeout_ms.
 * REQ-OAL-FLOW-003: send/receive shall accept an explicit timeout and
 *                   shall never block indefinitely by default - a
 *                   deliberate strengthening beyond the PI API sketch
 *                   (which relies on O_NONBLOCK alone), matching every
 *                   other blocking call in this framework (e.g.
 *                   REQ-OAL-NETLINK-003).
 * REQ-OAL-FLOW-004: this service provides no message ordering,
 *                   deduplication, or delivery guarantee of its own,
 *                   same posture as sapi_netlink (REQ-OAL-NETLINK-014).
 *
 * @defgroup FLOW OCORA PI-API-compatible Flow Service
 * @brief Name-addressed publish/subscribe message endpoint (OCORA PI API Annex A.1)
 * @{
 */
#ifndef SAFEAPI_OAL_FLOW_H
#define SAFEAPI_OAL_FLOW_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caller-owned, fixed-size storage backing one sapi_flow_handle_t. */
SAFEAPI_DECLARE_STORAGE(sapi_flow_storage_t, 64U);

/** @brief Opaque handle to an opened Flow, returned by sapi_flow_open(). */
typedef struct sapi_flow_impl_s *sapi_flow_handle_t;

/**
 * @brief Open-mode bitmask, matching OCORA PI API Annex A.1's
 *        `e_fl_oflags` values exactly (so an integrator porting real
 *        OCORA-authored configuration data does not need to remap
 *        numeric flag values).
 */
typedef enum sapi_flow_oflag_e
{
    SAPI_FLOW_O_REQUESTER = 1, /**< Sends a request, expects a matching response. */
    SAPI_FLOW_O_RESPONDER = 2, /**< Receives requests, sends matching responses. */
    SAPI_FLOW_O_PUBLISHER = 4, /**< Sends messages to any current/future subscriber. */
    SAPI_FLOW_O_SUBSCRIBER = 8, /**< Receives messages from any publisher on this name. */
    SAPI_FLOW_O_NONBLOCK  = 16  /**< Backend-defined non-blocking hint; callers should
                                  *   still pass an explicit timeout_ms (REQ-OAL-FLOW-003)
                                  *   rather than relying on this flag alone. */
} sapi_flow_oflag_t;

/**
 * @brief Message-channel discriminator, matching OCORA PI API Annex
 *        A.1's `e_fl_channels`: a Flow can carry ordinary application
 *        (user) traffic and platform control traffic on the same
 *        opened endpoint, distinguished per send/receive call.
 */
typedef enum sapi_flow_channel_e
{
    SAPI_FLOW_CHANNEL_USER = 1, /**< Application/user message. */
    SAPI_FLOW_CHANNEL_CTRL = 2  /**< Platform control message. */
} sapi_flow_channel_t;

/**
 * @brief Flow attributes (OCORA PI API Annex A.1 leaves `fl_attr`'s
 *        content as "to be defined" - this is this framework's own
 *        concrete, deliberately minimal choice of static+dynamic
 *        attributes, not a claim of OCORA-authored content).
 */
typedef struct sapi_flow_attr_s
{
    size_t   message_size; /**< Fixed size in bytes of every message on this Flow. */
    uint32_t oflags;       /**< The sapi_flow_oflag_t bitmask this Flow was opened with. */
    bool     is_connected; /**< Dynamic: whether the backend currently has at least
                             *   one live peer (publisher<->subscriber pairing,
                             *   or an established request/response pairing). */
} sapi_flow_attr_t;

/** @brief Configuration for sapi_flow_open(). */
typedef struct sapi_flow_config_s
{
    /** Flow name (topic). Backend-defined interpretation; a POSIX/TCP
     *  backend maps this to a configured host:port pair, a DDS backend
     *  maps it directly to a DDS topic name. Caller-owned; only read
     *  during sapi_flow_open(). Must not be NULL. */
    const char *name;
    /** Bitmask of sapi_flow_oflag_t values. Exactly one of
     *  (O_PUBLISHER|O_SUBSCRIBER|O_REQUESTER|O_RESPONDER) must be set. */
    uint32_t    oflags;
    /** Fixed size in bytes of every message exchanged on this Flow. */
    size_t      message_size;
    /** Maximum time sapi_flow_open() may block (REQ-OAL-FLOW-002). */
    sapi_duration_ms_t open_timeout_ms;
} sapi_flow_config_t;

/**
 * @brief Opens (joins/registers/subscribes to, per OCORA's own
 *        `fl_open()` doc) a named Flow.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Flow configuration. Must not be NULL; config->name must not
 *                    be NULL; config->message_size must be > 0; config->oflags
 *                    must set exactly one of PUBLISHER/SUBSCRIBER/REQUESTER/RESPONDER.
 * @param out_handle  Receives the opened Flow's handle. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM for a bad argument;
 *         SAPI_STATUS_TIMEOUT if not opened within config->open_timeout_ms;
 *         SAPI_STATUS_NOT_INITIALIZED if no backend is registered
 *         (sapi_flow_register_backend()); SAPI_STATUS_NOT_SUPPORTED if the
 *         registered backend does not implement open.
 * REQ-OAL-FLOW-010
 */
sapi_status_t sapi_flow_open(sapi_flow_storage_t *storage,
                              const sapi_flow_config_t *config,
                              sapi_flow_handle_t *out_handle);

/**
 * @brief Closes (leaves/unregisters/unsubscribes from) a Flow.
 * @param handle  Handle to close. Must not be NULL. Invalid to use after this call.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_flow_open().
 * REQ-OAL-FLOW-013
 */
sapi_status_t sapi_flow_close(sapi_flow_handle_t handle);

/**
 * @brief Sends one fixed-size message on the given channel, blocking at
 *        most timeout_ms.
 * @param handle       Flow handle. Must not be NULL.
 * @param data         Message data to send. Must not be NULL.
 * @param data_size    Size of data in bytes; must be > 0.
 * @param channel      Which logical channel (user/control) this message is on.
 * @param timeout_ms   Maximum time to wait for the send to complete.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_TIMEOUT
 *         if the send does not complete in time; SAPI_STATUS_HARDWARE_FAULT
 *         if the backend can positively confirm no peer is reachable (not
 *         guaranteed on every backend, see REQ-OAL-FLOW-004);
 *         SAPI_STATUS_NOT_INITIALIZED/SAPI_STATUS_NOT_SUPPORTED as in
 *         sapi_flow_open().
 * REQ-OAL-FLOW-011
 */
sapi_status_t sapi_flow_send(sapi_flow_handle_t handle,
                              const void *data,
                              size_t data_size,
                              sapi_flow_channel_t channel,
                              sapi_duration_ms_t timeout_ms);

/**
 * @brief Receives one fixed-size message, blocking at most timeout_ms.
 * @param handle       Flow handle. Must not be NULL.
 * @param out_data     Destination buffer. Must not be NULL.
 * @param buffer_size  Usable size of out_data in bytes; must be > 0.
 * @param out_channel  Receives which logical channel the message arrived on.
 *                     May be NULL if the caller does not need it.
 * @param timeout_ms   Maximum time to wait for a message to arrive.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_TIMEOUT
 *         if no message arrives in time; SAPI_STATUS_HARDWARE_FAULT/
 *         SAPI_STATUS_DATA_CORRUPTION per sapi_flow_send()'s own note;
 *         SAPI_STATUS_NOT_INITIALIZED/SAPI_STATUS_NOT_SUPPORTED as in
 *         sapi_flow_open().
 * REQ-OAL-FLOW-012
 */
sapi_status_t sapi_flow_receive(sapi_flow_handle_t handle,
                                 void *out_data,
                                 size_t buffer_size,
                                 sapi_flow_channel_t *out_channel,
                                 sapi_duration_ms_t timeout_ms);

/**
 * @brief Reads this Flow's current (static and dynamic) attributes.
 * @param handle    Flow handle. Must not be NULL.
 * @param out_attr  Receives the current attributes. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_flow_open().
 * REQ-OAL-FLOW-014
 */
sapi_status_t sapi_flow_getattr(sapi_flow_handle_t handle, sapi_flow_attr_t *out_attr);

/**
 * @brief Sets this Flow's mutable (dynamic) attributes.
 * @param handle       Flow handle. Must not be NULL.
 * @param new_attr     Attributes to apply. Must not be NULL. Backend-defined
 *                     which fields are actually mutable; immutable fields
 *                     (e.g. message_size) are ignored, not rejected.
 * @param out_old_attr Receives the attributes as they were before this call.
 *                     May be NULL if the caller does not need it.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_flow_open().
 * REQ-OAL-FLOW-015
 */
sapi_status_t sapi_flow_setattr(sapi_flow_handle_t handle,
                                 const sapi_flow_attr_t *new_attr,
                                 sapi_flow_attr_t *out_old_attr);

/*
 * The backend vtable (sapi_flow_backend_t) and
 * sapi_flow_register_backend() live in
 * safeapi_backend/flow/sapi_flow_backend.h, not here (ADR-021 seam,
 * same posture as sapi_netlink). This header is the consumer-facing
 * surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OAL_FLOW_H */

/** @} */ /* FLOW */

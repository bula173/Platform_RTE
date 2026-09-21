/**
 * @file rte_netlink.h
 * @brief OS Abstraction Layer - Point-to-point network link service.
 *
 * A sibling to rte_ipc (ADR-001 section 4, service 5), not a
 * replacement: rte_ipc models a bounded local queue between two ends
 * that already exist (e.g. a pipe created by one process for two of its
 * own threads/tasks). rte_netlink models a connection-oriented link
 * between two independent processes - possibly on separate machines -
 * that must first find each other (one side listens, the other
 * connects) before any data can flow. Distinct connection-establishment
 * semantics is why this is a new service instead of new fields bolted
 * onto rte_ipc_config_t (ADR-005's backend-agnostic config principle:
 * a config struct should not carry fields that are meaningless for most
 * backends of that service).
 *
 * Framework ships the interface and validate-then-dispatch layer only;
 * a concrete backend (e.g. POSIX TCP sockets) is integrator-supplied
 * (ADR-005) and lives with the application that registers it - see
 * safeAPIRBC2oo2's src/posix_backend/rte_posix_backend_netlink.c for
 * the reference POSIX/TCP implementation this header was designed
 * alongside.
 *
 * REQ-OAL-NETLINK-001: no dynamic allocation; caller supplies storage.
 * REQ-OAL-NETLINK-002: rte_netlink_open() shall never block longer than
 *                      config->connect_timeout_ms.
 * REQ-OAL-NETLINK-003: send/receive shall accept an explicit timeout and
 *                      shall never block indefinitely by default.
 * REQ-OAL-NETLINK-014: this service provides no message ordering,
 *                      deduplication, or delivery guarantee of its own -
 *                      a backend may be built on an unreliable transport
 *                      (e.g. UDP). Any such guarantee is the caller's
 *                      responsibility (see safeAPIFreamwork's
 *                      rte_dual_msgchannel/rte_dual_channel for a
 *                      reusable sequence+CRC+ACK layer, and ADR-027).
 *
 * @defgroup NETLINK Point-to-Point Network Link
 * @brief Connection-oriented link between independent processes (ADR-001)
 * @{
 */
#ifndef SAFEAPI_OS_NETLINK_H
#define SAFEAPI_OS_NETLINK_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caller-owned, fixed-size storage backing one rte_netlink_handle_t. */
SAFEAPI_DECLARE_STORAGE(rte_netlink_storage_t, 64U);

/** @brief Opaque handle to an established link, returned by rte_netlink_open(). */
typedef struct rte_netlink_impl_s *rte_netlink_handle_t;

/**
 * @brief Which side of the connection this link instance plays.
 *
 * A TCP-style backend needs exactly one LISTEN side (binds, accepts one
 * peer) and one CONNECT side (dials the listener) per link; which role a
 * given process/role/site plays is an application-level decision (e.g.
 * "site West listens, site East connects"), not something this service
 * decides on its own.
 */
typedef enum rte_netlink_role_e
{
    RTE_NETLINK_ROLE_LISTEN  = 0, /**< Bind config->port and accept one peer. */
    RTE_NETLINK_ROLE_CONNECT = 1  /**< Dial config->host:config->port. */
} rte_netlink_role_t;

/** @brief Configuration for rte_netlink_open(). */
typedef struct rte_netlink_config_s
{
    /** Which side of the connection this link instance plays. */
    rte_netlink_role_t role;
    /** LISTEN: bind address, backend-defined interpretation of NULL
     *  (typically "any"). CONNECT: target host to dial. Must not be NULL
     *  for CONNECT. Caller-owned; only read during rte_netlink_open(). */
    const char         *host;
    /** LISTEN: port to bind. CONNECT: port to dial. */
    uint16_t             port;
    /** Fixed size in bytes of every message exchanged on this link. */
    size_t               message_size;
    /** Maximum time rte_netlink_open() may block establishing the
     *  link (REQ-OAL-NETLINK-002). */
    rte_duration_ms_t   connect_timeout_ms;
} rte_netlink_config_t;

/**
 * @brief Establishes a point-to-point link: binds+accepts (LISTEN) or
 *        dials (CONNECT), per config->role.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Link configuration. Must not be NULL; config->message_size
 *                    must be > 0; config->host must not be NULL for CONNECT.
 * @param out_handle  Receives the established link's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_TIMEOUT if the link is not established within
 *         config->connect_timeout_ms; RTE_STATUS_NOT_INITIALIZED if no
 *         backend is registered (rte_netlink_register_backend());
 *         RTE_STATUS_NOT_SUPPORTED if the registered backend does not
 *         implement open.
 * REQ-OAL-NETLINK-010
 */
rte_status_t rte_netlink_open(rte_netlink_storage_t *storage,
                                 const rte_netlink_config_t *config,
                                 rte_netlink_handle_t *out_handle);

/**
 * @brief Sends one fixed-size message, blocking at most timeout_ms.
 * @param handle        Link handle. Must not be NULL.
 * @param message       Message data to send. Must not be NULL.
 * @param message_size  Size of message in bytes; must be > 0.
 * @param timeout_ms    Maximum time to wait for the send to complete.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_TIMEOUT
 *         if the send does not complete in time; RTE_STATUS_HARDWARE_FAULT
 *         if the backend can positively confirm the peer is gone (e.g. a
 *         TCP disconnect, or an unreliable-transport backend's own
 *         best-effort signal - not guaranteed on every backend, see
 *         REQ-OAL-NETLINK-014); RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_netlink_open().
 * REQ-OAL-NETLINK-011
 */
rte_status_t rte_netlink_send(rte_netlink_handle_t handle,
                                 const void *message,
                                 size_t message_size,
                                 rte_duration_ms_t timeout_ms);

/**
 * @brief Receives one fixed-size message, blocking at most timeout_ms.
 * @param handle       Link handle. Must not be NULL.
 * @param out_message  Destination buffer. Must not be NULL.
 * @param buffer_size  Usable size of out_message in bytes; must be > 0.
 * @param timeout_ms   Maximum time to wait for a message to arrive.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_TIMEOUT
 *         if no message arrives in time; RTE_STATUS_HARDWARE_FAULT per
 *         rte_netlink_send()'s own note above; RTE_STATUS_DATA_CORRUPTION
 *         if a backend can detect the received message violated this
 *         link's wire contract (e.g. wrong length) but not necessarily its
 *         content (content integrity, if needed, is the caller's job - see
 *         REQ-OAL-NETLINK-014); RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_netlink_open().
 * REQ-OAL-NETLINK-012
 */
rte_status_t rte_netlink_receive(rte_netlink_handle_t handle,
                                    void *out_message,
                                    size_t buffer_size,
                                    rte_duration_ms_t timeout_ms);

/**
 * @brief Closes a link.
 * @param handle  Handle to close. Must not be NULL. Invalid to use after this call.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_netlink_open().
 * REQ-OAL-NETLINK-013
 */
rte_status_t rte_netlink_close(rte_netlink_handle_t handle);

/*
 * The backend vtable (rte_netlink_backend_t) and
 * rte_netlink_register_backend() live in
 * safeapi_backend/netlink/rte_netlink_backend.h, not here (ADR-021). This
 * header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_NETLINK_H */

/** @} */ /* NETLINK */

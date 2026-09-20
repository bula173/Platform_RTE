/**
 * @file sapi_osadapter_socket.h
 * @brief OS Abstraction Layer - Raw OS Socket Interface for OSAdapters.
 *
 * Exposes low-level OS socket operations (UDP/TCP creation, bind, connect,
 * send, recv, poll, non-blocking configuration, close). OSAdapters (e.g.
 * POSIX, QNX, FreeRTOS) provide these primitives, and ProtocolAdapters (e.g.
 * UDP, TCP) consume them without calling platform-specific APIs directly.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 * No dynamic memory allocation. All functions are non-throwing and return
 * sapi_status_t.
 *
 * @defgroup OSADAPTER_SOCKET OSAdapter Raw Socket Layer
 * @brief OS-agnostic socket operations provided by OSAdapters
 * @{
 */
#ifndef SAFEAPI_OSADAPTER_SOCKET_H
#define SAFEAPI_OSADAPTER_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Invalid socket handle sentinel. */
#define SAPI_OS_SOCKET_INVALID_HANDLE ((intptr_t)(-1))

/** @brief Socket handle representation (pointer or file descriptor). */
typedef intptr_t sapi_os_socket_handle_t;

/**
 * @brief Socket transport type.
 */
typedef enum sapi_os_socket_type_e
{
    SAPI_OS_SOCKET_TYPE_UDP = 1, /**< Raw UDP datagram socket. */
    SAPI_OS_SOCKET_TYPE_TCP = 2  /**< Raw TCP stream socket. */
} sapi_os_socket_type_t;

/**
 * @brief Polling events bitmask.
 */
typedef enum sapi_os_socket_event_e
{
    SAPI_OS_SOCKET_EVENT_READ  = 1U, /**< Socket is ready for reading without blocking. */
    SAPI_OS_SOCKET_EVENT_WRITE = 2U, /**< Socket is ready for writing without blocking. */
    SAPI_OS_SOCKET_EVENT_ERROR = 4U  /**< Error condition on socket. */
} sapi_os_socket_event_t;

/**
 * @brief Vtable of low-level OS socket operations implemented by an OSAdapter.
 */
typedef struct sapi_os_socket_ops_s
{
    /**
     * @brief Creates a raw socket of the specified type.
     * @param type SAPI_OS_SOCKET_TYPE_UDP or SAPI_OS_SOCKET_TYPE_TCP.
     * @param out_handle Output socket handle. Must not be NULL.
     */
    sapi_status_t (*open)(sapi_os_socket_type_t type, sapi_os_socket_handle_t *out_handle);

    /**
     * @brief Binds a socket to a local host and port.
     * @param handle Valid socket handle.
     * @param host Local IP address string (e.g. "127.0.0.1" or NULL for INADDR_ANY).
     * @param port Local port in host byte order.
     */
    sapi_status_t (*bind)(sapi_os_socket_handle_t handle, const char *host, uint16_t port);

    /**
     * @brief Connects a socket to a remote host and port.
     * @param handle Valid socket handle.
     * @param host Remote IP address string (e.g. "127.0.0.1"). Must not be NULL.
     * @param port Remote port in host byte order.
     */
    sapi_status_t (*connect)(sapi_os_socket_handle_t handle, const char *host, uint16_t port);

    /**
     * @brief Sends data on a connected socket.
     * @param handle Valid connected socket handle.
     * @param buf Buffer containing data to send. Must not be NULL.
     * @param len Number of bytes to send.
     * @param out_sent Optional output parameter receiving bytes actually sent.
     */
    sapi_status_t (*send)(sapi_os_socket_handle_t handle, const void *buf, size_t len, size_t *out_sent);

    /**
     * @brief Sends a datagram to a specific remote host and port.
     * @param handle Valid socket handle.
     * @param buf Buffer containing datagram to send. Must not be NULL.
     * @param len Number of bytes to send.
     * @param host Destination IP address string. Must not be NULL.
     * @param port Destination port in host byte order.
     * @param out_sent Optional output parameter receiving bytes actually sent.
     */
    sapi_status_t (*sendto)(sapi_os_socket_handle_t handle, const void *buf, size_t len,
                            const char *host, uint16_t port, size_t *out_sent);

    /**
     * @brief Receives data from a connected socket.
     * @param handle Valid connected socket handle.
     * @param buf Buffer receiving data. Must not be NULL.
     * @param len Maximum bytes to receive.
     * @param out_recv Output parameter receiving bytes actually received. Must not be NULL.
     */
    sapi_status_t (*recv)(sapi_os_socket_handle_t handle, void *buf, size_t len, size_t *out_recv);

    /**
     * @brief Receives a datagram and captures sender endpoint.
     * @param handle Valid socket handle.
     * @param buf Buffer receiving datagram. Must not be NULL.
     * @param len Maximum bytes to receive.
     * @param out_host Buffer to store sender IP address string (may be NULL).
     * @param host_len Capacity of out_host buffer.
     * @param out_port Pointer to store sender port (may be NULL).
     * @param out_recv Output parameter receiving bytes actually received. Must not be NULL.
     */
    sapi_status_t (*recvfrom)(sapi_os_socket_handle_t handle, void *buf, size_t len,
                              char *out_host, size_t host_len, uint16_t *out_port, size_t *out_recv);

    /**
     * @brief Polls a socket for I/O readiness with timeout.
     * @param handle Valid socket handle.
     * @param events Bitmask of SAPI_OS_SOCKET_EVENT_* to monitor.
     * @param timeout_ms Maximum time to wait in milliseconds.
     * @param out_revents Output bitmask of events ready. Must not be NULL.
     */
    sapi_status_t (*poll)(sapi_os_socket_handle_t handle, uint32_t events,
                          sapi_duration_ms_t timeout_ms, uint32_t *out_revents);

    /**
     * @brief Configures non-blocking mode on socket.
     * @param handle Valid socket handle.
     * @param nonblocking True for non-blocking, false for blocking.
     */
    sapi_status_t (*set_nonblocking)(sapi_os_socket_handle_t handle, bool nonblocking);

    /**
     * @brief Closes a socket handle.
     * @param handle Valid socket handle.
     */
    sapi_status_t (*close)(sapi_os_socket_handle_t handle);

} sapi_os_socket_ops_t;

/**
 * @brief Registers the active OSAdapter's raw socket operations.
 * @param ops Vtable of socket operations. Must not be NULL.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if ops is NULL.
 */
sapi_status_t sapi_osadapter_register_socket_ops(const sapi_os_socket_ops_t *ops);

/**
 * @brief Retrieves the active OSAdapter's raw socket operations.
 * @return Pointer to registered ops, or NULL if none registered.
 */
const sapi_os_socket_ops_t *sapi_osadapter_get_socket_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_SOCKET_H */

/** @} */

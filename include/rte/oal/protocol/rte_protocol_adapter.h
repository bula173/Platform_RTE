/**
 * @file rte_protocol_adapter.h
 * @brief Communication Protocol Adapter Interface (UDP, TCP, DDS).
 *
 * ProtocolAdapters encapsulate wire protocols, framing, and communication patterns:
 * - UDP ProtocolAdapter: datagrams, hello/ack handshake, peering over OS UDP sockets.
 * - TCP ProtocolAdapter: stream framing, connections over OS TCP sockets.
 * - DDS ProtocolAdapter: publish/subscribe topics over DDS middleware.
 *
 * RTE Framework / RTE selects which ProtocolAdapter to use based on configuration
 * per channel/flow.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 * Fixed caller-owned storage, non-throwing, defensive checks.
 *
 * @defgroup PROTOCOL_ADAPTER Protocol Adapter Layer
 * @brief Decoupled communication protocols for Channels and Flows
 * @{
 */
#ifndef RTE_PROTOCOL_ADAPTER_H
#define RTE_PROTOCOL_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"
#include "rte_osadapter/rte_osadapter_socket.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum length of host, topic, or endpoint string. */
#define RTE_PROTOCOL_ENDPOINT_STR_MAX (64U)

/**
 * @brief Protocol transport identifier.
 */
typedef enum rte_protocol_type_e
{
    RTE_PROTOCOL_TYPE_RAW_UDP = 1, /**< Raw UDP datagram with optional handshake. */
    RTE_PROTOCOL_TYPE_RAW_TCP = 2, /**< Raw TCP stream connection with framing. */
    RTE_PROTOCOL_TYPE_DDS     = 3  /**< DDS pub/sub flow middleware. */
} rte_protocol_type_t;

/**
 * @brief Communication role of the protocol adapter instance.
 */
typedef enum rte_protocol_role_e
{
    RTE_PROTOCOL_ROLE_LISTEN  = 1, /**< Passive listener / server. */
    RTE_PROTOCOL_ROLE_CONNECT = 2, /**< Active connector / client. */
    RTE_PROTOCOL_ROLE_PUB     = 3, /**< Publisher. */
    RTE_PROTOCOL_ROLE_SUB     = 4  /**< Subscriber. */
} rte_protocol_role_t;

/**
 * @brief Configuration passed to open a protocol adapter instance.
 */
typedef struct rte_protocol_config_s
{
    rte_protocol_type_t protocol_type;
    rte_protocol_role_t role;

    /** @brief Host address (UDP/TCP) or domain partition. */
    char host[RTE_PROTOCOL_ENDPOINT_STR_MAX];

    /** @brief Port (UDP/TCP). */
    uint16_t port;

    /** @brief Topic name (DDS). */
    char topic[RTE_PROTOCOL_ENDPOINT_STR_MAX];

    /** @brief Domain ID (DDS). */
    uint32_t domain_id;

    /** @brief Maximum expected message payload size. */
    size_t max_message_size;

    /** @brief Open / handshake timeout in milliseconds. */
    rte_duration_ms_t open_timeout_ms;

    /** @brief Default read/write timeout in milliseconds. */
    rte_duration_ms_t default_timeout_ms;

    /** @brief Enable handshake (HELLO/ACK) for raw UDP. */
    bool use_handshake;
} rte_protocol_config_t;

/** @brief Opaque protocol handle pointer. */
typedef struct rte_protocol_impl_s *rte_protocol_handle_t;

/**
 * @brief Operations vtable implemented by a concrete ProtocolAdapter.
 */
typedef struct rte_protocol_adapter_ops_s
{
    /**
     * @brief Opens and initializes a protocol adapter endpoint.
     * @param storage Caller-allocated memory buffer for adapter state. Must not be NULL.
     * @param storage_size Size of storage buffer in bytes.
     * @param config Adapter configuration. Must not be NULL.
     * @param os_sockets Vtable of raw OS socket operations (may be NULL if protocol is standalone e.g. DDS).
     * @param out_handle Output handle. Must not be NULL.
     */
    rte_status_t (*open)(void *storage, size_t storage_size,
                          const rte_protocol_config_t *config,
                          const rte_os_socket_ops_t *os_sockets,
                          rte_protocol_handle_t *out_handle);

    /**
     * @brief Sends a message over the protocol adapter.
     * @param handle Valid protocol handle.
     * @param data Message buffer. Must not be NULL.
     * @param size Size of message in bytes.
     * @param timeout_ms Maximum time to wait.
     */
    rte_status_t (*send)(rte_protocol_handle_t handle, const void *data,
                          size_t size, rte_duration_ms_t timeout_ms);

    /**
     * @brief Receives a message from the protocol adapter.
     * @param handle Valid protocol handle.
     * @param out_buf Output buffer. Must not be NULL.
     * @param buffer_size Capacity of output buffer.
     * @param timeout_ms Maximum time to wait.
     */
    rte_status_t (*receive)(rte_protocol_handle_t handle, void *out_buf,
                             size_t buffer_size, rte_duration_ms_t timeout_ms);

    /**
     * @brief Closes the protocol adapter endpoint.
     * @param handle Valid protocol handle.
     */
    rte_status_t (*close)(rte_protocol_handle_t handle);

} rte_protocol_adapter_ops_t;

/**
 * @brief Registers a ProtocolAdapter implementation for a given protocol type.
 * @param type RTE_PROTOCOL_TYPE_RAW_UDP, RAW_TCP, or DDS.
 * @param ops Vtable of protocol adapter operations. Must not be NULL.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM on invalid argument.
 */
rte_status_t rte_protocol_adapter_register(rte_protocol_type_t type,
                                             const rte_protocol_adapter_ops_t *ops);

/**
 * @brief Retrieves the registered ProtocolAdapter for a given protocol type.
 * @param type RTE_PROTOCOL_TYPE_RAW_UDP, RAW_TCP, or DDS.
 * @return Pointer to operations vtable, or NULL if not registered.
 */
const rte_protocol_adapter_ops_t *rte_protocol_adapter_get(rte_protocol_type_t type);

/**
 * @brief Returns the built-in raw UDP ProtocolAdapter vtable.
 */
const rte_protocol_adapter_ops_t *rte_protocol_adapter_get_udp(void);

/**
 * @brief Returns the built-in raw TCP ProtocolAdapter vtable.
 */
const rte_protocol_adapter_ops_t *rte_protocol_adapter_get_tcp(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_PROTOCOL_ADAPTER_H */

/** @} */

/**
 * @file rte_ipc_request_reply.h
 * @brief IPC Request-Reply Pattern (RPC-style communication)
 * @ingroup IPC
 *
 * Implements synchronous request-reply communication where a client sends
 * a request and blocks (with timeout) waiting for a reply. Server receives
 * the request, processes it, and sends a reply back.
 *
 * This is a higher-level abstraction built on the base IPC layer.
 *
 * Safety Properties:
 * - Deadlock-free (timeout prevents indefinite blocking)
 * - Request IDs prevent reply mismatches
 * - Explicit error on timeout (no silent hangs)
 * - Type-safe message handling
 *
 * Use Case:
 * - Train controller queries signal database for current signal state
 * - Dispatcher requests route information from route manager
 * - Any synchronous RPC pattern
 */

#ifndef SAFEAPI_IPC_REQUEST_REPLY_H
#define SAFEAPI_IPC_REQUEST_REPLY_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"
#include "rte_ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Request-Reply Channel (Server Side)
 * ========================================================================== */

/**
 * @brief Request-Reply server channel handle
 */
typedef struct rte_ipc_rr_server_s *rte_ipc_rr_server_t;

/**
 * @brief Configuration for request-reply server
 */
typedef struct {
    const char *name;              /**< Channel name (diagnostic) */
    size_t request_size;           /**< Fixed size of request messages */
    size_t reply_size;             /**< Fixed size of reply messages */
    size_t queue_depth;            /**< Max pending requests */
} rte_ipc_rr_server_config_t;

/**
 * @brief Incoming request with context for sending reply
 */
typedef struct {
    uint32_t request_id;           /**< Unique request ID (for correlation) */
    uint8_t *request_data;         /**< Pointer to request payload */
    size_t request_size;           /**< Actual request size */
} rte_ipc_rr_request_t;

/**
 * @brief Create a request-reply server channel
 *
 * @param handle_out     Receives server handle (not NULL)
 * @param config         Server configuration (not NULL)
 * @return RTE_STATUS_OK or error
 *
 * Example:
 * @code
 * rte_ipc_rr_server_config_t config = {
 *     .name = "signal-server",
 *     .request_size = sizeof(signal_query_t),
 *     .reply_size = sizeof(signal_reply_t),
 *     .queue_depth = 10
 * };
 * rte_ipc_rr_server_t server;
 * rte_ipc_rr_server_create(&server, &config);
 * @endcode
 */
rte_status_t rte_ipc_rr_server_create(rte_ipc_rr_server_t *handle_out,
                                         const rte_ipc_rr_server_config_t *config);

/**
 * @brief Receive a request (blocking with timeout)
 *
 * @param server         Server handle (not NULL)
 * @param request_out    Receives request and context (not NULL)
 * @param timeout_ms     Max wait time; 0 = poll, UINT32_MAX = infinite
 * @return RTE_STATUS_OK on success
 *         RTE_STATUS_TIMEOUT if no request arrived within timeout
 *         RTE_STATUS_INVALID_PARAM on invalid handle/buffer
 *
 * Example:
 * @code
 * rte_ipc_rr_request_t request;
 * if (rte_ipc_rr_receive_request(server, &request, 1000) == RTE_STATUS_OK) {
 *     // Process request...
 *     signal_query_t *query = (signal_query_t *)request.request_data;
 *     // Send reply...
 * }
 * @endcode
 */
rte_status_t rte_ipc_rr_receive_request(rte_ipc_rr_server_t server,
                                           rte_ipc_rr_request_t *request_out,
                                           rte_duration_ms_t timeout_ms);

/**
 * @brief Send reply to a request
 *
 * @param server         Server handle (not NULL)
 * @param request_id     ID from the request being replied to
 * @param reply_data     Reply payload (not NULL)
 * @param reply_size     Size of reply (must match configured size)
 * @return RTE_STATUS_OK on success
 *         RTE_STATUS_ERROR if reply fails
 *         RTE_STATUS_TIMEOUT if client has given up waiting
 *
 * Example:
 * @code
 * signal_reply_t reply = {
 *     .signal_state = SIGNAL_GREEN,
 *     .track_speed = 80
 * };
 * rte_ipc_rr_send_reply(server, request.request_id, &reply, sizeof(reply));
 * @endcode
 */
rte_status_t rte_ipc_rr_send_reply(rte_ipc_rr_server_t server,
                                      uint32_t request_id,
                                      const void *reply_data,
                                      size_t reply_size);

/**
 * @brief Destroy request-reply server
 *
 * @param server Server handle (not NULL)
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_ipc_rr_server_destroy(rte_ipc_rr_server_t server);

/* ============================================================================
 * Request-Reply Client (Client Side)
 * ========================================================================== */

/**
 * @brief Request-reply client handle
 */
typedef struct rte_ipc_rr_client_s *rte_ipc_rr_client_t;

/**
 * @brief Configuration for request-reply client
 */
typedef struct {
    const char *server_name;       /**< Server channel name to connect to */
    size_t request_size;           /**< Size of request messages */
    size_t reply_size;             /**< Size of reply messages */
} rte_ipc_rr_client_config_t;

/**
 * @brief Create a request-reply client connection
 *
 * @param handle_out     Receives client handle (not NULL)
 * @param config         Client configuration (not NULL)
 * @return RTE_STATUS_OK on success
 *         RTE_STATUS_ERROR if server not found
 *
 * Example:
 * @code
 * rte_ipc_rr_client_config_t config = {
 *     .server_name = "signal-server",
 *     .request_size = sizeof(signal_query_t),
 *     .reply_size = sizeof(signal_reply_t)
 * };
 * rte_ipc_rr_client_t client;
 * rte_ipc_rr_client_create(&client, &config);
 * @endcode
 */
rte_status_t rte_ipc_rr_client_create(rte_ipc_rr_client_t *handle_out,
                                         const rte_ipc_rr_client_config_t *config);

/**
 * @brief Send request and wait for reply (blocking with timeout)
 *
 * This is the main client operation: send request, block waiting for reply.
 *
 * @param client         Client handle (not NULL)
 * @param request_data   Request to send (not NULL)
 * @param request_size   Size of request
 * @param reply_out      Destination buffer for reply (not NULL)
 * @param reply_size     Max size of reply buffer
 * @param timeout_ms     Max wait time for reply; 0 = poll, UINT32_MAX = infinite
 * @return RTE_STATUS_OK on success (reply received)
 *         RTE_STATUS_TIMEOUT if no reply within timeout
 *         RTE_STATUS_ERROR if server rejected request
 *         RTE_STATUS_INVALID_PARAM on bad arguments
 *
 * Example:
 * @code
 * signal_query_t query = {.train_id = 1, .location = 500};
 * signal_reply_t reply;
 *
 * rte_status_t status = rte_ipc_rr_request(
 *     client,
 *     &query, sizeof(query),
 *     &reply, sizeof(reply),
 *     5000  // Wait up to 5 seconds for reply
 * );
 *
 * if (status == RTE_STATUS_OK) {
 *     printf("Signal state: %d\n", reply.signal_state);
 * } else if (status == RTE_STATUS_TIMEOUT) {
 *     printf("Server did not reply in time\n");
 * }
 * @endcode
 */
rte_status_t rte_ipc_rr_request(rte_ipc_rr_client_t client,
                                   const void *request_data,
                                   size_t request_size,
                                   void *reply_out,
                                   size_t reply_size,
                                   rte_duration_ms_t timeout_ms);

/**
 * @brief Destroy request-reply client connection
 *
 * @param client Client handle (not NULL)
 * @return RTE_STATUS_OK on success
 */
rte_status_t rte_ipc_rr_client_destroy(rte_ipc_rr_client_t client);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_IPC_REQUEST_REPLY_H */

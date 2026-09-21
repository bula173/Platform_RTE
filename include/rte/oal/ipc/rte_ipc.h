/**
 * @file rte_ipc.h
 * @brief OS Abstraction Layer - Inter-process/inter-task communication service.
 *
 * Bounded message queues/channels between safety tasks. Intended as the
 * transport underneath a future L1 safety communication layer (sequence
 * numbers, timeouts, authentication per EN 50159) - those defenses are out
 * of scope here; this service only guarantees bounded, non-corrupting
 * transport of fixed-size messages. See ADR-001.
 *
 * REQ-OAL-IPC-001: queues are bounded and statically sized at creation;
 *                  no unbounded growth.
 * REQ-OAL-IPC-002: send/receive shall accept an explicit timeout and shall
 *                  never block indefinitely by default.
 *
 * @defgroup IPC Inter-Process Communication
 * @brief Bounded message queues between safety tasks (ADR-001)
 * @{
 */
#ifndef RTE_OS_IPC_H
#define RTE_OS_IPC_H

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caller-owned, fixed-size storage backing one rte_ipc_handle_t. */
RTE_DECLARE_STORAGE(rte_ipc_storage_t, 64U);

/** @brief Opaque handle to a created IPC channel, returned by rte_ipc_create(). */
typedef struct rte_ipc_impl_s *rte_ipc_handle_t;

/** @brief Configuration for rte_ipc_create(). */
typedef struct rte_ipc_config_s
{
    const char *name;          /**< Diagnostic/lookup name for the channel. */
    size_t      message_size;  /**< Fixed size in bytes of every message. */
    size_t      queue_depth;   /**< Maximum number of queued messages. */
} rte_ipc_config_t;

/**
 * @brief Creates a bounded message channel.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Channel configuration. Must not be NULL; config->message_size
 *                    and config->queue_depth must both be > 0.
 * @param out_handle  Receives the created channel's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (rte_osadapter_ipc_register()); RTE_STATUS_NOT_SUPPORTED if the
 *         registered OSAdapter does not implement create.
 * REQ-OAL-IPC-010
 */
rte_status_t rte_ipc_create(rte_ipc_storage_t *storage,
                               const rte_ipc_config_t *config,
                               rte_ipc_handle_t *out_handle);

/**
 * @brief Sends one message, blocking at most timeout_ms.
 * @param handle        Channel handle. Must not be NULL.
 * @param message       Message data to send. Must not be NULL.
 * @param message_size  Size of message in bytes; must be > 0.
 * @param timeout_ms    Maximum time to wait for queue space.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_TIMEOUT if
 *         the queue stays full for the whole timeout; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_ipc_create().
 * REQ-OAL-IPC-011
 */
rte_status_t rte_ipc_send(rte_ipc_handle_t handle,
                             const void *message,
                             size_t message_size,
                             rte_duration_ms_t timeout_ms);

/**
 * @brief Receives one message, blocking at most timeout_ms.
 * @param handle       Channel handle. Must not be NULL.
 * @param out_message  Destination buffer. Must not be NULL.
 * @param buffer_size  Usable size of out_message in bytes; must be > 0.
 * @param timeout_ms   Maximum time to wait for a message to arrive.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_TIMEOUT if
 *         no message arrives within the timeout; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_ipc_create().
 * REQ-OAL-IPC-012
 */
rte_status_t rte_ipc_receive(rte_ipc_handle_t handle,
                                void *out_message,
                                size_t buffer_size,
                                rte_duration_ms_t timeout_ms);

/**
 * @brief Destroys a message channel.
 * @param handle  Channel handle. Must not be NULL. Invalid to use after this call.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_ipc_create().
 * REQ-OAL-IPC-013
 */
rte_status_t rte_ipc_destroy(rte_ipc_handle_t handle);

/*
 * The OSAdapter vtable (rte_osadapter_ipc_t) and rte_osadapter_ipc_register()
 * live in rte_osadapter/ipc/rte_osadapter_ipc.h, not here (ADR-021).
 * This header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* RTE_OS_IPC_H */

/** @} */ /* IPC */

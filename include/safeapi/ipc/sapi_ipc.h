/**
 * @file sapi_ipc.h
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
 */
#ifndef SAFEAPI_OS_IPC_H
#define SAFEAPI_OS_IPC_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

SAFEAPI_DECLARE_STORAGE(sapi_ipc_storage_t, 64U);

typedef struct sapi_ipc_impl_s *sapi_ipc_handle_t;

typedef struct sapi_ipc_config_s
{
    const char *name;          /**< Diagnostic/lookup name for the channel. */
    size_t      message_size;  /**< Fixed size in bytes of every message. */
    size_t      queue_depth;   /**< Maximum number of queued messages. */
} sapi_ipc_config_t;

/**
 * @brief Creates a bounded message channel.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Channel configuration. Must not be NULL; config->message_size
 *                    and config->queue_depth must both be > 0.
 * @param out_handle  Receives the created channel's handle. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM for a bad argument;
 *         SAPI_STATUS_NOT_INITIALIZED if no backend is registered
 *         (sapi_ipc_register_backend()); SAPI_STATUS_NOT_SUPPORTED if the
 *         registered backend does not implement create.
 * REQ-OAL-IPC-010
 */
sapi_status_t sapi_ipc_create(sapi_ipc_storage_t *storage,
                               const sapi_ipc_config_t *config,
                               sapi_ipc_handle_t *out_handle);

/**
 * @brief Sends one message, blocking at most timeout_ms.
 * @param handle        Channel handle. Must not be NULL.
 * @param message       Message data to send. Must not be NULL.
 * @param message_size  Size of message in bytes; must be > 0.
 * @param timeout_ms    Maximum time to wait for queue space.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_TIMEOUT if
 *         the queue stays full for the whole timeout; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_ipc_create().
 * REQ-OAL-IPC-011
 */
sapi_status_t sapi_ipc_send(sapi_ipc_handle_t handle,
                             const void *message,
                             size_t message_size,
                             sapi_duration_ms_t timeout_ms);

/**
 * @brief Receives one message, blocking at most timeout_ms.
 * @param handle       Channel handle. Must not be NULL.
 * @param out_message  Destination buffer. Must not be NULL.
 * @param buffer_size  Usable size of out_message in bytes; must be > 0.
 * @param timeout_ms   Maximum time to wait for a message to arrive.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_TIMEOUT if
 *         no message arrives within the timeout; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_ipc_create().
 * REQ-OAL-IPC-012
 */
sapi_status_t sapi_ipc_receive(sapi_ipc_handle_t handle,
                                void *out_message,
                                size_t buffer_size,
                                sapi_duration_ms_t timeout_ms);

/**
 * @brief Destroys a message channel.
 * @param handle  Channel handle. Must not be NULL. Invalid to use after this call.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_ipc_create().
 * REQ-OAL-IPC-013
 */
sapi_status_t sapi_ipc_destroy(sapi_ipc_handle_t handle);

/**
 * @brief Backend vtable: an integrator's implementation of the IPC service
 *        for a specific OS/RTOS (ADR-005). Any slot may be NULL if
 *        unsupported by the backend (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_ipc_backend_s
{
    sapi_status_t (*create)(sapi_ipc_storage_t *storage,
                             const sapi_ipc_config_t *config,
                             sapi_ipc_handle_t *out_handle);
    sapi_status_t (*send)(sapi_ipc_handle_t handle, const void *message,
                           size_t message_size, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*receive)(sapi_ipc_handle_t handle, void *out_message,
                              size_t buffer_size, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*destroy)(sapi_ipc_handle_t handle);
} sapi_ipc_backend_t;

/**
 * @brief Registers the backend implementation used by every sapi_ipc_*
 *        call (ADR-005 section 2.1). Call once at startup.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-IPC-014
 */
sapi_status_t sapi_ipc_register_backend(const sapi_ipc_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_IPC_H */

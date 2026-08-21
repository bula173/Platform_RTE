/**
 * @file sapi_ipc_backend.h
 * @brief OS-backend adaptation surface for the IPC service (ADR-005,
 *        ADR-021).
 *
 * For platform integrators implementing a sapi_ipc_backend_t and calling
 * sapi_ipc_register_backend() - NOT part of the consumer API
 * (safeapi/ipc/sapi_ipc.h). A real application should never include this
 * file; only the startup code that wires a concrete backend does.
 *
 * @defgroup IPC_BACKEND Inter-Process Communication - Backend Adaptation
 * @brief Backend vtable and registration for the IPC service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_IPC_BACKEND_H
#define SAFEAPI_OS_IPC_BACKEND_H

#include "safeapi/ipc/sapi_ipc.h"
#include "safeapi/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the IPC service
 *        for a specific OS/RTOS (ADR-005). Any slot may be NULL if
 *        unsupported by the backend (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_ipc_backend_s
{
    /** @brief Backend implementation of sapi_ipc_create(). May be NULL. */
    sapi_status_t (*create)(sapi_ipc_storage_t *storage,
                             const sapi_ipc_config_t *config,
                             sapi_ipc_handle_t *out_handle);
    /** @brief Backend implementation of sapi_ipc_send(). May be NULL. */
    sapi_status_t (*send)(sapi_ipc_handle_t handle, const void *message,
                           size_t message_size, sapi_duration_ms_t timeout_ms);
    /** @brief Backend implementation of sapi_ipc_receive(). May be NULL. */
    sapi_status_t (*receive)(sapi_ipc_handle_t handle, void *out_message,
                              size_t buffer_size, sapi_duration_ms_t timeout_ms);
    /** @brief Backend implementation of sapi_ipc_destroy(). May be NULL. */
    sapi_status_t (*destroy)(sapi_ipc_handle_t handle);
} sapi_ipc_backend_t;

/**
 * @brief Registers the backend implementation used by every sapi_ipc_*
 *        call (ADR-005 section 2.1). Call once at startup.
 * @param backend Vtable of backend function pointers. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * @return SAPI_STATUS_INVALID_STATE if the application's setup phase is
 *         already locked (ADR-026).
 * REQ-OAL-IPC-014
 */
sapi_status_t sapi_ipc_register_backend(const sapi_ipc_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_IPC_BACKEND_H */

/** @} */ /* IPC_BACKEND */

/**
 * @file sapi_osadapter_ipc.h
 * @brief OSAdapter interface for inter-process communication.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_IPC_H
#define SAFEAPI_OSADAPTER_IPC_H

#include <stddef.h>
#include "safeapi/oal/ipc/sapi_ipc.h"
#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter IPC operations vtable.
 */
typedef struct sapi_osadapter_ipc_s
{
    sapi_status_t (*create)(sapi_ipc_storage_t *storage,
                             const sapi_ipc_config_t *config,
                             sapi_ipc_handle_t *out_handle);
    sapi_status_t (*send)(sapi_ipc_handle_t handle, const void *message,
                           size_t message_size, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*receive)(sapi_ipc_handle_t handle, void *out_message,
                              size_t buffer_size, sapi_duration_ms_t timeout_ms);
    sapi_status_t (*destroy)(sapi_ipc_handle_t handle);
} sapi_osadapter_ipc_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_ipc_t sapi_ipc_backend_t;

/**
 * @brief Registers the OSAdapter IPC implementation.
 * @param adapter Pointer to IPC operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_ipc_register(const sapi_osadapter_ipc_t *adapter);

sapi_status_t sapi_ipc_register_backend(const sapi_ipc_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_IPC_H */

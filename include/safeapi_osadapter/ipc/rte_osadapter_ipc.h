/**
 * @file rte_osadapter_ipc.h
 * @brief OSAdapter interface for inter-process communication.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_IPC_H
#define SAFEAPI_OSADAPTER_IPC_H

#include <stddef.h>
#include "safeapi/oal/ipc/rte_ipc.h"
#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter IPC operations vtable.
 */
typedef struct rte_osadapter_ipc_s
{
    rte_status_t (*create)(rte_ipc_storage_t *storage,
                             const rte_ipc_config_t *config,
                             rte_ipc_handle_t *out_handle);
    rte_status_t (*send)(rte_ipc_handle_t handle, const void *message,
                           size_t message_size, rte_duration_ms_t timeout_ms);
    rte_status_t (*receive)(rte_ipc_handle_t handle, void *out_message,
                              size_t buffer_size, rte_duration_ms_t timeout_ms);
    rte_status_t (*destroy)(rte_ipc_handle_t handle);
} rte_osadapter_ipc_t;

/* Backward compatibility typedef */
typedef rte_osadapter_ipc_t rte_ipc_backend_t;

/**
 * @brief Registers the OSAdapter IPC implementation.
 * @param adapter Pointer to IPC operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_ipc_register(const rte_osadapter_ipc_t *adapter);

rte_status_t rte_ipc_register_backend(const rte_ipc_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_IPC_H */

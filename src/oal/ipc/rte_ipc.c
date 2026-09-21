/**
 * @file rte_ipc.c
 * @ingroup IPC
 * @brief IPC service: validates parameters, then dispatches to the backend
 *        registered via rte_ipc_register_backend() (ADR-005).
 */
#include "safeapi/oal/ipc/rte_ipc.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_backend/ipc/rte_ipc_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const rte_ipc_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_ipc_register_backend(const rte_ipc_backend_t *backend)
{
    rte_status_t lifecycle_status;

    if (backend == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_backend = backend;
    return RTE_STATUS_OK;
}

rte_status_t rte_ipc_create(rte_ipc_storage_t *storage,
                               const rte_ipc_config_t *config,
                               rte_ipc_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->message_size == 0U) || (config->queue_depth == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, config, out_handle);
}

rte_status_t rte_ipc_send(rte_ipc_handle_t handle,
                             const void *message,
                             size_t message_size,
                             rte_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (message == NULL) || (message_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->send == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->send(handle, message, message_size, timeout_ms);
}

rte_status_t rte_ipc_receive(rte_ipc_handle_t handle,
                                void *out_message,
                                size_t buffer_size,
                                rte_duration_ms_t timeout_ms)
{
    if ((handle == NULL) || (out_message == NULL) || (buffer_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->receive == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->receive(handle, out_message, buffer_size, timeout_ms);
}

rte_status_t rte_ipc_destroy(rte_ipc_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->destroy == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->destroy(handle);
}

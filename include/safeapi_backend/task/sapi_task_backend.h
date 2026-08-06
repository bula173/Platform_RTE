/**
 * @file sapi_task_backend.h
 * @brief OS-backend adaptation surface for the Task/thread scheduling
 *        service (ADR-005, ADR-021).
 *
 * For platform integrators implementing a sapi_task_backend_t and calling
 * sapi_task_register_backend() - NOT part of the consumer API
 * (safeapi/task/sapi_task.h). A real application should never include
 * this file; only the startup code that wires a concrete backend does.
 *
 * @defgroup TASK_BACKEND Task/Thread Scheduling - Backend Adaptation
 * @brief Backend vtable and registration for the task service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_TASK_BACKEND_H
#define SAFEAPI_OS_TASK_BACKEND_H

#include "safeapi/task/sapi_task.h"
#include "safeapi/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the task
 *        scheduling service for a specific OS/RTOS (ADR-005). Any slot may
 *        be NULL if unsupported by the backend (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_task_backend_s
{
    /** @brief Backend implementation of sapi_task_create(). May be NULL. */
    sapi_status_t (*create)(sapi_task_storage_t *storage,
                             const sapi_task_config_t *config,
                             sapi_task_handle_t *out_handle);
    /** @brief Backend implementation of sapi_task_start(). May be NULL. */
    sapi_status_t (*start)(sapi_task_handle_t handle);
    /** @brief Backend implementation of sapi_task_suspend(). May be NULL. */
    sapi_status_t (*suspend)(sapi_task_handle_t handle);
    /** @brief Backend implementation of sapi_task_destroy(). May be NULL. */
    sapi_status_t (*destroy)(sapi_task_handle_t handle);
} sapi_task_backend_t;

/**
 * @brief Registers the backend implementation used by every sapi_task_*
 *        call (ADR-005 section 2.1). Call once at startup.
 * @param backend Vtable of backend function pointers. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-TASK-014
 */
sapi_status_t sapi_task_register_backend(const sapi_task_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_TASK_BACKEND_H */

/** @} */ /* TASK_BACKEND */

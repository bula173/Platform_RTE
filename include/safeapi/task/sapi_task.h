/**
 * @file sapi_task.h
 * @brief OS Abstraction Layer - Task/thread scheduling service.
 *
 * Creation of periodic/cyclic safety tasks with fixed priorities, matching
 * the cyclic processing model typical of RBC implementations. See ADR-001.
 *
 * REQ-OAL-TASK-001: no dynamic allocation; caller supplies storage and a
 *                   fixed-size stack/context region (backend-defined use).
 * REQ-OAL-TASK-002: priorities are fixed at creation time; no dynamic
 *                   priority inheritance/inversion handling is assumed by
 *                   this API - that is a backend/RTOS concern.
 */
#ifndef SAFEAPI_OS_TASK_H
#define SAFEAPI_OS_TASK_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

SAFEAPI_DECLARE_STORAGE(sapi_task_storage_t, 128U);

typedef struct sapi_task_impl_s *sapi_task_handle_t;

/** @brief Task entry point, invoked cyclically at the configured period. */
typedef void (*sapi_task_entry_t)(void *user_ctx);

typedef struct sapi_task_config_s
{
    const char        *name;         /**< Diagnostic name, e.g. for logging. */
    sapi_task_entry_t  entry;        /**< Must not be NULL. */
    void              *user_ctx;     /**< Passed to entry on every cycle. */
    sapi_duration_ms_t period_ms;    /**< 0 = run once (non-cyclic) task. */
    uint32_t           priority;     /**< Backend-defined priority scale; documented per backend. */
    size_t             stack_size;   /**< Requested stack size in bytes. */
} sapi_task_config_t;

/**
 * @brief Creates a task in the suspended state.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Task configuration. Must not be NULL; config->entry must not be NULL.
 * @param out_handle  Receives the created task's handle. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM for a bad argument;
 *         SAPI_STATUS_NOT_INITIALIZED if no backend is registered
 *         (sapi_task_register_backend()); SAPI_STATUS_NOT_SUPPORTED if the
 *         registered backend does not implement create.
 * REQ-OAL-TASK-010
 */
sapi_status_t sapi_task_create(sapi_task_storage_t *storage,
                                const sapi_task_config_t *config,
                                sapi_task_handle_t *out_handle);

/**
 * @brief Starts a created task.
 * @param handle  Task handle. Must not be NULL.
 * @return SAPI_STATUS_OK, SAPI_STATUS_INVALID_PARAM, SAPI_STATUS_NOT_INITIALIZED,
 *         or SAPI_STATUS_NOT_SUPPORTED (see sapi_task_create()).
 * REQ-OAL-TASK-011
 */
sapi_status_t sapi_task_start(sapi_task_handle_t handle);

/**
 * @brief Suspends a running task.
 * @param handle  Task handle. Must not be NULL.
 * @return See sapi_task_start().
 * REQ-OAL-TASK-012
 */
sapi_status_t sapi_task_suspend(sapi_task_handle_t handle);

/**
 * @brief Terminates and destroys a task.
 * @param handle  Task handle. Must not be NULL. Invalid to use after this call.
 * @return See sapi_task_start().
 * REQ-OAL-TASK-013
 */
sapi_status_t sapi_task_destroy(sapi_task_handle_t handle);

/**
 * @brief Backend vtable: an integrator's implementation of the task
 *        scheduling service for a specific OS/RTOS (ADR-005). Any slot may
 *        be NULL if unsupported by the backend (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_task_backend_s
{
    sapi_status_t (*create)(sapi_task_storage_t *storage,
                             const sapi_task_config_t *config,
                             sapi_task_handle_t *out_handle);
    sapi_status_t (*start)(sapi_task_handle_t handle);
    sapi_status_t (*suspend)(sapi_task_handle_t handle);
    sapi_status_t (*destroy)(sapi_task_handle_t handle);
} sapi_task_backend_t;

/**
 * @brief Registers the backend implementation used by every sapi_task_*
 *        call (ADR-005 section 2.1). Call once at startup.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-TASK-014
 */
sapi_status_t sapi_task_register_backend(const sapi_task_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_TASK_H */

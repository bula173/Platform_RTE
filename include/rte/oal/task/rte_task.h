/**
 * @file rte_task.h
 * @brief OS Abstraction Layer - Task/thread scheduling service.
 *
 * Creation of periodic/cyclic safety tasks with fixed priorities, matching
 * the cyclic processing model typical of RBC implementations. See ADR-001.
 *
 * REQ-OAL-TASK-001: no dynamic allocation; caller supplies storage and a
 *                   fixed-size stack/context region (osadapter-defined use).
 * REQ-OAL-TASK-002: priorities are fixed at creation time; no dynamic
 *                   priority inheritance/inversion handling is assumed by
 *                   this API - that is an OSAdapter/RTOS concern.
 *
 * @defgroup TASK Task/Thread Scheduling
 * @brief Periodic/cyclic safety task creation with fixed priorities (ADR-001)
 * @{
 */
#ifndef RTE_OS_TASK_H
#define RTE_OS_TASK_H

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caller-owned, fixed-size storage backing one rte_task_handle_t. */
RTE_DECLARE_STORAGE(rte_task_storage_t, 128U);

/** @brief Opaque handle to a created task, returned by rte_task_create(). */
typedef struct rte_task_impl_s *rte_task_handle_t;

/** @brief Task entry point, invoked cyclically at the configured period. */
typedef void (*rte_task_entry_t)(void *user_ctx);

/** @brief Configuration for rte_task_create(). */
typedef struct rte_task_config_s
{
    const char        *name;         /**< Diagnostic name, e.g. for logging. */
    rte_task_entry_t  entry;        /**< Must not be NULL. */
    void              *user_ctx;     /**< Passed to entry on every cycle. */
    rte_duration_ms_t period_ms;    /**< 0 = run once (non-cyclic) task. */
    uint32_t           priority;     /**< Backend-defined priority scale; documented per OSAdapter. */
    size_t             stack_size;   /**< Requested stack size in bytes. */
} rte_task_config_t;

/**
 * @brief Creates a task in the suspended state.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Task configuration. Must not be NULL; config->entry must not be NULL.
 * @param out_handle  Receives the created task's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (rte_osadapter_task_register()); RTE_STATUS_NOT_SUPPORTED if the
 *         registered OSAdapter does not implement create.
 * REQ-OAL-TASK-010
 */
rte_status_t rte_task_create(rte_task_storage_t *storage,
                                const rte_task_config_t *config,
                                rte_task_handle_t *out_handle);

/**
 * @brief Starts a created task.
 * @param handle  Task handle. Must not be NULL.
 * @return RTE_STATUS_OK, RTE_STATUS_INVALID_PARAM, RTE_STATUS_NOT_INITIALIZED,
 *         or RTE_STATUS_NOT_SUPPORTED (see rte_task_create()).
 * REQ-OAL-TASK-011
 */
rte_status_t rte_task_start(rte_task_handle_t handle);

/**
 * @brief Suspends a running task.
 * @param handle  Task handle. Must not be NULL.
 * @return See rte_task_start().
 * REQ-OAL-TASK-012
 */
rte_status_t rte_task_suspend(rte_task_handle_t handle);

/**
 * @brief Terminates and destroys a task.
 * @param handle  Task handle. Must not be NULL. Invalid to use after this call.
 * @return See rte_task_start().
 * REQ-OAL-TASK-013
 */
rte_status_t rte_task_destroy(rte_task_handle_t handle);

/*
 * The OSAdapter vtable (rte_osadapter_task_t) and rte_osadapter_task_register()
 * live in rte_osadapter/task/rte_osadapter_task.h, not here (ADR-021).
 * This header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* RTE_OS_TASK_H */

/** @} */ /* TASK */

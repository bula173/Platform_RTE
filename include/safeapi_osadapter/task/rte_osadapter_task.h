/**
 * @file rte_osadapter_task.h
 * @brief OSAdapter interface for task and thread scheduling.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_TASK_H
#define SAFEAPI_OSADAPTER_TASK_H

#include "safeapi/oal/task/rte_task.h"
#include "safeapi/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter task scheduling operations vtable.
 */
typedef struct rte_osadapter_task_s
{
    rte_status_t (*create)(rte_task_storage_t *storage,
                             const rte_task_config_t *config,
                             rte_task_handle_t *out_handle);
    rte_status_t (*start)(rte_task_handle_t handle);
    rte_status_t (*suspend)(rte_task_handle_t handle);
    rte_status_t (*destroy)(rte_task_handle_t handle);
} rte_osadapter_task_t;

/**
 * @brief Registers the OSAdapter task implementation.
 * @param adapter Pointer to task operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_task_register(const rte_osadapter_task_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_TASK_H */

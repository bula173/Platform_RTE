/**
 * @file sapi_osadapter_task.h
 * @brief OSAdapter interface for task and thread scheduling.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_TASK_H
#define SAFEAPI_OSADAPTER_TASK_H

#include "safeapi/oal/task/sapi_task.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter task scheduling operations vtable.
 */
typedef struct sapi_osadapter_task_s
{
    sapi_status_t (*create)(sapi_task_storage_t *storage,
                             const sapi_task_config_t *config,
                             sapi_task_handle_t *out_handle);
    sapi_status_t (*start)(sapi_task_handle_t handle);
    sapi_status_t (*suspend)(sapi_task_handle_t handle);
    sapi_status_t (*destroy)(sapi_task_handle_t handle);
} sapi_osadapter_task_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_task_t sapi_task_backend_t;

/**
 * @brief Registers the OSAdapter task implementation.
 * @param adapter Pointer to task operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_task_register(const sapi_osadapter_task_t *adapter);

sapi_status_t sapi_task_register_backend(const sapi_task_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_TASK_H */

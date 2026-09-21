/**
 * @file rte_osadapter_mutex.h
 * @brief OSAdapter interface for mutual exclusion.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_MUTEX_H
#define SAFEAPI_OSADAPTER_MUTEX_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/oal/mutex/rte_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter mutex operations vtable.
 */
typedef struct rte_osadapter_mutex_s
{
    rte_status_t (*create)(rte_mutex_storage_t *storage, rte_mutex_handle_t *out_handle);
    rte_status_t (*lock)(rte_mutex_handle_t handle);
    rte_status_t (*unlock)(rte_mutex_handle_t handle);
    rte_status_t (*destroy)(rte_mutex_handle_t handle);
} rte_osadapter_mutex_t;

/* Backward compatibility typedef */
typedef rte_osadapter_mutex_t rte_mutex_backend_t;

/**
 * @brief Registers the OSAdapter mutex implementation.
 * @param adapter Pointer to mutex operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_mutex_register(const rte_osadapter_mutex_t *adapter);

rte_status_t rte_mutex_register_backend(const rte_mutex_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_MUTEX_H */

/**
 * @file rte_osadapter_mutex.h
 * @brief OSAdapter interface for mutual exclusion.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef RTE_OSADAPTER_MUTEX_H
#define RTE_OSADAPTER_MUTEX_H

#include "rte/utils/status/rte_status.h"
#include "rte/oal/mutex/rte_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter mutex operations vtable.
 *
 * create() shall make a priority-inheritance mutex (REQ-OAL-MUTEX-004, OSA rule R2). It returns RTE_STATUS_OK,
 * RTE_STATUS_INVALID_PARAM for a NULL storage or out_handle, or RTE_STATUS_INTERNAL_ERROR when the platform cannot
 * create a priority-inheritance mutex: then *out_handle is not written, the refusal is reported through
 * rte_safestate_enter(RTE_SAFESTATE_LEVEL_DEGRADED, RTE_SAFESTATE_REASON_OSA_RT_ATTRIBUTE, ...), and the adapter
 * never falls back to a plain mutex.
 */
typedef struct rte_osadapter_mutex_s
{
    rte_status_t (*create)(rte_mutex_storage_t *storage, rte_mutex_handle_t *out_handle);
    rte_status_t (*lock)(rte_mutex_handle_t handle);
    rte_status_t (*unlock)(rte_mutex_handle_t handle);
    rte_status_t (*destroy)(rte_mutex_handle_t handle);
} rte_osadapter_mutex_t;

/**
 * @brief Registers the OSAdapter mutex implementation.
 * @param adapter Pointer to mutex operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_mutex_register(const rte_osadapter_mutex_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* RTE_OSADAPTER_MUTEX_H */

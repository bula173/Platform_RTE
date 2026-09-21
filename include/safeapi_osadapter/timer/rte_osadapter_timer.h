/**
 * @file rte_osadapter_timer.h
 * @brief OSAdapter interface for monotonic timers and timestamps.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_TIMER_H
#define SAFEAPI_OSADAPTER_TIMER_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/oal/timer/rte_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter timer operations vtable.
 */
typedef struct rte_osadapter_timer_s
{
    rte_status_t (*create)(rte_timer_storage_t *storage,
                             const rte_timer_config_t *config,
                             rte_timer_handle_t *out_handle);
    rte_status_t (*start)(rte_timer_handle_t handle);
    rte_status_t (*stop)(rte_timer_handle_t handle);
    rte_status_t (*destroy)(rte_timer_handle_t handle);
    rte_status_t (*now)(rte_timestamp_ms_t *out_now_ms);
} rte_osadapter_timer_t;

/* Backward compatibility typedef */
typedef rte_osadapter_timer_t rte_timer_backend_t;

/**
 * @brief Registers the OSAdapter timer implementation.
 * @param adapter Pointer to timer operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_timer_register(const rte_osadapter_timer_t *adapter);

rte_status_t rte_timer_register_backend(const rte_timer_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_TIMER_H */

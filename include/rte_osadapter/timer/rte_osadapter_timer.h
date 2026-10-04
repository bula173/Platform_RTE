/**
 * @file rte_osadapter_timer.h
 * @brief OSAdapter interface for monotonic timers and timestamps.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef RTE_OSADAPTER_TIMER_H
#define RTE_OSADAPTER_TIMER_H

#include "rte/utils/status/rte_status.h"
#include "rte/oal/timer/rte_timer.h"

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

/**
 * @brief Registers the OSAdapter timer implementation.
 * @param adapter Pointer to timer operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL, or RTE_STATUS_INVALID_STATE if the
 *         setup phase is already locked (REQ-LIFECYCLE-001, ADR-026; the previous registration, if any, is kept).
 *
 * REQ-OAL-TIMER-015: Follows the OSAdapter-registration contract (ADR-005 section 2.1): a NULL adapter is rejected
 * with RTE_STATUS_INVALID_PARAM; re-registering replaces the previous OSAdapter.
 */
rte_status_t rte_osadapter_timer_register(const rte_osadapter_timer_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* RTE_OSADAPTER_TIMER_H */

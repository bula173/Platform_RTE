/**
 * @file rte_osadapter_clocksync.h
 * @brief OSAdapter interface for clock synchronization.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_CLOCKSYNC_H
#define SAFEAPI_OSADAPTER_CLOCKSYNC_H

#include <stdint.h>
#include "safeapi/oal/clocksync/rte_clocksync.h"
#include "safeapi/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter clock synchronization operations vtable.
 */
typedef struct rte_osadapter_clocksync_s
{
    rte_status_t (*get_offset_ms)(int64_t *out_offset_ms);
    rte_status_t (*get_quality)(rte_clocksync_quality_t *out_quality);
} rte_osadapter_clocksync_t;

/**
 * @brief Registers the OSAdapter clock synchronization implementation.
 * @param adapter Pointer to clocksync operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_clocksync_register(const rte_osadapter_clocksync_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_CLOCKSYNC_H */

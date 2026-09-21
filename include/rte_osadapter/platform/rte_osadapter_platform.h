/**
 * @file rte_osadapter_platform.h
 * @brief OSAdapter interface for real-time platform configuration.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef RTE_OSADAPTER_PLATFORM_H
#define RTE_OSADAPTER_PLATFORM_H

#include <stdint.h>
#include "rte/oal/platform/rte_platform.h"
#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter platform operations vtable.
 */
typedef struct rte_osadapter_platform_s
{
    rte_status_t (*realtime_init)(uint32_t rt_priority);
} rte_osadapter_platform_t;

/**
 * @brief Registers the OSAdapter platform implementation.
 * @param adapter Pointer to platform operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_platform_register(const rte_osadapter_platform_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* RTE_OSADAPTER_PLATFORM_H */

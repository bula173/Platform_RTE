/**
 * @file rte_osadapter_reboot.h
 * @brief OSAdapter interface for controlled system reboot.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef RTE_OSADAPTER_REBOOT_H
#define RTE_OSADAPTER_REBOOT_H

#include <stdint.h>
#include "rte/oal/reboot/rte_reboot.h"
#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter reboot operations vtable.
 */
typedef struct rte_osadapter_reboot_s
{
    rte_status_t (*request)(uint16_t reason_code);
} rte_osadapter_reboot_t;

/**
 * @brief Registers the OSAdapter reboot implementation.
 * @param adapter Pointer to reboot operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_reboot_register(const rte_osadapter_reboot_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* RTE_OSADAPTER_REBOOT_H */

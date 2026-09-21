/**
 * @file rte_osadapter_reboot.h
 * @brief OSAdapter interface for controlled system reboot.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_REBOOT_H
#define SAFEAPI_OSADAPTER_REBOOT_H

#include <stdint.h>
#include "safeapi/oal/reboot/rte_reboot.h"
#include "safeapi/utils/status/rte_status.h"

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

/* Backward compatibility typedef */
typedef rte_osadapter_reboot_t rte_reboot_backend_t;

/**
 * @brief Registers the OSAdapter reboot implementation.
 * @param adapter Pointer to reboot operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_reboot_register(const rte_osadapter_reboot_t *adapter);

rte_status_t rte_reboot_register_backend(const rte_reboot_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_REBOOT_H */

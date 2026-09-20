/**
 * @file sapi_osadapter_reboot.h
 * @brief OSAdapter interface for controlled system reboot.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_REBOOT_H
#define SAFEAPI_OSADAPTER_REBOOT_H

#include <stdint.h>
#include "safeapi/oal/reboot/sapi_reboot.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter reboot operations vtable.
 */
typedef struct sapi_osadapter_reboot_s
{
    sapi_status_t (*request)(uint16_t reason_code);
} sapi_osadapter_reboot_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_reboot_t sapi_reboot_backend_t;

/**
 * @brief Registers the OSAdapter reboot implementation.
 * @param adapter Pointer to reboot operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_reboot_register(const sapi_osadapter_reboot_t *adapter);

sapi_status_t sapi_reboot_register_backend(const sapi_reboot_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_REBOOT_H */

/**
 * @file sapi_reboot.h
 * @brief OS Abstraction Layer - Controlled reboot service (ADR-004 section 3).
 *
 * Requests a controlled system restart (watchdog trigger, CPU reset
 * instruction, supervisory processor command - backend-defined). Intended
 * as the mechanism a SAPI_REBOOT()/SAPI_SAFESTATE_LEVEL_REBOOT handler
 * calls into (see ADR-004 section 2.3); this header has no dependency on
 * sapi_safestate.h and can be used directly.
 *
 * REQ-OAL-REBOOT-001: sapi_reboot_request() is not expected to return on
 *                     success; a return only occurs if the backend cannot
 *                     perform the reboot.
 *
 * @defgroup REBOOT Controlled Reboot
 * @brief Backend-defined controlled system restart (ADR-004 section 3)
 * @{
 */
#ifndef SAFEAPI_OS_REBOOT_H
#define SAFEAPI_OS_REBOOT_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Requests a controlled system restart.
 * @param reason_code  Backend-visible diagnostic code (e.g. persisted to
 *                     NVM/black-box storage before the reset, backend-defined).
 * @return Does not return on success (the CPU resets). Returns
 *         SAPI_STATUS_NOT_INITIALIZED if no backend is registered,
 *         SAPI_STATUS_NOT_SUPPORTED if the backend cannot perform a
 *         reboot, or a backend-defined error status.
 *
 * REQ-OAL-REBOOT-010
 */
sapi_status_t sapi_reboot_request(uint16_t reason_code);

/**
 * @brief Backend vtable: an integrator's implementation of the reboot
 *        mechanism for a specific target (ADR-005).
 */
typedef struct sapi_reboot_backend_s
{
    /** @brief Backend implementation of sapi_reboot_request(). May be NULL. */
    sapi_status_t (*request)(uint16_t reason_code);
} sapi_reboot_backend_t;

/**
 * @brief Registers the backend implementation used by
 *        sapi_reboot_request() (ADR-005 section 2.1). Call once at startup.
 * @param backend  Backend implementation. Must not be NULL. Registering
 *                 again replaces the previously registered backend.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-REBOOT-011
 */
sapi_status_t sapi_reboot_register_backend(const sapi_reboot_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_REBOOT_H */

/** @} */ /* REBOOT */

/**
 * @file sapi_reboot_backend.h
 * @brief OS-backend adaptation surface for the Controlled Reboot service
 *        (ADR-004 section 3, ADR-005, ADR-021).
 *
 * For platform integrators implementing a sapi_reboot_backend_t and
 * calling sapi_reboot_register_backend() - NOT part of the consumer API
 * (safeapi/reboot/sapi_reboot.h). A real application should never
 * include this file; only the startup code that wires a concrete
 * backend does.
 *
 * @defgroup REBOOT_BACKEND Controlled Reboot - Backend Adaptation
 * @brief Backend vtable and registration for the reboot service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_REBOOT_BACKEND_H
#define SAFEAPI_OS_REBOOT_BACKEND_H

#include "safeapi/reboot/sapi_reboot.h"
#include "safeapi/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#endif /* SAFEAPI_OS_REBOOT_BACKEND_H */

/** @} */ /* REBOOT_BACKEND */

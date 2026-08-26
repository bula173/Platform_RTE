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

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

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

/*
 * The backend vtable (sapi_reboot_backend_t) and
 * sapi_reboot_register_backend() live in
 * safeapi_backend/reboot/sapi_reboot_backend.h, not here (ADR-021). This
 * header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_REBOOT_H */

/** @} */ /* REBOOT */

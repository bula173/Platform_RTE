/**
 * @file rte_reboot.h
 * @brief OS Abstraction Layer - Controlled reboot service (ADR-004 section 3).
 *
 * Requests a controlled system restart (watchdog trigger, CPU reset
 * instruction, supervisory processor command - osadapter-defined). Intended
 * as the mechanism a RTE_REBOOT()/RTE_SAFESTATE_LEVEL_REBOOT handler
 * calls into (see ADR-004 section 2.3); this header has no dependency on
 * rte_safestate.h and can be used directly.
 *
 * REQ-OAL-REBOOT-001: rte_reboot_request() is not expected to return on
 *                     success; a return only occurs if the OSAdapter cannot
 *                     perform the reboot.
 *
 * @defgroup REBOOT Controlled Reboot
 * @brief Backend-defined controlled system restart (ADR-004 section 3)
 * @{
 */
#ifndef SAFEAPI_OS_REBOOT_H
#define SAFEAPI_OS_REBOOT_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Requests a controlled system restart.
 * @param reason_code  Backend-visible diagnostic code (e.g. persisted to
 *                     NVM/black-box storage before the reset, osadapter-defined).
 * @return Does not return on success (the CPU resets). Returns
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered,
 *         RTE_STATUS_NOT_SUPPORTED if the OSAdapter cannot perform a
 *         reboot, or a osadapter-defined error status.
 *
 * REQ-OAL-REBOOT-010
 */
rte_status_t rte_reboot_request(uint16_t reason_code);

/*
 * The OSAdapter vtable (rte_osadapter_reboot_t) and
 * rte_osadapter_reboot_register() live in
 * safeapi_osadapter/reboot/rte_osadapter_reboot.h, not here (ADR-021). This
 * header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_REBOOT_H */

/** @} */ /* REBOOT */

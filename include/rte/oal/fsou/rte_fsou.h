/**
 * @file rte_fsou.h
 * @brief Fail-Safe Output Unit (FSOU) output permission, optional OSAdapter seam (ADR-041, proposed).
 *
 * The SCP model has a hardware FSOU that passes a site's outputs only when both channels granted permission in the
 * current cycle window (Capella IF-SCP-02/03, baseline rule 4). This module is the RTE side of that seam: it
 * dispatches a per-cycle grant and a revoke to whatever FSOU OSAdapter the integrator registered
 * (rte_osadapter/fsou/rte_osadapter_fsou.h), following the ADR-005 registration pattern.
 *
 * The seam is optional. With no OSAdapter registered (e.g. the container build) there is no FSOU and the software
 * checkpoint (rte_channel_checkpoint()) stays the only output gate; rte_fsou_report_presence() says which case
 * applies, once.
 *
 * Nothing in RTE calls rte_fsou_grant() / rte_fsou_revoke() yet: where the grant hooks into the cycle is the open
 * decision of ADR-041.
 *
 * @defgroup FSOU FSOU output permission
 * @{
 *
 * REQ-FSOU-001: rte_osadapter_fsou_register() shall reject a NULL adapter with RTE_STATUS_INVALID_PARAM and shall be
 *               setup-only (RTE_STATUS_INVALID_STATE once the setup phase is locked).
 * REQ-FSOU-002: rte_fsou_grant() and rte_fsou_revoke() shall return RTE_STATUS_NOT_INITIALIZED if no OSAdapter is
 *               registered, RTE_STATUS_NOT_SUPPORTED if the registered OSAdapter's slot is NULL, and otherwise the
 *               OSAdapter's own status.
 * REQ-FSOU-003: rte_fsou_report_presence() shall log whether an FSOU OSAdapter is registered exactly once per
 *               process; later calls log nothing.
 */
#ifndef RTE_FSOU_H
#define RTE_FSOU_H

#include <stdbool.h>
#include <stdint.h>

#include "rte/utils/safestate/rte_safestate.h"
#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The OSAdapter vtable (rte_osadapter_fsou_t) and rte_osadapter_fsou_register() live in
 * rte_osadapter/fsou/rte_osadapter_fsou.h, not here (ADR-021). This header is the consumer-facing surface only.
 */

/**
 * @brief Grants this channel's output permission to the FSOU for one cycle (REQ-FSOU-002).
 * @param cycle_id  Cycle the permission is for (the cycle whose checkpoint returned OK, ADR-041).
 * @return RTE_STATUS_NOT_INITIALIZED if no FSOU OSAdapter is registered; RTE_STATUS_NOT_SUPPORTED if the registered
 *         OSAdapter's grant slot is NULL; otherwise the OSAdapter's status (RTE_STATUS_OK when granted).
 */
rte_status_t rte_fsou_grant(uint32_t cycle_id);

/**
 * @brief Withdraws this channel's output permission from the FSOU (REQ-FSOU-002).
 * @param reason  Why the permission is withdrawn (same codes as rte_safestate_enter()).
 * @return RTE_STATUS_NOT_INITIALIZED if no FSOU OSAdapter is registered; RTE_STATUS_NOT_SUPPORTED if the registered
 *         OSAdapter's revoke slot is NULL; otherwise the OSAdapter's status (RTE_STATUS_OK when revoked).
 */
rte_status_t rte_fsou_revoke(rte_safestate_reason_t reason);

/**
 * @brief Tells whether an FSOU OSAdapter is registered.
 * @return true if rte_osadapter_fsou_register() succeeded at least once, false otherwise.
 */
bool rte_fsou_is_present(void);

/**
 * @brief Logs once whether an FSOU is present (REQ-FSOU-003).
 *
 * The first call logs "FSOU present" or "no FSOU registered" (rte_log, INFO, tag "FSOU") and latches; every later
 * call logs nothing. Intended caller: the integrator's init, after OSAdapter registration, single-threaded.
 */
void rte_fsou_report_presence(void);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* RTE_FSOU_H */

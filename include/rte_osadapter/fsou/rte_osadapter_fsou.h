/**
 * @file rte_osadapter_fsou.h
 * @brief OSAdapter interface for the Fail-Safe Output Unit (FSOU) output permission (ADR-041).
 *
 * Optional seam: a platform with an FSOU registers this vtable; a build without one (the container build) leaves it
 * unregistered and the software checkpoint stays the only output gate. See rte/oal/fsou/rte_fsou.h for the
 * consumer-facing calls.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef RTE_OSADAPTER_FSOU_H
#define RTE_OSADAPTER_FSOU_H

#include <stdint.h>
#include "rte/oal/fsou/rte_fsou.h"
#include "rte/utils/safestate/rte_safestate.h"
#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter FSOU operations vtable.
 *
 * Each channel (A and B) owns its own FSOU input; the FSOU hardware does the AND of both channels' permissions in
 * the current cycle window (ADR-041). A slot may be NULL: the matching rte_fsou_*() call then returns
 * RTE_STATUS_NOT_SUPPORTED.
 */
typedef struct rte_osadapter_fsou_s
{
    /** Grants this channel's output permission for cycle @p cycle_id. */
    rte_status_t (*grant)(uint32_t cycle_id);
    /** Withdraws this channel's output permission at once, for @p reason. */
    rte_status_t (*revoke)(rte_safestate_reason_t reason);
} rte_osadapter_fsou_t;

/**
 * @brief Registers the OSAdapter FSOU implementation (REQ-FSOU-001).
 *
 * Setup-only (ADR-026): refused once the application's setup phase is locked.
 *
 * @param adapter Pointer to the FSOU operations vtable. Must stay valid for the life of the process.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if @p adapter is NULL; RTE_STATUS_INVALID_STATE if the
 *         setup phase is locked (the previous registration, if any, is kept).
 */
rte_status_t rte_osadapter_fsou_register(const rte_osadapter_fsou_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* RTE_OSADAPTER_FSOU_H */

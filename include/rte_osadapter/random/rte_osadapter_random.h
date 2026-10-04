/**
 * @file rte_osadapter_random.h
 * @brief OSAdapter interface for cryptographically secure random bytes.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef RTE_OSADAPTER_RANDOM_H
#define RTE_OSADAPTER_RANDOM_H

#include <stddef.h>
#include <stdint.h>
#include "rte/oal/random/rte_random.h"
#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter random operations vtable.
 *
 * @c fill receives an already-validated request (non-NULL @p out, 1..RTE_RANDOM_MAX_REQUEST bytes) and returns
 * RTE_STATUS_OK only when every byte came from a cryptographically secure source (REQ-OAL-RANDOM-001).
 */
typedef struct rte_osadapter_random_s
{
    rte_status_t (*fill)(uint8_t *out, size_t len);
} rte_osadapter_random_t;

/**
 * @brief Registers the OSAdapter random implementation.
 * @param adapter Pointer to random operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL, or RTE_STATUS_INVALID_STATE if the
 *         setup phase is already locked (REQ-LIFECYCLE-001, ADR-026; the previous registration, if any, is kept).
 *
 * REQ-OAL-RANDOM-011
 */
rte_status_t rte_osadapter_random_register(const rte_osadapter_random_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* RTE_OSADAPTER_RANDOM_H */

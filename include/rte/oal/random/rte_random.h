/**
 * @file rte_random.h
 * @brief OS Abstraction Layer - Cryptographically secure random bytes.
 *
 * The seam through which application code obtains unpredictable random bytes (e.g. the EuroRadio responder random RB,
 * Subset-037-2) without naming an OSAdapter or calling the OS itself (ADR-005). What backs it is OSAdapter-defined: a
 * POSIX OSAdapter uses the kernel CSPRNG; a bare-metal one would use a hardware TRNG.
 *
 * REQ-OAL-RANDOM-001: the registered OSAdapter shall return bytes from a cryptographically secure source, or fail;
 *                     it shall never fall back to a predictable generator.
 *
 * @defgroup RANDOM Secure Random Bytes
 * @brief OSAdapter-backed cryptographically secure random bytes
 * @{
 */
#ifndef RTE_OAL_RANDOM_H
#define RTE_OAL_RANDOM_H

#include <stddef.h>
#include <stdint.h>

#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Largest request rte_random_fill() accepts in one call (bytes). */
#define RTE_RANDOM_MAX_REQUEST 256U

/**
 * @brief Fills @p out with @p len cryptographically secure random bytes from the registered OSAdapter.
 *
 * @param out  Destination buffer. Non-NULL.
 * @param len  Number of bytes, 1..RTE_RANDOM_MAX_REQUEST.
 * @return RTE_STATUS_OK with @p out filled.
 *         RTE_STATUS_INVALID_PARAM for a NULL @p out or @p len outside 1..RTE_RANDOM_MAX_REQUEST.
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered.
 *         RTE_STATUS_NOT_SUPPORTED if the registered OSAdapter does not implement this operation.
 *         RTE_STATUS_HARDWARE_FAULT if the OSAdapter's source failed; @p out must then not be used.
 *
 * REQ-OAL-RANDOM-010
 */
rte_status_t rte_random_fill(uint8_t *out, size_t len);

/*
 * The OSAdapter vtable (rte_osadapter_random_t) and rte_osadapter_random_register() live in
 * rte_osadapter/random/rte_osadapter_random.h (ADR-021). This header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* RTE_OAL_RANDOM_H */

/** @} */ /* RANDOM */

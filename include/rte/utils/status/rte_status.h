/**
 * @file rte_status.h
 * @brief Common status/error codes returned by every Safe API Framework
 *        function. Shared across all OS Abstraction Layer (OAL) services.
 *
 * REQ-OAL-COMMON-001: every fallible API function shall return rte_status_t
 * and shall not use exceptions or errno-style side channels.
 *
 * @defgroup STATUS Common Status and Error Codes
 * @brief Shared result codes returned by every framework function
 * @{
 */
#ifndef RTE_COMMON_STATUS_H
#define RTE_COMMON_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Common result/status codes.
 *
 * Explicit numeric values are fixed and part of the ABI: do not renumber
 * existing entries, only append.
 */
typedef enum rte_status_e
{
    RTE_STATUS_OK                    = 0,  /**< Operation succeeded. */
    RTE_STATUS_INVALID_PARAM         = 1,  /**< Null pointer or out-of-range argument. */
    RTE_STATUS_NOT_INITIALIZED       = 2,  /**< Object used before create/init. */
    RTE_STATUS_ALREADY_INITIALIZED   = 3,  /**< Object already created/initialized. */
    RTE_STATUS_TIMEOUT               = 4,  /**< Blocking call exceeded its deadline. */
    RTE_STATUS_RESOURCE_EXHAUSTED    = 5,  /**< Static pool/storage/slots full. */
    RTE_STATUS_NOT_SUPPORTED         = 6,  /**< Valid request, OSAdapter cannot perform it. */
    RTE_STATUS_NOT_IMPLEMENTED       = 7,  /**< OSAdapter is a stub (skeleton state). */
    RTE_STATUS_HARDWARE_FAULT        = 8,  /**< OSAdapter reported a hardware-level fault. */
    RTE_STATUS_DATA_CORRUPTION       = 9,  /**< Integrity check (e.g. NVM CRC) failed. */
    RTE_STATUS_INTERNAL_ERROR        = 10, /**< Defensive catch-all: should never happen. */
    RTE_STATUS_VALUE_OUT_OF_RANGE    = 11, /**< Checked cast: value does not fit the destination type. */
    RTE_STATUS_INVALID_STATE         = 12  /**< Operation not permitted in the application's current
                                              *   lifecycle phase (e.g. a setup-only constructor called
                                              *   after rte_appmanager_run() has locked setup - see
                                              *   rte_lifecycle.h). */
} rte_status_t;

/**
 * @brief Returns a short, static, human-readable string for a status code.
 *        Intended for diagnostics/logging only; never on a safety-decision path.
 *
 * REQ-OAL-COMMON-002
 */
const char *rte_status_to_string(rte_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* RTE_COMMON_STATUS_H */

/** @} */ /* STATUS */

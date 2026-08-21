/**
 * @file sapi_status.h
 * @brief Common status/error codes returned by every Safe API Framework
 *        function. Shared across all OS Abstraction Layer (OAL) services.
 *
 * REQ-OAL-COMMON-001: every fallible API function shall return sapi_status_t
 * and shall not use exceptions or errno-style side channels.
 *
 * @defgroup STATUS Common Status and Error Codes
 * @brief Shared result codes returned by every framework function
 * @{
 */
#ifndef SAFEAPI_COMMON_STATUS_H
#define SAFEAPI_COMMON_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Common result/status codes.
 *
 * Explicit numeric values are fixed and part of the ABI: do not renumber
 * existing entries, only append.
 */
typedef enum sapi_status_e
{
    SAPI_STATUS_OK                    = 0,  /**< Operation succeeded. */
    SAPI_STATUS_INVALID_PARAM         = 1,  /**< Null pointer or out-of-range argument. */
    SAPI_STATUS_NOT_INITIALIZED       = 2,  /**< Object used before create/init. */
    SAPI_STATUS_ALREADY_INITIALIZED   = 3,  /**< Object already created/initialized. */
    SAPI_STATUS_TIMEOUT               = 4,  /**< Blocking call exceeded its deadline. */
    SAPI_STATUS_RESOURCE_EXHAUSTED    = 5,  /**< Static pool/storage/slots full. */
    SAPI_STATUS_NOT_SUPPORTED         = 6,  /**< Valid request, backend cannot perform it. */
    SAPI_STATUS_NOT_IMPLEMENTED       = 7,  /**< Backend is a stub (skeleton state). */
    SAPI_STATUS_HARDWARE_FAULT        = 8,  /**< Backend reported a hardware-level fault. */
    SAPI_STATUS_DATA_CORRUPTION       = 9,  /**< Integrity check (e.g. NVM CRC) failed. */
    SAPI_STATUS_INTERNAL_ERROR        = 10, /**< Defensive catch-all: should never happen. */
    SAPI_STATUS_VALUE_OUT_OF_RANGE    = 11, /**< Checked cast: value does not fit the destination type. */
    SAPI_STATUS_INVALID_STATE         = 12  /**< Operation not permitted in the application's current
                                              *   lifecycle phase (e.g. a setup-only constructor called
                                              *   after sapi_appmanager_run() has locked setup - see
                                              *   sapi_lifecycle.h). */
} sapi_status_t;

/**
 * @brief Returns a short, static, human-readable string for a status code.
 *        Intended for diagnostics/logging only; never on a safety-decision path.
 *
 * REQ-OAL-COMMON-002
 */
const char *sapi_status_to_string(sapi_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_STATUS_H */

/** @} */ /* STATUS */

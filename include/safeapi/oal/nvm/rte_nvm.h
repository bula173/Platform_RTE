/**
 * @file rte_nvm.h
 * @brief OS Abstraction Layer - Non-Volatile Memory service.
 *
 * Persistent storage for safety-related data (e.g. train/track database
 * state) with mandatory integrity checking on read. See ADR-001.
 *
 * REQ-OAL-NVM-001: every read shall be integrity-checked (e.g. CRC or
 *                  redundant-copy voting) before data is returned to the caller.
 * REQ-OAL-NVM-002: no dynamic allocation; caller supplies storage/buffers.
 *
 * @defgroup NVM Non-Volatile Memory
 * @brief Persistent storage with mandatory integrity checking on read (ADR-001)
 * @{
 */
#ifndef SAFEAPI_OS_NVM_H
#define SAFEAPI_OS_NVM_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caller-owned, fixed-size storage backing one rte_nvm_handle_t. */
SAFEAPI_DECLARE_STORAGE(rte_nvm_storage_t, 64U);

/** @brief Opaque handle to an open NVM region, returned by rte_nvm_open(). */
typedef struct rte_nvm_impl_s *rte_nvm_handle_t;

/** @brief Configuration for rte_nvm_open(). */
typedef struct rte_nvm_config_s
{
    const char *region_name;   /**< Backend-defined logical region identifier. */
    size_t      region_size;   /**< Required capacity in bytes. */
} rte_nvm_config_t;

/**
 * @brief Opens (creating if necessary) a named NVM region.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Region configuration. Must not be NULL; config->region_name
 *                    must not be NULL and config->region_size must be > 0.
 * @param out_handle  Receives the opened region's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (rte_osadapter_nvm_register()); RTE_STATUS_NOT_SUPPORTED if the
 *         registered OSAdapter does not implement open.
 * REQ-OAL-NVM-010
 */
rte_status_t rte_nvm_open(rte_nvm_storage_t *storage,
                             const rte_nvm_config_t *config,
                             rte_nvm_handle_t *out_handle);

/**
 * @brief Reads and integrity-checks data from an NVM region.
 * @param handle       Open region handle. Must not be NULL.
 * @param offset       Byte offset within the region to read from.
 * @param out_buffer   Destination buffer. Must not be NULL.
 * @param buffer_size  Number of bytes to read into out_buffer; must be > 0.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_DATA_CORRUPTION if the integrity check fails (the
 *         output buffer content is then undefined and shall not be used);
 *         RTE_STATUS_NOT_INITIALIZED/RTE_STATUS_NOT_SUPPORTED as in rte_nvm_open().
 * REQ-OAL-NVM-011
 */
rte_status_t rte_nvm_read(rte_nvm_handle_t handle,
                             size_t offset,
                             void *out_buffer,
                             size_t buffer_size);

/**
 * @brief Writes data and its integrity metadata to an NVM region.
 * @param handle       Open region handle. Must not be NULL.
 * @param offset       Byte offset within the region to write to.
 * @param buffer       Source data. Must not be NULL.
 * @param buffer_size  Number of bytes to write from buffer; must be > 0.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_NOT_INITIALIZED/RTE_STATUS_NOT_SUPPORTED as in rte_nvm_open().
 * REQ-OAL-NVM-012
 */
rte_status_t rte_nvm_write(rte_nvm_handle_t handle,
                              size_t offset,
                              const void *buffer,
                              size_t buffer_size);

/**
 * @brief Forces any buffered writes to durable storage.
 * @param handle  Open region handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_nvm_open().
 * REQ-OAL-NVM-013
 */
rte_status_t rte_nvm_sync(rte_nvm_handle_t handle);

/**
 * @brief Closes an NVM region handle.
 * @param handle  Handle to close. Must not be NULL. Invalid to use after this call.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_nvm_open().
 * REQ-OAL-NVM-014
 */
rte_status_t rte_nvm_close(rte_nvm_handle_t handle);

/*
 * The OSAdapter vtable (rte_osadapter_nvm_t) and rte_osadapter_nvm_register()
 * live in safeapi_osadapter/nvm/rte_osadapter_nvm.h, not here (ADR-021).
 * This header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_NVM_H */

/** @} */ /* NVM */

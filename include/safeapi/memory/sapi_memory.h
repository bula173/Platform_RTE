/**
 * @file sapi_memory.h
 * @brief OS Abstraction Layer - Static memory reservation service.
 *
 * Reserves fixed-size memory pools/partitions at initialization time.
 * There is deliberately no free-form malloc/free in this API: safety-related
 * code shall only draw fixed-size blocks from pools sized and reserved up
 * front. See ADR-001, section 3.2.
 *
 * REQ-OAL-MEM-001: all pools are reserved during system initialization;
 *                  reservation after the init phase is backend-defined and
 *                  may be refused (SAPI_STATUS_NOT_SUPPORTED).
 */
#ifndef SAFEAPI_OS_MEMORY_H
#define SAFEAPI_OS_MEMORY_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

SAFEAPI_DECLARE_STORAGE(sapi_mem_pool_storage_t, 64U);

typedef struct sapi_mem_pool_impl_s *sapi_mem_pool_handle_t;

typedef struct sapi_mem_pool_config_s
{
    size_t block_size;    /**< Fixed size in bytes of every block in the pool. */
    size_t block_count;   /**< Number of blocks reserved in the pool. */
} sapi_mem_pool_config_t;

/**
 * @brief Reserves a fixed-size-block memory pool.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Pool configuration. Must not be NULL; config->block_size
 *                    and config->block_count must both be > 0.
 * @param out_handle  Receives the created pool's handle. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM for a bad argument;
 *         SAPI_STATUS_NOT_INITIALIZED if no backend is registered
 *         (sapi_mem_pool_register_backend()); SAPI_STATUS_NOT_SUPPORTED if
 *         the registered backend does not implement create.
 * REQ-OAL-MEM-010
 */
sapi_status_t sapi_mem_pool_create(sapi_mem_pool_storage_t *storage,
                                    const sapi_mem_pool_config_t *config,
                                    sapi_mem_pool_handle_t *out_handle);

/**
 * @brief Acquires one fixed-size block from the pool.
 * @param handle     Pool handle. Must not be NULL.
 * @param out_block  Receives a pointer to the acquired block. Must not be
 *                   NULL; set to NULL on failure.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_RESOURCE_EXHAUSTED
 *         if no blocks remain; SAPI_STATUS_NOT_INITIALIZED/SAPI_STATUS_NOT_SUPPORTED
 *         as in sapi_mem_pool_create().
 * REQ-OAL-MEM-011
 */
sapi_status_t sapi_mem_pool_acquire(sapi_mem_pool_handle_t handle, void **out_block);

/**
 * @brief Returns a previously acquired block to its pool.
 * @param handle  Pool handle. Must not be NULL.
 * @param block   Block previously returned by sapi_mem_pool_acquire(). Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_mem_pool_create().
 * REQ-OAL-MEM-012
 */
sapi_status_t sapi_mem_pool_release(sapi_mem_pool_handle_t handle, void *block);

/**
 * @brief Reports current free/used block counts for diagnostics.
 * @param handle           Pool handle. Must not be NULL.
 * @param out_free_blocks  Receives the number of free blocks. Must not be NULL.
 * @param out_used_blocks  Receives the number of used blocks. Must not be NULL.
 * @return SAPI_STATUS_OK; SAPI_STATUS_INVALID_PARAM; SAPI_STATUS_NOT_INITIALIZED/
 *         SAPI_STATUS_NOT_SUPPORTED as in sapi_mem_pool_create().
 * REQ-OAL-MEM-013
 */
sapi_status_t sapi_mem_pool_stats(sapi_mem_pool_handle_t handle,
                                   size_t *out_free_blocks,
                                   size_t *out_used_blocks);

/**
 * @brief Backend vtable: an integrator's implementation of the memory pool
 *        service (ADR-005). Any slot may be NULL if unsupported by the
 *        backend (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_mem_pool_backend_s
{
    sapi_status_t (*create)(sapi_mem_pool_storage_t *storage,
                             const sapi_mem_pool_config_t *config,
                             sapi_mem_pool_handle_t *out_handle);
    sapi_status_t (*acquire)(sapi_mem_pool_handle_t handle, void **out_block);
    sapi_status_t (*release)(sapi_mem_pool_handle_t handle, void *block);
    sapi_status_t (*stats)(sapi_mem_pool_handle_t handle,
                            size_t *out_free_blocks, size_t *out_used_blocks);
} sapi_mem_pool_backend_t;

/**
 * @brief Registers the backend implementation used by every
 *        sapi_mem_pool_* call (ADR-005 section 2.1). Call once at startup.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-MEM-014
 */
sapi_status_t sapi_mem_pool_register_backend(const sapi_mem_pool_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_MEMORY_H */

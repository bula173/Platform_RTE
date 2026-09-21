/**
 * @file rte_memory.h
 * @brief OS Abstraction Layer - Static memory reservation service.
 *
 * Reserves fixed-size memory pools/partitions at initialization time.
 * There is deliberately no free-form malloc/free in this API: safety-related
 * code shall only draw fixed-size blocks from pools sized and reserved up
 * front. See ADR-001, section 3.2.
 *
 * REQ-OAL-MEM-001: all pools are reserved during system initialization;
 *                  reservation after the init phase is osadapter-defined and
 *                  may be refused (RTE_STATUS_NOT_SUPPORTED).
 *
 * @defgroup MEMORY Static Memory Reservation
 * @brief Fixed-size memory pools reserved at initialization (ADR-001)
 * @{
 */
#ifndef RTE_OS_MEMORY_H
#define RTE_OS_MEMORY_H

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caller-owned, fixed-size storage backing one rte_mem_pool_handle_t. */
RTE_DECLARE_STORAGE(rte_mem_pool_storage_t, 64U);

/** @brief Opaque handle to a reserved pool, returned by rte_mem_pool_create(). */
typedef struct rte_mem_pool_impl_s *rte_mem_pool_handle_t;

/** @brief Configuration for rte_mem_pool_create(). */
typedef struct rte_mem_pool_config_s
{
    size_t block_size;    /**< Fixed size in bytes of every block in the pool. */
    size_t block_count;   /**< Number of blocks reserved in the pool. */
} rte_mem_pool_config_t;

/**
 * @brief Reserves a fixed-size-block memory pool.
 * @param storage     Caller-owned storage for the handle's state. Must not be NULL.
 * @param config      Pool configuration. Must not be NULL; config->block_size
 *                    and config->block_count must both be > 0.
 * @param out_handle  Receives the created pool's handle. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (rte_osadapter_memory_register()); RTE_STATUS_NOT_SUPPORTED if
 *         the registered OSAdapter does not implement create.
 * REQ-OAL-MEM-010
 */
rte_status_t rte_mem_pool_create(rte_mem_pool_storage_t *storage,
                                    const rte_mem_pool_config_t *config,
                                    rte_mem_pool_handle_t *out_handle);

/**
 * @brief Acquires one fixed-size block from the pool.
 * @param handle     Pool handle. Must not be NULL.
 * @param out_block  Receives a pointer to the acquired block. Must not be
 *                   NULL; set to NULL on failure.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_RESOURCE_EXHAUSTED
 *         if no blocks remain; RTE_STATUS_NOT_INITIALIZED/RTE_STATUS_NOT_SUPPORTED
 *         as in rte_mem_pool_create().
 * REQ-OAL-MEM-011
 */
rte_status_t rte_mem_pool_acquire(rte_mem_pool_handle_t handle, void **out_block);

/**
 * @brief Returns a previously acquired block to its pool.
 * @param handle  Pool handle. Must not be NULL.
 * @param block   Block previously returned by rte_mem_pool_acquire(). Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_mem_pool_create().
 * REQ-OAL-MEM-012
 */
rte_status_t rte_mem_pool_release(rte_mem_pool_handle_t handle, void *block);

/**
 * @brief Reports current free/used block counts for diagnostics.
 * @param handle           Pool handle. Must not be NULL.
 * @param out_free_blocks  Receives the number of free blocks. Must not be NULL.
 * @param out_used_blocks  Receives the number of used blocks. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_NOT_INITIALIZED/
 *         RTE_STATUS_NOT_SUPPORTED as in rte_mem_pool_create().
 * REQ-OAL-MEM-013
 */
rte_status_t rte_mem_pool_stats(rte_mem_pool_handle_t handle,
                                   size_t *out_free_blocks,
                                   size_t *out_used_blocks);

/*
 * The OSAdapter vtable (rte_osadapter_memory_t) and
 * rte_osadapter_memory_register() live in
 * rte_osadapter/memory/rte_osadapter_memory.h, not here (ADR-021). This
 * header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* RTE_OS_MEMORY_H */

/** @} */ /* MEMORY */

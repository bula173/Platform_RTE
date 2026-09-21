/**
 * @file rte_osadapter_memory.h
 * @brief OSAdapter interface for static memory pool management.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef RTE_OSADAPTER_MEMORY_H
#define RTE_OSADAPTER_MEMORY_H

#include "rte/oal/memory/rte_memory.h"
#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter memory pool operations vtable.
 */
typedef struct rte_osadapter_memory_s
{
    rte_status_t (*create)(rte_mem_pool_storage_t *storage,
                             const rte_mem_pool_config_t *config,
                             rte_mem_pool_handle_t *out_handle);
    rte_status_t (*acquire)(rte_mem_pool_handle_t handle, void **out_block);
    rte_status_t (*release)(rte_mem_pool_handle_t handle, void *block);
    rte_status_t (*stats)(rte_mem_pool_handle_t handle,
                            size_t *out_free_blocks, size_t *out_used_blocks);
} rte_osadapter_memory_t;

/**
 * @brief Registers the OSAdapter memory implementation.
 * @param adapter Pointer to memory operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_memory_register(const rte_osadapter_memory_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* RTE_OSADAPTER_MEMORY_H */

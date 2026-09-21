/**
 * @file rte_memory.c
 * @ingroup MEMORY
 * @brief Memory pool service: validates parameters, then dispatches to the
 *        backend registered via rte_mem_pool_register_backend() (ADR-005).
 */
#include "safeapi/oal/memory/rte_memory.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_backend/memory/rte_memory_backend.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const rte_mem_pool_backend_t *s_backend = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_mem_pool_register_backend(const rte_mem_pool_backend_t *backend)
{
    rte_status_t lifecycle_status;

    if (backend == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering a backend is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_backend = backend;
    return RTE_STATUS_OK;
}

rte_status_t rte_mem_pool_create(rte_mem_pool_storage_t *storage,
                                    const rte_mem_pool_config_t *config,
                                    rte_mem_pool_handle_t *out_handle)
{
    rte_status_t lifecycle_status;

    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->block_size == 0U) || (config->block_count == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    /* REQ-LIFECYCLE-001 (ADR-026): a memory pool is a setup-only resource -
     * refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, config, out_handle);
}

rte_status_t rte_mem_pool_acquire(rte_mem_pool_handle_t handle, void **out_block)
{
    if ((handle == NULL) || (out_block == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_block = NULL;
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->acquire == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->acquire(handle, out_block);
}

rte_status_t rte_mem_pool_release(rte_mem_pool_handle_t handle, void *block)
{
    if ((handle == NULL) || (block == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->release == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->release(handle, block);
}

rte_status_t rte_mem_pool_stats(rte_mem_pool_handle_t handle,
                                   size_t *out_free_blocks,
                                   size_t *out_used_blocks)
{
    if ((handle == NULL) || (out_free_blocks == NULL) || (out_used_blocks == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_free_blocks = 0U;
    *out_used_blocks = 0U;
    if (s_backend == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->stats == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_backend->stats(handle, out_free_blocks, out_used_blocks);
}

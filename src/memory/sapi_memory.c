/**
 * @file sapi_memory.c
 * @ingroup MEMORY
 * @brief Memory pool service: validates parameters, then dispatches to the
 *        backend registered via sapi_mem_pool_register_backend() (ADR-005).
 */
#include "safeapi/memory/sapi_memory.h"
#include "safeapi_backend/memory/sapi_memory_backend.h"

/** @brief Currently registered backend, or NULL if none (ADR-005). */
static const sapi_mem_pool_backend_t *s_backend = NULL;

sapi_status_t sapi_mem_pool_register_backend(const sapi_mem_pool_backend_t *backend)
{
    if (backend == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    s_backend = backend;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_mem_pool_create(sapi_mem_pool_storage_t *storage,
                                    const sapi_mem_pool_config_t *config,
                                    sapi_mem_pool_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if ((config->block_size == 0U) || (config->block_count == 0U))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->create == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->create(storage, config, out_handle);
}

sapi_status_t sapi_mem_pool_acquire(sapi_mem_pool_handle_t handle, void **out_block)
{
    if ((handle == NULL) || (out_block == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_block = NULL;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->acquire == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->acquire(handle, out_block);
}

sapi_status_t sapi_mem_pool_release(sapi_mem_pool_handle_t handle, void *block)
{
    if ((handle == NULL) || (block == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->release == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->release(handle, block);
}

sapi_status_t sapi_mem_pool_stats(sapi_mem_pool_handle_t handle,
                                   size_t *out_free_blocks,
                                   size_t *out_used_blocks)
{
    if ((handle == NULL) || (out_free_blocks == NULL) || (out_used_blocks == NULL))
    {
        return SAPI_STATUS_INVALID_PARAM;
    }
    *out_free_blocks = 0U;
    *out_used_blocks = 0U;
    if (s_backend == NULL)
    {
        return SAPI_STATUS_NOT_INITIALIZED;
    }
    if (s_backend->stats == NULL)
    {
        return SAPI_STATUS_NOT_SUPPORTED;
    }
    return s_backend->stats(handle, out_free_blocks, out_used_blocks);
}

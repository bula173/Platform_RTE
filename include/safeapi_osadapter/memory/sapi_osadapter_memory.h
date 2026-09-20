/**
 * @file sapi_osadapter_memory.h
 * @brief OSAdapter interface for static memory pool management.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_MEMORY_H
#define SAFEAPI_OSADAPTER_MEMORY_H

#include "safeapi/oal/memory/sapi_memory.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter memory pool operations vtable.
 */
typedef struct sapi_osadapter_memory_s
{
    sapi_status_t (*create)(sapi_mem_pool_storage_t *storage,
                             const sapi_mem_pool_config_t *config,
                             sapi_mem_pool_handle_t *out_handle);
    sapi_status_t (*acquire)(sapi_mem_pool_handle_t handle, void **out_block);
    sapi_status_t (*release)(sapi_mem_pool_handle_t handle, void *block);
    sapi_status_t (*stats)(sapi_mem_pool_handle_t handle,
                            size_t *out_free_blocks, size_t *out_used_blocks);
} sapi_osadapter_memory_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_memory_t sapi_mem_pool_backend_t;

/**
 * @brief Registers the OSAdapter memory implementation.
 * @param adapter Pointer to memory operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_memory_register(const sapi_osadapter_memory_t *adapter);

sapi_status_t sapi_mem_pool_register_backend(const sapi_mem_pool_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_MEMORY_H */

/**
 * @file sapi_memory_backend.h
 * @brief OS-backend adaptation surface for the static memory pool service
 *        (ADR-005, ADR-021).
 *
 * For platform integrators implementing a sapi_mem_pool_backend_t and
 * calling sapi_mem_pool_register_backend() - NOT part of the consumer
 * API (safeapi/memory/sapi_memory.h). A real application should never
 * include this file; only the startup code that wires a concrete backend
 * does.
 *
 * @defgroup MEMORY_BACKEND Static Memory Reservation - Backend Adaptation
 * @brief Backend vtable and registration for the memory pool service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_MEMORY_BACKEND_H
#define SAFEAPI_OS_MEMORY_BACKEND_H

#include "safeapi/oal/memory/sapi_memory.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the memory pool
 *        service (ADR-005). Any slot may be NULL if unsupported by the
 *        backend (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_mem_pool_backend_s
{
    /** @brief Backend implementation of sapi_mem_pool_create(). May be NULL. */
    sapi_status_t (*create)(sapi_mem_pool_storage_t *storage,
                             const sapi_mem_pool_config_t *config,
                             sapi_mem_pool_handle_t *out_handle);
    /** @brief Backend implementation of sapi_mem_pool_acquire(). May be NULL. */
    sapi_status_t (*acquire)(sapi_mem_pool_handle_t handle, void **out_block);
    /** @brief Backend implementation of sapi_mem_pool_release(). May be NULL. */
    sapi_status_t (*release)(sapi_mem_pool_handle_t handle, void *block);
    /** @brief Backend implementation of sapi_mem_pool_stats(). May be NULL. */
    sapi_status_t (*stats)(sapi_mem_pool_handle_t handle,
                            size_t *out_free_blocks, size_t *out_used_blocks);
} sapi_mem_pool_backend_t;

/**
 * @brief Registers the backend implementation used by every
 *        sapi_mem_pool_* call (ADR-005 section 2.1). Call once at startup.
 * @param backend Vtable of backend function pointers. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-MEM-014
 */
sapi_status_t sapi_mem_pool_register_backend(const sapi_mem_pool_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_MEMORY_BACKEND_H */

/** @} */ /* MEMORY_BACKEND */

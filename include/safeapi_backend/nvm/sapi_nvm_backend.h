/**
 * @file sapi_nvm_backend.h
 * @brief OS-backend adaptation surface for the NVM service (ADR-005,
 *        ADR-021).
 *
 * For platform integrators implementing a sapi_nvm_backend_t and calling
 * sapi_nvm_register_backend() - NOT part of the consumer API
 * (safeapi/nvm/sapi_nvm.h). A real application should never include this
 * file; only the startup code that wires a concrete backend does.
 *
 * @defgroup NVM_BACKEND NVM Service - Backend Adaptation
 * @brief Backend vtable and registration for the NVM service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_NVM_BACKEND_H
#define SAFEAPI_OS_NVM_BACKEND_H

#include "safeapi/nvm/sapi_nvm.h"
#include "safeapi/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the NVM service
 *        for a specific storage medium (ADR-005). Any slot may be NULL if
 *        unsupported by the backend (-> SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_nvm_backend_s
{
    /** @brief Backend implementation of sapi_nvm_open(). May be NULL. */
    sapi_status_t (*open)(sapi_nvm_storage_t *storage,
                           const sapi_nvm_config_t *config,
                           sapi_nvm_handle_t *out_handle);
    /** @brief Backend implementation of sapi_nvm_read(). May be NULL. */
    sapi_status_t (*read)(sapi_nvm_handle_t handle, size_t offset,
                           void *out_buffer, size_t buffer_size);
    /** @brief Backend implementation of sapi_nvm_write(). May be NULL. */
    sapi_status_t (*write)(sapi_nvm_handle_t handle, size_t offset,
                            const void *buffer, size_t buffer_size);
    /** @brief Backend implementation of sapi_nvm_sync(). May be NULL. */
    sapi_status_t (*sync)(sapi_nvm_handle_t handle);
    /** @brief Backend implementation of sapi_nvm_close(). May be NULL. */
    sapi_status_t (*close)(sapi_nvm_handle_t handle);
} sapi_nvm_backend_t;

/**
 * @brief Registers the backend implementation used by every sapi_nvm_*
 *        call (ADR-005 section 2.1). Call once at startup.
 * @param backend Vtable of backend function pointers. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-NVM-015
 */
sapi_status_t sapi_nvm_register_backend(const sapi_nvm_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_NVM_BACKEND_H */

/** @} */ /* NVM_BACKEND */

/**
 * @file sapi_osadapter_nvm.h
 * @brief OSAdapter interface for non-volatile memory (NVM).
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_NVM_H
#define SAFEAPI_OSADAPTER_NVM_H

#include "safeapi/oal/nvm/sapi_nvm.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter NVM operations vtable.
 */
typedef struct sapi_osadapter_nvm_s
{
    sapi_status_t (*open)(sapi_nvm_storage_t *storage,
                           const sapi_nvm_config_t *config,
                           sapi_nvm_handle_t *out_handle);
    sapi_status_t (*read)(sapi_nvm_handle_t handle, size_t offset,
                           void *out_buffer, size_t buffer_size);
    sapi_status_t (*write)(sapi_nvm_handle_t handle, size_t offset,
                            const void *buffer, size_t buffer_size);
    sapi_status_t (*sync)(sapi_nvm_handle_t handle);
    sapi_status_t (*close)(sapi_nvm_handle_t handle);
} sapi_osadapter_nvm_t;

/* Backward compatibility typedef */
typedef sapi_osadapter_nvm_t sapi_nvm_backend_t;

/**
 * @brief Registers the OSAdapter NVM implementation.
 * @param adapter Pointer to NVM operations vtable.
 * @return SAPI_STATUS_OK on success, SAPI_STATUS_INVALID_PARAM if adapter is NULL.
 */
sapi_status_t sapi_osadapter_nvm_register(const sapi_osadapter_nvm_t *adapter);

sapi_status_t sapi_nvm_register_backend(const sapi_nvm_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_NVM_H */

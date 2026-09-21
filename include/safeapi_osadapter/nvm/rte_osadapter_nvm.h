/**
 * @file rte_osadapter_nvm.h
 * @brief OSAdapter interface for non-volatile memory (NVM).
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */
#ifndef SAFEAPI_OSADAPTER_NVM_H
#define SAFEAPI_OSADAPTER_NVM_H

#include "safeapi/oal/nvm/rte_nvm.h"
#include "safeapi/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OSAdapter NVM operations vtable.
 */
typedef struct rte_osadapter_nvm_s
{
    rte_status_t (*open)(rte_nvm_storage_t *storage,
                           const rte_nvm_config_t *config,
                           rte_nvm_handle_t *out_handle);
    rte_status_t (*read)(rte_nvm_handle_t handle, size_t offset,
                           void *out_buffer, size_t buffer_size);
    rte_status_t (*write)(rte_nvm_handle_t handle, size_t offset,
                            const void *buffer, size_t buffer_size);
    rte_status_t (*sync)(rte_nvm_handle_t handle);
    rte_status_t (*close)(rte_nvm_handle_t handle);
} rte_osadapter_nvm_t;

/**
 * @brief Registers the OSAdapter NVM implementation.
 * @param adapter Pointer to NVM operations vtable.
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if adapter is NULL.
 */
rte_status_t rte_osadapter_nvm_register(const rte_osadapter_nvm_t *adapter);


#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OSADAPTER_NVM_H */

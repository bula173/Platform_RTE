/**
 * @file rte_nvm.c
 * @ingroup NVM
 * @brief NVM service: validates parameters, then dispatches to the OSAdapter
 *        registered via rte_osadapter_nvm_register() (ADR-005).
 */
#include "safeapi/oal/nvm/rte_nvm.h"
#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi_osadapter/nvm/rte_osadapter_nvm.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_nvm_t *s_osadapter = NULL;

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
rte_status_t rte_osadapter_nvm_register(const rte_osadapter_nvm_t *osadapter)
{
    rte_status_t lifecycle_status;

    if (osadapter == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering an OSAdapter is a setup-only
     * action - refuse once the application's setup phase has been locked. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_osadapter = osadapter;
    return RTE_STATUS_OK;
}

rte_status_t rte_nvm_open(rte_nvm_storage_t *storage,
                             const rte_nvm_config_t *config,
                             rte_nvm_handle_t *out_handle)
{
    if ((storage == NULL) || (config == NULL) || (out_handle == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((config->region_name == NULL) || (config->region_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    *out_handle = NULL;
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->open == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->open(storage, config, out_handle);
}

rte_status_t rte_nvm_read(rte_nvm_handle_t handle,
                             size_t offset,
                             void *out_buffer,
                             size_t buffer_size)
{
    if ((handle == NULL) || (out_buffer == NULL) || (buffer_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->read == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->read(handle, offset, out_buffer, buffer_size);
}

rte_status_t rte_nvm_write(rte_nvm_handle_t handle,
                              size_t offset,
                              const void *buffer,
                              size_t buffer_size)
{
    if ((handle == NULL) || (buffer == NULL) || (buffer_size == 0U))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->write == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->write(handle, offset, buffer, buffer_size);
}

rte_status_t rte_nvm_sync(rte_nvm_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->sync == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->sync(handle);
}

rte_status_t rte_nvm_close(rte_nvm_handle_t handle)
{
    if (handle == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->close == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->close(handle);
}

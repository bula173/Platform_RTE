/**
 * @file rte_random.c
 * @ingroup RANDOM
 * @brief Secure random bytes: validates parameters, then dispatches to the OSAdapter registered via
 *        rte_osadapter_random_register() (ADR-005).
 */
#include "rte/oal/random/rte_random.h"
#include "rte/utils/lifecycle/rte_lifecycle.h"
#include "rte_osadapter/random/rte_osadapter_random.h"

/** Local variables declarations */
/** @brief Currently registered OSAdapter, or NULL if none (ADR-005). */
static const rte_osadapter_random_t *s_osadapter = NULL;

/** Global functions */
rte_status_t rte_osadapter_random_register(const rte_osadapter_random_t *osadapter)
{
    rte_status_t lifecycle_status;

    if (osadapter == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    /* REQ-LIFECYCLE-001 (ADR-026): registering an OSAdapter is a setup-only action. */
    lifecycle_status = rte_lifecycle_check_setup_allowed();
    if (lifecycle_status != RTE_STATUS_OK)
    {
        return lifecycle_status;
    }
    s_osadapter = osadapter;
    return RTE_STATUS_OK;
}

rte_status_t rte_random_fill(uint8_t *out, size_t len)
{
    if ((out == NULL) || (len == 0U) || (len > (size_t)RTE_RANDOM_MAX_REQUEST))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (s_osadapter == NULL)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    if (s_osadapter->fill == NULL)
    {
        return RTE_STATUS_NOT_SUPPORTED;
    }
    return s_osadapter->fill(out, len);
}

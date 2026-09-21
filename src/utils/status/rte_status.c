/**
 * @file rte_status.c
 * @brief Implementation of rte_status_to_string().
 * @ingroup STATUS
 */
#include "rte/utils/status/rte_status.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
/**
 * @brief Convert a status code to a short, human-readable string.
 * @param status Status code to describe.
 * @return Static, non-NULL string literal naming @p status (e.g.
 *         "RTE_STATUS_OK"); an unrecognized value yields "UNKNOWN_STATUS".
 * @safety Never returns NULL; safe to call with any int-range value.
 */
const char *rte_status_to_string(rte_status_t status)
{
    switch (status)
    {
        case RTE_STATUS_OK:                  return "OK";
        case RTE_STATUS_INVALID_PARAM:       return "INVALID_PARAM";
        case RTE_STATUS_NOT_INITIALIZED:     return "NOT_INITIALIZED";
        case RTE_STATUS_ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case RTE_STATUS_TIMEOUT:             return "TIMEOUT";
        case RTE_STATUS_RESOURCE_EXHAUSTED:  return "RESOURCE_EXHAUSTED";
        case RTE_STATUS_NOT_SUPPORTED:       return "NOT_SUPPORTED";
        case RTE_STATUS_NOT_IMPLEMENTED:     return "NOT_IMPLEMENTED";
        case RTE_STATUS_HARDWARE_FAULT:      return "HARDWARE_FAULT";
        case RTE_STATUS_DATA_CORRUPTION:     return "DATA_CORRUPTION";
        case RTE_STATUS_INTERNAL_ERROR:      return "INTERNAL_ERROR";
        case RTE_STATUS_VALUE_OUT_OF_RANGE:  return "VALUE_OUT_OF_RANGE";
        case RTE_STATUS_INVALID_STATE:       return "INVALID_STATE";
        default:                              return "UNKNOWN_STATUS";
    }
}

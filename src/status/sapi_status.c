/**
 * @file sapi_status.c
 * @brief Implementation of sapi_status_to_string().
 * @ingroup STATUS
 */
#include "safeapi/status/sapi_status.h"

/**
 * @brief Convert a status code to a short, human-readable string.
 * @param status Status code to describe.
 * @return Static, non-NULL string literal naming @p status (e.g.
 *         "SAPI_STATUS_OK"); an unrecognized value yields "UNKNOWN_STATUS".
 * @safety Never returns NULL; safe to call with any int-range value.
 */
const char *sapi_status_to_string(sapi_status_t status)
{
    switch (status)
    {
        case SAPI_STATUS_OK:                  return "OK";
        case SAPI_STATUS_INVALID_PARAM:       return "INVALID_PARAM";
        case SAPI_STATUS_NOT_INITIALIZED:     return "NOT_INITIALIZED";
        case SAPI_STATUS_ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case SAPI_STATUS_TIMEOUT:             return "TIMEOUT";
        case SAPI_STATUS_RESOURCE_EXHAUSTED:  return "RESOURCE_EXHAUSTED";
        case SAPI_STATUS_NOT_SUPPORTED:       return "NOT_SUPPORTED";
        case SAPI_STATUS_NOT_IMPLEMENTED:     return "NOT_IMPLEMENTED";
        case SAPI_STATUS_HARDWARE_FAULT:      return "HARDWARE_FAULT";
        case SAPI_STATUS_DATA_CORRUPTION:     return "DATA_CORRUPTION";
        case SAPI_STATUS_INTERNAL_ERROR:      return "INTERNAL_ERROR";
        case SAPI_STATUS_VALUE_OUT_OF_RANGE:  return "VALUE_OUT_OF_RANGE";
        case SAPI_STATUS_INVALID_STATE:       return "INVALID_STATE";
        default:                              return "UNKNOWN_STATUS";
    }
}

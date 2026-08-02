#include "safeapi/status/sapi_status.h"

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
        default:                              return "UNKNOWN_STATUS";
    }
}

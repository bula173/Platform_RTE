/**
 * @file sapi_dual_types.c
 * @ingroup DUAL
 * @brief Diagnostics-only string rendering for the sapi_dual module's
 *        shared enums (ADR-020).
 */
#include "safeapi/dual/sapi_dual_types.h"

/** Local makros */

/** Local types declarations */

/** Local variables declarations */

/** Global variables declarations */

/** Local function declarations */


/** Global functions */
const char *sapi_dual_state_to_string(sapi_dual_state_t state)
{
    const char *result;

    switch (state)
    {
        case SAPI_DUAL_STATE_IDLE:
            result = "IDLE";
            break;
        case SAPI_DUAL_STATE_UNKNOWN:
            result = "UNKNOWN";
            break;
        case SAPI_DUAL_STATE_ONLINE:
            result = "ONLINE";
            break;
        case SAPI_DUAL_STATE_HOTSTANDBY:
            result = "HOTSTANDBY";
            break;
        case SAPI_DUAL_STATE_COLDSTANDBY:
            result = "COLDSTANDBY";
            break;
        default:
            /* Defensive only - not reachable through the public enum. */
            result = "UNKNOWN_STATE";
            break;
    }
    return result;
}

const char *sapi_dual_channel_status_to_string(sapi_dual_channel_status_t status)
{
    const char *result;

    switch (status)
    {
        case SAPI_DUAL_CHANNEL_STATUS_DOWN:
            result = "DOWN";
            break;
        case SAPI_DUAL_CHANNEL_STATUS_DEGRADED:
            result = "DEGRADED";
            break;
        case SAPI_DUAL_CHANNEL_STATUS_FULL:
            result = "FULL";
            break;
        default:
            /* Defensive only - not reachable through the public enum. */
            result = "UNKNOWN_STATUS";
            break;
    }
    return result;
}

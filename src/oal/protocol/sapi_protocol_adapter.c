/**
 * @file sapi_protocol_adapter.c
 * @brief Registry and Dispatch for Protocol Adapters.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */

#include "safeapi/oal/protocol/sapi_protocol_adapter.h"

static const sapi_protocol_adapter_ops_t *s_udp_adapter = NULL;
static const sapi_protocol_adapter_ops_t *s_tcp_adapter = NULL;
static const sapi_protocol_adapter_ops_t *s_dds_adapter = NULL;

sapi_status_t sapi_protocol_adapter_register(sapi_protocol_type_t type,
                                             const sapi_protocol_adapter_ops_t *ops)
{
    if (ops == NULL)
    {
        return SAPI_STATUS_INVALID_PARAM;
    }

    switch (type)
    {
        case SAPI_PROTOCOL_TYPE_RAW_UDP:
            s_udp_adapter = ops;
            break;

        case SAPI_PROTOCOL_TYPE_RAW_TCP:
            s_tcp_adapter = ops;
            break;

        case SAPI_PROTOCOL_TYPE_DDS:
            s_dds_adapter = ops;
            break;

        default:
            return SAPI_STATUS_INVALID_PARAM;
    }

    return SAPI_STATUS_OK;
}

const sapi_protocol_adapter_ops_t *sapi_protocol_adapter_get(sapi_protocol_type_t type)
{
    switch (type)
    {
        case SAPI_PROTOCOL_TYPE_RAW_UDP:
            return (s_udp_adapter != NULL) ? s_udp_adapter : sapi_protocol_adapter_get_udp();

        case SAPI_PROTOCOL_TYPE_RAW_TCP:
            return (s_tcp_adapter != NULL) ? s_tcp_adapter : sapi_protocol_adapter_get_tcp();

        case SAPI_PROTOCOL_TYPE_DDS:
            return s_dds_adapter;

        default:
            return NULL;
    }
}

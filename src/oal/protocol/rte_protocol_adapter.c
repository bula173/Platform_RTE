/**
 * @file rte_protocol_adapter.c
 * @brief Registry and Dispatch for Protocol Adapters.
 *
 * Compliant with CENELEC EN 50128 SIL 4 and MISRA C:2012.
 */

#include "rte/oal/protocol/rte_protocol_adapter.h"

static const rte_protocol_adapter_ops_t *s_udp_adapter = NULL;
static const rte_protocol_adapter_ops_t *s_tcp_adapter = NULL;
static const rte_protocol_adapter_ops_t *s_dds_adapter = NULL;

rte_status_t rte_protocol_adapter_register(rte_protocol_type_t type,
                                             const rte_protocol_adapter_ops_t *ops)
{
    if (ops == NULL)
    {
        return RTE_STATUS_INVALID_PARAM;
    }

    switch (type)
    {
        case RTE_PROTOCOL_TYPE_RAW_UDP:
            s_udp_adapter = ops;
            break;

        case RTE_PROTOCOL_TYPE_RAW_TCP:
            s_tcp_adapter = ops;
            break;

        case RTE_PROTOCOL_TYPE_DDS:
            s_dds_adapter = ops;
            break;

        default:
            return RTE_STATUS_INVALID_PARAM;
    }

    return RTE_STATUS_OK;
}

const rte_protocol_adapter_ops_t *rte_protocol_adapter_get(rte_protocol_type_t type)
{
    switch (type)
    {
        case RTE_PROTOCOL_TYPE_RAW_UDP:
            return (s_udp_adapter != NULL) ? s_udp_adapter : rte_protocol_adapter_get_udp();

        case RTE_PROTOCOL_TYPE_RAW_TCP:
            return (s_tcp_adapter != NULL) ? s_tcp_adapter : rte_protocol_adapter_get_tcp();

        case RTE_PROTOCOL_TYPE_DDS:
            return s_dds_adapter;

        default:
            return NULL;
    }
}

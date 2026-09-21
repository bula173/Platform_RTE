/**
 * @file rte_channel_service_flow_osadapter.h
 * @brief rte_osadapter_channel_service_t implementation over rte_flow.
 *
 * Added per direct request as part of the RCA/OCORA compatibility initiative's
 * Phase 4 (see docs/rca/ at the workspace root and TODO.md's "RCA/OCORA
 * compatibility" section): a real, drop-in alternative to
 * safeAPIBackendPosix's rte_posix_osadapter_channel_service, so an integrator
 * (e.g. safeAPIRBC2oo2GP) can route its named channels (e.g. "ab-peer",
 * "ab-negotiate") through rte_flow (and, once a real DDS OSAdapter exists,
 * through actual DDS) with ZERO change to its own transport-agnostic
 * rte_channel_service_setup()/_read()/_send()/_close() call sites.
 *
 * Deliberately reuses the EXACT SAME resolver function shape
 * (rte_channel_service_flow_resolve_fn, filling a rte_netlink_config_t)
 * safeAPIBackendPosix's own resolver does - an integrator that already has a
 * working resolve_channel() for the POSIX OSAdapter can register the SAME
 * function here unchanged; this OSAdapter translates
 * rte_netlink_config_t::role into a rte_flow_oflag_t (CONNECT->PUBLISHER,
 * LISTEN->SUBSCRIBER, matching safeAPIFlowBackendDDS's own oflags->role
 * mapping in reverse) and opens exactly one Flow per channel name, used for
 * both send and receive - full duplex on one Flow, the same shape a single
 * netlink link already provides (verified against safeAPIFlowBackendDDS's
 * own stub: rte_flow_send()/_receive() both operate on the one underlying
 * rte_netlink_handle_t a Flow opens, regardless of which oflag it was
 * opened with).
 *
 * This module is deliberately osadapter-agnostic - it calls only rte_flow_*
 * API, never anything specific to safeAPIFlowBackendDDS or any other
 * concrete rte_osadapter_flow_t. Registering a ROUTE (mapping a channel name
 * to host:port) for today's stub OSAdapter is the INTEGRATOR's job, done
 * before rte_channel_service_setup() is called (e.g. inside the
 * integrator's own resolver, which already knows which flow OSAdapter it
 * registered) - a real DDS OSAdapter would need no such route at all (DDS
 * topics are self-describing via discovery), so this module was kept free
 * of that stub-specific concern on purpose.
 *
 * @defgroup channel_service_flow_osadapter Channel service over rte_flow
 * @{
 */
#ifndef RTE_CHANNEL_SERVICE_FLOW_BACKEND_H
#define RTE_CHANNEL_SERVICE_FLOW_BACKEND_H

#include "rte/redundancy/channel_service/rte_channel_service.h"
#include "rte/oal/netlink/rte_netlink.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Same shape as safeAPIBackendPosix's rte_posix_channel_resolve_fn -
 *  deliberately, so one resolver function can serve either OSAdapter. */
typedef rte_status_t (*rte_channel_service_flow_resolve_fn)(const char *channel_name,
                                                               rte_netlink_config_t *out_config,
                                                               void *context);

/**
 * @brief Registers the resolver this OSAdapter calls from setup() to learn a
 *        channel name's role/host/port/message_size.
 * @param resolver  Must not be NULL.
 * @param context   Passed verbatim to resolver on every call; may be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if resolver is NULL.
 */
rte_status_t rte_channel_service_flow_osadapter_register_resolver(rte_channel_service_flow_resolve_fn resolver,
                                                                    void *context);

/**
 * @brief Returns the rte_osadapter_channel_service_t vtable to pass to
 *        rte_osadapter_channel_service_register().
 */
const rte_osadapter_channel_service_t *rte_channel_service_flow_osadapter(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_CHANNEL_SERVICE_FLOW_BACKEND_H */
/** @} */

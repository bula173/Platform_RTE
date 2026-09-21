/**
 * @file rte_channel_service_flow_backend.h
 * @brief rte_channel_service_backend_t implementation over rte_flow.
 *
 * Added per direct request as part of the RCA/OCORA compatibility initiative's
 * Phase 4 (see docs/rca/ at the workspace root and TODO.md's "RCA/OCORA
 * compatibility" section): a real, drop-in alternative to
 * safeAPIBackendPosix's rte_posix_backend_channel_service, so an integrator
 * (e.g. safeAPIRBC2oo2GP) can route its named channels (e.g. "ab-peer",
 * "ab-negotiate") through rte_flow (and, once a real DDS backend exists,
 * through actual DDS) with ZERO change to its own transport-agnostic
 * rte_channel_service_setup()/_read()/_send()/_close() call sites.
 *
 * Deliberately reuses the EXACT SAME resolver function shape
 * (rte_channel_service_flow_resolve_fn, filling a rte_netlink_config_t)
 * safeAPIBackendPosix's own resolver does - an integrator that already has a
 * working resolve_channel() for the POSIX backend can register the SAME
 * function here unchanged; this backend translates
 * rte_netlink_config_t::role into a rte_flow_oflag_t (CONNECT->PUBLISHER,
 * LISTEN->SUBSCRIBER, matching safeAPIFlowBackendDDS's own oflags->role
 * mapping in reverse) and opens exactly one Flow per channel name, used for
 * both send and receive - full duplex on one Flow, the same shape a single
 * netlink link already provides (verified against safeAPIFlowBackendDDS's
 * own stub: rte_flow_send()/_receive() both operate on the one underlying
 * rte_netlink_handle_t a Flow opens, regardless of which oflag it was
 * opened with).
 *
 * This module is deliberately backend-agnostic - it calls only rte_flow_*
 * API, never anything specific to safeAPIFlowBackendDDS or any other
 * concrete rte_flow_backend_t. Registering a ROUTE (mapping a channel name
 * to host:port) for today's stub backend is the INTEGRATOR's job, done
 * before rte_channel_service_setup() is called (e.g. inside the
 * integrator's own resolver, which already knows which flow backend it
 * registered) - a real DDS backend would need no such route at all (DDS
 * topics are self-describing via discovery), so this module was kept free
 * of that stub-specific concern on purpose.
 *
 * @defgroup channel_service_flow_backend Channel service over rte_flow
 * @{
 */
#ifndef RTE_CHANNEL_SERVICE_FLOW_BACKEND_H
#define RTE_CHANNEL_SERVICE_FLOW_BACKEND_H

#include "safeapi/redundancy/channel_service/rte_channel_service.h"
#include "safeapi/oal/netlink/rte_netlink.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Same shape as safeAPIBackendPosix's rte_posix_channel_resolve_fn -
 *  deliberately, so one resolver function can serve either backend. */
typedef rte_status_t (*rte_channel_service_flow_resolve_fn)(const char *channel_name,
                                                               rte_netlink_config_t *out_config,
                                                               void *context);

/**
 * @brief Registers the resolver this backend calls from setup() to learn a
 *        channel name's role/host/port/message_size.
 * @param resolver  Must not be NULL.
 * @param context   Passed verbatim to resolver on every call; may be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if resolver is NULL.
 */
rte_status_t rte_channel_service_flow_backend_register_resolver(rte_channel_service_flow_resolve_fn resolver,
                                                                    void *context);

/**
 * @brief Returns the rte_channel_service_backend_t vtable to pass to
 *        rte_channel_service_register_backend().
 */
const rte_channel_service_backend_t *rte_channel_service_flow_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_CHANNEL_SERVICE_FLOW_BACKEND_H */
/** @} */

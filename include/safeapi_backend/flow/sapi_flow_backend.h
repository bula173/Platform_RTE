/**
 * @file sapi_flow_backend.h
 * @brief OS-backend adaptation surface for the OCORA PI-API-compatible
 *        Flow service (ADR-005, ADR-021 - same seam pattern as
 *        safeapi_backend/netlink/sapi_netlink_backend.h).
 *
 * For platform integrators implementing a sapi_flow_backend_t and
 * calling sapi_flow_register_backend() - NOT part of the consumer API
 * (safeapi/oal/flow/sapi_flow.h). A real application should never
 * include this file; only the startup code that wires a concrete
 * backend does.
 *
 * A POSIX/TCP backend emulates the name-addressed publish/subscribe
 * model over point-to-point links (e.g. a static name->host:port
 * mapping, one sapi_netlink link per registered peer); a DDS backend
 * implements it natively (DDS topics/DataWriters/DataReaders already
 * are exactly this) - see this project's ISSUES.md for the DDS backend
 * follow-up work this header is deliberately shaped to support without
 * further change.
 *
 * @defgroup FLOW_BACKEND OCORA PI-API-compatible Flow Service - Backend Adaptation
 * @brief Backend vtable and registration for the Flow service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_FLOW_BACKEND_H
#define SAFEAPI_OS_FLOW_BACKEND_H

#include "safeapi/oal/flow/sapi_flow.h"
#include "safeapi/utils/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the Flow
 *        service for a specific transport/target (ADR-005). Any slot
 *        may be NULL if unsupported by the backend (->
 *        SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_flow_backend_s
{
    /** @brief Backend implementation of sapi_flow_open(). May be NULL. */
    sapi_status_t (*open)(sapi_flow_storage_t *storage,
                           const sapi_flow_config_t *config,
                           sapi_flow_handle_t *out_handle);
    /** @brief Backend implementation of sapi_flow_send(). May be NULL. */
    sapi_status_t (*send)(sapi_flow_handle_t handle, const void *data, size_t data_size,
                           sapi_flow_channel_t channel, sapi_duration_ms_t timeout_ms);
    /** @brief Backend implementation of sapi_flow_receive(). May be NULL. */
    sapi_status_t (*receive)(sapi_flow_handle_t handle, void *out_data, size_t buffer_size,
                              sapi_flow_channel_t *out_channel, sapi_duration_ms_t timeout_ms);
    /** @brief Backend implementation of sapi_flow_close(). May be NULL. */
    sapi_status_t (*close)(sapi_flow_handle_t handle);
    /** @brief Backend implementation of sapi_flow_getattr(). May be NULL
     *  - the dispatch layer serves message_size/oflags from the handle's
     *  own recorded config either way; a backend only needs to supply
     *  this slot to report a real is_connected value. */
    sapi_status_t (*getattr)(sapi_flow_handle_t handle, sapi_flow_attr_t *out_attr);
    /** @brief Backend implementation of sapi_flow_setattr(). May be NULL. */
    sapi_status_t (*setattr)(sapi_flow_handle_t handle, const sapi_flow_attr_t *new_attr,
                              sapi_flow_attr_t *out_old_attr);
} sapi_flow_backend_t;

/**
 * @brief Registers the backend implementation used by every
 *        sapi_flow_* call (ADR-005 section 2.1). Call once at startup.
 * @param backend Vtable of backend function pointers. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-FLOW-016
 */
sapi_status_t sapi_flow_register_backend(const sapi_flow_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_FLOW_BACKEND_H */

/** @} */ /* FLOW_BACKEND */

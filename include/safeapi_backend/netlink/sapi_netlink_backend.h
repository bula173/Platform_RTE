/**
 * @file sapi_netlink_backend.h
 * @brief OS-backend adaptation surface for the Point-to-Point Network
 *        Link service (ADR-005, ADR-021).
 *
 * For platform integrators implementing a sapi_netlink_backend_t and
 * calling sapi_netlink_register_backend() - NOT part of the consumer API
 * (safeapi/netlink/sapi_netlink.h). A real application should never
 * include this file; only the startup code that wires a concrete
 * backend does (e.g. safeAPIExample's POSIX/TCP backend).
 *
 * @defgroup NETLINK_BACKEND Point-to-Point Network Link - Backend Adaptation
 * @brief Backend vtable and registration for the netlink service (ADR-005)
 * @{
 */
#ifndef SAFEAPI_OS_NETLINK_BACKEND_H
#define SAFEAPI_OS_NETLINK_BACKEND_H

#include "safeapi/netlink/sapi_netlink.h"
#include "safeapi/status/sapi_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backend vtable: an integrator's implementation of the network
 *        link service for a specific transport/target (ADR-005). Any
 *        slot may be NULL if unsupported by the backend (->
 *        SAPI_STATUS_NOT_SUPPORTED).
 */
typedef struct sapi_netlink_backend_s
{
    /** @brief Backend implementation of sapi_netlink_open(). May be NULL. */
    sapi_status_t (*open)(sapi_netlink_storage_t *storage,
                           const sapi_netlink_config_t *config,
                           sapi_netlink_handle_t *out_handle);
    /** @brief Backend implementation of sapi_netlink_send(). May be NULL. */
    sapi_status_t (*send)(sapi_netlink_handle_t handle, const void *message,
                           size_t message_size, sapi_duration_ms_t timeout_ms);
    /** @brief Backend implementation of sapi_netlink_receive(). May be NULL. */
    sapi_status_t (*receive)(sapi_netlink_handle_t handle, void *out_message,
                              size_t buffer_size, sapi_duration_ms_t timeout_ms);
    /** @brief Backend implementation of sapi_netlink_close(). May be NULL. */
    sapi_status_t (*close)(sapi_netlink_handle_t handle);
} sapi_netlink_backend_t;

/**
 * @brief Registers the backend implementation used by every
 *        sapi_netlink_* call (ADR-005 section 2.1). Call once at startup.
 * @param backend Vtable of backend function pointers. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if backend is NULL; SAPI_STATUS_OK otherwise.
 * REQ-OAL-NETLINK-014
 */
sapi_status_t sapi_netlink_register_backend(const sapi_netlink_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_NETLINK_BACKEND_H */

/** @} */ /* NETLINK_BACKEND */

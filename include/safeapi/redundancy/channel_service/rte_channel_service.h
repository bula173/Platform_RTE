#ifndef RTE_CHANNEL_SERVICE_H
#define RTE_CHANNEL_SERVICE_H

/**
 * @file rte_channel_service.h
 * @brief Application-facing named channel service.
 *
 * The application addresses a channel by its configured name and performs
 * setup, read, and send operations from its own execution thread. Transport
 * lifecycle, reconnect, buffering, and synchronization belong to the
 * registered backend.
 */

#include <stddef.h>
#include <stdint.h>

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_CHANNEL_SERVICE_NAME_SIZE (32U)

/** @brief Caller-owned storage for one named channel service instance. */
typedef struct
{
    uint8_t reserved[128U];
} rte_channel_service_storage_t;

/** @brief Opaque handle to a named channel service instance. */
typedef rte_channel_service_storage_t rte_channel_service_t;

/**
 * @brief Backend implementation for named channel operations.
 *
 * A backend may perform blocking or non-blocking transport work internally,
 * but it must return from each operation within the supplied timeout. The
 * backend owns all transport-specific state and recovery behavior.
 */
typedef struct
{
    rte_status_t (*setup)(rte_channel_service_storage_t *storage,
                           const char *channel_name);
    rte_status_t (*read)(rte_channel_service_storage_t *storage,
                          void *data,
                          size_t data_size,
                          rte_duration_ms_t timeout_ms);
    rte_status_t (*send)(rte_channel_service_storage_t *storage,
                          const void *data,
                          size_t data_size,
                          rte_duration_ms_t timeout_ms);
    rte_status_t (*close)(rte_channel_service_storage_t *storage);
} rte_channel_service_backend_t;

rte_status_t rte_channel_service_register_backend(const rte_channel_service_backend_t *backend);
rte_status_t rte_channel_service_setup(rte_channel_service_t *storage, const char *channel_name);
rte_status_t rte_channel_service_setup_by_id(rte_channel_service_t *storage, uint32_t channel_id);
rte_status_t rte_channel_service_read(rte_channel_service_t *storage,
                                         void *data,
                                         size_t data_size,
                                         rte_duration_ms_t timeout_ms);
rte_status_t rte_channel_service_send(rte_channel_service_t *storage,
                                         const void *data,
                                         size_t data_size,
                                         rte_duration_ms_t timeout_ms);
rte_status_t rte_channel_service_close(rte_channel_service_t *storage);

#ifdef __cplusplus
}
#endif

#endif
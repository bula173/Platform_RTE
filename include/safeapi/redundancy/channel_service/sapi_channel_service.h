#ifndef SAPI_CHANNEL_SERVICE_H
#define SAPI_CHANNEL_SERVICE_H

/**
 * @file sapi_channel_service.h
 * @brief Application-facing named channel service.
 *
 * The application addresses a channel by its configured name and performs
 * setup, read, and send operations from its own execution thread. Transport
 * lifecycle, reconnect, buffering, and synchronization belong to the
 * registered backend.
 */

#include <stddef.h>
#include <stdint.h>

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAPI_CHANNEL_SERVICE_NAME_SIZE (32U)

/** @brief Caller-owned storage for one named channel service instance. */
typedef struct
{
    uint8_t reserved[128U];
} sapi_channel_service_storage_t;

/** @brief Opaque handle to a named channel service instance. */
typedef sapi_channel_service_storage_t sapi_channel_service_t;

/**
 * @brief Backend implementation for named channel operations.
 *
 * A backend may perform blocking or non-blocking transport work internally,
 * but it must return from each operation within the supplied timeout. The
 * backend owns all transport-specific state and recovery behavior.
 */
typedef struct
{
    sapi_status_t (*setup)(sapi_channel_service_storage_t *storage,
                           const char *channel_name);
    sapi_status_t (*read)(sapi_channel_service_storage_t *storage,
                          void *data,
                          size_t data_size,
                          sapi_duration_ms_t timeout_ms);
    sapi_status_t (*send)(sapi_channel_service_storage_t *storage,
                          const void *data,
                          size_t data_size,
                          sapi_duration_ms_t timeout_ms);
    sapi_status_t (*close)(sapi_channel_service_storage_t *storage);
} sapi_channel_service_backend_t;

sapi_status_t sapi_channel_service_register_backend(const sapi_channel_service_backend_t *backend);
sapi_status_t sapi_channel_service_setup(sapi_channel_service_t *storage, const char *channel_name);
sapi_status_t sapi_channel_service_read(sapi_channel_service_t *storage,
                                         void *data,
                                         size_t data_size,
                                         sapi_duration_ms_t timeout_ms);
sapi_status_t sapi_channel_service_send(sapi_channel_service_t *storage,
                                         const void *data,
                                         size_t data_size,
                                         sapi_duration_ms_t timeout_ms);
sapi_status_t sapi_channel_service_close(sapi_channel_service_t *storage);

#ifdef __cplusplus
}
#endif

#endif
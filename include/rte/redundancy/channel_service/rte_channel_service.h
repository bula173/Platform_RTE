#ifndef RTE_CHANNEL_SERVICE_H
#define RTE_CHANNEL_SERVICE_H

/**
 * @file rte_channel_service.h
 * @brief Application-facing named channel service.
 *
 * The application addresses a channel by its configured name and performs
 * setup, read, and send operations from its own execution thread. Transport
 * lifecycle, reconnect, buffering, and synchronization belong to the
 * registered OSAdapter.
 */

#include <stddef.h>
#include <stdint.h>

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

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
 * @brief OSAdapter implementation for named channel operations.
 *
 * An OSAdapter may perform blocking or non-blocking transport work internally,
 * but it must return from each operation within the supplied timeout. The
 * OSAdapter owns all transport-specific state and recovery behavior.
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
    /**
     * @brief Optional: same as @c read, but also reports the true number of bytes the transport
     * actually received (a datagram transport preserves message boundaries; @c read alone gives
     * the caller no way to learn a received message was shorter than its buffer). May be NULL -
     * an OSAdapter that does not implement it is unchanged; @ref rte_channel_service_read_ex
     * falls back to @c read and reports @p data_size as the actual size (today's behaviour).
     */
    rte_status_t (*read_ex)(rte_channel_service_storage_t *storage,
                             void *data,
                             size_t data_size,
                             rte_duration_ms_t timeout_ms,
                             size_t *out_actual_size);
} rte_osadapter_channel_service_t;

/**
 * @brief Registers the OSAdapter that implements named channels (ADR-005).
 * @return RTE_STATUS_OK on success, RTE_STATUS_INVALID_PARAM if osadapter is NULL.
 */
rte_status_t rte_osadapter_channel_service_register(const rte_osadapter_channel_service_t *osadapter);

rte_status_t rte_channel_service_setup(rte_channel_service_t *storage, const char *channel_name);
rte_status_t rte_channel_service_setup_by_id(rte_channel_service_t *storage, uint32_t channel_id);
rte_status_t rte_channel_service_read(rte_channel_service_t *storage,
                                         void *data,
                                         size_t data_size,
                                         rte_duration_ms_t timeout_ms);
/**
 * @brief Same as @ref rte_channel_service_read, but also reports the actual number of bytes
 * received into @p data (never more than @p data_size). Use this instead of @ref
 * rte_channel_service_read whenever a channel may carry messages shorter than its configured
 * buffer size - a caller that only has the plain @c read can't tell a short message from one
 * that filled the buffer exactly, and forwarding @p data_size bytes verbatim would forward
 * trailing garbage. @p out_actual_size must not be NULL. On any non-OK return its value is
 * unspecified - the caller does not have a valid message to look at anyway.
 * @return RTE_STATUS_OK, or whatever the registered OSAdapter's read returns.
 */
rte_status_t rte_channel_service_read_ex(rte_channel_service_t *storage,
                                          void *data,
                                          size_t data_size,
                                          rte_duration_ms_t timeout_ms,
                                          size_t *out_actual_size);
rte_status_t rte_channel_service_send(rte_channel_service_t *storage,
                                         const void *data,
                                         size_t data_size,
                                         rte_duration_ms_t timeout_ms);
rte_status_t rte_channel_service_close(rte_channel_service_t *storage);

#ifdef __cplusplus
}
#endif

#endif
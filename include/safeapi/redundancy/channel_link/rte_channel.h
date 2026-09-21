/**
 * @file rte_channel.h
 * @brief A single redundant "channel": an opaque transport handle plus
 *        send/receive callbacks, with basic health bookkeeping.
 *
 * REDESIGNED (ADR-025): prior to this, `rte_channel_t` wrapped N
 * redundant channels *and* did 2oo2/2oo3/NMR voting between them in one
 * type. That voting engine has moved to `rte_voter` (register N
 * `rte_channel_t` instances into a `rte_voter_t`); a new
 * `rte_cross_comparator` does the same for exactly 2 channels. This
 * type is now the unit both of those register: one link, validated
 * dispatch to its backend, nothing more - the same "validate then
 * dispatch to a backend callback" shape as `rte_timer`/`rte_nvm`
 * elsewhere in this codebase, just with the backend supplied directly
 * in the config (no separate register_backend() call, since a channel
 * has exactly one backend for its whole lifetime, unlike a
 * process-global OAL service).
 *
 * @defgroup channel_link Channel Link (single redundant link)
 * @{
 */

#ifndef RTE_CHANNEL_LINK_H
#define RTE_CHANNEL_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "safeapi/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Health statistics for one vital channel.
 */
typedef struct {
    /** Number of successful sends on this channel */
    uint32_t send_count;
    /** Number of failed sends on this channel */
    uint32_t send_error_count;
    /** Number of successful receives on this channel */
    uint32_t receive_count;
    /** Number of failed receives (timeout/error) on this channel */
    uint32_t receive_error_count;
    /** Is this channel currently considered healthy (set false by its
     *  owner - a rte_voter_t/rte_cross_comparator_t - after enough
     *  consecutive I/O failures; this module itself never clears it). */
    bool is_healthy;
    /** Last error status observed on this channel */
    rte_status_t last_error;
} rte_channel_health_t;

/**
 * @brief Backend send callback: transmits on this channel's underlying
 *        transport.
 * @param[in] channel_handle  Opaque transport handle (as supplied in
 *                            rte_channel_config_t::channel_handle)
 * @param[in] data            Data to send
 * @param[in] data_size       Size of data in bytes
 * @return RTE_STATUS_OK on success; RTE_STATUS_TIMEOUT if the
 *         operation times out; RTE_STATUS_HARDWARE_FAULT on any other
 *         send error.
 */
typedef rte_status_t (*rte_channel_send_fn)(void *channel_handle,
                                               const void *data,
                                               size_t data_size);

/**
 * @brief Backend receive callback: reads from this channel's underlying
 *        transport.
 * @param[in]  channel_handle  Opaque transport handle
 * @param[out] data            Buffer to receive data
 * @param[in]  data_size       Size of data buffer
 * @param[in]  timeout_ms      Timeout in milliseconds
 * @return RTE_STATUS_OK on success; RTE_STATUS_TIMEOUT if the
 *         operation times out; RTE_STATUS_HARDWARE_FAULT on any other
 *         receive error.
 */
typedef rte_status_t (*rte_channel_recv_fn)(void *channel_handle,
                                               void *data,
                                               size_t data_size,
                                               uint32_t timeout_ms);

/**
 * @brief Configuration for rte_channel_init().
 */
typedef struct {
    /** Opaque transport handle, passed verbatim to send/recv below.
     *  Caller-owned; this module never dereferences it. May be NULL if
     *  the send/recv callbacks don't need it. */
    void *channel_handle;
    /** Backend send function. Must not be NULL. */
    rte_channel_send_fn send;
    /** Backend receive function. Must not be NULL. */
    rte_channel_recv_fn recv;
    /** Optional channel name (e.g. "ChannelAtoB"), for lookup
     *  (rte_voter_get_channel_by_name()) and logging - same
     *  caller-owned-pointer convention as rte_watchdog_config_t::name:
     *  not copied, so it must outlive this channel (a string literal is
     *  the common case). May be NULL, in which case this channel is
     *  never matched by rte_voter_get_channel_by_name(). */
    const char *name;
} rte_channel_config_t;

/**
 * @brief Storage for one vital channel instance (opaque to caller).
 *
 * Caller allocates this structure and passes it to rte_channel_init().
 * No dynamic memory.
 */
typedef struct {
    rte_channel_config_t config;  /**< Configuration this instance was initialized with. */
    rte_channel_health_t health;  /**< This channel's own health/statistics. */
    bool initialized;                    /**< Set true by rte_channel_init(). */
} rte_channel_storage_t;

/**
 * @brief Opaque handle to a vital channel instance.
 *
 * Created by rte_channel_init(); used directly, or registered
 * into a rte_voter_t (N-way voting) or rte_cross_comparator_t
 * (2-way comparison) for the redundancy/comparison use case.
 */
typedef rte_channel_storage_t rte_channel_t;

/**
 * @brief Initializes one vital channel over a caller-supplied transport.
 *
 * @param[out] storage  Pre-allocated storage. Must not be NULL.
 * @param[in]  config   Configuration with send/recv callbacks. Must not
 *                      be NULL; config->send and config->recv must not
 *                      be NULL.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if storage or config is NULL, or
 *         config->send/config->recv is NULL
 *
 * @pre storage != NULL
 * @pre config != NULL, config->send != NULL, config->recv != NULL
 * @post On success, storage is initialized with is_healthy = true and
 *       all counters at 0.
 *
 * @safety No dynamic memory allocation. Performs no I/O itself - the
 *         underlying transport must already be operational.
 *
 * REQ-CHANNEL-001: No dynamic allocation; caller supplies storage.
 * REQ-CHANNEL-002: Rejects a NULL config->send/config->recv.
 */
rte_status_t rte_channel_init(rte_channel_storage_t *storage,
                                       const rte_channel_config_t *config);

/**
 * @brief Sends data on this channel via its backend send callback.
 *
 * @param[in] handle     Vital channel handle. Must not be NULL and must
 *                       have been initialized.
 * @param[in] data       Data to send. Must not be NULL.
 * @param[in] data_size  Size of data in bytes. Must be > 0.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if handle/data is NULL, data_size
 *         is 0, or handle was never initialized
 * @return Whatever config->send() itself returns on failure (typically
 *         RTE_STATUS_TIMEOUT or RTE_STATUS_HARDWARE_FAULT)
 *
 * @post health.send_count or health.send_error_count is incremented;
 *       on failure, health.last_error is updated.
 *
 * REQ-CHANNEL-003: Dispatches to config->send and updates health
 * counters on every call, regardless of outcome.
 */
rte_status_t rte_channel_send(rte_channel_t *handle,
                                       const void *data,
                                       size_t data_size);

/**
 * @brief Receives data on this channel via its backend receive callback.
 *
 * @param[in]  handle      Vital channel handle. Must not be NULL and
 *                         must have been initialized.
 * @param[out] data        Buffer to receive data. Must not be NULL.
 * @param[in]  data_size   Size of data buffer. Must be > 0.
 * @param[in]  timeout_ms  Timeout passed to config->recv().
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if handle/data is NULL, data_size
 *         is 0, or handle was never initialized
 * @return Whatever config->recv() itself returns on failure
 *
 * @post health.receive_count or health.receive_error_count is
 *       incremented; on failure, health.last_error is updated.
 *
 * REQ-CHANNEL-003: Dispatches to config->recv and updates health
 * counters on every call, regardless of outcome.
 */
rte_status_t rte_channel_receive(rte_channel_t *handle,
                                          void *data,
                                          size_t data_size,
                                          uint32_t timeout_ms);

/**
 * @brief Queries this channel's health statistics.
 *
 * @param[in]  handle       Vital channel handle. Must not be NULL.
 * @param[out] out_health   Receives a copy of the current health state.
 *                          Must not be NULL.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if handle or out_health is NULL
 *
 * @safety Read-only; safe to call from any context.
 */
rte_status_t rte_channel_get_health(const rte_channel_t *handle,
                                             rte_channel_health_t *out_health);

/**
 * @brief Returns this channel's name, as given in
 *        rte_channel_config_t::name at rte_channel_init() time.
 *
 * @param[in] handle  Vital channel handle. Must not be NULL.
 * @return The channel's name (may itself be NULL if none was configured,
 *         or if handle was never initialized); NULL if handle is NULL.
 *
 * @safety Read-only; safe to call from any context.
 */
const char *rte_channel_get_name(const rte_channel_t *handle);

/**
 * @brief Marks this channel healthy/unhealthy.
 *
 * Intended for a rte_voter_t/rte_cross_comparator_t (or any other
 * owner) to call after deciding, by its own policy, that this channel
 * should stop (or resume) participating - this module itself never
 * changes is_healthy on its own (a single I/O failure alone does not
 * mark a channel unhealthy; that decision belongs to the owner tracking
 * failures across multiple channels).
 *
 * @param[in] handle      Vital channel handle. Must not be NULL.
 * @param[in] is_healthy  New health state.
 *
 * @return RTE_STATUS_OK on success
 * @return RTE_STATUS_INVALID_PARAM if handle is NULL
 *
 * REQ-CHANNEL-004: is_healthy defaults true at init and is never
 * cleared automatically by this module - only via this explicit call.
 */
rte_status_t rte_channel_set_healthy(rte_channel_t *handle,
                                              bool is_healthy);

/**
 * @brief Destroys a vital channel instance.
 *
 * No dynamic memory to free; the underlying transport is NOT closed
 * (caller retains ownership of channel_handle).
 *
 * @param[in] handle  Vital channel handle. May be NULL.
 *
 * @return RTE_STATUS_OK always (including when handle is NULL).
 *
 * @safety Idempotent; safe to call with NULL.
 */
rte_status_t rte_channel_destroy(rte_channel_t *handle);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* RTE_CHANNEL_LINK_H */

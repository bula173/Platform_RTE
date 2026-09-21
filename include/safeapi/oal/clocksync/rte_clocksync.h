/**
 * @file rte_clocksync.h
 * @brief Pluggable wall-clock synchronization OSAdapter (ADR-017)
 *
 * Exposes a per-node clock offset/quality query, backed by whatever real
 * synchronization mechanism an integrator plugs in (PTP, GPS discipline,
 * NTP, or a custom link) - no protocol implementation is bundled here,
 * only the pluggable interface, following the same
 * `rte_<service>_osadapter_t` + `rte_<service>_register_osadapter()` pattern
 * rte_timer already uses (ADR-005).
 *
 * IMPORTANT - what this module is *not* for: it does **not** decide
 * whether two vital channels are synchronized for comparison purposes.
 * That correctness argument comes entirely from rte_checkpoint.h's
 * bounded checkpoint-ID rendezvous (ADR-017 section 2.2), which works
 * correctly even with zero clock synchronization. rte_clocksync exists
 * for two narrower purposes only:
 *
 *  1. Diagnostics - correlating log/event timestamps across
 *     geographically separate nodes that otherwise have no common time
 *     reference.
 *  2. Sizing rte_checkpoint_config_t::max_delay_ms for a given
 *     deployment's actual known clock/network jitter, instead of
 *     guessing.
 *
 * A caller that reads a low reported offset and concludes "therefore the
 * channels' results are simultaneous" is misusing this module - that
 * conclusion is never valid on its own; only a confirmed matching
 * checkpoint ID within rte_checkpoint's timeout is.
 *
 * @defgroup CLOCKSYNC Clock Synchronization (diagnostic/timeout-sizing only)
 * @{
 *
 * REQ-CLOCKSYNC-001: rte_clocksync_get_offset_ms() and
 *                    rte_clocksync_get_quality() shall return
 *                    RTE_STATUS_NOT_INITIALIZED if no OSAdapter has been
 *                    registered.
 * REQ-CLOCKSYNC-002: this module shall never be called from, or
 *                    influence the outcome of, rte_channel_checkpoint()
 *                    or any other vital comparison - see the file-level
 *                    note above.
 */

#ifndef RTE_CLOCKSYNC_H
#define RTE_CLOCKSYNC_H

#include <stdint.h>

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Coarse quality/confidence of the current clock synchronization.
 */
typedef enum
{
    /** No synchronization has ever been established (e.g. just booted,
     *  or the sync source is unreachable). Reported offset, if any, must
     *  not be trusted. */
    RTE_CLOCKSYNC_UNSYNCHRONIZED = 0,
    /** Synchronized in the past but the OSAdapter judges the estimate
     *  stale/degraded (e.g. sync source lost, drift accumulating). */
    RTE_CLOCKSYNC_DEGRADED = 1,
    /** Currently synchronized within the OSAdapter's normal operating
     *  bound. */
    RTE_CLOCKSYNC_SYNCHRONIZED = 2
} rte_clocksync_quality_t;

/*
 * The OSAdapter vtable (rte_osadapter_clocksync_t) and
 * rte_osadapter_clocksync_register() live in
 * safeapi_osadapter/clocksync/rte_osadapter_clocksync.h, not here (ADR-021).
 * This header is the consumer-facing surface only.
 */

/**
 * @brief Reports this node's estimated clock offset (diagnostic/timeout-
 *        sizing only - see the file-level note; never use this to decide
 *        whether channel results are comparable).
 * @param out_offset_ms  Milliseconds, positive if local clock is ahead
 *                       of the OSAdapter's reference. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if out_offset_ms is
 *         NULL; RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (REQ-CLOCKSYNC-001); RTE_STATUS_NOT_SUPPORTED if the
 *         registered OSAdapter's get_offset_ms slot is NULL.
 */
rte_status_t rte_clocksync_get_offset_ms(int64_t *out_offset_ms);

/**
 * @brief Reports the OSAdapter's current confidence in the clock offset.
 * @param out_quality  Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if out_quality is
 *         NULL; RTE_STATUS_NOT_INITIALIZED if no OSAdapter is registered
 *         (REQ-CLOCKSYNC-001); RTE_STATUS_NOT_SUPPORTED if the
 *         registered OSAdapter's get_quality slot is NULL.
 */
rte_status_t rte_clocksync_get_quality(rte_clocksync_quality_t *out_quality);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* RTE_CLOCKSYNC_H */

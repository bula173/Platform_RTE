/**
 * @file rte_watchdog.c
 * @brief Real watchdog implementation: a fixed-size pool of watchdog
 *        slots, timed via the already-portable rte_timer_now() OAL
 *        primitive rather than any new OS-specific timing code of its
 *        own (REQ-OAL-COMMON-010: no dynamic allocation).
 *
 * Timeout detection is polling-based: something (the application's own
 * loop, or a periodic rte_timer callback) must call
 * rte_watchdog_timer_tick() regularly for a fired watchdog to actually
 * be detected - see that function's own doc for why a poll-driven design
 * was chosen here over a true ISR/thread-driven one.
 *
 * Replaces a previous stub where kick()/start()/get_status() were all
 * no-ops and no timeout was ever detected (see git history) - discovered
 * while wiring a real per-role watchdog into safeAPIRBC2oo2.
 *
 * @ingroup WATCHDOG
 */
#include "safeapi/redundancy/watchdog/rte_watchdog.h"

#include "safeapi/utils/lifecycle/rte_lifecycle.h"
#include "safeapi/oal/log/rte_log.h"
#include "safeapi/utils/safestate/rte_safestate.h"
#include "safeapi/oal/timer/rte_timer.h"

#include <stdbool.h>
#include <string.h>

#ifndef RTE_WATCHDOG_MAX_COUNT
/** Fixed pool size. rte_watchdog_create() returns
 *  RTE_STATUS_RESOURCE_EXHAUSTED once this many watchdogs are live at
 *  once - no dynamic growth, per this framework's no-malloc rule. */
#define RTE_WATCHDOG_MAX_COUNT 8U
#endif

/**
 * One pool slot's full state. The public rte_watchdog_t handle is a
 * pointer to one of these slots. This module owns the storage itself (a
 * static pool), unlike most other OAL services in this framework, which
 * take caller-owned storage via SAFEAPI_DECLARE_STORAGE - rte_watchdog.h's
 * own documented API (rte_watchdog_create() takes only a config and
 * returns a handle from an implicit "manager", with no caller-storage
 * parameter anywhere in its signature or its own doc examples) was
 * written against a pool-owned design from the start; this implementation
 * follows that, rather than silently changing the public API's contract.
 */
typedef struct rte_watchdog_s
{
    uint8_t                in_use;       /**< 1 if this pool slot is allocated to a live watchdog. */
    uint8_t                active;       /**< 1 if counting down (started and not stopped). */
    uint8_t                fired;      /**< Fired and not yet restarted via start(). */
    rte_watchdog_config_t config;        /**< Configuration this watchdog was created with. */
    rte_timestamp_ms_t    deadline_ms;   /**< Absolute time at which this watchdog next fires. */
    rte_timestamp_ms_t    last_kick_ms;  /**< Absolute time of the most recent kick/start. */
    uint32_t               kicks;         /**< Total number of successful kicks. */
    uint32_t               fires;         /**< Total number of times this watchdog has fired. */
    uint32_t               recoveries;    /**< Total number of recovery actions dispatched. */
} rte_watchdog_s;

/** @brief Fixed-size static pool backing every rte_watchdog_t handle. */
static rte_watchdog_s g_watchdog_pool[RTE_WATCHDOG_MAX_COUNT];
/** @brief 1 once rte_watchdog_manager_initialize() has been called. */
static uint8_t          g_manager_initialized = 0U;

/* ============================================================================
 * Internal helpers
 * ========================================================================== */

/**
 * @brief Defensive check that handle actually points at one of this
 *        module's own live pool slots, not an arbitrary caller pointer.
 *
 * Pointer comparison against both ends of the same array object is
 * well-defined in C (unlike comparing unrelated pointers).
 *
 * @param watchdog  Handle to validate.
 * @return true if watchdog points at an in-use slot of g_watchdog_pool;
 *         false otherwise (including NULL).
 */
static bool is_valid_handle(rte_watchdog_t watchdog)
{
    const rte_watchdog_s *slot = (const rte_watchdog_s *)watchdog;

    return (slot != NULL) && (slot >= &g_watchdog_pool[0]) && (slot < &g_watchdog_pool[RTE_WATCHDOG_MAX_COUNT])
           && (slot->in_use != 0U);
}

/**
 * @brief Checks whether action is one of the defined rte_watchdog_action_t enumerators.
 * @param action  Value to validate.
 * @return true if action is a recognized enumerator; false otherwise.
 */
static bool is_valid_action(rte_watchdog_action_t action)
{
    bool valid;

    switch (action)
    {
        case RTE_WATCHDOG_ACTION_LOG:
        case RTE_WATCHDOG_ACTION_SAFESTATE:
        case RTE_WATCHDOG_ACTION_REBOOT:
        case RTE_WATCHDOG_ACTION_FAILOVER:
        case RTE_WATCHDOG_ACTION_CUSTOM:
            valid = true;
            break;
        default:
            valid = false;
            break;
    }
    return valid;
}

/* ============================================================================
 * API: Watchdog Manager
 * ========================================================================== */

rte_status_t rte_watchdog_manager_initialize(void)
{
    if (g_manager_initialized == 0U)
    {
        (void)memset(g_watchdog_pool, 0, sizeof(g_watchdog_pool));
        g_manager_initialized = 1U;
        rte_log_write(RTE_LOG_LEVEL_INFO, "watchdog", "manager initialized");
    }
    return RTE_STATUS_OK;
}

rte_status_t rte_watchdog_manager_shutdown(void)
{
    size_t i;

    for (i = 0U; i < (size_t)RTE_WATCHDOG_MAX_COUNT; i++)
    {
        g_watchdog_pool[i].active = 0U;
    }
    g_manager_initialized = 0U;
    rte_log_write(RTE_LOG_LEVEL_INFO, "watchdog", "manager shut down");
    return RTE_STATUS_OK;
}

/* ============================================================================
 * API: Core Watchdog Operations
 * ========================================================================== */

rte_status_t rte_watchdog_create(rte_watchdog_t *handle_out, const rte_watchdog_config_t *config)
{
    size_t i;

    if ((handle_out == NULL) || (config == NULL) || (config->timeout_ms == 0U) || !is_valid_action(config->action))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if (((config->action == RTE_WATCHDOG_ACTION_CUSTOM) || (config->action == RTE_WATCHDOG_ACTION_FAILOVER))
        && (config->custom_action == NULL))
    {
        /* FAILOVER requires a real handler just like CUSTOM does (see
         * rte_watchdog_timeout_handler()'s own doc) - a FAILOVER watchdog
         * with no handler wired up can only ever log, which is what
         * ACTION_LOG is already for; requiring custom_action here catches
         * that misconfiguration at create() time instead of silently
         * degrading to a log line the first time it actually fires. */
        return RTE_STATUS_INVALID_PARAM;
    }
    if (g_manager_initialized == 0U)
    {
        return RTE_STATUS_NOT_INITIALIZED;
    }
    {
        /* REQ-LIFECYCLE-001 (ADR-026): a watchdog is a setup-only resource - refuse once the
         * application's setup phase has been locked. */
        rte_status_t lifecycle_status = rte_lifecycle_check_setup_allowed();

        if (lifecycle_status != RTE_STATUS_OK)
        {
            return lifecycle_status;
        }
    }

    for (i = 0U; i < (size_t)RTE_WATCHDOG_MAX_COUNT; i++)
    {
        if (g_watchdog_pool[i].in_use == 0U)
        {
            (void)memset(&g_watchdog_pool[i], 0, sizeof(g_watchdog_pool[i]));
            g_watchdog_pool[i].in_use = 1U;
            g_watchdog_pool[i].config = *config;
            *handle_out = (rte_watchdog_t)&g_watchdog_pool[i];
            rte_log_write(RTE_LOG_LEVEL_INFO, config->name, "watchdog created");
            return RTE_STATUS_OK;
        }
    }
    return RTE_STATUS_RESOURCE_EXHAUSTED;
}

rte_status_t rte_watchdog_start(rte_watchdog_t watchdog)
{
    rte_watchdog_s *slot = (rte_watchdog_s *)watchdog;
    rte_timestamp_ms_t now_ms = 0U;

    if (!is_valid_handle(watchdog))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    (void)rte_timer_now(&now_ms);
    slot->active = 1U;
    slot->fired = 0U;
    slot->deadline_ms = now_ms + slot->config.timeout_ms;
    slot->last_kick_ms = now_ms;
    return RTE_STATUS_OK;
}

rte_status_t rte_watchdog_stop(rte_watchdog_t watchdog)
{
    rte_watchdog_s *slot = (rte_watchdog_s *)watchdog;

    if (!is_valid_handle(watchdog))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    slot->active = 0U;
    return RTE_STATUS_OK;
}

rte_status_t rte_watchdog_kick(rte_watchdog_t watchdog)
{
    rte_watchdog_s *slot = (rte_watchdog_s *)watchdog;
    rte_timestamp_ms_t now_ms = 0U;

    if (!is_valid_handle(watchdog))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    if ((slot->active == 0U) || (slot->fired != 0U))
    {
        /* Not running, or already fired and awaiting an explicit
         * start() before it can be kicked again - this codebase's real
         * status enum has no generic RTE_STATUS_ERROR (see
         * rte_status.h); RTE_STATUS_INTERNAL_ERROR is the closest
         * "kick while not in a kickable state" signal available. */
        return RTE_STATUS_INTERNAL_ERROR;
    }
    (void)rte_timer_now(&now_ms);
    slot->deadline_ms = now_ms + slot->config.timeout_ms;
    slot->last_kick_ms = now_ms;
    slot->kicks++;
    return RTE_STATUS_OK;
}

rte_status_t rte_watchdog_get_status(rte_watchdog_t watchdog, rte_watchdog_status_t *status_out)
{
    const rte_watchdog_s *slot = (const rte_watchdog_s *)watchdog;
    rte_timestamp_ms_t now_ms = 0U;

    if (!is_valid_handle(watchdog) || (status_out == NULL))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    (void)rte_timer_now(&now_ms);
    status_out->active = slot->active;
    status_out->kicks = slot->kicks;
    status_out->fires = slot->fires;
    status_out->recoveries = slot->recoveries;
    status_out->time_since_last_kick =
        (rte_duration_ms_t)((now_ms >= slot->last_kick_ms) ? (now_ms - slot->last_kick_ms) : 0U);
    status_out->time_until_fire = (rte_duration_ms_t)((slot->deadline_ms > now_ms) ? (slot->deadline_ms - now_ms) : 0U);
    return RTE_STATUS_OK;
}

rte_status_t rte_watchdog_destroy(rte_watchdog_t watchdog)
{
    rte_watchdog_s *slot = (rte_watchdog_s *)watchdog;

    if (!is_valid_handle(watchdog))
    {
        return RTE_STATUS_INVALID_PARAM;
    }
    slot->active = 0U;
    slot->in_use = 0U;
    return RTE_STATUS_OK;
}

/* ============================================================================
 * Internal: Timeout Dispatch
 * ========================================================================== */

void rte_watchdog_timeout_handler(uint32_t watchdog_id)
{
    rte_watchdog_s *slot;

    if (watchdog_id >= (uint32_t)RTE_WATCHDOG_MAX_COUNT)
    {
        return;
    }
    slot = &g_watchdog_pool[watchdog_id];
    if ((slot->in_use == 0U) || (slot->active == 0U) || (slot->fired != 0U))
    {
        return;
    }

    slot->fired = 1U;
    slot->fires++;

    switch (slot->config.action)
    {
        case RTE_WATCHDOG_ACTION_LOG:
            rte_log_write(RTE_LOG_LEVEL_ERROR, slot->config.name, "watchdog timeout");
            break;
        case RTE_WATCHDOG_ACTION_SAFESTATE:
            slot->recoveries++;
            rte_log_write(RTE_LOG_LEVEL_ERROR, slot->config.name, "watchdog timeout - entering SAFE state");
            rte_safestate_enter(RTE_SAFESTATE_LEVEL_SAFE, RTE_SAFESTATE_REASON_UNSPECIFIED, __FILE__,
                                  (int32_t)__LINE__, slot->config.name);
            break; /* Not statically unreachable: rte_safestate_enter() has no
                    * [[noreturn]]/_Noreturn attribute, so the compiler cannot
                    * prove this dead - kept for switch-statement completeness. */
        case RTE_WATCHDOG_ACTION_REBOOT:
            slot->recoveries++;
            rte_log_write(RTE_LOG_LEVEL_ERROR, slot->config.name, "watchdog timeout - requesting reboot");
            rte_safestate_enter(RTE_SAFESTATE_LEVEL_REBOOT, RTE_SAFESTATE_REASON_UNSPECIFIED, __FILE__,
                                  (int32_t)__LINE__, slot->config.name);
            break; /* Same non-return note as SAFESTATE above. */
        case RTE_WATCHDOG_ACTION_FAILOVER:
            slot->recoveries++;
            rte_log_write(RTE_LOG_LEVEL_ERROR, slot->config.name, "watchdog timeout - failover requested");
            /* rte_watchdog_create() requires config.custom_action != NULL
             * for this action (see its own doc) - dispatched exactly like
             * ACTION_CUSTOM, just under a name that documents *why* the
             * integrator registered this watchdog (loss of a redundant
             * peer/channel) rather than *how* it reacts, which is
             * identical to CUSTOM's own mechanism. This used to be a dead
             * stub ("no generic failover primitive in this framework;
             * caller must poll rte_watchdog_get_status()") - replaced
             * once a real integrator (safeAPIRBC2oo2's dual-channel A/B
             * link) needed exactly this reaction and found nothing to
             * call. */
            if (slot->config.custom_action != NULL)
            {
                slot->config.custom_action(slot->config.context);
            }
            break;
        case RTE_WATCHDOG_ACTION_CUSTOM:
            slot->recoveries++;
            if (slot->config.custom_action != NULL)
            {
                slot->config.custom_action(slot->config.context);
            }
            break;
        default: /* GCOVR_EXCL_LINE - see rationale below */
            /* Defense-in-depth, not a reachable API path:
             * rte_watchdog_create() rejects any config.action that fails
             * is_valid_action() before a slot is ever populated, so a live
             * slot's action is always one of the 5 named cases above.
             * There is no public way to mutate an already-created slot's
             * action, so this default cannot be reached by any caller -
             * kept for switch-statement completeness against a future
             * rte_watchdog_action_t value, not because it is exercised
             * today. See docs/COVERAGE_REPORT.md for the accepted-
             * exception rationale (this is one of the project's two
             * documented gaps against the 100% branch coverage target). */
            rte_log_write(RTE_LOG_LEVEL_ERROR, slot->config.name, "watchdog timeout - unrecognized action"); /* GCOVR_EXCL_LINE */
            break; /* GCOVR_EXCL_LINE */
    }
}

/* ============================================================================
 * Timer Integration
 * ========================================================================== */

void rte_watchdog_timer_tick(void)
{
    /* Polling design, not an ISR/hardware-timer callback: this framework's
     * only portable time source is rte_timer_now() (a plain "read the
     * clock" query, not a way to register a recurring OS-level interrupt
     * across every target this framework claims to support - POSIX,
     * QNX, bare-metal SysTick, etc.). Giving this module its own
     * platform-specific interrupt setup would duplicate what
     * safeapi::timer already exists to abstract, and would break the
     * "OSAdapters are integrator-supplied" philosophy (ADR-005) for a
     * module whose own header was never given an OSAdapter vtable. Instead:
     * whatever already runs periodically in the integrating application
     * (its own rte_timer periodic callback, or just its own main loop)
     * is expected to call this function regularly - each call is an O(N)
     * scan (N = RTE_WATCHDOG_MAX_COUNT, a small fixed pool) comparing
     * each active watchdog's deadline against the current time. */
    rte_timestamp_ms_t now_ms = 0U;
    uint32_t i;

    if (g_manager_initialized == 0U)
    {
        return;
    }
    (void)rte_timer_now(&now_ms);
    for (i = 0U; i < (uint32_t)RTE_WATCHDOG_MAX_COUNT; i++)
    {
        const rte_watchdog_s *slot = &g_watchdog_pool[i];

        if ((slot->in_use != 0U) && (slot->active != 0U) && (slot->fired == 0U) && (now_ms >= slot->deadline_ms))
        {
            rte_watchdog_timeout_handler(i);
        }
    }
}

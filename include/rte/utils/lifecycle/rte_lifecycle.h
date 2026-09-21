/**
 * @file rte_lifecycle.h
 * @brief Process-wide application setup-phase lock (ADR-026).
 *
 * A safety-critical application shall have a well-defined INIT phase
 * (create timers, channels, voters, cross-comparators, watchdogs) that
 * completes before its RUN phase begins - and once RUN has begun, no
 * further such construction is legitimate: an application whose resource
 * set can still change while it is executing cyclically is not one whose
 * behavior can be fully verified ahead of time.
 *
 * `rte_appmanager_run()` is this framework's own INIT/RUN boundary: it
 * calls rte_lifecycle_lock() the moment `ops->init()` returns
 * successfully (see that function's own doc) and rte_lifecycle_unlock()
 * at the very start of every call (mirroring its own existing
 * `g_app_state` reset - see rte_appmanager.c). Every setup-only
 * constructor this framework ships (`rte_timer_create()`,
 * `rte_channel_init()`, `rte_voter_init()`/`_register_channel()`,
 * `rte_cross_comparator_init()`/`_register_channel()`,
 * `rte_watchdog_create()`) calls rte_lifecycle_check_setup_allowed()
 * as one of its own first checks and returns `RTE_STATUS_INVALID_STATE`
 * once locked - see each function's own doc for exactly where.
 *
 * **Deliberately excluded** (ADR-026 section 3): `rte_netlink_open()`,
 * `rte_dual_channel_init()`, and `rte_dual_negotiator_init()` are NOT
 * gated by this lock. All three are legitimately re-invoked after RUN has
 * begun by an application's own reconnect-after-link-loss logic (e.g.
 * safeAPIRBC2oo2's `channel_ab_io.c`/`channel_ab_negotiate_reconnect()`),
 * which is re-establishing a link the application already owns, not
 * adding a new one the application's own design never accounted for -
 * see ADR-026 for the full rationale on why that distinction, not the
 * "called after RUN" timing alone, is what this lock actually polices.
 *
 * This is a single, process-wide flag (ADR-026 section 2, same
 * single-instance assumption `rte_appmanager`'s own `g_app_state`
 * already makes - see that module's own file header) - not one lock per
 * `rte_appmanager_config_t`, since this framework has no concept of
 * more than one concurrently-running application per process today.
 *
 * REQ-LIFECYCLE-001: setup-only constructors shall reject their call with
 *                     `RTE_STATUS_INVALID_STATE` once the application's
 *                     setup phase has been locked.
 * REQ-LIFECYCLE-002: the setup-phase lock shall be a plain, unsynchronized
 *                     global (no dynamic allocation, no OS dependency) -
 *                     this module has no OS dependency of its own,
 *                     matching `rte_safestate`'s own posture (ADR-002
 *                     section 4).
 *
 * @defgroup LIFECYCLE Application Setup-Phase Lock
 * @brief Process-wide INIT/RUN boundary enforcement (ADR-026)
 * @{
 */
#ifndef RTE_COMMON_LIFECYCLE_H
#define RTE_COMMON_LIFECYCLE_H

#include <stdbool.h>

#include "rte/utils/status/rte_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Locks the application's setup phase.
 *
 * After this call, rte_lifecycle_check_setup_allowed() (and therefore
 * every setup-only constructor that calls it) returns
 * `RTE_STATUS_INVALID_STATE` until the next rte_lifecycle_unlock().
 * Idempotent: locking an already-locked application is a no-op.
 *
 * Intended caller: `rte_appmanager_run()`, immediately after `ops->init()`
 * returns `RTE_STATUS_OK` - integrator application code does not call
 * this directly under normal use.
 */
void rte_lifecycle_lock(void);

/**
 * @brief Unlocks the application's setup phase (setup is allowed again).
 *
 * Idempotent: unlocking an already-unlocked application is a no-op.
 *
 * Intended caller: `rte_appmanager_run()`, at the start of every call -
 * this is what lets this framework's own test suite (and any integrator
 * code) call `rte_appmanager_run()` more than once, sequentially, in one
 * process and have each call's own setup phase behave independently.
 */
void rte_lifecycle_unlock(void);

/**
 * @brief @return `true` if the application's setup phase is currently
 *        locked (see rte_lifecycle_lock()), `false` otherwise. Intended
 *        for diagnostics; prefer rte_lifecycle_check_setup_allowed() at
 *        an actual constructor's own call site.
 */
bool rte_lifecycle_is_locked(void);

/**
 * @brief Convenience check for a setup-only constructor: call this as one
 *        of the first checks in any function that creates/registers a new
 *        long-lived resource (a timer, channel, voter, cross-comparator,
 *        or watchdog - see this file's own header for the excluded
 *        netlink/dual exceptions).
 * @return `RTE_STATUS_INVALID_STATE` if the application's setup phase is
 *         currently locked; `RTE_STATUS_OK` otherwise.
 */
rte_status_t rte_lifecycle_check_setup_allowed(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_COMMON_LIFECYCLE_H */

/** @} */ /* LIFECYCLE */

/**
 * @file sapi_safety_violation.h
 * @brief Opt-in notification hook for the three new safety primitives
 *        (safe pointer, checked integer arithmetic, bounds check): a
 *        SEPARATE, dedicated handler from sapi_safestate_handler_t
 *        (sapi_safestate.h) - registering it does not change any of
 *        those primitives' own return-code contract (REQ-COMMON-CAST-004/
 *        005, REQ-OAL-SAFEPTR-001/002) in any way; with no handler
 *        registered (the default), nothing about their behavior changes
 *        at all.
 *
 * Why a separate mechanism from sapi_safestate: unlike a checkpoint
 * timeout or a voter disagreement (which are ALWAYS genuine faults, so
 * REQ-CHECKPOINT-003/REQ-VOTER-005 escalate through sapi_safestate_enter()
 * unconditionally), these three primitives are also called during
 * ordinary, expected control flow (e.g. sapi_cast_bounds_check() used to
 * ask "is this the last valid index" is not a fault). Forcing every one
 * of their failures through sapi_safestate_enter() would trigger a
 * permanent halt (REQ-COMMON-SAFESTATE-002) on entirely normal code
 * paths. This module instead gives an integrator who WANTS visibility
 * into every such event (e.g. to log it, count it, or make their OWN
 * judgment call about escalating) an explicit, separate opt-in - they
 * decide what a violation means for their own application, this module
 * does not decide for them.
 *
 * @note The file/line reported is the DETECTION site inside this
 *       framework's own implementation (sapi_safe_ptr.c/sapi_cast.c),
 *       not the ultimate caller's call site - unlike SAPI_ASSERT/
 *       SAPI_SAFESTATE (macros, so they capture the caller's own
 *       __FILE__/__LINE__), the three primitives this module instruments
 *       are ordinary functions, not macros, per sapi_cast.h's own
 *       72-function convention. The violation `kind` and `message`
 *       (which does include the specific values involved) are the
 *       primary diagnostic content; the file/line narrows down which
 *       primitive detected it, not where it was called from.
 *
 * REQ-COMMON-SAFETYVIOLATION-001: no dynamic allocation; a single fixed
 *                                 handler slot (this is a lower-frequency,
 *                                 broader-scope hook than
 *                                 sapi_safestate's own per-level slots -
 *                                 one registered handler covers all three
 *                                 primitive families).
 * REQ-COMMON-SAFETYVIOLATION-002: with no handler registered,
 *                                 sapi_safety_violation_report() is a
 *                                 no-op - reporting a violation never
 *                                 itself becomes a new failure mode.
 *
 * @defgroup SAFETYVIOLATION Safety Primitive Violation Notification
 * @brief Opt-in handler for safe-pointer/checked-cast/bounds-check violations
 * @{
 */
#ifndef SAFEAPI_COMMON_SAFETY_VIOLATION_H
#define SAFEAPI_COMMON_SAFETY_VIOLATION_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Which safety primitive detected the violation.
 */
typedef enum sapi_safety_violation_kind_e
{
    /** sapi_safe_ptr_get()/_offset() found a corrupted canary
     *  (REQ-OAL-SAFEPTR-001) - never a normal condition. */
    SAPI_SAFETY_VIOLATION_CORRUPTION = 0,
    /** A sapi_cast_checked_add_*()/_sub_*()/_mul_*() call detected
     *  overflow or underflow (REQ-COMMON-CAST-004). */
    SAPI_SAFETY_VIOLATION_OVERFLOW = 1,
    /** sapi_cast_bounds_check() (REQ-COMMON-CAST-005) or
     *  sapi_safe_ptr_offset()'s own bounds check
     *  (REQ-OAL-SAFEPTR-002) found index/offset+length outside the
     *  valid range. */
    SAPI_SAFETY_VIOLATION_OUT_OF_RANGE = 2
} sapi_safety_violation_kind_t;

/**
 * @brief Handler invoked by sapi_safety_violation_report().
 *
 * @param kind     Which kind of violation was detected.
 * @param file     Detection site file (see this header's own @note on
 *                 why this is the primitive's own file, not the
 *                 caller's), may be NULL.
 * @param line     Detection site line.
 * @param message  Human-readable detail (typically includes the actual
 *                 values involved), may be NULL.
 *
 * @note Unlike sapi_safestate_handler_t, this handler is expected to
 *       return - it is a notification, not a fault-reaction level. Do
 *       not call sapi_safestate_enter() unconditionally from inside it;
 *       that would defeat the whole point of keeping this separate from
 *       sapi_safestate (see this header's own file-level doc) - make
 *       that decision deliberately, in your own handler, if that is what
 *       your application wants for a specific kind.
 */
typedef void (*sapi_safety_violation_handler_t)(sapi_safety_violation_kind_t kind,
                                                  const char *file,
                                                  int32_t line,
                                                  const char *message);

/**
 * @brief Registers the single handler invoked by
 *        sapi_safety_violation_report(). Registering again replaces the
 *        previous handler (same convention as
 *        sapi_safestate_register_handler()).
 *
 * @param handler  Handler to invoke on every reported violation. Must
 *                 not be NULL - pass a handler that does nothing if you
 *                 want to temporarily stop reacting without un-instrumenting
 *                 the primitives themselves (there is no "unregister").
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         handler is NULL.
 */
sapi_status_t sapi_safety_violation_register_handler(sapi_safety_violation_handler_t handler);

/**
 * @brief Reports one violation to the registered handler, if any.
 *
 * Called internally by sapi_safe_ptr_get()/_offset() and the
 * sapi_cast_checked_*()/sapi_cast_bounds_check() functions at their own
 * detection points - not normally called directly by application code.
 *
 * @param kind     Which kind of violation occurred.
 * @param file     Detection site file, may be NULL.
 * @param line     Detection site line.
 * @param message  Human-readable detail, may be NULL.
 *
 * @safety No-op (REQ-COMMON-SAFETYVIOLATION-002) if no handler is
 *         registered - reporting a violation can never itself introduce
 *         a new failure mode.
 */
void sapi_safety_violation_report(sapi_safety_violation_kind_t kind,
                                   const char *file,
                                   int32_t line,
                                   const char *message);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_SAFETY_VIOLATION_H */

/** @} */ /* SAFETYVIOLATION */

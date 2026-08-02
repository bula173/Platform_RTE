/**
 * @file sapi_safestate.h
 * @brief Safe-state transitions and checked assertions (ADR-004).
 *
 * A layer-agnostic fault-reaction facility usable from any layer (OAL,
 * the future L1, and L2 RBC core): SAPI_ASSERT() replaces the
 * production-banned standard assert(); SAPI_SAFESTATE()/SAPI_REBOOT()
 * let any code explicitly declare "this fault requires the system to
 * leave normal operation."
 *
 * This module has no OS dependency (see ADR-002 section 4, ADR-004
 * section 2.3): the actual fail-safe reaction is supplied by the
 * integrator via sapi_safestate_register_handler(), not by this module
 * calling into src/os itself.
 *
 * REQ-COMMON-SAFESTATE-001: no dynamic allocation; handler storage is a
 *                           fixed array of 3 slots (one per level).
 * REQ-COMMON-SAFESTATE-002: SAPI_SAFESTATE_LEVEL_SAFE and
 *                           SAPI_SAFESTATE_LEVEL_REBOOT shall never return
 *                           control to the caller, even if no handler is
 *                           registered or the registered handler itself
 *                           returns (see sapi_safestate_enter()).
 */
#ifndef SAFEAPI_COMMON_SAFESTATE_H
#define SAFEAPI_COMMON_SAFESTATE_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe-state severity levels (ADR-004 section 2.1).
 */
typedef enum sapi_safestate_level_e
{
    /** Continue operating with reduced functionality. The only level for
     *  which sapi_safestate_enter() is permitted to return to the caller. */
    SAPI_SAFESTATE_LEVEL_DEGRADED = 0,

    /** Fail-safe restrictive state (e.g. withdraw movement authorities).
     *  sapi_safestate_enter() does not return for this level. */
    SAPI_SAFESTATE_LEVEL_SAFE = 1,

    /** SAFE is judged insufficient; a controlled restart is required.
     *  sapi_safestate_enter() does not return for this level. */
    SAPI_SAFESTATE_LEVEL_REBOOT = 2
} sapi_safestate_level_t;

/**
 * @brief Diagnostic reason code accompanying a safe-state transition.
 *
 * Values 0-4095 are reserved for the framework itself (see the
 * SAPI_SAFESTATE_REASON_* constants below). Application/RBC-specific
 * reason codes shall use 4096 and above.
 */
typedef uint16_t sapi_safestate_reason_t;

/** Reserved framework reason codes (0-4095). */
#define SAPI_SAFESTATE_REASON_UNSPECIFIED    ((sapi_safestate_reason_t)0U)
/** Set by SAPI_ASSERT() when its condition evaluates to false. */
#define SAPI_SAFESTATE_REASON_ASSERT_FAILED  ((sapi_safestate_reason_t)1U)
/** First reason code value applications are free to define their own meaning for. */
#define SAPI_SAFESTATE_REASON_APPLICATION_BASE ((sapi_safestate_reason_t)4096U)

/**
 * @brief Application-supplied reaction for one safe-state level.
 *
 * @param level    The level being entered (matches the slot this handler
 *                 was registered for).
 * @param reason   Diagnostic reason code.
 * @param file     Source file of the call site (from __FILE__), may be NULL.
 * @param line     Source line of the call site (from __LINE__).
 * @param message  Optional human-readable detail (e.g. the failed
 *                 expression text from SAPI_ASSERT), may be NULL.
 *
 * @note For SAPI_SAFESTATE_LEVEL_SAFE and SAPI_SAFESTATE_LEVEL_REBOOT this
 *       handler shall not return. If it does anyway,
 *       sapi_safestate_enter() falls back to a defensive infinite loop
 *       (REQ-COMMON-SAFESTATE-002) rather than resuming the caller.
 */
typedef void (*sapi_safestate_handler_t)(sapi_safestate_level_t level,
                                          sapi_safestate_reason_t reason,
                                          const char *file,
                                          int32_t line,
                                          const char *message);

/**
 * @brief Registers the reaction handler for one safe-state level.
 *        This is how an integrator supplies their own implementation of
 *        "what happens when this level is entered" (ADR-004 section 2.2).
 *
 * @param level    The level to register a handler for.
 * @param handler  Handler to invoke on entry to this level. Must not be NULL.
 * @return SAPI_STATUS_INVALID_PARAM if handler is NULL or level is not a
 *         valid sapi_safestate_level_t value; SAPI_STATUS_OK otherwise.
 *         Registering again for the same level replaces the previous handler.
 *
 * REQ-COMMON-SAFESTATE-010
 */
sapi_status_t sapi_safestate_register_handler(sapi_safestate_level_t level,
                                               sapi_safestate_handler_t handler);

/**
 * @brief Enters a safe-state level: invokes the registered handler (if
 *        any), then, for SAPI_SAFESTATE_LEVEL_SAFE and
 *        SAPI_SAFESTATE_LEVEL_REBOOT, guarantees the call never returns
 *        (REQ-COMMON-SAFESTATE-002) even if no handler was registered or
 *        the handler itself returns.
 *
 * @param level    Level to enter.
 * @param reason   Diagnostic reason code.
 * @param file     Call site file (typically __FILE__), may be NULL.
 * @param line     Call site line (typically __LINE__).
 * @param message  Optional human-readable detail, may be NULL.
 *
 * @note Prefer the SAPI_ASSERT / SAPI_SAFESTATE / SAPI_REBOOT macros over
 *       calling this directly, so __FILE__/__LINE__ are captured correctly.
 *
 * REQ-COMMON-SAFESTATE-011
 */
void sapi_safestate_enter(sapi_safestate_level_t level,
                           sapi_safestate_reason_t reason,
                           const char *file,
                           int32_t line,
                           const char *message);

/**
 * @def SAPI_ASSERT
 * @brief Checked assertion. Always active in every build configuration,
 *        including production (ADR-004 section 2.5) - unlike standard
 *        assert(), which CLAUDE.md prohibits in production paths. On
 *        failure, enters SAPI_SAFESTATE_LEVEL_SAFE with reason
 *        SAPI_SAFESTATE_REASON_ASSERT_FAILED.
 */
#define SAPI_ASSERT(cond)                                                    \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_SAFE,                  \
                                  SAPI_SAFESTATE_REASON_ASSERT_FAILED,        \
                                  __FILE__, (int32_t)__LINE__, #cond);        \
        }                                                                    \
    } while (0)

/**
 * @def SAPI_SAFESTATE
 * @brief Explicitly enters the given safe-state level with a reason code,
 *        capturing the call site automatically.
 */
#define SAPI_SAFESTATE(level, reason) \
    sapi_safestate_enter((level), (reason), __FILE__, (int32_t)__LINE__, NULL)

/**
 * @def SAPI_REBOOT
 * @brief Enters SAPI_SAFESTATE_LEVEL_REBOOT with the given reason code.
 *        Equivalent to SAPI_SAFESTATE(SAPI_SAFESTATE_LEVEL_REBOOT, reason).
 */
#define SAPI_REBOOT(reason) \
    sapi_safestate_enter(SAPI_SAFESTATE_LEVEL_REBOOT, (reason), __FILE__, (int32_t)__LINE__, NULL)

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_SAFESTATE_H */

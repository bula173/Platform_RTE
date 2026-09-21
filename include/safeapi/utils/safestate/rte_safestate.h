/**
 * @file rte_safestate.h
 * @brief Safe-state transitions and checked assertions (ADR-004).
 *
 * A layer-agnostic fault-reaction facility usable from any layer (OAL,
 * the future L1, and L2 RBC core): RTE_ASSERT() replaces the
 * production-banned standard assert(); RTE_SAFESTATE()/RTE_REBOOT()
 * let any code explicitly declare "this fault requires the system to
 * leave normal operation."
 *
 * This module has no OS dependency (see ADR-002 section 4, ADR-004
 * section 2.3): the actual fail-safe reaction is supplied by the
 * integrator via rte_safestate_register_handler(), not by this module
 * calling into src/os itself.
 *
 * REQ-COMMON-SAFESTATE-001: no dynamic allocation; handler storage is a
 *                           fixed array of 3 slots (one per level).
 * REQ-COMMON-SAFESTATE-002: RTE_SAFESTATE_LEVEL_SAFE and
 *                           RTE_SAFESTATE_LEVEL_REBOOT shall never return
 *                           control to the caller, even if no handler is
 *                           registered or the registered handler itself
 *                           returns (see rte_safestate_enter()).
 *
 * @defgroup SAFESTATE Safe-State Transitions and Checked Assertions
 * @brief Layer-agnostic fault-reaction facility (ADR-004)
 * @{
 */
#ifndef SAFEAPI_COMMON_SAFESTATE_H
#define SAFEAPI_COMMON_SAFESTATE_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe-state severity levels (ADR-004 section 2.1).
 */
typedef enum rte_safestate_level_e
{
    /** Continue operating with reduced functionality. The only level for
     *  which rte_safestate_enter() is permitted to return to the caller. */
    RTE_SAFESTATE_LEVEL_DEGRADED = 0,

    /** Fail-safe restrictive state (e.g. withdraw movement authorities).
     *  rte_safestate_enter() does not return for this level. */
    RTE_SAFESTATE_LEVEL_SAFE = 1,

    /** SAFE is judged insufficient; a controlled restart is required.
     *  rte_safestate_enter() does not return for this level. */
    RTE_SAFESTATE_LEVEL_REBOOT = 2
} rte_safestate_level_t;

/**
 * @brief Diagnostic reason code accompanying a safe-state transition.
 *
 * Values 0-4095 are reserved for the framework itself (see the
 * RTE_SAFESTATE_REASON_* constants below). Application/RBC-specific
 * reason codes shall use 4096 and above.
 */
typedef uint16_t rte_safestate_reason_t;

/** Reserved framework reason codes (0-4095). */
#define RTE_SAFESTATE_REASON_UNSPECIFIED    ((rte_safestate_reason_t)0U)
/** Set by RTE_ASSERT() when its condition evaluates to false. */
#define RTE_SAFESTATE_REASON_ASSERT_FAILED  ((rte_safestate_reason_t)1U)
/** Set by rte_channel_checkpoint() (ADR-017) when fewer than
 *  expected_node_count peers confirm a checkpoint within max_delay_ms. */
#define RTE_SAFESTATE_REASON_CHECKPOINT_TIMEOUT ((rte_safestate_reason_t)2U)
/** First reason code value applications are free to define their own meaning for. */
#define RTE_SAFESTATE_REASON_APPLICATION_BASE ((rte_safestate_reason_t)4096U)

/**
 * @brief Application-supplied reaction for one safe-state level.
 *
 * @param level    The level being entered (matches the slot this handler
 *                 was registered for).
 * @param reason   Diagnostic reason code.
 * @param file     Source file of the call site (from __FILE__), may be NULL.
 * @param line     Source line of the call site (from __LINE__).
 * @param message  Optional human-readable detail (e.g. the failed
 *                 expression text from RTE_ASSERT), may be NULL.
 *
 * @note For RTE_SAFESTATE_LEVEL_SAFE and RTE_SAFESTATE_LEVEL_REBOOT this
 *       handler shall not return. If it does anyway,
 *       rte_safestate_enter() falls back to a defensive infinite loop
 *       (REQ-COMMON-SAFESTATE-002) rather than resuming the caller.
 */
typedef void (*rte_safestate_handler_t)(rte_safestate_level_t level,
                                          rte_safestate_reason_t reason,
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
 * @return RTE_STATUS_INVALID_PARAM if handler is NULL or level is not a
 *         valid rte_safestate_level_t value; RTE_STATUS_OK otherwise.
 *         Registering again for the same level replaces the previous handler.
 *
 * REQ-COMMON-SAFESTATE-010
 */
rte_status_t rte_safestate_register_handler(rte_safestate_level_t level,
                                               rte_safestate_handler_t handler);

/**
 * @brief Enters a safe-state level: invokes the registered handler (if
 *        any), then, for RTE_SAFESTATE_LEVEL_SAFE and
 *        RTE_SAFESTATE_LEVEL_REBOOT, guarantees the call never returns
 *        (REQ-COMMON-SAFESTATE-002) even if no handler was registered or
 *        the handler itself returns.
 *
 * @param level    Level to enter.
 * @param reason   Diagnostic reason code.
 * @param file     Call site file (typically __FILE__), may be NULL.
 * @param line     Call site line (typically __LINE__).
 * @param message  Optional human-readable detail, may be NULL.
 *
 * @note Prefer the RTE_ASSERT / RTE_SAFESTATE / RTE_REBOOT macros over
 *       calling this directly, so __FILE__/__LINE__ are captured correctly.
 *
 * REQ-COMMON-SAFESTATE-011
 */
void rte_safestate_enter(rte_safestate_level_t level,
                           rte_safestate_reason_t reason,
                           const char *file,
                           int32_t line,
                           const char *message);

/**
 * @def RTE_ASSERT
 * @brief Checked assertion. Always active in every build configuration,
 *        including production (ADR-004 section 2.5) - unlike standard
 *        assert(), which CLAUDE.md prohibits in production paths. On
 *        failure, enters RTE_SAFESTATE_LEVEL_SAFE with reason
 *        RTE_SAFESTATE_REASON_ASSERT_FAILED.
 */
#define RTE_ASSERT(cond)                                                    \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            rte_safestate_enter(RTE_SAFESTATE_LEVEL_SAFE,                  \
                                  RTE_SAFESTATE_REASON_ASSERT_FAILED,        \
                                  __FILE__, (int32_t)__LINE__, #cond);        \
        }                                                                    \
    } while (0)

/**
 * @def RTE_SAFESTATE
 * @brief Explicitly enters the given safe-state level with a reason code,
 *        capturing the call site automatically.
 */
#define RTE_SAFESTATE(level, reason) \
    rte_safestate_enter((level), (reason), __FILE__, (int32_t)__LINE__, NULL)

/**
 * @def RTE_REBOOT
 * @brief Enters RTE_SAFESTATE_LEVEL_REBOOT with the given reason code.
 *        Equivalent to RTE_SAFESTATE(RTE_SAFESTATE_LEVEL_REBOOT, reason).
 */
#define RTE_REBOOT(reason) \
    rte_safestate_enter(RTE_SAFESTATE_LEVEL_REBOOT, (reason), __FILE__, (int32_t)__LINE__, NULL)

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_SAFESTATE_H */

/** @} */ /* SAFESTATE */

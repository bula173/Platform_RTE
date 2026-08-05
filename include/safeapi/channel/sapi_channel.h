/**
 * @file sapi_channel.h
 * @brief Dual-channel identity and result comparison (ADR-008).
 *
 * Common-cause-failure (CCF) mitigation support for a 2-channel ("2oo2")
 * vital architecture where both channels run the same CPU architecture.
 * This module does not, by itself, make the two channels independent -
 * that independence comes from build diversity (channel A and channel B
 * compiled by two different, independently-configured toolchains; see
 * ADR-008 section 2.1 and cmake/toolchain-channel-a.cmake /
 * cmake/toolchain-channel-b.cmake) plus whatever physical/environmental
 * diversity the target hardware provides. What this module provides is:
 *
 *  - an unambiguous, compile-time-checked answer to "which channel is
 *    this binary?" (sapi_channel_local_id());
 *  - a bounds-checked comparison of the local channel's computed result
 *    against its peer's (sapi_channel_compare());
 *  - the standard fail-safe reaction to disagreement, reusing
 *    sapi_safestate (sapi_channel_compare_and_enter_safestate()).
 *
 * This module has no OS dependency (like sapi_buffer/sapi_cast/
 * sapi_safestate/sapi_string, see ADR-002 section 4): it does not read or
 * transport the peer channel's result itself. The caller is expected to
 * obtain the peer's result bytes via whatever transport is appropriate
 * (e.g. sapi_ipc, ADR-001 section 4 service 5) and pass both results in.
 *
 * With exactly two channels there is no majority to take on disagreement,
 * so this is a comparator, not a voter: the only sound reaction to a
 * mismatch is "neither channel is trusted, enter SAFE" (ADR-008
 * section 2.2). A 2oo2 architecture built on this module is a fault
 * *detector* with a fail-safe reaction, not a fault *masker* with
 * continued operation.
 *
 * REQ-COMMON-CHANNEL-001: exactly one of SAPI_CHANNEL_BUILD_A /
 *                         SAPI_CHANNEL_BUILD_B shall be defined by the
 *                         build for any translation unit including this
 *                         header; a binary that does not know which
 *                         channel it is shall fail to compile.
 * REQ-COMMON-CHANNEL-002: sapi_channel_compare() shall never dereference
 *                         a NULL buffer data pointer when the
 *                         corresponding length is nonzero; a length
 *                         mismatch between the two inputs shall itself be
 *                         reported as SAPI_CHANNEL_COMPARE_MISMATCH, not
 *                         as an error.
 */
#ifndef SAFEAPI_COMMON_CHANNEL_H
#define SAFEAPI_COMMON_CHANNEL_H

#include "safeapi/buffer/sapi_buffer.h"
#include "safeapi/safestate/sapi_safestate.h"
#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#if defined(SAPI_CHANNEL_BUILD_A) && defined(SAPI_CHANNEL_BUILD_B)
#error "sapi_channel.h: define exactly one of SAPI_CHANNEL_BUILD_A / SAPI_CHANNEL_BUILD_B, not both (REQ-COMMON-CHANNEL-001)."
#elif !defined(SAPI_CHANNEL_BUILD_A) && !defined(SAPI_CHANNEL_BUILD_B)
#error "sapi_channel.h: define SAPI_CHANNEL_BUILD_A or SAPI_CHANNEL_BUILD_B for this build (REQ-COMMON-CHANNEL-001). See ADR-008 / cmake/toolchain-channel-{a,b}.cmake."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Which of the two vital channels this binary was built as
 *        (ADR-008 section 2.2). Fixed at compile time, never runtime
 *        state - see SAPI_CHANNEL_BUILD_A / SAPI_CHANNEL_BUILD_B above.
 */
typedef enum sapi_channel_id_e
{
    SAPI_CHANNEL_ID_A = 0, /**< Built with SAPI_CHANNEL_BUILD_A defined. */
    SAPI_CHANNEL_ID_B = 1  /**< Built with SAPI_CHANNEL_BUILD_B defined. */
} sapi_channel_id_t;

/**
 * @brief Outcome of comparing the local channel's result to its peer's.
 */
typedef enum sapi_channel_compare_result_e
{
    /** Local and peer results are byte-for-byte identical (same length,
     *  same content). */
    SAPI_CHANNEL_COMPARE_MATCH = 0,
    /** Local and peer results differ (including a plain length
     *  difference) - treated as a channel disagreement, not an error. */
    SAPI_CHANNEL_COMPARE_MISMATCH = 1
} sapi_channel_compare_result_t;

/**
 * @brief Returns the local channel identity, fixed at compile time by
 *        which of SAPI_CHANNEL_BUILD_A / SAPI_CHANNEL_BUILD_B was defined
 *        for this translation unit's build.
 * @return SAPI_CHANNEL_ID_A or SAPI_CHANNEL_ID_B.
 */
sapi_channel_id_t sapi_channel_local_id(void);

/**
 * @brief Returns a short, static, human-readable string for a channel id.
 *        Diagnostics/logging only; never on a safety-decision path
 *        (mirrors sapi_status_to_string(), ADR-001 section 3.6).
 * @param id  Channel id to describe.
 * @return A non-NULL static string, including for an unrecognized id
 *         (a defensive "unknown" string, never NULL).
 */
const char *sapi_channel_id_to_string(sapi_channel_id_t id);

/**
 * @brief Bounds-checked, byte-for-byte comparison of the local channel's
 *        computed result against the peer channel's result.
 *
 * @param local_result  This channel's computed result. data may be NULL
 *                       only if length is 0.
 * @param peer_result    The peer channel's result (obtained via whatever
 *                       transport the caller uses, e.g. sapi_ipc). data
 *                       may be NULL only if length is 0.
 * @param out_result     Set to SAPI_CHANNEL_COMPARE_MATCH or
 *                       SAPI_CHANNEL_COMPARE_MISMATCH on success. Must
 *                       not be NULL. Left unmodified on error.
 * @return SAPI_STATUS_OK on success (see out_result for the outcome);
 *         SAPI_STATUS_INVALID_PARAM if out_result is NULL, or if either
 *         buffer has a NULL data pointer with a nonzero length.
 *
 * @note A length difference between local_result and peer_result is
 *       itself reported as SAPI_CHANNEL_COMPARE_MISMATCH (REQ-COMMON-
 *       CHANNEL-002), not as SAPI_STATUS_INVALID_PARAM: two channels
 *       that computed different-sized results have disagreed, which is
 *       exactly the condition this function exists to detect.
 *
 * REQ-COMMON-CHANNEL-002
 */
sapi_status_t sapi_channel_compare(sapi_const_buffer_t local_result,
                                    sapi_const_buffer_t peer_result,
                                    sapi_channel_compare_result_t *out_result);

/**
 * @brief Convenience wrapper: compares local_result against peer_result,
 *        and on a mismatch enters SAPI_SAFESTATE_LEVEL_SAFE with
 *        SAPI_SAFESTATE_REASON_CHANNEL_MISMATCH (or reason, if nonzero -
 *        see below), reusing sapi_safestate_enter() (ADR-004).
 *
 * @param local_result  As sapi_channel_compare().
 * @param peer_result    As sapi_channel_compare().
 * @param reason        Reason code to report on mismatch. Pass
 *                       SAPI_SAFESTATE_REASON_UNSPECIFIED to use the
 *                       default SAPI_SAFESTATE_REASON_CHANNEL_MISMATCH.
 * @return SAPI_STATUS_OK if the comparison itself succeeded and the
 *         results matched. SAPI_STATUS_INVALID_PARAM if the comparison
 *         inputs were invalid (see sapi_channel_compare()) - in this
 *         case no safe-state transition is triggered, since the
 *         disagreement could not actually be evaluated.
 * @note On SAPI_CHANNEL_COMPARE_MISMATCH this function does not return
 *       (per REQ-COMMON-SAFESTATE-002): it calls sapi_safestate_enter()
 *       at SAPI_SAFESTATE_LEVEL_SAFE, which never returns to the caller.
 */
sapi_status_t sapi_channel_compare_and_enter_safestate(sapi_const_buffer_t local_result,
                                                         sapi_const_buffer_t peer_result,
                                                         sapi_safestate_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_CHANNEL_H */

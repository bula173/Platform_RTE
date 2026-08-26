/**
 * @file sapi_log.h
 * @brief OS Abstraction Layer - Logging/diagnostics service.
 *
 * NON-SAFETY-RELATED. Black-box/event-recorder style diagnostic logging.
 * This service must never sit on a safety execution path: it must not
 * block callers, must not be able to cause a safety function to miss a
 * deadline, and its failure shall never affect safety behavior. See
 * ADR-001, section 4 (item 6).
 *
 * REQ-OAL-LOG-001: log calls are best-effort and non-blocking; a full
 *                  backend buffer silently drops the newest entries rather
 *                  than blocking or erroring the caller's control flow.
 *
 * @defgroup LOG Logging and Diagnostics
 * @brief Non-safety-related black-box/event-recorder logging (ADR-001)
 * @{
 */
#ifndef SAFEAPI_OS_LOG_H
#define SAFEAPI_OS_LOG_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Log message severity, passed to sapi_log_write(). */
typedef enum sapi_log_level_e
{
    SAPI_LOG_LEVEL_DEBUG   = 0, /**< Verbose diagnostic detail. */
    SAPI_LOG_LEVEL_INFO    = 1, /**< Normal operational events. */
    SAPI_LOG_LEVEL_WARNING = 2, /**< Unexpected but recovered condition. */
    SAPI_LOG_LEVEL_ERROR   = 3  /**< Failure worth operator attention. */
} sapi_log_level_t;

/**
 * @brief Initializes the logging backend. Safe to call once at startup.
 * @return SAPI_STATUS_OK if no backend is registered yet (a no-op logger
 *         is a valid state) or if the registered backend's init succeeds;
 *         a backend-defined error status otherwise.
 * REQ-OAL-LOG-010
 */
sapi_status_t sapi_log_init(void);

/**
 * @brief Emits one log message. Non-blocking; never fails the caller's
 *        control flow even if the message is dropped.
 * @param level    Severity level.
 * @param tag      Short diagnostic source tag, backend-defined interpretation, may be NULL.
 * @param message  Human-readable message text, may be NULL.
 * REQ-OAL-LOG-011
 */
void sapi_log_write(sapi_log_level_t level, const char *tag, const char *message);

/**
 * @brief Renders a sapi_log_level_t as a fixed, human-readable tag -
 *        "DEBUG"/"INFO"/"WARNING"/"ERROR" - used as the LEVEL field of
 *        sapi_log_write_event()'s structured line and available to any
 *        backend/integrator wanting the same canonical spelling.
 * @param level  Level to render; an unrecognized value (defensive only -
 *               not reachable through the public enum) renders as
 *               "UNKNOWN".
 * @return A NUL-terminated string literal - static storage, never NULL;
 *         caller must not modify or free it.
 * REQ-OAL-LOG-013
 */
const char *sapi_log_level_to_string(sapi_log_level_t level);

/** @brief Max length, in bytes and not counting the NUL terminator, of
 *         one sapi_log_write_event() line - fields are truncated, not
 *         rejected, if the formatted line would exceed this (REQ-OAL-LOG-001:
 *         a formatting limit must never turn into a blocked/failed
 *         caller). Sized generously for the 8 mandatory fields plus a
 *         typical extra_fields value; a longer extra_fields is where
 *         truncation would first show up in practice. */
#define SAPI_LOG_EVENT_LINE_MAX_LEN 256U

/**
 * @brief Emits one structured inter-channel event/message-trail log
 *        line - for a message actually sent/received/decided between
 *        channels or nodes (e.g. an AB_SAMPLE frame, an M136 report, a
 *        checkpoint REQUEST/REPLY, a cross-compare AGREE/DISAGREE) -
 *        distinct from sapi_log_write()'s free-text diagnostic logging,
 *        which remains the right call for an event with no natural
 *        cycle/source/destination (e.g. an NVM write failure).
 *
 * Emits, via the same backend as sapi_log_write() (non-blocking,
 * best-effort, same REQ-OAL-LOG-001 contract - a full backend buffer or
 * an unregistered backend silently drops this call, never blocks or
 * errors the caller):
 *
 * @code
 * Site=<site> Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src> Destination=<dst> Type=<type> Info=<info>[ <extra_fields>]
 * @endcode
 *
 * i.e. space-separated `Key=Value` pairs, one per mandatory field, in
 * that fixed order, as the `message` argument of the registered
 * sapi_log_backend_t::write(), with `source` passed as that call's `tag`
 * argument - an existing backend needs no changes to receive structured
 * events (e.g. the shipped POSIX backend still prefixes its own
 * "[LEVEL] tag: " for terminal readability; a consumer parsing the
 * *structured* `Key=Value` fields should parse from the `message` value
 * itself, not whatever cosmetic wrapping a specific backend adds around
 * it).
 *
 * Timestamp is sourced internally via sapi_timer_now() (milliseconds);
 * "0" is emitted if no sapi_timer backend is registered or the call
 * otherwise fails - REQ-OAL-LOG-001 means a timer problem must never
 * prevent this call from returning, so a timestamp failure degrades the
 * Timestamp field rather than skipping the whole event.
 *
 * @param level         Severity.
 * @param site          Caller-supplied node/site/instance identifier
 *                       (e.g. "WEST", "EAST") - the framework has no
 *                       concept of a "site" itself; this is opaque,
 *                       caller-defined text, first in the emitted line so
 *                       a log consumer merging output from multiple
 *                       redundant instances can always tell them apart
 *                       at a glance. Must not be NULL; pass "" if this
 *                       integration has no such concept.
 * @param cycle         Cycle/iteration counter this event belongs to
 *                       (e.g. sapi_appmanager_state_t::iteration_count).
 *                       Use 0 if this event has no natural cycle.
 * @param source        Short identifier of the event's origin (e.g.
 *                       "A/WEST"). Must not be NULL; pass "" if unknown.
 * @param destination   Short identifier of the event's target (e.g. "B",
 *                       "C", "-" for no specific destination). Must not
 *                       be NULL.
 * @param type          Short event/message-type tag (e.g. "AB_SAMPLE",
 *                       "M136", "CHECKPOINT_REQUEST", "AGREE"). Must not
 *                       be NULL.
 * @param info          Free-text human-readable detail. May be NULL (an
 *                       empty `Info=` value is emitted).
 * @param extra_fields  Optional, caller-preformatted additional
 *                       `Key=Value` text (e.g. "D_LRBG=42 CRC=OK"),
 *                       appended verbatim after `Info=...` behind one
 *                       separating space - not wrapped in another key,
 *                       so it reads as more of the same space-separated
 *                       `Key=Value` convention. May be NULL (omitted
 *                       entirely - no trailing space is emitted either).
 *
 * @note Fixed arity, no variadic/`<stdarg.h>` (MISRA C:2012 Rule 17.1) -
 *       the single `extra_fields` parameter is how a caller adds more
 *       than the eight mandatory fields; build it first with
 *       `safeapi::string`'s bounded helpers (`sapi_string_concat()` etc.),
 *       the same primitives this function uses internally.
 * @note Every field is truncated, not rejected, if the internal
 *       fixed-size line buffer (SAPI_LOG_EVENT_LINE_MAX_LEN) is too
 *       small for the full line - consistent with REQ-OAL-LOG-001's
 *       "never affect caller control flow"; there is no error return
 *       (this function returns void, like sapi_log_write()).
 * REQ-OAL-LOG-014
 */
void sapi_log_write_event(sapi_log_level_t level,
                           const char *site,
                           uint32_t cycle,
                           const char *source,
                           const char *destination,
                           const char *type,
                           const char *info,
                           const char *extra_fields);

/*
 * The backend vtable (sapi_log_backend_t) and sapi_log_register_backend()
 * live in safeapi_backend/log/sapi_log_backend.h, not here (ADR-021).
 * This header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_LOG_H */

/** @} */ /* LOG */

/**
 * @file rte_log.h
 * @brief OS Abstraction Layer - Logging/diagnostics service.
 *
 * NON-SAFETY-RELATED. Black-box/event-recorder style diagnostic logging.
 * This service must never sit on a safety execution path: it must not
 * block callers, must not be able to cause a safety function to miss a
 * deadline, and its failure shall never affect safety behavior. See
 * ADR-001, section 4 (item 6).
 *
 * REQ-OAL-LOG-001: log calls are best-effort and non-blocking; a full
 *                  OSAdapter buffer silently drops the newest entries rather
 *                  than blocking or erroring the caller's control flow.
 *
 * @defgroup LOG Logging and Diagnostics
 * @brief Non-safety-related black-box/event-recorder logging (ADR-001)
 * @{
 */
#ifndef SAFEAPI_OS_LOG_H
#define SAFEAPI_OS_LOG_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"
#include "safeapi/utils/string/rte_string.h" /* rte_log_fields_t backing store */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Log message severity, passed to rte_log_write(). */
typedef enum rte_log_level_e
{
    RTE_LOG_LEVEL_DEBUG   = 0, /**< Verbose diagnostic detail. */
    RTE_LOG_LEVEL_INFO    = 1, /**< Normal operational events. */
    RTE_LOG_LEVEL_WARNING = 2, /**< Unexpected but recovered condition. */
    RTE_LOG_LEVEL_ERROR   = 3  /**< Failure worth operator attention. */
} rte_log_level_t;

/**
 * @brief Initializes the logging OSAdapter. Safe to call once at startup.
 * @return RTE_STATUS_OK if no OSAdapter is registered yet (a no-op logger
 *         is a valid state) or if the registered OSAdapter's init succeeds;
 *         a osadapter-defined error status otherwise.
 * REQ-OAL-LOG-010
 */
rte_status_t rte_log_init(void);

/**
 * @brief Emits one log message. Non-blocking; never fails the caller's
 *        control flow even if the message is dropped.
 * @param level    Severity level.
 * @param tag      Short diagnostic source tag, osadapter-defined interpretation, may be NULL.
 * @param message  Human-readable message text, may be NULL.
 * REQ-OAL-LOG-011
 */
void rte_log_write(rte_log_level_t level, const char *tag, const char *message);

/**
 * @brief Renders a rte_log_level_t as a fixed, human-readable tag -
 *        "DEBUG"/"INFO"/"WARNING"/"ERROR" - used as the LEVEL field of
 *        rte_log_write_event()'s structured line and available to any
 *        OSAdapter/integrator wanting the same canonical spelling.
 * @param level  Level to render; an unrecognized value (defensive only -
 *               not reachable through the public enum) renders as
 *               "UNKNOWN".
 * @return A NUL-terminated string literal - static storage, never NULL;
 *         caller must not modify or free it.
 * REQ-OAL-LOG-013
 */
const char *rte_log_level_to_string(rte_log_level_t level);

/**
 * @brief Parses one of the canonical level spellings ("DEBUG", "INFO",
 *        "WARNING", "ERROR" - exactly as rte_log_level_to_string()
 *        renders them, case-sensitive) into a rte_log_level_t.
 * @param name       NUL-terminated candidate spelling. Must not be NULL.
 * @param out_level  Receives the parsed level on success. Must not be NULL
 *                   (left unchanged on failure).
 * @return RTE_STATUS_OK on a match; RTE_STATUS_INVALID_PARAM if name or
 *         out_level is NULL, or name matches none of the four spellings.
 * REQ-OAL-LOG-016
 */
rte_status_t rte_log_level_from_string(const char *name, rte_log_level_t *out_level);

/**
 * @brief Sets the minimum severity that rte_log_write() and
 *        rte_log_write_event() forward to the OSAdapter - a call whose
 *        level is below min_level is dropped before dispatch (and before
 *        any formatting work). Default is RTE_LOG_LEVEL_DEBUG: nothing is
 *        filtered until this is called.
 *
 * Runtime-settable so an integrator can quiet or open up logging without a
 * rebuild (e.g. a remote "set log level" command). NOT a setup-only action
 * (unlike rte_osadapter_log_register()) - may be called at any time, from
 * any thread: the threshold is one int-sized value, so a concurrent change
 * racing an in-flight rte_log_write() only ever means that one call sees
 * the old or the new threshold, never a torn value, and at worst one
 * best-effort log line is kept or dropped unexpectedly (REQ-OAL-LOG-001).
 *
 * @param min_level  Lowest level to keep. A value outside
 *                   RTE_LOG_LEVEL_DEBUG..RTE_LOG_LEVEL_ERROR is ignored
 *                   (the current threshold is left unchanged).
 * REQ-OAL-LOG-015
 */
void rte_log_set_level(rte_log_level_t min_level);

/**
 * @brief Returns the current minimum severity (see rte_log_set_level()).
 * REQ-OAL-LOG-015
 */
rte_log_level_t rte_log_get_level(void);

/** @brief Max length, in bytes and not counting the NUL terminator, of
 *         one rte_log_write_event() line - fields are truncated, not
 *         rejected, if the formatted line would exceed this (REQ-OAL-LOG-001:
 *         a formatting limit must never turn into a blocked/failed
 *         caller). Sized generously for the 8 mandatory fields plus a
 *         typical extra_fields value; a longer extra_fields is where
 *         truncation would first show up in practice. */
#define RTE_LOG_EVENT_LINE_MAX_LEN 256U

/**
 * @brief Emits one structured inter-channel event/message-trail log
 *        line - for a message actually sent/received/decided between
 *        channels or nodes (e.g. an AB_SAMPLE frame, an M136 report, a
 *        checkpoint REQUEST/REPLY, a cross-compare AGREE/DISAGREE) -
 *        distinct from rte_log_write()'s free-text diagnostic logging,
 *        which remains the right call for an event with no natural
 *        cycle/source/destination (e.g. an NVM write failure).
 *
 * Emits, via the same OSAdapter as rte_log_write() (non-blocking,
 * best-effort, same REQ-OAL-LOG-001 contract - a full OSAdapter buffer or
 * an unregistered OSAdapter silently drops this call, never blocks or
 * errors the caller):
 *
 * @code
 * Site=<site> Timestamp=<ms> Level=<LEVEL> Cycle=<n> Source=<src> Destination=<dst> Type=<type> Info=<info>[ <extra_fields>]
 * @endcode
 *
 * i.e. space-separated `Key=Value` pairs, one per mandatory field, in
 * that fixed order, as the `message` argument of the registered
 * rte_osadapter_log_t::write(), with `source` passed as that call's `tag`
 * argument - an existing OSAdapter needs no changes to receive structured
 * events (e.g. the shipped POSIX OSAdapter still prefixes its own
 * "[LEVEL] tag: " for terminal readability; a consumer parsing the
 * *structured* `Key=Value` fields should parse from the `message` value
 * itself, not whatever cosmetic wrapping a specific OSAdapter adds around
 * it).
 *
 * Timestamp is sourced internally via rte_timer_now() (milliseconds);
 * "0" is emitted if no rte_timer OSAdapter is registered or the call
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
 *                       (e.g. rte_appmanager_state_t::iteration_count).
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
 *       `safeapi::string`'s bounded helpers (`rte_string_concat()` etc.),
 *       the same primitives this function uses internally.
 * @note Every field is truncated, not rejected, if the internal
 *       fixed-size line buffer (RTE_LOG_EVENT_LINE_MAX_LEN) is too
 *       small for the full line - consistent with REQ-OAL-LOG-001's
 *       "never affect caller control flow"; there is no error return
 *       (this function returns void, like rte_log_write()).
 * REQ-OAL-LOG-014
 */
void rte_log_write_event(rte_log_level_t level,
                           const char *site,
                           uint32_t cycle,
                           const char *source,
                           const char *destination,
                           const char *type,
                           const char *info,
                           const char *extra_fields);

/**
 * @brief Bounded builder for `rte_log_write_event()`'s `extra_fields`
 *        (or `info`) argument: accumulates space-separated `key=value`
 *        pairs, each typed at the call site, so a caller adding a couple
 *        of variable values does not hand-roll a
 *        `rte_string_concat()`/`rte_string_append_u32()` chain every
 *        time (MISRA C:2012 Rule 17.1 rules out a `printf`-style variadic
 *        here).
 *
 * All storage is inside the struct - no allocation. Over-long content is
 * truncated, never rejected, matching `rte_log_write_event()`'s own
 * REQ-OAL-LOG-001 posture. Typical use:
 * @code
 * rte_log_fields_t f;
 * rte_log_fields_reset(&f);
 * rte_log_fields_add_u32(&f, "route", route_id);
 * rte_log_fields_add_i32(&f, "d_lrbg", d_lrbg);
 * rte_log_fields_add_str(&f, "result", "OK");
 * rte_log_write_event_fields(RTE_LOG_LEVEL_INFO, "WEST", cycle,
 *                             "A/WEST", "IL", "ROUTE", "route connected", &f);
 * @endcode
 */
typedef struct rte_log_fields_s
{
    rte_string_t str;                         /**< Bound to @ref storage by rte_log_fields_reset(). */
    char          storage[RTE_LOG_EVENT_LINE_MAX_LEN]; /**< Backing bytes - do not touch directly. */
} rte_log_fields_t;

/**
 * @brief (Re)initialises a builder to empty. MUST be called before the
 *        first `rte_log_fields_add_*()`; safe to call again to reuse the
 *        same builder for another line.
 * @param fields  Builder to reset. A NULL @p fields is a silent no-op.
 * REQ-OAL-LOG-017
 */
void rte_log_fields_reset(rte_log_fields_t *fields);

/**
 * @brief Appends one `key=value` pair (preceded by a single separating
 *        space only when the builder is already non-empty).
 * @param fields  Builder (must have been `rte_log_fields_reset()`).
 * @param key     Field key/label, e.g. `"route"`. Must not be NULL.
 * @param value   Value: for `_add_str` a NULL string is written as an
 *                empty value; for `_add_hex_u32`, @p min_digits is the
 *                minimum digit count (zero-padded), prefixed with `0x`;
 *                for `_add_bool`, `"true"`/`"false"` is written.
 * @return @p fields, so a few calls can be chained inline.
 * @note Best-effort: a NULL @p fields / NULL @p key, or a full backing
 *       store, silently leaves the builder unchanged (REQ-OAL-LOG-001).
 * REQ-OAL-LOG-017
 * @{
 */
rte_log_fields_t *rte_log_fields_add_str(rte_log_fields_t *fields, const char *key, const char *value);
rte_log_fields_t *rte_log_fields_add_u32(rte_log_fields_t *fields, const char *key, uint32_t value);
rte_log_fields_t *rte_log_fields_add_i32(rte_log_fields_t *fields, const char *key, int32_t value);
rte_log_fields_t *rte_log_fields_add_u64(rte_log_fields_t *fields, const char *key, uint64_t value);
rte_log_fields_t *rte_log_fields_add_i64(rte_log_fields_t *fields, const char *key, int64_t value);
rte_log_fields_t *rte_log_fields_add_hex_u32(rte_log_fields_t *fields, const char *key,
                                               uint32_t value, uint8_t min_digits);
rte_log_fields_t *rte_log_fields_add_bool(rte_log_fields_t *fields, const char *key, bool value);
/** @} */

/**
 * @brief NUL-terminated view of the accumulated pairs.
 * @param fields  Builder. NULL / empty yields `""`.
 * @return A pointer into @p fields->storage, valid until the next
 *         `rte_log_fields_reset()`/`_add_*()` on the same builder; never
 *         NULL. Pass it straight as `rte_log_write_event()`'s
 *         @p extra_fields (or @p info).
 * REQ-OAL-LOG-017
 */
const char *rte_log_fields_c_str(rte_log_fields_t *fields);

/**
 * @brief `rte_log_write_event()` with a builder passed directly as the
 *        extra fields - no `rte_log_fields_c_str()` at the call site.
 * @param fields  Extra `key=value` pairs; NULL or an empty builder emits
 *                no extra fields (identical to passing NULL to
 *                `rte_log_write_event()`). Every other parameter is
 *                exactly as `rte_log_write_event()`.
 * REQ-OAL-LOG-017
 */
void rte_log_write_event_fields(rte_log_level_t level,
                                  const char *site,
                                  uint32_t cycle,
                                  const char *source,
                                  const char *destination,
                                  const char *type,
                                  const char *info,
                                  rte_log_fields_t *fields);

/*
 * The OSAdapter vtable (rte_osadapter_log_t) and rte_osadapter_log_register()
 * live in safeapi_osadapter/log/rte_osadapter_log.h, not here (ADR-021).
 * This header is the consumer-facing surface only.
 */

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OS_LOG_H */

/** @} */ /* LOG */

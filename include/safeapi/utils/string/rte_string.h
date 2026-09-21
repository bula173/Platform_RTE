/**
 * @file rte_string.h
 * @brief Bounded, checked string manipulation (ADR-006). Replaces
 *        strcpy/strcat/sprintf/atoi/strtok-style unbounded operations
 *        with checked equivalents built on rte_buffer_t.
 *
 * rte_string_t wraps a rte_buffer_t; buf.length tracks the string's
 * length NOT counting a NUL terminator. A NUL is only materialized on
 * demand by rte_string_c_str() (ADR-006 section 2.1) - callers that hand
 * a rte_string_t's storage directly to a legacy API without calling
 * rte_string_c_str() first will not find a NUL terminator there.
 *
 * REQ-COMMON-STR-001: no dynamic allocation; the caller owns the backing
 *                     storage for the lifetime of the string.
 * REQ-COMMON-STR-002: rte_string_copy() shall never call strlen() on its
 *                     source; it scans for a NUL only up to the
 *                     destination's capacity and fails rather than
 *                     reading past it.
 * REQ-COMMON-STR-003: content is treated as raw bytes/ASCII; no
 *                     multi-byte/UTF-8-aware operations are provided.
 *
 * @defgroup STRING Bounded String Manipulation
 * @brief Checked replacements for strcpy/strcat/sprintf-style operations (ADR-006)
 * @{
 */
#ifndef SAFEAPI_COMMON_STRING_H
#define SAFEAPI_COMMON_STRING_H

#include "safeapi/utils/buffer/rte_buffer.h"
#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Bounded string: buf.length is the string length, not counting a NUL. */
typedef struct rte_string_s
{
    rte_buffer_t buf; /**< Backing bounds-checked byte view; length excludes any NUL. */
} rte_string_t;

/**
 * @brief Binds a string to caller-owned storage. Initial length is 0 (empty string).
 * @param str       String to initialize. Must not be NULL.
 * @param storage   Caller-owned backing storage. Must not be NULL and must outlive str.
 * @param capacity  Usable size of storage in bytes. Must be > 0.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument.
 * REQ-COMMON-STR-010
 */
rte_status_t rte_string_init(rte_string_t *str, char *storage, size_t capacity);

/**
 * @brief Resets a string to empty. Capacity and storage are unchanged.
 * @param str  String to clear. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if str is NULL.
 * REQ-COMMON-STR-011
 */
rte_status_t rte_string_clear(rte_string_t *str);

/**
 * @brief Returns a string's current length (not counting a NUL terminator).
 * @param str  String to query; NULL is a valid input (returns 0).
 * @return Current length in bytes, or 0 if str is NULL/invalid.
 * REQ-COMMON-STR-012
 */
size_t rte_string_length(const rte_string_t *str);

/**
 * @brief Ensures a NUL terminator is present within capacity (without
 *        incrementing length) and returns a pointer to the string's
 *        storage, suitable for passing to a legacy NUL-terminated-string API.
 * @param str        String to terminate. Must not be NULL.
 * @param out_cstr   Receives the NUL-terminated pointer. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_RESOURCE_EXHAUSTED if str has no spare byte of
 *         capacity for the terminator (length == capacity).
 * REQ-COMMON-STR-013
 */
rte_status_t rte_string_c_str(rte_string_t *str, const char **out_cstr);

/**
 * @brief Bounded strcpy equivalent. Never calls strlen(src) - scans for a
 *        NUL only up to dest's capacity (REQ-COMMON-STR-002).
 * @param dest  Destination string. Must not be NULL. Replaces dest's
 *              previous content on success; left unmodified on failure.
 * @param src   NUL-terminated source. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument;
 *         RTE_STATUS_RESOURCE_EXHAUSTED if no NUL is found within
 *         dest's capacity.
 * REQ-COMMON-STR-014
 */
rte_status_t rte_string_copy(rte_string_t *dest, const char *src);

/**
 * @brief Bounded copy of an exact-length, not-necessarily-NUL-terminated
 *        source (e.g. a length-prefixed field). Never scans src.
 * @param dest     Destination string. Must not be NULL. Replaces dest's
 *                 previous content on success; left unmodified on failure.
 * @param src      Source bytes. Must not be NULL.
 * @param src_len  Number of bytes to copy from src.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_RESOURCE_EXHAUSTED
 *         if src_len exceeds dest's capacity.
 * REQ-COMMON-STR-015
 */
rte_status_t rte_string_copy_n(rte_string_t *dest, const char *src, size_t src_len);

/**
 * @brief Bounded strcat equivalent. Never calls strlen(src) - scans for a
 *        NUL only up to dest's remaining capacity.
 * @param dest  Destination string. Must not be NULL. Appended to on
 *              success; left unmodified on failure.
 * @param src   NUL-terminated source. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM; RTE_STATUS_RESOURCE_EXHAUSTED
 *         if no NUL is found within dest's remaining capacity.
 * REQ-COMMON-STR-016
 */
rte_status_t rte_string_concat(rte_string_t *dest, const char *src);

/**
 * @brief Bounded strcmp equivalent. Compares up to the shorter string's
 *        length, then by length if that prefix is equal.
 * @param a        First string. Must not be NULL.
 * @param b        Second string. Must not be NULL.
 * @param out_cmp  Receives <0, 0, or >0 (like strcmp). Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a bad argument.
 * REQ-COMMON-STR-017
 */
rte_status_t rte_string_compare(const rte_string_t *a, const rte_string_t *b, int32_t *out_cmp);

/**
 * @brief Bounded strchr equivalent. "Not found" is a normal outcome, not
 *        an error - see out_found.
 * @param str        String to search. Must not be NULL.
 * @param c          Character to find.
 * @param out_found  Receives whether c was found. Must not be NULL.
 * @param out_index  Receives the index of the first occurrence if found;
 *                    unmodified if not found. Must not be NULL.
 * @return RTE_STATUS_OK on a completed search (regardless of whether c
 *         was found); RTE_STATUS_INVALID_PARAM for a bad argument.
 * REQ-COMMON-STR-018
 */
rte_status_t rte_string_find_char(const rte_string_t *str, char c,
                                     bool *out_found, size_t *out_index);

/**
 * @brief Bounded strstr equivalent. "Not found" is a normal outcome, not
 *        an error - see out_found.
 * @param haystack   String to search within. Must not be NULL.
 * @param needle     Substring to find. Must not be NULL. An empty needle
 *                    (length 0) is always found at index 0.
 * @param out_found  Receives whether needle was found. Must not be NULL.
 * @param out_index  Receives the index of the first occurrence if found;
 *                    unmodified if not found. Must not be NULL.
 * @return RTE_STATUS_OK on a completed search; RTE_STATUS_INVALID_PARAM
 *         for a bad argument.
 * REQ-COMMON-STR-019
 */
rte_status_t rte_string_find_substr(const rte_string_t *haystack, const rte_string_t *needle,
                                       bool *out_found, size_t *out_index);

/**
 * @brief Reentrant, bounded string splitting - unlike strtok(), all state
 *        is caller-owned via io_cursor, so multiple splits can run
 *        concurrently on different tasks (ADR-006 section 2.4).
 * @param str            String to split. Must not be NULL.
 * @param delimiter      Delimiter character.
 * @param io_cursor      Caller-owned cursor; caller initializes to 0
 *                        before the first call. Updated on each call.
 *                        Must not be NULL.
 * @param out_token       Receives a zero-copy view of the next token
 *                        (valid only as long as str's storage is), when
 *                        out_has_token is true. Must not be NULL.
 * @param out_has_token   Receives whether a token was produced (false
 *                        once the cursor has consumed the whole string).
 *                        Must not be NULL.
 * @return RTE_STATUS_OK on a completed call (regardless of out_has_token);
 *         RTE_STATUS_INVALID_PARAM for a bad argument, including
 *         *io_cursor > str's length.
 * REQ-COMMON-STR-020
 */
rte_status_t rte_string_split_next(const rte_string_t *str, char delimiter,
                                      size_t *io_cursor,
                                      rte_const_buffer_t *out_token,
                                      bool *out_has_token);

/**
 * @brief Bounded base-10 itoa equivalent for uint32_t. Replaces dest's content.
 * @param dest   Destination string. Must not be NULL.
 * @param value  Value to format.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if dest is
 *         NULL; RTE_STATUS_RESOURCE_EXHAUSTED if the formatted digits do
 *         not fit dest's capacity.
 * REQ-COMMON-STR-021
 */
rte_status_t rte_string_from_u32(rte_string_t *dest, uint32_t value);
/**
 * @brief Bounded base-10 itoa equivalent for int32_t. Replaces dest's content.
 * @param dest   Destination string. Must not be NULL.
 * @param value  Value to format; a leading '-' is emitted for negative values.
 * @return Same status contract as rte_string_from_u32().
 * REQ-COMMON-STR-022
 */
rte_status_t rte_string_from_i32(rte_string_t *dest, int32_t value);
/**
 * @brief Bounded base-10 itoa equivalent for uint64_t. Replaces dest's content.
 * @param dest   Destination string. Must not be NULL.
 * @param value  Value to format.
 * @return Same status contract as rte_string_from_u32().
 * REQ-COMMON-STR-023
 */
rte_status_t rte_string_from_u64(rte_string_t *dest, uint64_t value);
/**
 * @brief Bounded base-10 itoa equivalent for int64_t. Replaces dest's content.
 * @param dest   Destination string. Must not be NULL.
 * @param value  Value to format; a leading '-' is emitted for negative values.
 * @return Same status contract as rte_string_from_u32().
 * REQ-COMMON-STR-024
 */
rte_status_t rte_string_from_i64(rte_string_t *dest, int64_t value);

/**
 * @brief Bounded base-10 append of a uint32_t to dest's existing content
 *        (unlike rte_string_from_u32(), which replaces it). Intended to
 *        replace `snprintf(buf, n, "...%u...", v)`-style line assembly with
 *        a checked, non-variadic, MISRA-clean primitive (ADR-006).
 * @param dest   Destination string. Must not be NULL. Appended to on
 *               success; left unmodified on failure.
 * @param value  Value to format and append.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if dest is NULL or its
 *         backing buffer is invalid; RTE_STATUS_RESOURCE_EXHAUSTED if the
 *         formatted digits do not fit dest's remaining capacity.
 * REQ-COMMON-STR-029
 */
rte_status_t rte_string_append_u32(rte_string_t *dest, uint32_t value);
/**
 * @brief Bounded base-10 append of an int32_t; a leading '-' is emitted for
 *        negative values. See rte_string_append_u32().
 * @param dest   Destination string. Must not be NULL. Appended to on
 *               success; left unmodified on failure.
 * @param value  Value to format and append.
 * @return Same status contract as rte_string_append_u32().
 * REQ-COMMON-STR-030
 */
rte_status_t rte_string_append_i32(rte_string_t *dest, int32_t value);
/**
 * @brief Bounded base-10 append of a uint64_t. See rte_string_append_u32().
 * @param dest   Destination string. Must not be NULL. Appended to on
 *               success; left unmodified on failure.
 * @param value  Value to format and append.
 * @return Same status contract as rte_string_append_u32().
 * REQ-COMMON-STR-031
 */
rte_status_t rte_string_append_u64(rte_string_t *dest, uint64_t value);
/**
 * @brief Bounded base-10 append of an int64_t; a leading '-' is emitted for
 *        negative values. See rte_string_append_u32().
 * @param dest   Destination string. Must not be NULL. Appended to on
 *               success; left unmodified on failure.
 * @param value  Value to format and append.
 * @return Same status contract as rte_string_append_u32().
 * REQ-COMMON-STR-032
 */
rte_status_t rte_string_append_i64(rte_string_t *dest, int64_t value);
/**
 * @brief Bounded append of a uint32_t formatted as lowercase hexadecimal
 *        (no "0x" prefix - the caller prepends a literal with
 *        rte_string_concat() if wanted). Replaces `snprintf(..., "%02x",
 *        v)` / `"0x%x"`-style formatting.
 * @param dest        Destination string. Must not be NULL. Appended to on
 *                    success; left unmodified on failure.
 * @param value       Value to format and append.
 * @param min_digits  Minimum digit count, zero-padded on the left; clamped
 *                    to the range 1..8 (0 is treated as 1, values > 8 as 8).
 *                    A value needing more than min_digits digits is emitted
 *                    in full, never truncated.
 * @return Same status contract as rte_string_append_u32().
 * REQ-COMMON-STR-033
 */
rte_status_t rte_string_append_hex_u32(rte_string_t *dest, uint32_t value, uint8_t min_digits);

/**
 * @brief Bounded base-10 atoi equivalent for uint32_t.
 * @param str        Source string. Must not be NULL and must be entirely
 *                    composed of ASCII digits (no sign, no whitespace).
 * @param out_value  Receives the parsed value. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM if str is empty or
 *         contains a non-digit character; RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if the value does not fit uint32_t.
 * REQ-COMMON-STR-025
 */
rte_status_t rte_string_to_u32(const rte_string_t *str, uint32_t *out_value);
/**
 * @brief As rte_string_to_u32(), for int32_t; a leading '-' is accepted.
 * @param str        Source string. Must not be NULL; digits with an
 *                    optional leading '-', no whitespace.
 * @param out_value  Receives the parsed value. Must not be NULL.
 * @return Same status contract as rte_string_to_u32().
 * REQ-COMMON-STR-026
 */
rte_status_t rte_string_to_i32(const rte_string_t *str, int32_t *out_value);
/**
 * @brief As rte_string_to_u32(), for uint64_t.
 * @param str        Source string. Must not be NULL and must be entirely
 *                    composed of ASCII digits (no sign, no whitespace).
 * @param out_value  Receives the parsed value. Must not be NULL.
 * @return Same status contract as rte_string_to_u32().
 * REQ-COMMON-STR-027
 */
rte_status_t rte_string_to_u64(const rte_string_t *str, uint64_t *out_value);
/**
 * @brief As rte_string_to_i32(), for int64_t.
 * @param str        Source string. Must not be NULL; digits with an
 *                    optional leading '-', no whitespace.
 * @param out_value  Receives the parsed value. Must not be NULL.
 * @return Same status contract as rte_string_to_u32().
 * REQ-COMMON-STR-028
 */
rte_status_t rte_string_to_i64(const rte_string_t *str, int64_t *out_value);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_STRING_H */

/** @} */ /* STRING */

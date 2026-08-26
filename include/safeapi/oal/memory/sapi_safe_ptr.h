/**
 * @file sapi_safe_ptr.h
 * @brief Safe-pointer wrapper: bounds + NULL + corruption-canary checked
 *        access to a raw memory region, so callers never perform raw
 *        pointer arithmetic on a buffer directly.
 *
 * Wraps a `{pointer, size}` pair plus a canary written at init time and
 * verified on every access - defense-in-depth against a caller passing an
 * uninitialized or corrupted wrapper (e.g. a bit-flip in a statically
 * allocated struct), the same posture sapi_checksum.c's own init-state
 * check already uses for its internal manager state. No dynamic
 * allocation: sapi_safe_ptr_t wraps memory the caller already owns (a
 * static buffer, a pool block from sapi_memory.h, a stack buffer whose
 * lifetime outlives the wrapper's use), it never allocates anything
 * itself.
 *
 * REQ-OAL-SAFEPTR-001: every access function shall verify the canary
 *                      first; a corrupted wrapper yields
 *                      SAPI_STATUS_DATA_CORRUPTION before any other check
 *                      runs.
 * REQ-OAL-SAFEPTR-002: sapi_safe_ptr_offset() shall verify
 *                      offset + length <= size before computing the
 *                      resulting pointer - restricted pointer arithmetic,
 *                      never raw `ptr + offset` at the call site (MISRA
 *                      C:2012 rules 18.1/18.4).
 * REQ-OAL-SAFEPTR-003: sapi_safe_ptr_invalidate() shall clear both the
 *                      wrapped pointer and the canary, so a subsequent
 *                      access on the same wrapper fails
 *                      SAPI_STATUS_DATA_CORRUPTION rather than silently
 *                      succeeding against a logically-freed region.
 *
 * @defgroup SAFEPTR Safe Pointer Wrapper
 * @brief Bounds + NULL + corruption-canary checked pointer access
 * @{
 */
#ifndef SAFEAPI_OAL_SAFE_PTR_H
#define SAFEAPI_OAL_SAFE_PTR_H

#include "safeapi/utils/status/sapi_status.h"
#include "safeapi/utils/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A bounds-checked, canary-protected wrapper around one raw memory
 *        region. Opaque to callers in spirit (every field is manipulated
 *        only through the functions below) but a plain struct, not a
 *        handle, so it can be embedded directly in a caller's own
 *        statically-allocated storage - no dynamic allocation, matching
 *        CLAUDE.md.
 */
typedef struct
{
    void    *ptr;    /**< Wrapped pointer; NULL once invalidated. */
    size_t   size;    /**< Size in bytes of the region ptr points to. */
    uint32_t canary;  /**< Corruption check, written by init, cleared by invalidate. */
} sapi_safe_ptr_t;

/**
 * @brief Initializes sp to wrap [ptr, ptr + size), writing the canary.
 *
 * @param sp   Wrapper to initialize. Must not be NULL.
 * @param ptr  Region to wrap. May be NULL only if size is 0 (an
 *             intentionally empty wrapper - every access function then
 *             fails SAPI_STATUS_INVALID_PARAM, not SAPI_STATUS_DATA_CORRUPTION,
 *             since the canary is still written correctly).
 * @param size Size in bytes of the region ptr points to.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if sp is
 *         NULL, or ptr is NULL while size is non-zero.
 */
sapi_status_t sapi_safe_ptr_init(sapi_safe_ptr_t *sp, void *ptr, size_t size);

/**
 * @brief Checks whether sp currently wraps a valid, non-invalidated region.
 *
 * @param sp Wrapper to check. Must not be NULL.
 * @return true if sp's canary is intact and its wrapped pointer is
 *         non-NULL; false otherwise (including sp itself being NULL).
 */
bool sapi_safe_ptr_is_valid(const sapi_safe_ptr_t *sp);

/**
 * @brief Retrieves the wrapped pointer directly (the whole region, no
 *        offset) after validating it.
 *
 * @param sp       Wrapper to read. Must not be NULL.
 * @param out_ptr  Receives sp's wrapped pointer on success; left
 *                 untouched on failure. Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if sp or
 *         out_ptr is NULL; SAPI_STATUS_DATA_CORRUPTION if sp's canary is
 *         wrong (REQ-OAL-SAFEPTR-001).
 */
sapi_status_t sapi_safe_ptr_get(const sapi_safe_ptr_t *sp, void **out_ptr);

/**
 * @brief Computes a bounds-checked pointer at offset within sp's region,
 *        verifying the caller's intended access of length bytes starting
 *        at offset stays within the wrapped region.
 *
 * @param sp       Wrapper to read. Must not be NULL.
 * @param offset   Byte offset from the start of sp's region.
 * @param length   Number of bytes the caller intends to access starting
 *                 at offset.
 * @param out_ptr  Receives `(uint8_t *)sp->ptr + offset` on success; left
 *                 untouched on failure. Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if sp or
 *         out_ptr is NULL; SAPI_STATUS_DATA_CORRUPTION if sp's canary is
 *         wrong (REQ-OAL-SAFEPTR-001); SAPI_STATUS_VALUE_OUT_OF_RANGE if
 *         offset + length > sp->size, including the case where
 *         offset + length itself would overflow size_t
 *         (REQ-OAL-SAFEPTR-002).
 */
sapi_status_t sapi_safe_ptr_offset(const sapi_safe_ptr_t *sp, size_t offset, size_t length, void **out_ptr);

/**
 * @brief Invalidates sp: clears the wrapped pointer and the canary, so
 *        every subsequent access on sp fails SAPI_STATUS_DATA_CORRUPTION.
 *
 * Relevant when the underlying region is logically released back to a
 * pool (sapi_mem_pool_release()) or otherwise stops being valid for this
 * wrapper to describe, while sp itself (the struct) persists (e.g. a
 * reusable static wrapper) - no dynamic allocation is involved on either
 * side of this call.
 *
 * @param sp Wrapper to invalidate. NULL is a documented no-op.
 */
void sapi_safe_ptr_invalidate(sapi_safe_ptr_t *sp);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_OAL_SAFE_PTR_H */

/** @} */ /* SAFEPTR */

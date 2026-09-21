#ifndef SAFEAPI_MEM_UTIL_H
#define SAFEAPI_MEM_UTIL_H

/**
 * @file rte_mem_util.h
 * @brief Thin, MISRA-visible wrappers over the three raw memory-block
 *        primitives (fill/copy/compare) every other safeAPIFreamwork or
 *        downstream-application module needs but has no business calling
 *        libc's <string.h> directly for - rte_mem_set()/rte_mem_copy()/
 *        rte_mem_compare() are the ONE sanctioned call site for each
 *        underlying libc function, so an audit for "does this codebase
 *        call libc string/memory functions directly" has exactly one
 *        file to check instead of grepping every translation unit for
 *        <string.h>. Deliberately NOT a "safe string library" (no
 *        strcpy/strcat/strlen-style variable-length text handling) -
 *        this framework and every application built on it work in
 *        fixed-size buffers throughout (CLAUDE.md: no malloc/free), so
 *        the only primitives actually needed are fixed-length block
 *        operations.
 *
 * static inline, header-only, no backend/.c file: these are trivial,
 * stateless wrappers with no OS dependency at all (unlike the seven real
 * OAL services - rte_timer, rte_nvm, rte_memory, etc. - see
 * rte_appmanager.h's own note on that distinction) - same convention as
 * rte_notify.h's SAFEAPI_DECLARE_CALLBACK_LIST (ADR-030).
 */

#include <stddef.h>
#include <string.h>

/** @brief Fills @p count bytes starting at @p dest with @p value - the
 *         one sanctioned call site for libc memset() in this framework
 *         (see this file's own doc).
 * @param dest   Caller-owned storage, at least @p count bytes. Must not
 *               be NULL unless @p count is 0.
 * @param value  Byte value to write, same truncation-to-unsigned-char
 *               convention as memset() itself.
 * @param count  Number of bytes to fill; 0 is a no-op. */
static inline void rte_mem_set(void *dest, int value, size_t count)
{
    (void)memset(dest, value, count);
}

/** @brief Copies @p count bytes from @p src to @p dest - the one
 *         sanctioned call site for libc memcpy() in this framework (see
 *         this file's own doc).
 * @param dest   Destination, caller-owned, at least @p count bytes. Must
 *               not be NULL unless @p count is 0.
 * @param src    Source, caller-owned, at least @p count bytes. Must not
 *               overlap @p dest (same contract as memcpy() itself - use
 *               a genuinely overlap-safe primitive instead if that is
 *               ever needed; none is provided here since nothing in this
 *               codebase currently copies overlapping ranges). Must not
 *               be NULL unless @p count is 0.
 * @param count  Number of bytes to copy; 0 is a no-op. */
static inline void rte_mem_copy(void *dest, const void *src, size_t count)
{
    (void)memcpy(dest, src, count);
}

/** @brief Byte-for-byte compares @p count bytes of @p a against @p b -
 *         the one sanctioned call site for libc memcmp() in this
 *         framework (see this file's own doc).
 * @param a,b    Caller-owned, at least @p count bytes each. Must not be
 *               NULL unless @p count is 0.
 * @param count  Number of bytes to compare; 0 always returns 0.
 * @return 0 if every byte matches; nonzero otherwise. Callers should
 *         only rely on the zero/nonzero distinction, not the sign - this
 *         framework never orders byte buffers by memcmp()'s sign. */
static inline int rte_mem_compare(const void *a, const void *b, size_t count)
{
    return memcmp(a, b, count);
}

#endif /* SAFEAPI_MEM_UTIL_H */

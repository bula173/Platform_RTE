/**
 * @file sapi_types.h
 * @brief Common fixed-width types and the caller-owned-storage handle
 *        pattern used by every OAL service (see ADR-001, section 3.4).
 *
 * No dynamic memory allocation is used by the framework after
 * initialization (REQ-OAL-COMMON-010). Every stateful object is created
 * from storage supplied by the caller, typically a statically allocated
 * struct.
 */
#ifndef SAFEAPI_COMMON_TYPES_H
#define SAFEAPI_COMMON_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Milliseconds, used by all timer/deadline related APIs. */
typedef uint32_t sapi_duration_ms_t;

/** Monotonic timestamp in milliseconds since an arbitrary epoch. */
typedef uint64_t sapi_timestamp_ms_t;

/**
 * @def SAFEAPI_STORAGE_ALIGN
 * @brief Alignment (bytes) guaranteed for every opaque storage block.
 */
#define SAFEAPI_STORAGE_ALIGN 8U

/* Portable alignment specifier: prefer C11 _Alignas, fall back to compiler
 * builtins so the headers remain usable from strict C99 toolchains. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#include <stdalign.h>
#define SAFEAPI_ALIGNED_(n) _Alignas(n)
#elif defined(_MSC_VER)
#define SAFEAPI_ALIGNED_(n) __declspec(align(n))
#elif defined(__GNUC__) || defined(__clang__)
#define SAFEAPI_ALIGNED_(n) __attribute__((aligned(n)))
#else
#define SAFEAPI_ALIGNED_(n) /* no portable alignment available */
#endif

/**
 * @def SAFEAPI_DECLARE_STORAGE
 * @brief Declares an opaque, fixed-size, aligned storage type for a
 *        service's handle. The real internal layout is private to the
 *        backend implementation; callers only reserve the bytes.
 *
 * Usage: SAFEAPI_DECLARE_STORAGE(sapi_timer_storage_t, 64U);
 */
#define SAFEAPI_DECLARE_STORAGE(type_name, byte_size)                   \
    typedef struct                                                      \
    {                                                                    \
        SAFEAPI_ALIGNED_(SAFEAPI_STORAGE_ALIGN)                          \
        unsigned char reserved_[(byte_size)];                           \
    } type_name

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_TYPES_H */

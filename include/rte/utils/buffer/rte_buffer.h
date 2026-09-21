/**
 * @file rte_buffer.h
 * @brief Cross-layer data buffer abstraction (ADR-002).
 *
 * rte_buffer_t is a lightweight, caller-owned VIEW over static storage -
 * it never allocates and never frees memory. Every operation is
 * bounds-checked against the declared capacity.
 *
 * REQ-COMMON-BUF-001: no dynamic allocation; the caller owns the backing
 *                     storage (e.g. a static or stack-scoped byte array)
 *                     for the lifetime of the buffer view.
 * REQ-COMMON-BUF-002: every copy operation shall be bounds-checked and
 *                     shall never write past the declared capacity.
 *
 * @defgroup BUFFER Cross-Layer Data Buffer
 * @brief Caller-owned, bounds-checked view over static storage (ADR-002)
 * @{
 */
#ifndef RTE_COMMON_BUFFER_H
#define RTE_COMMON_BUFFER_H

#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mutable, bounds-tracked view over caller-owned storage.
 *
 * Invariant (checked by rte_buffer_is_valid): data != NULL and
 * length <= capacity.
 */
typedef struct rte_buffer_s
{
    void   *data;      /**< Caller-owned backing storage. */
    size_t  capacity;  /**< Total usable bytes available in data. */
    size_t  length;    /**< Bytes currently holding valid data (<= capacity). */
} rte_buffer_t;

/**
 * @brief Read-only view of a buffer's currently valid bytes.
 *        Grants no write access to the underlying storage.
 */
typedef struct rte_const_buffer_s
{
    const void *data;    /**< Read-only pointer into caller-owned storage. */
    size_t      length;  /**< Number of valid bytes at data. */
} rte_const_buffer_t;

/**
 * @brief Binds a buffer view to caller-owned storage. Initial length is 0.
 * @param buf       Buffer view to initialize. Must not be NULL.
 * @param storage   Caller-owned backing storage. Must not be NULL and must
 *                  remain valid for the lifetime of buf.
 * @param capacity  Usable size of storage in bytes. Must be > 0.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if buf or
 *         storage is NULL, or capacity is 0.
 * REQ-COMMON-BUF-010
 */
rte_status_t rte_buffer_init(rte_buffer_t *buf, void *storage, size_t capacity);

/**
 * @brief Resets length to 0; capacity and data are unchanged.
 * @param buf  Buffer to clear. Must not be NULL and must satisfy
 *             rte_buffer_is_valid().
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM otherwise.
 * REQ-COMMON-BUF-011
 */
rte_status_t rte_buffer_clear(rte_buffer_t *buf);

/**
 * @brief Marks `length` bytes of already-written storage as valid.
 * @param buf     Buffer to update. Must not be NULL.
 * @param length  New valid length in bytes.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if buf is
 *         NULL; RTE_STATUS_RESOURCE_EXHAUSTED if length > buf->capacity.
 * REQ-COMMON-BUF-012
 */
rte_status_t rte_buffer_set_length(rte_buffer_t *buf, size_t length);

/**
 * @brief Bounds-checked copy of external data into the buffer; sets length.
 * @param buf      Destination buffer. Must not be NULL.
 * @param src      Source data to copy in. Must not be NULL.
 * @param src_len  Number of bytes to copy from src.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if buf or
 *         src is NULL; RTE_STATUS_RESOURCE_EXHAUSTED if src_len exceeds
 *         buf->capacity (buf is left unmodified in that case).
 * REQ-COMMON-BUF-013
 */
rte_status_t rte_buffer_copy_in(rte_buffer_t *buf, const void *src, size_t src_len);

/**
 * @brief Bounds-checked copy of the buffer's valid bytes to an external
 *        destination.
 * @param buf            Source buffer. Must not be NULL.
 * @param dest           Destination to copy into. Must not be NULL.
 * @param dest_capacity  Usable size of dest in bytes.
 * @param out_copied     Receives the number of bytes actually copied
 *                       (== buf->length on success; 0 on failure). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if any
 *         pointer argument is NULL; RTE_STATUS_RESOURCE_EXHAUSTED if
 *         buf->length exceeds dest_capacity.
 * REQ-COMMON-BUF-014
 */
rte_status_t rte_buffer_copy_out(const rte_buffer_t *buf,
                                    void *dest,
                                    size_t dest_capacity,
                                    size_t *out_copied);

/**
 * @brief Produces a read-only view of the buffer's current valid bytes.
 *        The view is only valid as long as the source buffer's storage is.
 * @param buf       Source buffer. Must not be NULL.
 * @param out_view  Receives the read-only view. Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if buf or
 *         out_view is NULL; RTE_STATUS_INTERNAL_ERROR if buf's own
 *         length/capacity invariant is violated (defensive check).
 * REQ-COMMON-BUF-015
 */
rte_status_t rte_buffer_as_const(const rte_buffer_t *buf, rte_const_buffer_t *out_view);

/**
 * @brief Defensive validity check: non-null data and length <= capacity.
 * @param buf  Buffer to check; NULL is a valid input (returns false).
 * @return true if buf is non-NULL, buf->data is non-NULL, and
 *         buf->length <= buf->capacity; false otherwise.
 * REQ-COMMON-BUF-016
 */
bool rte_buffer_is_valid(const rte_buffer_t *buf);

/*
 * Endianness-safe multi-byte access (ADR-006 section 2.5). Byte order is
 * always explicit at the call site (_le / _be suffix) - there is no
 * default/"network order" variant. write_* functions append at buf's
 * current length and advance it, like rte_buffer_copy_in(); read_*
 * functions take an explicit offset and do not mutate the buffer, like
 * rte_buffer_copy_out().
 */

/**
 * @brief Appends a little-endian uint16_t at buf's current length.
 * @param buf    Destination buffer. Must not be NULL.
 * @param value  Value to encode and append.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if buf is
 *         NULL; RTE_STATUS_RESOURCE_EXHAUSTED if 2 bytes will not fit
 *         before buf->capacity.
 * REQ-COMMON-BUF-020
 */
rte_status_t rte_buffer_write_u16_le(rte_buffer_t *buf, uint16_t value);
/**
 * @brief Appends a big-endian uint16_t. See rte_buffer_write_u16_le().
 * @param buf    Destination buffer. Must not be NULL.
 * @param value  Value to encode and append.
 * @return Same status contract as rte_buffer_write_u16_le().
 * REQ-COMMON-BUF-021
 */
rte_status_t rte_buffer_write_u16_be(rte_buffer_t *buf, uint16_t value);
/**
 * @brief Appends a little-endian uint32_t at buf's current length.
 * @param buf    Destination buffer. Must not be NULL.
 * @param value  Value to encode and append.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if buf is
 *         NULL; RTE_STATUS_RESOURCE_EXHAUSTED if 4 bytes will not fit
 *         before buf->capacity.
 * REQ-COMMON-BUF-022
 */
rte_status_t rte_buffer_write_u32_le(rte_buffer_t *buf, uint32_t value);
/**
 * @brief Appends a big-endian uint32_t. See rte_buffer_write_u32_le().
 * @param buf    Destination buffer. Must not be NULL.
 * @param value  Value to encode and append.
 * @return Same status contract as rte_buffer_write_u32_le().
 * REQ-COMMON-BUF-023
 */
rte_status_t rte_buffer_write_u32_be(rte_buffer_t *buf, uint32_t value);
/**
 * @brief Appends a little-endian uint64_t at buf's current length.
 * @param buf    Destination buffer. Must not be NULL.
 * @param value  Value to encode and append.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if buf is
 *         NULL; RTE_STATUS_RESOURCE_EXHAUSTED if 8 bytes will not fit
 *         before buf->capacity.
 * REQ-COMMON-BUF-024
 */
rte_status_t rte_buffer_write_u64_le(rte_buffer_t *buf, uint64_t value);
/**
 * @brief Appends a big-endian uint64_t. See rte_buffer_write_u64_le().
 * @param buf    Destination buffer. Must not be NULL.
 * @param value  Value to encode and append.
 * @return Same status contract as rte_buffer_write_u64_le().
 * REQ-COMMON-BUF-025
 */
rte_status_t rte_buffer_write_u64_be(rte_buffer_t *buf, uint64_t value);

/**
 * @brief Reads a little-endian uint16_t at the given offset.
 * @param buf         Source buffer. Must not be NULL.
 * @param offset      Byte offset within buf's valid bytes to read from.
 * @param out_value   Receives the decoded value. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a NULL pointer;
 *         RTE_STATUS_RESOURCE_EXHAUSTED if offset+2 exceeds buf->length.
 * REQ-COMMON-BUF-026
 */
rte_status_t rte_buffer_read_u16_le(const rte_buffer_t *buf, size_t offset, uint16_t *out_value);
/**
 * @brief Reads a big-endian uint16_t. See rte_buffer_read_u16_le().
 * @param buf        Source buffer. Must not be NULL.
 * @param offset     Byte offset within buf's valid bytes to read from.
 * @param out_value  Receives the decoded value. Must not be NULL.
 * @return Same status contract as rte_buffer_read_u16_le().
 * REQ-COMMON-BUF-027
 */
rte_status_t rte_buffer_read_u16_be(const rte_buffer_t *buf, size_t offset, uint16_t *out_value);
/**
 * @brief Reads a little-endian uint32_t at the given offset.
 * @param buf        Source buffer. Must not be NULL.
 * @param offset     Byte offset within buf's valid bytes to read from.
 * @param out_value  Receives the decoded value. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a NULL pointer;
 *         RTE_STATUS_RESOURCE_EXHAUSTED if offset+4 exceeds buf->length.
 * REQ-COMMON-BUF-028
 */
rte_status_t rte_buffer_read_u32_le(const rte_buffer_t *buf, size_t offset, uint32_t *out_value);
/**
 * @brief Reads a big-endian uint32_t. See rte_buffer_read_u32_le().
 * @param buf        Source buffer. Must not be NULL.
 * @param offset     Byte offset within buf's valid bytes to read from.
 * @param out_value  Receives the decoded value. Must not be NULL.
 * @return Same status contract as rte_buffer_read_u32_le().
 * REQ-COMMON-BUF-029
 */
rte_status_t rte_buffer_read_u32_be(const rte_buffer_t *buf, size_t offset, uint32_t *out_value);
/**
 * @brief Reads a little-endian uint64_t at the given offset.
 * @param buf        Source buffer. Must not be NULL.
 * @param offset     Byte offset within buf's valid bytes to read from.
 * @param out_value  Receives the decoded value. Must not be NULL.
 * @return RTE_STATUS_OK; RTE_STATUS_INVALID_PARAM for a NULL pointer;
 *         RTE_STATUS_RESOURCE_EXHAUSTED if offset+8 exceeds buf->length.
 * REQ-COMMON-BUF-030
 */
rte_status_t rte_buffer_read_u64_le(const rte_buffer_t *buf, size_t offset, uint64_t *out_value);
/**
 * @brief Reads a big-endian uint64_t. See rte_buffer_read_u64_le().
 * @param buf        Source buffer. Must not be NULL.
 * @param offset     Byte offset within buf's valid bytes to read from.
 * @param out_value  Receives the decoded value. Must not be NULL.
 * @return Same status contract as rte_buffer_read_u64_le().
 * REQ-COMMON-BUF-031
 */
rte_status_t rte_buffer_read_u64_be(const rte_buffer_t *buf, size_t offset, uint64_t *out_value);

#ifdef __cplusplus
}
#endif

#endif /* RTE_COMMON_BUFFER_H */

/** @} */ /* BUFFER */

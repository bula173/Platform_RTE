/**
 * @file sapi_cast.h
 * @brief Checked integer casting between all fixed-width types and size_t
 *        (ADR-003). Every conversion in the codebase - narrowing, widening,
 *        or sign-changing - goes through one of these functions instead of
 *        a bare C-style cast.
 *
 * REQ-COMMON-CAST-001: a checked cast function shall never write to *out
 *                      when the value does not fit the destination type;
 *                      the caller's variable is left exactly as passed in.
 * REQ-COMMON-CAST-002: out == NULL yields SAPI_STATUS_INVALID_PARAM without
 *                      dereferencing out.
 * REQ-COMMON-CAST-003: a value that does not fit the destination range
 *                      yields SAPI_STATUS_VALUE_OUT_OF_RANGE.
 */
#ifndef SAFEAPI_COMMON_CAST_H
#define SAFEAPI_COMMON_CAST_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checked cast from int8_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_i16(int8_t in, int16_t *out);

/**
 * @brief Checked cast from int8_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_i32(int8_t in, int32_t *out);

/**
 * @brief Checked cast from int8_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_i64(int8_t in, int64_t *out);

/**
 * @brief Checked cast from int8_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_u8(int8_t in, uint8_t *out);

/**
 * @brief Checked cast from int8_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_u16(int8_t in, uint16_t *out);

/**
 * @brief Checked cast from int8_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_u32(int8_t in, uint32_t *out);

/**
 * @brief Checked cast from int8_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_u64(int8_t in, uint64_t *out);

/**
 * @brief Checked cast from int8_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i8_to_size(int8_t in, size_t *out);

/**
 * @brief Checked cast from int16_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_i8(int16_t in, int8_t *out);

/**
 * @brief Checked cast from int16_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_i32(int16_t in, int32_t *out);

/**
 * @brief Checked cast from int16_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_i64(int16_t in, int64_t *out);

/**
 * @brief Checked cast from int16_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_u8(int16_t in, uint8_t *out);

/**
 * @brief Checked cast from int16_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_u16(int16_t in, uint16_t *out);

/**
 * @brief Checked cast from int16_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_u32(int16_t in, uint32_t *out);

/**
 * @brief Checked cast from int16_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_u64(int16_t in, uint64_t *out);

/**
 * @brief Checked cast from int16_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i16_to_size(int16_t in, size_t *out);

/**
 * @brief Checked cast from int32_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_i8(int32_t in, int8_t *out);

/**
 * @brief Checked cast from int32_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_i16(int32_t in, int16_t *out);

/**
 * @brief Checked cast from int32_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_i64(int32_t in, int64_t *out);

/**
 * @brief Checked cast from int32_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_u8(int32_t in, uint8_t *out);

/**
 * @brief Checked cast from int32_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_u16(int32_t in, uint16_t *out);

/**
 * @brief Checked cast from int32_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_u32(int32_t in, uint32_t *out);

/**
 * @brief Checked cast from int32_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_u64(int32_t in, uint64_t *out);

/**
 * @brief Checked cast from int32_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i32_to_size(int32_t in, size_t *out);

/**
 * @brief Checked cast from int64_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_i8(int64_t in, int8_t *out);

/**
 * @brief Checked cast from int64_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_i16(int64_t in, int16_t *out);

/**
 * @brief Checked cast from int64_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_i32(int64_t in, int32_t *out);

/**
 * @brief Checked cast from int64_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_u8(int64_t in, uint8_t *out);

/**
 * @brief Checked cast from int64_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_u16(int64_t in, uint16_t *out);

/**
 * @brief Checked cast from int64_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_u32(int64_t in, uint32_t *out);

/**
 * @brief Checked cast from int64_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_u64(int64_t in, uint64_t *out);

/**
 * @brief Checked cast from int64_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_i64_to_size(int64_t in, size_t *out);

/**
 * @brief Checked cast from uint8_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_i8(uint8_t in, int8_t *out);

/**
 * @brief Checked cast from uint8_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_i16(uint8_t in, int16_t *out);

/**
 * @brief Checked cast from uint8_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_i32(uint8_t in, int32_t *out);

/**
 * @brief Checked cast from uint8_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_i64(uint8_t in, int64_t *out);

/**
 * @brief Checked cast from uint8_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_u16(uint8_t in, uint16_t *out);

/**
 * @brief Checked cast from uint8_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_u32(uint8_t in, uint32_t *out);

/**
 * @brief Checked cast from uint8_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_u64(uint8_t in, uint64_t *out);

/**
 * @brief Checked cast from uint8_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u8_to_size(uint8_t in, size_t *out);

/**
 * @brief Checked cast from uint16_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_i8(uint16_t in, int8_t *out);

/**
 * @brief Checked cast from uint16_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_i16(uint16_t in, int16_t *out);

/**
 * @brief Checked cast from uint16_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_i32(uint16_t in, int32_t *out);

/**
 * @brief Checked cast from uint16_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_i64(uint16_t in, int64_t *out);

/**
 * @brief Checked cast from uint16_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_u8(uint16_t in, uint8_t *out);

/**
 * @brief Checked cast from uint16_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_u32(uint16_t in, uint32_t *out);

/**
 * @brief Checked cast from uint16_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_u64(uint16_t in, uint64_t *out);

/**
 * @brief Checked cast from uint16_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u16_to_size(uint16_t in, size_t *out);

/**
 * @brief Checked cast from uint32_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_i8(uint32_t in, int8_t *out);

/**
 * @brief Checked cast from uint32_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_i16(uint32_t in, int16_t *out);

/**
 * @brief Checked cast from uint32_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_i32(uint32_t in, int32_t *out);

/**
 * @brief Checked cast from uint32_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_i64(uint32_t in, int64_t *out);

/**
 * @brief Checked cast from uint32_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_u8(uint32_t in, uint8_t *out);

/**
 * @brief Checked cast from uint32_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_u16(uint32_t in, uint16_t *out);

/**
 * @brief Checked cast from uint32_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_u64(uint32_t in, uint64_t *out);

/**
 * @brief Checked cast from uint32_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u32_to_size(uint32_t in, size_t *out);

/**
 * @brief Checked cast from uint64_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_i8(uint64_t in, int8_t *out);

/**
 * @brief Checked cast from uint64_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_i16(uint64_t in, int16_t *out);

/**
 * @brief Checked cast from uint64_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_i32(uint64_t in, int32_t *out);

/**
 * @brief Checked cast from uint64_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_i64(uint64_t in, int64_t *out);

/**
 * @brief Checked cast from uint64_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_u8(uint64_t in, uint8_t *out);

/**
 * @brief Checked cast from uint64_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_u16(uint64_t in, uint16_t *out);

/**
 * @brief Checked cast from uint64_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_u32(uint64_t in, uint32_t *out);

/**
 * @brief Checked cast from uint64_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_u64_to_size(uint64_t in, size_t *out);

/**
 * @brief Checked cast from size_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_i8(size_t in, int8_t *out);

/**
 * @brief Checked cast from size_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_i16(size_t in, int16_t *out);

/**
 * @brief Checked cast from size_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_i32(size_t in, int32_t *out);

/**
 * @brief Checked cast from size_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_i64(size_t in, int64_t *out);

/**
 * @brief Checked cast from size_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_u8(size_t in, uint8_t *out);

/**
 * @brief Checked cast from size_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_u16(size_t in, uint16_t *out);

/**
 * @brief Checked cast from size_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_u32(size_t in, uint32_t *out);

/**
 * @brief Checked cast from size_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return SAPI_STATUS_OK on success; SAPI_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); SAPI_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
sapi_status_t sapi_cast_size_to_u64(size_t in, uint64_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_CAST_H */

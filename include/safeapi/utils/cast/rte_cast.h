/**
 * @file rte_cast.h
 * @brief Checked integer casting between all fixed-width types and size_t
 *        (ADR-003). Every conversion in the codebase - narrowing, widening,
 *        or sign-changing - goes through one of these functions instead of
 *        a bare C-style cast.
 *
 * REQ-COMMON-CAST-001: a checked cast function shall never write to *out
 *                      when the value does not fit the destination type;
 *                      the caller's variable is left exactly as passed in.
 * REQ-COMMON-CAST-002: out == NULL yields RTE_STATUS_INVALID_PARAM without
 *                      dereferencing out.
 * REQ-COMMON-CAST-003: a value that does not fit the destination range
 *                      yields RTE_STATUS_VALUE_OUT_OF_RANGE.
 * REQ-COMMON-CAST-004: rte_cast_checked_add_u32()/_sub_u32()/_mul_u32() and
 *                      the size_t equivalents shall detect overflow/
 *                      underflow and yield RTE_STATUS_VALUE_OUT_OF_RANGE
 *                      instead of wrapping silently; *out is left untouched
 *                      on failure, same as every other function here
 *                      (REQ-COMMON-CAST-001).
 * REQ-COMMON-CAST-005: rte_cast_bounds_check(index, count) shall return
 *                      RTE_STATUS_OK if index < count, else
 *                      RTE_STATUS_VALUE_OUT_OF_RANGE - a single checked
 *                      helper standardizing the array/index-bounds check
 *                      pattern already hand-rolled ad hoc across this
 *                      codebase's consumers.
 *
 * @defgroup CAST Checked Integer Casting
 * @brief Bounds-checked conversion between fixed-width types (ADR-003)
 * @{
 */
#ifndef SAFEAPI_COMMON_CAST_H
#define SAFEAPI_COMMON_CAST_H

#include "safeapi/utils/status/rte_status.h"
#include "safeapi/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checked cast from int8_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_i16(int8_t in, int16_t *out);

/**
 * @brief Checked cast from int8_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_i32(int8_t in, int32_t *out);

/**
 * @brief Checked cast from int8_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_i64(int8_t in, int64_t *out);

/**
 * @brief Checked cast from int8_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_u8(int8_t in, uint8_t *out);

/**
 * @brief Checked cast from int8_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_u16(int8_t in, uint16_t *out);

/**
 * @brief Checked cast from int8_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_u32(int8_t in, uint32_t *out);

/**
 * @brief Checked cast from int8_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_u64(int8_t in, uint64_t *out);

/**
 * @brief Checked cast from int8_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i8_to_size(int8_t in, size_t *out);

/**
 * @brief Checked cast from int16_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_i8(int16_t in, int8_t *out);

/**
 * @brief Checked cast from int16_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_i32(int16_t in, int32_t *out);

/**
 * @brief Checked cast from int16_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_i64(int16_t in, int64_t *out);

/**
 * @brief Checked cast from int16_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_u8(int16_t in, uint8_t *out);

/**
 * @brief Checked cast from int16_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_u16(int16_t in, uint16_t *out);

/**
 * @brief Checked cast from int16_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_u32(int16_t in, uint32_t *out);

/**
 * @brief Checked cast from int16_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_u64(int16_t in, uint64_t *out);

/**
 * @brief Checked cast from int16_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i16_to_size(int16_t in, size_t *out);

/**
 * @brief Checked cast from int32_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_i8(int32_t in, int8_t *out);

/**
 * @brief Checked cast from int32_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_i16(int32_t in, int16_t *out);

/**
 * @brief Checked cast from int32_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_i64(int32_t in, int64_t *out);

/**
 * @brief Checked cast from int32_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_u8(int32_t in, uint8_t *out);

/**
 * @brief Checked cast from int32_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_u16(int32_t in, uint16_t *out);

/**
 * @brief Checked cast from int32_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_u32(int32_t in, uint32_t *out);

/**
 * @brief Checked cast from int32_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_u64(int32_t in, uint64_t *out);

/**
 * @brief Checked cast from int32_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i32_to_size(int32_t in, size_t *out);

/**
 * @brief Checked cast from int64_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_i8(int64_t in, int8_t *out);

/**
 * @brief Checked cast from int64_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_i16(int64_t in, int16_t *out);

/**
 * @brief Checked cast from int64_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_i32(int64_t in, int32_t *out);

/**
 * @brief Checked cast from int64_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_u8(int64_t in, uint8_t *out);

/**
 * @brief Checked cast from int64_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_u16(int64_t in, uint16_t *out);

/**
 * @brief Checked cast from int64_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_u32(int64_t in, uint32_t *out);

/**
 * @brief Checked cast from int64_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_u64(int64_t in, uint64_t *out);

/**
 * @brief Checked cast from int64_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_i64_to_size(int64_t in, size_t *out);

/**
 * @brief Checked cast from uint8_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_i8(uint8_t in, int8_t *out);

/**
 * @brief Checked cast from uint8_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_i16(uint8_t in, int16_t *out);

/**
 * @brief Checked cast from uint8_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_i32(uint8_t in, int32_t *out);

/**
 * @brief Checked cast from uint8_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_i64(uint8_t in, int64_t *out);

/**
 * @brief Checked cast from uint8_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_u16(uint8_t in, uint16_t *out);

/**
 * @brief Checked cast from uint8_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_u32(uint8_t in, uint32_t *out);

/**
 * @brief Checked cast from uint8_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_u64(uint8_t in, uint64_t *out);

/**
 * @brief Checked cast from uint8_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u8_to_size(uint8_t in, size_t *out);

/**
 * @brief Checked cast from uint16_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_i8(uint16_t in, int8_t *out);

/**
 * @brief Checked cast from uint16_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_i16(uint16_t in, int16_t *out);

/**
 * @brief Checked cast from uint16_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_i32(uint16_t in, int32_t *out);

/**
 * @brief Checked cast from uint16_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_i64(uint16_t in, int64_t *out);

/**
 * @brief Checked cast from uint16_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_u8(uint16_t in, uint8_t *out);

/**
 * @brief Checked cast from uint16_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_u32(uint16_t in, uint32_t *out);

/**
 * @brief Checked cast from uint16_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_u64(uint16_t in, uint64_t *out);

/**
 * @brief Checked cast from uint16_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u16_to_size(uint16_t in, size_t *out);

/**
 * @brief Checked cast from uint32_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_i8(uint32_t in, int8_t *out);

/**
 * @brief Checked cast from uint32_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_i16(uint32_t in, int16_t *out);

/**
 * @brief Checked cast from uint32_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_i32(uint32_t in, int32_t *out);

/**
 * @brief Checked cast from uint32_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_i64(uint32_t in, int64_t *out);

/**
 * @brief Checked cast from uint32_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_u8(uint32_t in, uint8_t *out);

/**
 * @brief Checked cast from uint32_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_u16(uint32_t in, uint16_t *out);

/**
 * @brief Checked cast from uint32_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_u64(uint32_t in, uint64_t *out);

/**
 * @brief Checked cast from uint32_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u32_to_size(uint32_t in, size_t *out);

/**
 * @brief Checked cast from uint64_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_i8(uint64_t in, int8_t *out);

/**
 * @brief Checked cast from uint64_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_i16(uint64_t in, int16_t *out);

/**
 * @brief Checked cast from uint64_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_i32(uint64_t in, int32_t *out);

/**
 * @brief Checked cast from uint64_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_i64(uint64_t in, int64_t *out);

/**
 * @brief Checked cast from uint64_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_u8(uint64_t in, uint8_t *out);

/**
 * @brief Checked cast from uint64_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_u16(uint64_t in, uint16_t *out);

/**
 * @brief Checked cast from uint64_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_u32(uint64_t in, uint32_t *out);

/**
 * @brief Checked cast from uint64_t to size_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit size_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_u64_to_size(uint64_t in, size_t *out);

/**
 * @brief Checked cast from size_t to int8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_i8(size_t in, int8_t *out);

/**
 * @brief Checked cast from size_t to int16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_i16(size_t in, int16_t *out);

/**
 * @brief Checked cast from size_t to int32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_i32(size_t in, int32_t *out);

/**
 * @brief Checked cast from size_t to int64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit int64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_i64(size_t in, int64_t *out);

/**
 * @brief Checked cast from size_t to uint8_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint8_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_u8(size_t in, uint8_t *out);

/**
 * @brief Checked cast from size_t to uint16_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint16_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_u16(size_t in, uint16_t *out);

/**
 * @brief Checked cast from size_t to uint32_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint32_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_u32(size_t in, uint32_t *out);

/**
 * @brief Checked cast from size_t to uint64_t.
 * @param in   Value to convert.
 * @param out  Receives the converted value on success; left
 *             untouched on failure (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if
 *         out is NULL (REQ-COMMON-CAST-002); RTE_STATUS_VALUE_OUT_OF_RANGE
 *         if in does not fit uint64_t (REQ-COMMON-CAST-003).
 */
rte_status_t rte_cast_size_to_u64(size_t in, uint64_t *out);

/**
 * @brief Checked addition: *out = a + b, only if the result fits uint32_t.
 * @param a, b Operands.
 * @param out  Receives a + b on success; left untouched on failure
 *             (REQ-COMMON-CAST-001). Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if out is
 *         NULL; RTE_STATUS_VALUE_OUT_OF_RANGE if a + b overflows uint32_t
 *         (REQ-COMMON-CAST-004).
 */
rte_status_t rte_cast_checked_add_u32(uint32_t a, uint32_t b, uint32_t *out);

/**
 * @brief Checked subtraction: *out = a - b, only if b <= a (no underflow).
 * @param a, b Operands.
 * @param out  Receives a - b on success; left untouched on failure.
 *             Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if out is
 *         NULL; RTE_STATUS_VALUE_OUT_OF_RANGE if b > a
 *         (REQ-COMMON-CAST-004).
 */
rte_status_t rte_cast_checked_sub_u32(uint32_t a, uint32_t b, uint32_t *out);

/**
 * @brief Checked multiplication: *out = a * b, only if the result fits uint32_t.
 * @param a, b Operands.
 * @param out  Receives a * b on success; left untouched on failure.
 *             Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if out is
 *         NULL; RTE_STATUS_VALUE_OUT_OF_RANGE if a * b overflows uint32_t
 *         (REQ-COMMON-CAST-004).
 */
rte_status_t rte_cast_checked_mul_u32(uint32_t a, uint32_t b, uint32_t *out);

/**
 * @brief Checked addition: *out = a + b, only if the result fits size_t.
 * @param a, b Operands.
 * @param out  Receives a + b on success; left untouched on failure.
 *             Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if out is
 *         NULL; RTE_STATUS_VALUE_OUT_OF_RANGE if a + b overflows size_t
 *         (REQ-COMMON-CAST-004).
 */
rte_status_t rte_cast_checked_add_size(size_t a, size_t b, size_t *out);

/**
 * @brief Checked subtraction: *out = a - b, only if b <= a (no underflow).
 * @param a, b Operands.
 * @param out  Receives a - b on success; left untouched on failure.
 *             Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if out is
 *         NULL; RTE_STATUS_VALUE_OUT_OF_RANGE if b > a
 *         (REQ-COMMON-CAST-004).
 */
rte_status_t rte_cast_checked_sub_size(size_t a, size_t b, size_t *out);

/**
 * @brief Checked multiplication: *out = a * b, only if the result fits size_t.
 * @param a, b Operands.
 * @param out  Receives a * b on success; left untouched on failure.
 *             Must not be NULL.
 * @return RTE_STATUS_OK on success; RTE_STATUS_INVALID_PARAM if out is
 *         NULL; RTE_STATUS_VALUE_OUT_OF_RANGE if a * b overflows size_t
 *         (REQ-COMMON-CAST-004).
 */
rte_status_t rte_cast_checked_mul_size(size_t a, size_t b, size_t *out);

/**
 * @brief Checked array/index bounds check: is index a valid index into an
 *        array of count elements?
 * @param index Candidate index.
 * @param count Number of elements in the array (a count of 0 means no
 *              index is ever valid).
 * @return RTE_STATUS_OK if index < count; RTE_STATUS_VALUE_OUT_OF_RANGE
 *         otherwise (REQ-COMMON-CAST-005). Never RTE_STATUS_INVALID_PARAM -
 *         both parameters are plain values, not pointers, so there is
 *         nothing to NULL-check.
 */
rte_status_t rte_cast_bounds_check(size_t index, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_COMMON_CAST_H */

/** @} */ /* CAST */

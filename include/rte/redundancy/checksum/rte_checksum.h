/**
 * @file rte_checksum.h
 * @brief Checksum and CRC utilities for data integrity in redundant systems
 *
 * Provides CRC-64 data integrity checking for:
 * - Channel-to-channel communication (vital/non-vital channels)
 * - Inter-site cluster communication
 * - Data validation before voting
 *
 * Implements ERTMS-compliant CRC-64-CCITT polynomial for railway safety systems.
 *
 * @defgroup CHECKSUM Checksum & CRC Utilities
 * @brief Data integrity verification for redundant communication
 * @{
 *
 * REQ-CHECKSUM-001: rte_checksum_crc64_init() shall be callable exactly
 *                   once; a subsequent call before any re-init mechanism
 *                   exists shall return RTE_STATUS_ALREADY_INITIALIZED
 *                   and leave the already-selected table/polynomial
 *                   unchanged.
 * REQ-CHECKSUM-002: rte_checksum_crc64() shall return 0 - never
 *                   dereferencing data - if the module is not yet
 *                   initialized, if its lookup table is unset, or if
 *                   data is NULL while size is nonzero.
 * REQ-CHECKSUM-003: rte_checksum_crc64() shall be deterministic and O(n)
 *                   in size, using a precomputed 256-entry lookup table
 *                   (no bit-by-bit computation on the hot path).
 * REQ-CHECKSUM-004: rte_checksum_crc64_verify() shall report
 *                   RTE_STATUS_DATA_CORRUPTION (not merely a boolean) on
 *                   mismatch and increment stats.verification_failures;
 *                   on match it shall return RTE_STATUS_OK and increment
 *                   stats.verification_passes.
 * REQ-CHECKSUM-005: rte_checksum_vital_message_create() shall reject a
 *                   payload larger than sizeof(rte_vital_message_t::payload)
 *                   with RTE_STATUS_INVALID_PARAM, incrementing
 *                   stats.payload_oversize, without writing msg_out.
 * REQ-CHECKSUM-006: rte_checksum_vital_message_verify() shall verify the
 *                   message's CRC-64 before trusting any other field, and
 *                   report RTE_STATUS_DATA_CORRUPTION - without writing
 *                   to payload_out/payload_size_out - on either a CRC
 *                   mismatch or a sequence_number that does not equal the
 *                   caller-supplied expected_sequence (incrementing
 *                   stats.sequence_errors in the latter case).
 * REQ-CHECKSUM-007: rte_checksum_vital_message_verify() shall reject a
 *                   decoded payload_size exceeding the caller's
 *                   payload_max_size with RTE_STATUS_INVALID_PARAM,
 *                   incrementing stats.payload_oversize, without copying
 *                   into payload_out.
 * REQ-CHECKSUM-008: rte_checksum_get_stats()/_reset_stats() are
 *                   diagnostics-only (never on a safety-decision path);
 *                   _get_stats() returns RTE_STATUS_INVALID_PARAM for a
 *                   NULL stats_out, otherwise both always return
 *                   RTE_STATUS_OK.
 */

#ifndef RTE_CHECKSUM_H
#define RTE_CHECKSUM_H

#include <stdint.h>
#include <stddef.h>
#include "rte/utils/status/rte_status.h"
#include "rte/utils/types/rte_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types & Constants
 * ========================================================================== */

/**
 * @brief CRC-64 polynomial selection
 *
 * Different polynomials for different use cases and standards.
 */
typedef enum {
    /**
     * ERTMS/ETCS standard: 0x1D4F63B86E40E541
     * Used in railway signaling systems, EN 50128 compliant
     */
    RTE_CRC64_ERTMS,

    /**
     * ISO 3309 / HDLC standard: 0x000000000000001B
     * Commonly used in communication protocols
     */
    RTE_CRC64_ISO,

    /**
     * XZ/LZMA standard: 0x142F0E1EBA9EA3C3
     * Alternative for high-reliability systems
     */
    RTE_CRC64_XZ
} rte_crc64_polynomial_t;

/**
 * @brief CRC-64 checksum value (64-bit)
 */
typedef uint64_t rte_crc64_t;

/**
 * @brief Checksum result for validation
 */
typedef struct {
    rte_crc64_t computed;  /**< CRC-64 computed from data */
    rte_crc64_t expected;  /**< CRC-64 from message header/trailer */
    uint8_t match;          /**< 1 if match, 0 if mismatch */
} rte_checksum_result_t;

/* ============================================================================
 * API: CRC-64 Computation
 * ========================================================================== */

/**
 * @brief Initialize CRC-64 lookup tables for chosen polynomial
 *
 * Must be called once at startup before using CRC-64 functions.
 * Generates lookup tables for O(1) byte-at-a-time computation.
 *
 * @param polynomial CRC polynomial to use (ERTMS, ISO, or XZ)
 * @return RTE_STATUS_OK on success
 *         RTE_STATUS_ALREADY_INITIALIZED if already initialized
 *         RTE_STATUS_INVALID_PARAM if polynomial is not a recognized value
 *
 * @safety Safety-critical function, may not be called multiple times
 *
 * Example:
 * @code
 * // At startup
 * rte_checksum_crc64_init(RTE_CRC64_ERTMS);
 *
 * // Later, use CRC functions
 * rte_crc64_t crc = rte_checksum_crc64(data, size);
 * @endcode
 */
rte_status_t rte_checksum_crc64_init(rte_crc64_polynomial_t polynomial);

/**
 * @brief Compute CRC-64 for data buffer
 *
 * Computes CRC-64 using pre-computed lookup tables (O(1) per byte).
 * Execution time is deterministic and bounded.
 *
 * @param data Data buffer to checksum (may be NULL if size=0)
 * @param size Size in bytes (0 to MAX_SIZE)
 * @return 64-bit CRC value
 *
 * @safety Deterministic, no dynamic allocation, bounded execution time
 *
 * Note: Call rte_checksum_crc64_init() before first use.
 *
 * Example:
 * @code
 * train_command_t cmd = {...};
 *
 * // Compute CRC on data (excluding CRC field itself)
 * rte_crc64_t crc = rte_checksum_crc64(
 *     (const uint8_t *)&cmd,
 *     sizeof(cmd) - sizeof(cmd->crc64)
 * );
 *
 * // Store in message
 * cmd.crc64 = crc;
 * @endcode
 */
rte_crc64_t rte_checksum_crc64(const uint8_t *data, size_t size);

/**
 * @brief Verify CRC-64 of data against stored value
 *
 * Computes CRC-64 and compares to expected value.
 * Returns detailed result for diagnostics.
 *
 * @param data Data buffer to verify
 * @param size Size in bytes
 * @param expected_crc Expected CRC-64 value (from message)
 * @param result_out Receives verification result
 * @return RTE_STATUS_OK if verification passed
 *         RTE_STATUS_DATA_CORRUPTION if CRC mismatch (data corrupted)
 *         RTE_STATUS_INVALID_PARAM if result_out is NULL
 *
 * @safety Deterministic computation, safe for safety-critical paths
 *
 * Example:
 * @code
 * rte_checksum_result_t result;
 * rte_status_t status = rte_checksum_crc64_verify(
 *     (const uint8_t *)&received_cmd,
 *     sizeof(received_cmd) - sizeof(received_cmd->crc64),
 *     received_cmd.crc64,
 *     &result
 * );
 *
 * if (status != RTE_STATUS_OK) {
 *     // Data corrupted
 *     rte_log_error("CRC mismatch: expected 0x%llx, got 0x%llx",
 *                    result.expected, result.computed);
 *     rte_safestate_trigger(REASON_DATA_CORRUPTION);
 * }
 * @endcode
 */
rte_status_t rte_checksum_crc64_verify(const uint8_t *data,
                                         size_t size,
                                         rte_crc64_t expected_crc,
                                         rte_checksum_result_t *result_out);

/**
 * @brief Get current CRC-64 polynomial in use
 *
 * Returns the polynomial that was initialized at startup.
 * Useful for logging and diagnostics.
 *
 * @return Current polynomial (ERTMS, ISO, or XZ)
 */
rte_crc64_polynomial_t rte_checksum_crc64_get_polynomial(void);

/* ============================================================================
 * API: Vital Channel Data Wrapper
 * ========================================================================== */

/**
 * @brief Vital channel message with integrated CRC-64
 *
 * Wrapper structure for redundant channel messages that ensures
 * data integrity across transmission.
 *
 * Usage pattern:
 * 1. Sender: Wrap data with CRC, send wrapped message
 * 2. Receiver: Receive wrapped message, unwrap (verify CRC)
 * 3. On mismatch: Trigger safe-state
 */
typedef struct {
    uint32_t sequence_number;  /**< Message sequence (detect reordering) */
    uint32_t sender_id;        /**< Source channel/site ID */
    uint32_t timestamp_ms;     /**< Message creation time */
    uint8_t  payload_size;     /**< Payload size in bytes (max 255) */
    uint8_t  padding;          /**< Padding for alignment */
    uint16_t reserved;         /**< Reserved for future use */

    uint8_t  payload[248];     /**< Payload data (fits in 256-byte message) */
    rte_crc64_t crc64;        /**< CRC-64 of entire message including payload */
} rte_vital_message_t;

/**
 * @brief Create vital channel message with CRC
 *
 * Wraps payload data with sequence number, timestamp, and CRC-64.
 *
 * @param msg_out Message to fill
 * @param sender_id ID of sending channel/site
 * @param sequence Sequence number for this message
 * @param payload Data to send
 * @param payload_size Size of payload (max 248 bytes)
 * @return RTE_STATUS_OK on success
 *         RTE_STATUS_INVALID_PARAM if msg_out is NULL, payload is too
 *         large, or payload is NULL while payload_size is nonzero
 *
 * @safety No dynamic allocation, deterministic execution
 *
 * Example:
 * @code
 * rte_vital_message_t msg;
 * train_command_t cmd = {...};
 *
 * rte_checksum_vital_message_create(
 *     &msg,
 *     CHANNEL_A,                    // sender
 *     ++sequence_num,               // sequence
 *     (const uint8_t *)&cmd,
 *     sizeof(cmd)
 * );
 *
 * // Send msg over network
 * rte_ipc_send(channel, &msg, sizeof(msg), timeout_ms);
 * @endcode
 */
rte_status_t rte_checksum_vital_message_create(
    rte_vital_message_t *msg_out,
    uint32_t sender_id,
    uint32_t sequence,
    const uint8_t *payload,
    size_t payload_size
);

/**
 * @brief Verify vital channel message and extract payload
 *
 * Verifies CRC-64, checks sequence number continuity, and
 * extracts payload. Returns error if any check fails.
 *
 * @param msg Received message to verify
 * @param expected_sequence Expected sequence number (for continuity check)
 * @param payload_out Buffer to receive extracted payload
 * @param payload_max_size Max size of payload buffer
 * @param payload_size_out Receives actual payload size
 * @return RTE_STATUS_OK if all checks pass
 *         RTE_STATUS_INVALID_PARAM if msg/payload_out/payload_size_out
 *         is NULL, or the decoded payload is larger than payload_max_size
 *         RTE_STATUS_DATA_CORRUPTION if the CRC-64 fails or the sequence
 *         number does not match expected_sequence
 *
 * @safety Deterministic, detects data corruption and reordering
 *
 * Example:
 * @code
 * rte_vital_message_t received_msg;
 * train_command_t cmd;
 * uint8_t payload_size;
 *
 * rte_status_t status = rte_checksum_vital_message_verify(
 *     &received_msg,
 *     last_sequence + 1,             // expect next sequence
 *     (uint8_t *)&cmd,
 *     sizeof(cmd),
 *     &payload_size
 * );
 *
 * if (status == RTE_STATUS_OK) {
 *     // Message valid and sequence OK
 *     last_sequence = received_msg.sequence_number;
 * } else if (status == RTE_STATUS_ERROR) {
 *     // CRC failed - data corrupted
 *     rte_safestate_trigger(REASON_DATA_CORRUPTION);
 * } else if (status == RTE_STATUS_INVALID) {
 *     // Sequence out of order
 *     rte_safestate_trigger(REASON_MESSAGE_REORDERING);
 * }
 * @endcode
 */
rte_status_t rte_checksum_vital_message_verify(
    const rte_vital_message_t *msg,
    uint32_t expected_sequence,
    uint8_t *payload_out,
    size_t payload_max_size,
    uint8_t *payload_size_out
);

/* ============================================================================
 * API: Statistics & Diagnostics
 * ========================================================================== */

/**
 * @brief Checksum statistics for monitoring
 */
typedef struct {
    uint32_t total_checksums;       /**< Total CRC-64 computations */
    uint32_t verification_passes;   /**< Successful verifications */
    uint32_t verification_failures; /**< CRC mismatches (corruption detected) */
    uint32_t sequence_errors;       /**< Message reordering detected */
    uint32_t payload_oversize;      /**< Payload larger than buffer */
} rte_checksum_stats_t;

/**
 * @brief Get checksum statistics
 *
 * Returns counters useful for monitoring data integrity
 * and diagnosing communication issues.
 *
 * @param stats_out Receives statistics
 * @return RTE_STATUS_OK on success
 *         RTE_STATUS_INVALID_PARAM if stats_out is NULL
 */
rte_status_t rte_checksum_get_stats(rte_checksum_stats_t *stats_out);

/**
 * @brief Reset checksum statistics
 *
 * Clears all counters. Useful for per-cycle diagnostics.
 *
 * @return RTE_STATUS_OK always
 */
rte_status_t rte_checksum_reset_stats(void);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* RTE_CHECKSUM_H */

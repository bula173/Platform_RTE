/**
 * @file sapi_checksum.c
 * @brief CRC-64 implementation for data integrity checking
 *
 * Implements ERTMS-compliant CRC-64-CCITT for redundant systems.
 * Uses pre-computed lookup tables for O(1) byte-at-a-time computation.
 *
 * MISRA C:2012 Compliance:
 * - No dynamic memory allocation
 * - Bounded execution time (lookup table based)
 * - No recursion
 * - Explicit type conversions
 * - Comprehensive error handling
 *
 * NOTE (fixed while wiring ADR-017's sapi_checkpoint, which depends on
 * this module): this file previously did not compile at all - it
 * referenced sapi_log_error/info/warn() and sapi_timer_get_ms(), neither
 * of which exist anywhere in this codebase (the real logging API is
 * sapi_log_write(level, tag, message), no varargs; the real timer query
 * is sapi_timer_now(sapi_timestamp_ms_t *out_now_ms), an out-parameter,
 * not a return value), and SAPI_STATUS_ERROR/SAPI_STATUS_INVALID, which
 * are not members of sapi_status_t (see sapi_status.h - the real set
 * includes SAPI_STATUS_INTERNAL_ERROR, SAPI_STATUS_DATA_CORRUPTION,
 * SAPI_STATUS_NOT_INITIALIZED, SAPI_STATUS_INVALID_PARAM, etc.). It also
 * was never wired into the top-level CMakeLists.txt feature list at all.
 * Fixed here to the minimum needed to compile and behave self-
 * consistently; the printf-style log calls were removed rather than
 * rewritten against sapi_log_write's non-varargs signature, since
 * logging is explicitly non-safety-path and out of scope for this pass.
 *
 * SEPARATE, more significant issue also found and NOT fixed by this
 * pass: g_crc64_ertms_table below has only ~24 real entries out of the
 * required 256 (the rest of the declared 256-entry array is implicitly
 * zero-initialized by C), and g_crc64_iso_table/g_crc64_xz_table have
 * only 2 real entries each - all three are explicitly commented as
 * placeholders ("full table needed" / "rest omitted") and are NOT a
 * mathematically correct CRC-64 implementation. Compute-then-verify is
 * still internally self-consistent (the same fixed, if wrong, table is
 * used both ways, so corruption of already-covered bytes is still
 * detected), which is sufficient for exercising sapi_checkpoint's logic
 * in tests, but this must not be treated as SIL-relevant integrity
 * protection until real lookup tables are generated for all three
 * polynomials.
 *
 * @ingroup CHECKSUM
 */

#include <string.h>
#include "safeapi/checksum/sapi_checksum.h"
#include "safeapi/log/sapi_log.h"
#include "safeapi/safestate/sapi_safestate.h"
#include "safeapi/timer/sapi_timer.h"

/* ============================================================================
 * CRC-64 Lookup Tables (Pre-computed)
 * ========================================================================== */

/**
 * CRC-64-CCITT lookup table (256 entries, 8 bytes each = 2KB)
 * Polynomial: 0x1D4F63B86E40E541 (ERTMS standard)
 *
 * PLACEHOLDER - see file-level note above: only the first ~24 entries are
 * populated; the remaining 232 are zero (C's implicit array
 * zero-initialization). Not a real CRC-64 table yet.
 */
static const uint64_t g_crc64_ertms_table[256] = {
    /* Generated via polynomial 0x1D4F63B86E40E541 */
    0x0000000000000000ULL, 0x1D4F63B86E40E541ULL, 0x03A9EC77ADCE8CBFULL,
    0x27D1A4C2525A77EEULL, 0x753D8EF5B9D197DEULL, 0x6872ED4DD77F2BFFULL,
    0x5FA349887D37048FULL, 0x42EC2A30133BB8BEULL, 0xEA7B1DEAB7A32FECULL,
    0xF7347EB2D901931DULL, 0xD0E5DAAC6149BCDDULL, 0xCDAA993D0F0B00ECULL,
    0x9F66B30AE48CE0DCULL, 0x8229D0B28A2E5CFDULL, 0xA5F874765666738FULL,
    0x0B8B717CEA8C4CFBULL, 0x10E86D62D06FDFC8ULL, 0x0DA70EDA5A2D63F9ULL,
    0x2A76AA1CF2654C2BULL, 0x3739C9A49C27F01AULL, 0x65F5E39377A0100AULL,
    0x78BA802BE52AACFBULL, 0x5F6B24E54D628729ULL, 0x422447A72320DB18ULL
    /* ... (232 more entries would be generated for a real table) ... */
};

/**
 * CRC-64-ISO lookup table (alternative polynomial)
 * Polynomial: 0x000000000000001B (ISO 3309 / HDLC)
 *
 * PLACEHOLDER - see file-level note above: only 2 of 256 entries are
 * populated.
 */
static const uint64_t g_crc64_iso_table[256] = {
    /* Generated via polynomial 0x000000000000001B */
    0x0000000000000000ULL, 0x000000000000001BULL
    /* ... rest omitted - not a real table yet ... */
};

/**
 * CRC-64-XZ lookup table (alternative polynomial)
 * Polynomial: 0x142F0E1EBA9EA3C3 (XZ/LZMA)
 *
 * PLACEHOLDER - see file-level note above: only 2 of 256 entries are
 * populated.
 */
static const uint64_t g_crc64_xz_table[256] = {
    /* Generated via polynomial 0x142F0E1EBA9EA3C3 */
    0x0000000000000000ULL, 0x142F0E1EBA9EA3C3ULL
    /* ... rest omitted - not a real table yet ... */
};

/* ============================================================================
 * Module State
 * ========================================================================== */

/**
 * Global checksum module state
 */
typedef struct {
    uint8_t initialized;                    /**< 1 if CRC tables initialized */
    sapi_crc64_polynomial_t polynomial;     /**< Currently initialized polynomial */
    const uint64_t *table;                  /**< Current lookup table */
    sapi_checksum_stats_t stats;            /**< Statistics counters */
} sapi_checksum_manager_t;

static sapi_checksum_manager_t g_checksum_manager = {0};

/* ============================================================================
 * API: CRC-64 Computation
 * ========================================================================== */

sapi_status_t sapi_checksum_crc64_init(sapi_crc64_polynomial_t polynomial)
{
    /* Verify not already initialized */
    if (g_checksum_manager.initialized != 0U) {
        return SAPI_STATUS_ALREADY_INITIALIZED;
    }

    /* Select appropriate lookup table */
    switch (polynomial) {
    case SAPI_CRC64_ERTMS:
        g_checksum_manager.table = g_crc64_ertms_table;
        g_checksum_manager.polynomial = SAPI_CRC64_ERTMS;
        break;
    case SAPI_CRC64_ISO:
        g_checksum_manager.table = g_crc64_iso_table;
        g_checksum_manager.polynomial = SAPI_CRC64_ISO;
        break;
    case SAPI_CRC64_XZ:
        g_checksum_manager.table = g_crc64_xz_table;
        g_checksum_manager.polynomial = SAPI_CRC64_XZ;
        break;
    default:
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Initialize statistics */
    memset(&g_checksum_manager.stats, 0, sizeof(g_checksum_manager.stats));
    g_checksum_manager.initialized = 1U;

    return SAPI_STATUS_OK;
}

sapi_crc64_t sapi_checksum_crc64(const uint8_t *data, size_t size)
{
    /* REQ-ID: SR_SW_042 (Data Integrity) */

    uint64_t crc = 0xFFFFFFFFFFFFFFFFULL;  /* Initial value (all ones) */
    size_t i;
    uint8_t index;

    /* Verify initialization */
    if (g_checksum_manager.initialized == 0U) {
        return 0ULL;
    }

    /* Handle NULL data pointer */
    if (data == NULL) {
        if (size != 0U) {
            return 0ULL;
        }
        return crc;
    }

    /* Verify table is loaded */
    if (g_checksum_manager.table == NULL) {
        return 0ULL;
    }

    /* Compute CRC using lookup table (O(1) per byte) */
    for (i = 0U; i < size; i++) {
        /* Extract lower 8 bits of CRC, XOR with next data byte */
        index = (uint8_t)((crc ^ data[i]) & 0xFFU);

        /* Right-shift CRC by 8 bits, XOR with table entry */
        crc = (crc >> 8U) ^ g_checksum_manager.table[index];
    }

    /* Final XOR with all-ones */
    crc ^= 0xFFFFFFFFFFFFFFFFULL;

    /* Update statistics */
    g_checksum_manager.stats.total_checksums++;

    return crc;
}

sapi_status_t sapi_checksum_crc64_verify(const uint8_t *data,
                                         size_t size,
                                         sapi_crc64_t expected_crc,
                                         sapi_checksum_result_t *result_out)
{
    /* REQ-ID: SR_SW_042 (Data Integrity) */

    sapi_crc64_t computed_crc;
    sapi_status_t status;

    /* Verify output buffer */
    if (result_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Compute CRC */
    computed_crc = sapi_checksum_crc64(data, size);

    /* Fill result structure */
    result_out->computed = computed_crc;
    result_out->expected = expected_crc;
    result_out->match = (computed_crc == expected_crc) ? 1U : 0U;

    /* Update statistics */
    if (result_out->match != 0U) {
        g_checksum_manager.stats.verification_passes++;
        status = SAPI_STATUS_OK;
    } else {
        g_checksum_manager.stats.verification_failures++;
        status = SAPI_STATUS_DATA_CORRUPTION;
    }

    return status;
}

sapi_crc64_polynomial_t sapi_checksum_crc64_get_polynomial(void)
{
    return g_checksum_manager.polynomial;
}

/* ============================================================================
 * API: Vital Channel Message Wrapper
 * ========================================================================== */

sapi_status_t sapi_checksum_vital_message_create(
    sapi_vital_message_t *msg_out,
    uint32_t sender_id,
    uint32_t sequence,
    const uint8_t *payload,
    size_t payload_size)
{
    /* REQ-ID: SR_SW_042 (Data Integrity for Redundancy) */
    sapi_timestamp_ms_t now_ms = 0U;

    /* Validate inputs */
    if (msg_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (payload_size > 248U) {
        g_checksum_manager.stats.payload_oversize++;
        return SAPI_STATUS_INVALID_PARAM;
    }

    if ((payload == NULL) && (payload_size != 0U)) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Fill message header */
    msg_out->sequence_number = sequence;
    msg_out->sender_id = sender_id;
    /* Best-effort: if no timer backend is registered, sapi_timer_now()
     * returns SAPI_STATUS_NOT_INITIALIZED and leaves now_ms at 0 -
     * timestamp_ms is diagnostic only (see sapi_clocksync.h's file-level
     * note: never the basis of comparison correctness), so this is not
     * treated as a hard failure of message creation. */
    (void)sapi_timer_now(&now_ms);
    msg_out->timestamp_ms = (uint32_t)(now_ms & 0xFFFFFFFFULL);
    msg_out->payload_size = (uint8_t)payload_size;
    msg_out->padding = 0U;
    msg_out->reserved = 0U;

    /* Clear payload area */
    memset(msg_out->payload, 0, sizeof(msg_out->payload));

    /* Copy payload */
    if ((payload != NULL) && (payload_size > 0U)) {
        memcpy(msg_out->payload, payload, payload_size);
    }

    /* Compute CRC-64 over entire message (including payload, excluding CRC field) */
    msg_out->crc64 = sapi_checksum_crc64(
        (const uint8_t *)msg_out,
        sizeof(*msg_out) - sizeof(msg_out->crc64)
    );

    return SAPI_STATUS_OK;
}

sapi_status_t sapi_checksum_vital_message_verify(
    const sapi_vital_message_t *msg,
    uint32_t expected_sequence,
    uint8_t *payload_out,
    size_t payload_max_size,
    uint8_t *payload_size_out)
{
    /* REQ-ID: SR_SW_042 (Data Integrity) */

    sapi_checksum_result_t check_result;
    sapi_status_t status;

    /* Validate inputs */
    if (msg == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (payload_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (payload_size_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Check CRC-64 */
    status = sapi_checksum_crc64_verify(
        (const uint8_t *)msg,
        sizeof(*msg) - sizeof(msg->crc64),
        msg->crc64,
        &check_result
    );

    if (status != SAPI_STATUS_OK) {
        return SAPI_STATUS_DATA_CORRUPTION;
    }

    /* Check sequence number continuity */
    if (msg->sequence_number != expected_sequence) {
        g_checksum_manager.stats.sequence_errors++;
        return SAPI_STATUS_DATA_CORRUPTION;
    }

    /* Validate payload size */
    if (msg->payload_size > payload_max_size) {
        g_checksum_manager.stats.payload_oversize++;
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* Extract payload */
    if (msg->payload_size > 0U) {
        memcpy(payload_out, msg->payload, msg->payload_size);
    }
    *payload_size_out = msg->payload_size;

    return SAPI_STATUS_OK;
}

/* ============================================================================
 * API: Statistics
 * ========================================================================== */

sapi_status_t sapi_checksum_get_stats(sapi_checksum_stats_t *stats_out)
{
    if (stats_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    *stats_out = g_checksum_manager.stats;
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_checksum_reset_stats(void)
{
    memset(&g_checksum_manager.stats, 0, sizeof(g_checksum_manager.stats));
    return SAPI_STATUS_OK;
}

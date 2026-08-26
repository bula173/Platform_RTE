/**
 * @page checksum_architecture Checksum Module - Architecture
 *
 * CRC-64 polynomial-based checksums for data integrity. Supports ERTMS,
 * ISO, XZ polynomials. O(n) computation over data buffer.
 *
 * @section checksum_architecture_init Initialization
 *
 * sapi_checksum_init(polynomial) - Initialize lookup tables
 * Call once at startup before using CRC functions.
 *
 * @section checksum_architecture_compute Computation
 *
 * sapi_checksum_crc64(data, length) - Compute CRC-64
 * Returns 64-bit checksum for data integrity verification.
 *
 * @section checksum_architecture_performance O(n) Per Buffer
 *
 * Time proportional to data length. One lookup table per polynomial.
 *
 * @section checksum_architecture_misra MISRA Compliant
 *
 * ✓ No dynamic allocation
 * ✓ Bounded computation
 *
 */


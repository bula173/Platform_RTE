/**
 * @file redundancy.h
 * @brief 2oo2 (2-out-of-2) Redundant Voting Architecture
 *
 * This module implements the core voting logic for SIL 4 dual-channel systems
 * (EN 50128 / EN 50129 railway safety standard).
 *
 * Pattern: Two independent channels continuously compare results. On mismatch,
 * both channels enter safe state (e.g., red signal, speed=0).
 *
 * Hardware Configuration Examples:
 *   - PowerPC dual-core (big-endian): ERTMS RBC classic
 *   - ARM Cortex-A72 dual-core: Modern ERTMS LTE
 *   - x86 dual-socket: Lab/simulation environment
 *
 * @ingroup REDUNDANCY_2OO2
 */

#ifndef REDUNDANCY_H
#define REDUNDANCY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Voting Result Type
 * ========================================================================== */

typedef enum {
    VOTE_MATCH       = 0,  /* Both channels agree */
    VOTE_MISMATCH    = 1,  /* Channels disagree */
    VOTE_TIMEOUT     = 2,  /* One channel did not respond in time */
    VOTE_CHANNEL_A_FAULT = 3,  /* Channel A detected fault */
    VOTE_CHANNEL_B_FAULT = 4,  /* Channel B detected fault */
} vote_result_t;

/* ============================================================================
 * Safety-Related Signal: Train Command
 * ========================================================================== */

typedef struct {
    uint32_t train_id;
    uint32_t position;      /* Absolute position on track (meters) */
    uint8_t signal_state;   /* 0=red, 1=yellow, 2=green */
    uint8_t emergency_stop; /* 1=request emergency stop */
    uint16_t speed_limit;   /* Speed limit (km/h) */
    uint32_t timestamp_ms;  /* When command was computed */
    uint32_t checksum;      /* CRC32 for integrity (SIL 4 requirement) */
} train_command_t;

/* ============================================================================
 * Channel State Tracking
 * ========================================================================== */

typedef struct {
    uint32_t cycle_count;
    uint32_t compute_time_us;
    uint32_t last_timestamp_ms;
    uint8_t channel_healthy;
    uint32_t error_count;
    uint32_t timeout_count;
} channel_stats_t;

typedef struct {
    train_command_t last_command;
    channel_stats_t stats;
} channel_state_t;

/* ============================================================================
 * Voter Configuration (Hardware-Specific)
 * ========================================================================== */

typedef struct {
    /* Timing constraints (EN 50128 SIL 4) */
    uint32_t channel_timeout_ms;    /* Max time to wait for channel response */
    uint32_t max_age_ms;            /* Max age of cached command before recompute */
    uint32_t decision_deadline_ms;  /* Deadline for voting decision */

    /* Safety thresholds */
    uint32_t max_consecutive_mismatches;  /* Trigger safe state if exceeded */
    uint32_t max_channel_errors;          /* Trigger channel shutdown if exceeded */

    /* Hardware configuration */
    const char *channel_a_name;  /* E.g., "CPU0" (PowerPC core 0) */
    const char *channel_b_name;  /* E.g., "CPU1" (PowerPC core 1) */
    uint8_t is_big_endian;       /* 1 if PowerPC/big-endian, 0 if x86/ARM/LE */
} voter_config_t;

/* ============================================================================
 * Voter State Machine
 * ========================================================================== */

typedef struct {
    voter_config_t config;
    channel_state_t channel_a;
    channel_state_t channel_b;
    uint32_t consecutive_mismatches;
    uint32_t total_votes;
    uint32_t total_matches;
    uint8_t voter_healthy;
} voter_state_t;

/* ============================================================================
 * Public API: Voter Management
 * ========================================================================== */

/**
 * @brief Initialize 2oo2 voter
 * @param voter      Voter state (static allocation required)
 * @param config     Hardware-specific voter configuration
 * @return 0 if OK, -1 if invalid config
 *
 * Pre-condition: voter and config must not be NULL
 * Post-condition: voter->voter_healthy = 1 if successful
 */
int redundancy_voter_init(voter_state_t *voter, const voter_config_t *config);

/**
 * @brief Compare two train commands (voting)
 * @param voter      Voter state machine
 * @param cmd_a      Command from channel A
 * @param cmd_b      Command from channel B
 * @param result_out Voting result (0=match, 1=mismatch, 2=timeout, etc.)
 * @param safe_cmd_out Safe command to execute (filled on mismatch)
 * @return 0 if voting successful, -1 on fatal error
 *
 * Safety (SIL 4):
 * - On mismatch: safe_cmd_out contains "safe state" (red signal, speed=0)
 * - On timeout: both channels assumed faulty, enter safe state
 * - Deterministic: O(1) time, no allocation
 *
 * EN 50128 traceability: REQ-2OO2-VOTE-001
 */
int redundancy_vote(voter_state_t *voter,
                    const train_command_t *cmd_a,
                    const train_command_t *cmd_b,
                    vote_result_t *result_out,
                    train_command_t *safe_cmd_out);

/**
 * @brief Get voter statistics for diagnostics
 * @param voter      Voter state
 * @param total_votes_out Total votes executed
 * @param match_rate_out  Percentage of matches (0-100)
 * @return 0 if OK
 */
int redundancy_get_stats(const voter_state_t *voter,
                         uint32_t *total_votes_out,
                         uint8_t *match_rate_out);

/**
 * @brief Shutdown voter and free resources
 * @param voter Voter state
 * @return 0 if OK
 */
int redundancy_voter_shutdown(voter_state_t *voter);

/* ============================================================================
 * Utility: Safe State Generation
 * ========================================================================== */

/**
 * @brief Create safe command (red signal, speed=0)
 * @param train_id Train to stop
 * @param reason Reason for safe state (for logging)
 * @return Safe train command
 *
 * Safety: Returns deterministic safe state. Always safe to call.
 */
train_command_t redundancy_safe_command(uint32_t train_id, const char *reason);

/**
 * @brief Compute CRC32 checksum for integrity check
 * @param data Data to checksum
 * @param len  Length in bytes
 * @return CRC32 value
 *
 * Used to detect corruption in IPC or memory.
 * EN 50128: Data integrity verification (SIL 4 requirement)
 */
uint32_t redundancy_crc32(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* REDUNDANCY_H */

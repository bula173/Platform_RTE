/**
 * @file redundancy_multi_site.h
 * @brief Multi-Site 2oo2 Redundancy with Active/Standby Failover
 *
 * Implements geographic redundancy for railway control systems:
 * - West site (ONLINE): Active processing channel
 * - East site (STANDBY): Backup ready to take over
 * - Each site has 2oo2 voting (channels A, B) + service channel C
 * - Automatic failover on site failure
 *
 * Typical deployment: ERTMS RBC with dual control centers
 *   - West: Primary RBC (Madrid)
 *   - East: Backup RBC (Barcelona) - standby, synced via heartbeat
 *
 * @ingroup REDUNDANCY_MULTI_SITE
 */

#ifndef REDUNDANCY_MULTI_SITE_H
#define REDUNDANCY_MULTI_SITE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Site Identification
 * ========================================================================== */

typedef enum {
    SITE_WEST   = 0,  /* Primary / Online */
    SITE_EAST   = 1,  /* Backup / Standby */
} site_id_t;

typedef enum {
    SITE_STATE_ONLINE    = 0,  /* Active, processing commands */
    SITE_STATE_STANDBY   = 1,  /* Passive, synced but not voting */
    SITE_STATE_ISOLATED  = 2,  /* No comms with other site */
    SITE_STATE_FAULTY    = 3,  /* Self-detected fault, shutdown */
} site_state_t;

/* ============================================================================
 * Safety-Related Command (Same as 2oo2 example)
 * ========================================================================== */

typedef struct {
    uint32_t train_id;
    uint32_t position;
    uint8_t signal_state;      /* 0=red, 1=yellow, 2=green */
    uint8_t emergency_stop;
    uint16_t speed_limit;
    uint32_t timestamp_ms;
    uint32_t checksum;
    uint32_t sequence_number;  /* For ordering & replay detection */
} train_command_t;

/* ============================================================================
 * Per-Site Channel Configuration
 * ========================================================================== */

typedef struct {
    /* Vital channels (2oo2 voting) */
    train_command_t channel_a;  /* CPU0 computation */
    train_command_t channel_b;  /* CPU1 computation */

    /* Service channel (monitoring/output) */
    char service_output[256];   /* Printf output, logging, etc */

    /* Voting result */
    uint8_t voting_result;      /* 0=match, 1=mismatch, 2=timeout */
    train_command_t voted_command;  /* Result of 2oo2 voting */
} site_channels_t;

/* ============================================================================
 * Per-Site Statistics & Health
 * ========================================================================== */

typedef struct {
    uint32_t total_votes;
    uint32_t total_matches;
    uint32_t total_mismatches;
    uint32_t channel_a_errors;
    uint32_t channel_b_errors;
    uint32_t voting_timeouts;
    uint32_t sequence_number;   /* Last processed command sequence */
} site_stats_t;

typedef struct {
    site_id_t site_id;
    site_state_t state;
    site_channels_t channels;
    site_stats_t stats;
    uint32_t heartbeat_timestamp_ms;
    uint32_t heartbeat_miss_count;
} site_context_t;

/* ============================================================================
 * Multi-Site Voter Configuration
 * ========================================================================== */

typedef struct {
    /* Timing */
    uint32_t site_heartbeat_interval_ms;  /* How often to check peer */
    uint32_t heartbeat_timeout_ms;        /* If no response, site is down */
    uint32_t channel_timeout_ms;          /* Voting deadline per site */

    /* Failover thresholds */
    uint32_t max_heartbeat_misses;  /* Trigger failover after N misses */
    uint32_t max_voting_errors;     /* Shutdown site after N errors */

    /* Hardware info */
    const char *west_site_name;  /* E.g., "ERTMS-RBC-West (Madrid)" */
    const char *east_site_name;  /* E.g., "ERTMS-RBC-East (Barcelona)" */
} multi_site_config_t;

/* ============================================================================
 * Multi-Site Voter State
 * ========================================================================== */

typedef struct {
    multi_site_config_t config;
    site_context_t west;
    site_context_t east;
    site_id_t active_site;  /* Which site is online */
} multi_site_voter_t;

/* ============================================================================
 * Public API: Multi-Site Initialization
 * ========================================================================== */

/**
 * @brief Initialize multi-site voter with West online, East standby
 * @param voter Multi-site voter state
 * @param config Configuration
 * @return 0 if OK, -1 on error
 */
int multi_site_voter_init(multi_site_voter_t *voter,
                          const multi_site_config_t *config);

/**
 * @brief Process voting cycle on one site
 *
 * Each site independently:
 * 1. Receives/computes commands from channels A, B
 * 2. Votes (2oo2 comparison)
 * 3. Outputs result via service channel C
 *
 * @param voter Multi-site state
 * @param site Which site (WEST or EAST)
 * @param cmd_a Channel A command
 * @param cmd_b Channel B command
 * @return 0 if voting OK, -1 on fatal error
 */
int multi_site_vote_cycle(multi_site_voter_t *voter,
                          site_id_t site,
                          const train_command_t *cmd_a,
                          const train_command_t *cmd_b);

/**
 * @brief Check heartbeat between sites and trigger failover if needed
 *
 * Called periodically to:
 * - Check if standby site is still responsive
 * - Detect if active site has failed
 * - Transition standby to ONLINE if active is down
 *
 * @param voter Multi-site state
 * @return SITE_WEST or SITE_EAST (current active site)
 */
site_id_t multi_site_heartbeat(multi_site_voter_t *voter);

/**
 * @brief Trigger manual failover (for maintenance)
 * @param voter Multi-site state
 * @param new_active New active site
 * @return 0 if OK
 *
 * Allows graceful switchover without waiting for timeout.
 * Example: "Switch to East for maintenance on West"
 */
int multi_site_failover_request(multi_site_voter_t *voter,
                                 site_id_t new_active);

/**
 * @brief Get voted command from currently active site
 * @param voter Multi-site state
 * @param cmd_out Receives voted command
 * @return 0 if OK
 */
int multi_site_get_active_command(multi_site_voter_t *voter,
                                   train_command_t *cmd_out);

/**
 * @brief Get service output from active site (for printf, logging)
 * @param voter Multi-site state
 * @param output_buf Buffer to receive output
 * @param buf_size Size of buffer
 * @return Number of bytes written
 */
int multi_site_get_service_output(multi_site_voter_t *voter,
                                   char *output_buf,
                                   uint32_t buf_size);

/**
 * @brief Print multi-site statistics for diagnostics
 * @param voter Multi-site state
 */
void multi_site_print_stats(const multi_site_voter_t *voter);

/**
 * @brief Shutdown multi-site voter
 * @param voter Multi-site state
 * @return 0 if OK
 */
int multi_site_voter_shutdown(multi_site_voter_t *voter);

#ifdef __cplusplus
}
#endif

#endif /* REDUNDANCY_MULTI_SITE_H */

/**
 * @file main.c
 * @brief Multi-Site Geographic Redundancy Example
 *
 * Demonstrates realistic railway architecture with two geographic sites:
 *
 * WEST (Online/Active):
 *   - Channel A: CPU0 computes train commands
 *   - Channel B: CPU1 verifies channel A (2oo2 voting)
 *   - Channel C: Service channel (printf results, logging)
 *   - Produces actual control commands
 *
 * EAST (Standby):
 *   - Mirrors WEST channels A, B for synchronization
 *   - Maintains own 2oo2 voting state
 *   - Ready to take over if WEST fails
 *   - Can be switched manually for maintenance
 *
 * Scenario: ERTMS Radio Block Centre with dual sites
 *   - West RBC (Madrid): Primary processing
 *   - East RBC (Barcelona): Backup, can failover
 *   - Heartbeat every 50ms between sites
 *   - On 3 missed heartbeats → automatic failover
 *
 * @ingroup EXAMPLES_MULTI_SITE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "redundancy_multi_site.h"

/* ============================================================================
 * Simulated Trains
 * ========================================================================== */

typedef struct {
    uint32_t train_id;
    uint32_t position;
    uint32_t last_position;
} train_state_t;

static train_state_t trains[] = {
    { .train_id = 101, .position = 500, .last_position = 500 },
    { .train_id = 102, .position = 2500, .last_position = 2500 },
    { .train_id = 103, .position = 7500, .last_position = 7500 },
};

#define TRAIN_COUNT (sizeof(trains) / sizeof(trains[0]))

/* ============================================================================
 * Simulated Channel A (CPU0)
 * ========================================================================== */

static train_command_t channel_a_compute(const train_state_t *train)
{
    train_command_t cmd = {
        .train_id = train->train_id,
        .position = train->position,
        .emergency_stop = 0,
        .timestamp_ms = (uint32_t)(time(NULL) * 1000) & 0xffffffff,
        .sequence_number = 0,
    };

    /* Signal logic */
    if (train->position < 1000) {
        cmd.signal_state = 2;
        cmd.speed_limit = 80;
    } else if (train->position < 5000) {
        cmd.signal_state = 1;
        cmd.speed_limit = 40;
    } else if (train->position < 9000) {
        cmd.signal_state = 0;
        cmd.speed_limit = 0;
    } else {
        cmd.emergency_stop = 1;
        cmd.signal_state = 0;
        cmd.speed_limit = 0;
    }

    /* Compute checksum */
    uint8_t *cmd_bytes = (uint8_t *)&cmd;
    cmd.checksum = 0;
    uint32_t crc = 0xffffffff;
    for (uint32_t i = 0; i < sizeof(cmd) - sizeof(cmd.checksum); i++) {
        uint8_t byte = cmd_bytes[i];
        crc = (crc >> 8) ^ (uint32_t)(crc ^ byte);
    }
    cmd.checksum = crc ^ 0xffffffff;

    return cmd;
}

/* ============================================================================
 * Simulated Channel B (CPU1) - Verifies Channel A
 * ========================================================================== */

static train_command_t channel_b_compute(const train_state_t *train)
{
    /* In real system: independent verification
     * In simulation: mostly same as A, with controlled disagreements
     */
    train_command_t cmd = channel_a_compute(train);

    /* Occasionally introduce mismatch for testing */
    static uint32_t call_count = 0;
    call_count++;

    if ((call_count % 25) == 0) {
        cmd.signal_state = (cmd.signal_state + 1) % 3;
        /* Recompute checksum */
        uint8_t *cmd_bytes = (uint8_t *)&cmd;
        cmd.checksum = 0;
        uint32_t crc = 0xffffffff;
        for (uint32_t i = 0; i < sizeof(cmd) - sizeof(cmd.checksum); i++) {
            uint8_t byte = cmd_bytes[i];
            crc = (crc >> 8) ^ (uint32_t)(crc ^ byte);
        }
        cmd.checksum = crc ^ 0xffffffff;
    }

    return cmd;
}

/* ============================================================================
 * Simulate Train Movement
 * ========================================================================== */

static void simulate_movement(train_state_t *train)
{
    train->last_position = train->position;

    switch (train->train_id) {
        case 101:
            train->position += 5;
            break;
        case 102:
            train->position += 8;
            break;
        case 103:
            train->position += 3;
            break;
    }

    if (train->position > 10000) {
        train->position = 10000;
    }
}

/* ============================================================================
 * Main Simulation
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    multi_site_voter_t voter;
    multi_site_config_t config = {
        .site_heartbeat_interval_ms = 50,
        .heartbeat_timeout_ms = 150,
        .channel_timeout_ms = 25,
        .max_heartbeat_misses = 3,
        .max_voting_errors = 10,
        .west_site_name = "ERTMS-RBC-West (Madrid)",
        .east_site_name = "ERTMS-RBC-East (Barcelona)",
    };

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  Multi-Site Geographic Redundancy Example                    ║\n");
    printf("║  ERTMS Radio Block Centre with Active/Standby Failover       ║\n");
    printf("║                                                               ║\n");
    printf("║  Architecture:                                                ║\n");
    printf("║    WEST (Madrid):     ONLINE  - Vital channels A,B + Svc C   ║\n");
    printf("║    EAST (Barcelona):  STANDBY - Mirrors, ready to failover   ║\n");
    printf("║                                                               ║\n");
    printf("║  Safety: EN 50128 SIL 4 / EN 50129                            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    /* Initialize */
    if (multi_site_voter_init(&voter, &config) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize voter\n");
        return EXIT_FAILURE;
    }

    /* Run simulation: 50 cycles with failover at cycle 25 */
    uint32_t total_cycles = 50;
    uint32_t failover_trigger_cycle = 25;

    for (uint32_t cycle = 0; cycle < total_cycles; cycle++) {
        printf("┌─ Cycle %2u (%s) ─────────────────────────────────────────┐\n",
               cycle + 1,
               voter.active_site == SITE_WEST ? "WEST active" : "EAST active");

        /* Simulate train movement */
        for (uint32_t t = 0; t < TRAIN_COUNT; t++) {
            simulate_movement(&trains[t]);
        }

        /* Each site independently votes */
        for (uint32_t t = 0; t < TRAIN_COUNT; t++) {
            train_state_t *train = &trains[t];

            /* Compute channels A and B */
            train_command_t cmd_a = channel_a_compute(train);
            train_command_t cmd_b = channel_b_compute(train);

            /* WEST votes */
            multi_site_vote_cycle(&voter, SITE_WEST, &cmd_a, &cmd_b);

            /* EAST votes (mirrored) */
            multi_site_vote_cycle(&voter, SITE_EAST, &cmd_a, &cmd_b);

            /* Display active site output (Channel C: Service/printf) */
            if (voter.active_site == SITE_WEST) {
                printf("│ WEST: %s\n", voter.west.channels.service_output);
            } else {
                printf("│ EAST: %s\n", voter.east.channels.service_output);
            }
        }

        /* Check heartbeat and failover */
        multi_site_heartbeat(&voter);

        printf("└────────────────────────────────────────────────────────────┘\n");

        /* Inject fault at specific cycle */
        if (cycle == failover_trigger_cycle) {
            printf("\n");
            printf("⚠️  INJECTING FAULT: WEST site lost heartbeat (simulating failure)\n");
            printf("   Heartbeat miss counter will trigger automatic failover...\n");
            printf("\n");

            /* Simulate WEST losing heartbeat by advancing its last timestamp */
            voter.west.heartbeat_timestamp_ms = 0;
        }

        usleep(50000);  /* 50ms between cycles */
    }

    /* Final statistics */
    printf("\n");
    multi_site_print_stats(&voter);

    /* Shutdown */
    multi_site_voter_shutdown(&voter);

    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  Multi-Site Example Completed Successfully                   ║\n");
    printf("║                                                               ║\n");
    printf("║  Key Features Demonstrated:                                   ║\n");
    printf("║  ✓ Two geographic sites (WEST/EAST)                           ║\n");
    printf("║  ✓ Active/Standby role switching                              ║\n");
    printf("║  ✓ Automatic failover on heartbeat loss                       ║\n");
    printf("║  ✓ 2oo2 vital voting (channels A, B)                          ║\n");
    printf("║  ✓ Service channel (C) for output/logging                     ║\n");
    printf("║  ✓ Dual redundancy across sites                               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    return EXIT_SUCCESS;
}

/**
 * @file main.c
 * @brief 2oo2 Redundant Cluster Example - SIL 4 Railway Safety
 *
 * Demonstrates a dual-channel (2-out-of-2) voting architecture for railway
 * control systems, as specified in EN 50128 / EN 50129 (SIL 4).
 *
 * This example simulates two independent processing channels running on separate
 * CPU cores (or separate systems in a redundant cluster). Each channel:
 *   1. Independently processes train position updates
 *   2. Computes signal commands and speed limits
 *   3. Sends results to a voting arbiter
 *   4. On mismatch, both enter safe state (red signal, speed=0)
 *
 * Supported Hardware Architectures:
 *   - PowerPC 32-bit / 64-bit (big-endian) - typical ERTMS RBC
 *   - ARM Cortex-A (little-endian) - modern ERTMS LTE
 *   - x86-64 (little-endian) - simulation / lab environment
 *   - MIPS (configurable endianness) - legacy railway systems
 *
 * Safety Requirements (EN 50128 SIL 4):
 *   - Deterministic timing: voting completes within deadline
 *   - Fail-safe: on doubt, vote for safe state
 *   - No single point of failure: either channel failure detected
 *   - Data integrity: CRC32 checksums on all safety-critical data
 *   - Traceability: every decision logged with timestamp
 *
 * Compilation:
 *   For native Linux x86-64:
 *     gcc -std=c99 -Wall -Wextra -O2 redundancy.c main.c -o 2oo2-demo
 *   For cross-compilation (PowerPC big-endian):
 *     powerpc-linux-gnu-gcc -std=c99 -Wall -Wextra -O2 \
 *       -DHARDWARE_CONFIG_BIG_ENDIAN redundancy.c main.c -o 2oo2-demo
 *
 * @ingroup EXAMPLES_REDUNDANCY
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "redundancy.h"

/* ============================================================================
 * Hardware Configuration (Compile-Time Selection)
 * ========================================================================== */

#if defined(HARDWARE_CONFIG_BIG_ENDIAN)
    #define IS_BIG_ENDIAN 1
    #define HW_NAME "PowerPC (big-endian)"
#else
    #define IS_BIG_ENDIAN 0
    #define HW_NAME "x86-64/ARM (little-endian)"
#endif

/* ============================================================================
 * Simulation: Two Independent Channels
 * ========================================================================== */

typedef struct {
    uint32_t train_id;
    uint32_t position;      /* Current position (meters) */
    uint32_t last_position; /* For detecting movement */
} train_state_t;

static train_state_t trains[] = {
    { .train_id = 101, .position = 500, .last_position = 500 },
    { .train_id = 102, .position = 2500, .last_position = 2500 },
    { .train_id = 103, .position = 7500, .last_position = 7500 },
};

#define TRAIN_COUNT (sizeof(trains) / sizeof(trains[0]))

/**
 * @brief Channel A: Compute train command (first independent channel)
 *
 * In a real system, this would run on CPU0 with separate cache/memory.
 */
static train_command_t channel_a_compute(const train_state_t *train)
{
    train_command_t cmd = {
        .train_id = train->train_id,
        .position = train->position,
        .emergency_stop = 0,
        .timestamp_ms = (uint32_t)(time(NULL) * 1000) & 0xffffffff,
    };

    /* Signal logic based on position */
    if (train->position < 1000) {
        cmd.signal_state = 2;   /* Green */
        cmd.speed_limit = 80;
    } else if (train->position < 5000) {
        cmd.signal_state = 1;   /* Yellow */
        cmd.speed_limit = 40;
    } else if (train->position < 9000) {
        cmd.signal_state = 0;   /* Red */
        cmd.speed_limit = 0;
    } else {
        cmd.emergency_stop = 1; /* Emergency stop past end of track */
        cmd.signal_state = 0;
        cmd.speed_limit = 0;
    }

    /* Compute integrity checksum (required for SIL 4) */
    uint8_t *cmd_bytes = (uint8_t *)&cmd;
    cmd.checksum = redundancy_crc32(cmd_bytes,
                                     sizeof(cmd) - sizeof(cmd.checksum));

    return cmd;
}

/**
 * @brief Channel B: Compute train command (second independent channel)
 *
 * In a real system, this would run on CPU1 with separate cache/memory.
 * This version has slight intentional differences to show voting behavior.
 */
static train_command_t channel_b_compute(const train_state_t *train)
{
    train_command_t cmd = channel_a_compute(train);

    /* Optional: Introduce controlled variation to demonstrate voting
     * In production, both channels should be byte-identical.
     * We occasionally introduce a mismatch for testing.
     */
    static uint32_t call_count = 0;
    call_count++;

    if ((call_count % 20) == 0) {
        /* Every 20th call, introduce a deliberate mismatch */
        fprintf(stderr,
                "[SIM] Channel B: Injecting signal mismatch for train %u\n",
                train->train_id);
        cmd.signal_state = (cmd.signal_state + 1) % 3;
        /* Recompute checksum */
        uint8_t *cmd_bytes = (uint8_t *)&cmd;
        cmd.checksum = redundancy_crc32(cmd_bytes,
                                         sizeof(cmd) - sizeof(cmd.checksum));
    }

    return cmd;
}

/**
 * @brief Update simulated train position (movement simulation)
 */
static void simulate_train_movement(train_state_t *train)
{
    train->last_position = train->position;

    /* Simulate varying speeds */
    switch (train->train_id) {
        case 101:
            train->position += 5;  /* Train 101 moves 5m per cycle */
            break;
        case 102:
            train->position += 8;  /* Train 102 moves 8m per cycle */
            break;
        case 103:
            train->position += 3;  /* Train 103 moves 3m per cycle */
            break;
        default:
            break;
    }

    /* Boundary check */
    if (train->position > 10000) {
        train->position = 10000;
    }
}

/* ============================================================================
 * Main Simulation Loop
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    voter_state_t voter;
    voter_config_t voter_config = {
        .channel_timeout_ms = 100,
        .max_age_ms = 500,
        .decision_deadline_ms = 50,
        .max_consecutive_mismatches = 3,
        .max_channel_errors = 10,
        .channel_a_name = "CPU0",
        .channel_b_name = "CPU1",
        .is_big_endian = IS_BIG_ENDIAN,
    };

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("2oo2 Redundant Cluster - Railway Control System\n");
    printf("Hardware: %s\n", HW_NAME);
    printf("Safety: EN 50128 SIL 4 / EN 50129 Railway\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    /* Initialize voter */
    if (redundancy_voter_init(&voter, &voter_config) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize voter\n");
        return EXIT_FAILURE;
    }

    /* Main simulation loop: 30 cycles */
    uint32_t total_votes = 0;
    uint32_t total_matches = 0;
    uint32_t total_mismatches = 0;

    for (uint32_t cycle = 0; cycle < 30; cycle++) {
        printf("\n");
        printf("─── Cycle %u ───\n", cycle + 1);

        for (uint32_t t = 0; t < TRAIN_COUNT; t++) {
            train_state_t *train = &trains[t];

            /* Simulate train movement */
            simulate_train_movement(train);

            /* Each channel independently computes the command */
            train_command_t cmd_a = channel_a_compute(train);
            train_command_t cmd_b = channel_b_compute(train);

            /* Vote */
            vote_result_t vote_result;
            train_command_t safe_cmd;

            if (redundancy_vote(&voter, &cmd_a, &cmd_b, &vote_result,
                                &safe_cmd) != 0) {
                fprintf(stderr, "ERROR: Voting failed for train %u\n",
                        train->train_id);
                continue;
            }

            total_votes++;

            /* Process vote result */
            switch (vote_result) {
                case VOTE_MATCH:
                    total_matches++;
                    printf("  Train %u (pos=%u): [✓ MATCH] signal=%u speed=%u\n",
                           train->train_id,
                           train->position,
                           safe_cmd.signal_state,
                           safe_cmd.speed_limit);
                    break;

                case VOTE_MISMATCH:
                    total_mismatches++;
                    printf("  Train %u (pos=%u): [✗ MISMATCH] "
                           "A:sig=%u,spd=%u vs B:sig=%u,spd=%u → SAFE_STATE\n",
                           train->train_id,
                           train->position,
                           cmd_a.signal_state, cmd_a.speed_limit,
                           cmd_b.signal_state, cmd_b.speed_limit);
                    break;

                case VOTE_CHANNEL_A_FAULT:
                    printf("  Train %u (pos=%u): [⚠ CHANNEL_A_FAULT] "
                           "→ SAFE_STATE\n",
                           train->train_id,
                           train->position);
                    break;

                case VOTE_CHANNEL_B_FAULT:
                    printf("  Train %u (pos=%u): [⚠ CHANNEL_B_FAULT] "
                           "→ SAFE_STATE\n",
                           train->train_id,
                           train->position);
                    break;

                default:
                    break;
            }
        }

        /* Brief pause between cycles */
        usleep(100000);  /* 100ms */
    }

    /* Final statistics */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Voting Statistics\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Total votes:       %u\n", total_votes);
    printf("Matches:           %u (%.1f%%)\n", total_matches,
           total_votes > 0 ? (100.0 * total_matches / total_votes) : 0.0);
    printf("Mismatches:        %u\n", total_mismatches);
    printf("═══════════════════════════════════════════════════════════════\n");

    uint32_t stat_votes;
    uint8_t stat_match_rate;
    if (redundancy_get_stats(&voter, &stat_votes, &stat_match_rate) == 0) {
        printf("Voter: total_votes=%u, match_rate=%u%%\n",
               stat_votes, stat_match_rate);
    }

    printf("\n");

    /* Shutdown voter */
    redundancy_voter_shutdown(&voter);

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Demo completed successfully\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    return EXIT_SUCCESS;
}

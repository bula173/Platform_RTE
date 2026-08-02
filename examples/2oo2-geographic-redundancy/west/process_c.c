/**
 * @file west/process_c.c
 * @brief WEST Site - Process C (Non-Vital Service Channel)
 *
 * Service/output channel for logging, diagnostics, and printf.
 * NOT safety-critical. Can be subject to delays and is non-deterministic.
 *
 * Responsibilities:
 * 1. Receive voted commands from Voter process
 * 2. Display results to user (printf)
 * 3. Log diagnostics and statistics
 * 4. Monitor process A and B health
 * 5. Send heartbeat to Coordinator (for failover detection)
 *
 * Safety: Non-vital - failure does not affect voting
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

/* ============================================================================
 * Shared Memory (Same as Processes A & B)
 * ========================================================================== */

typedef struct {
    uint32_t train_id;
    uint32_t position;
    uint8_t signal_state;
    uint8_t emergency_stop;
    uint16_t speed_limit;
    uint32_t timestamp_ms;
    uint32_t checksum;
    uint32_t sequence_number;
} train_command_t;

typedef struct {
    train_command_t cmd_a;
    train_command_t cmd_b;
    uint32_t process_a_seq;
    uint32_t process_b_seq;
    volatile uint8_t ready_a;
    volatile uint8_t ready_b;
    volatile uint8_t process_a_healthy;
    volatile uint8_t process_b_healthy;
    uint32_t timestamp_ms;
} shared_state_t;

__attribute__((aligned(64)))
extern shared_state_t shared_mem;

/* ============================================================================
 * Voted Result (from Voter process)
 * ========================================================================== */

typedef struct {
    train_command_t voted_command;
    uint8_t vote_result;  /* 0=match, 1=mismatch, 2=timeout */
    uint32_t voter_sequence;
    char log_message[256];
} voted_result_t;

__attribute__((aligned(64)))
static voted_result_t voted_result = {0};

/* ============================================================================
 * Statistics
 * ========================================================================== */

typedef struct {
    uint32_t total_results;
    uint32_t matches;
    uint32_t mismatches;
    uint32_t timeouts;
    uint32_t process_a_errors;
    uint32_t process_b_errors;
} service_stats_t;

static service_stats_t stats = {0};

/* ============================================================================
 * Main Loop: Display & Logging
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t cycle_count = 0;
    uint32_t last_voter_seq = 0;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  WEST Site - Process C (Non-Vital Service Channel)        ║\n");
    printf("║  Displays voting results and diagnostics                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    while (1) {
        cycle_count++;

        /* Check process health */
        uint8_t process_a_healthy = shared_mem.process_a_healthy;
        uint8_t process_b_healthy = shared_mem.process_b_healthy;

        if (!process_a_healthy) {
            stats.process_a_errors++;
            printf("[WEST:C] ⚠️  Process A is NOT RESPONDING\n");
        }

        if (!process_b_healthy) {
            stats.process_b_errors++;
            printf("[WEST:C] ⚠️  Process B is NOT RESPONDING\n");
        }

        /* Display voting results (when available from Voter) */
        if (voted_result.voter_sequence != last_voter_seq) {
            stats.total_results++;
            last_voter_seq = voted_result.voter_sequence;

            /* Update statistics based on vote result */
            switch (voted_result.vote_result) {
                case 0:
                    stats.matches++;
                    break;
                case 1:
                    stats.mismatches++;
                    break;
                case 2:
                    stats.timeouts++;
                    break;
            }

            /* Display the result */
            printf("[WEST:C] Cycle %u: ", cycle_count);
            printf("%s\n", voted_result.log_message);
            printf("         → Signal=%u, Speed=%u km/h, EmergencyStop=%u\n",
                   voted_result.voted_command.signal_state,
                   voted_result.voted_command.speed_limit,
                   voted_result.voted_command.emergency_stop);
        }

        /* Periodically print statistics */
        if ((cycle_count % 20) == 0) {
            printf("\n");
            printf("┌────── WEST:C Statistics ──────┐\n");
            printf("│ Total results:  %u\n", stats.total_results);
            printf("│ Matches:        %u (%.0f%%)\n", stats.matches,
                   stats.total_results > 0 ?
                   (100.0 * stats.matches / stats.total_results) : 0.0);
            printf("│ Mismatches:     %u\n", stats.mismatches);
            printf("│ Timeouts:       %u\n", stats.timeouts);
            printf("│ Process A err:  %u\n", stats.process_a_errors);
            printf("│ Process B err:  %u\n", stats.process_b_errors);
            printf("│ A healthy: %s, B healthy: %s\n",
                   process_a_healthy ? "YES" : "NO ",
                   process_b_healthy ? "YES" : "NO ");
            printf("└───────────────────────────────┘\n");
            printf("\n");
        }

        /* Service can be slow (non-deterministic) */
        usleep(50000);  /* 50ms - not deterministic like A & B */
    }

    return EXIT_SUCCESS;
}

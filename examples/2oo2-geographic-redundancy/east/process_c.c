/**
 * @file east/process_c.c
 * @brief EAST Site - Process C (Non-Vital Service Channel)
 *
 * Service/output channel for EAST site (Standby RBC).
 * Mirrors WEST Process C, ready to display results if promoted.
 * Sends heartbeat to Coordinator for failover detection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

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

typedef struct {
    train_command_t voted_command;
    uint8_t vote_result;
    uint32_t voter_sequence;
    char log_message[256];
} voted_result_t;

__attribute__((aligned(64)))
static voted_result_t voted_result = {0};

typedef struct {
    uint32_t total_results;
    uint32_t matches;
    uint32_t mismatches;
    uint32_t timeouts;
    uint32_t process_a_errors;
    uint32_t process_b_errors;
} service_stats_t;

static service_stats_t stats = {0};

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t cycle_count = 0;
    uint32_t last_voter_seq = 0;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  EAST Site - Process C (Standby Service Channel)          ║\n");
    printf("║  Ready to display results if promoted to ONLINE           ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    while (1) {
        cycle_count++;

        uint8_t process_a_healthy = shared_mem.process_a_healthy;
        uint8_t process_b_healthy = shared_mem.process_b_healthy;

        if (!process_a_healthy) {
            stats.process_a_errors++;
            printf("[EAST:C] ⚠️  Process A is NOT RESPONDING (STANDBY)\n");
        }

        if (!process_b_healthy) {
            stats.process_b_errors++;
            printf("[EAST:C] ⚠️  Process B is NOT RESPONDING (STANDBY)\n");
        }

        if (voted_result.voter_sequence != last_voter_seq) {
            stats.total_results++;
            last_voter_seq = voted_result.voter_sequence;

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

            printf("[EAST:C] Cycle %u (STANDBY mirrored): ", cycle_count);
            printf("%s\n", voted_result.log_message);
        }

        if ((cycle_count % 20) == 0) {
            printf("\n");
            printf("┌────── EAST:C Statistics ──────┐\n");
            printf("│ Role: STANDBY (ready to failover)\n");
            printf("│ Total results mirrored: %u\n", stats.total_results);
            printf("│ Matches:   %u (%.0f%%)\n", stats.matches,
                   stats.total_results > 0 ?
                   (100.0 * stats.matches / stats.total_results) : 0.0);
            printf("│ Mismatches: %u\n", stats.mismatches);
            printf("│ A healthy: %s, B healthy: %s\n",
                   process_a_healthy ? "YES" : "NO ",
                   process_b_healthy ? "YES" : "NO ");
            printf("└───────────────────────────────┘\n");
            printf("\n");
        }

        usleep(50000);
    }

    return EXIT_SUCCESS;
}

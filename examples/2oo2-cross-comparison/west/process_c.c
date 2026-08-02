/**
 * @file west/process_c.c
 * @brief Process C - Non-Vital Service Channel
 *
 * Service channel for cross-comparison architecture:
 * 1. Reads voted results from A & B
 * 2. Displays to user via printf
 * 3. Monitors for output mismatches
 * 4. Non-vital, can skip frames
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
    train_command_t cmd_a_out;
    train_command_t cmd_b_out;
    volatile uint32_t a_ready;
    volatile uint32_t b_ready;
    volatile uint32_t a_compared;
    volatile uint32_t b_compared;
    volatile uint8_t outputs_match;
    uint32_t mismatch_count;
    uint32_t sequence;
    uint32_t timestamp_ms;
} shared_state_t;

__attribute__((aligned(64)))
extern shared_state_t shared_mem;

typedef struct {
    uint32_t total_cycles;
    uint32_t matches;
    uint32_t mismatches;
    uint32_t output_errors;
} service_stats_t;

static service_stats_t stats = {0};

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t cycle_count = 0;
    uint32_t last_sequence = 0;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║ Process C - Non-Vital Service Channel                 ║\n");
    printf("║ Displays voted results, monitors A/B agreement        ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");

    while (1) {
        cycle_count++;

        /* Check if A and B have voted */
        if (shared_mem.sequence != last_sequence) {
            stats.total_cycles++;
            last_sequence = shared_mem.sequence;

            /* Display voted command */
            printf("[WEST:C] Cycle %u:\n", cycle_count);

            if (shared_mem.outputs_match) {
                stats.matches++;
                printf("         A & B AGREE ✓\n");
            } else {
                stats.output_errors++;
                printf("         A & B DISAGREE ✗ (SHOULD NOT HAPPEN!)\n");
            }

            printf("         Joint Output: train=%u, sig=%u, speed=%u km/h\n",
                   shared_mem.cmd_a_out.train_id,
                   shared_mem.cmd_a_out.signal_state,
                   shared_mem.cmd_a_out.speed_limit);

            printf("         A input: sig=%u, speed=%u | "
                   "B input: sig=%u, speed=%u\n",
                   shared_mem.cmd_a.signal_state,
                   shared_mem.cmd_a.speed_limit,
                   shared_mem.cmd_b.signal_state,
                   shared_mem.cmd_b.speed_limit);

            /* Detect mismatches (comparison disagreement) */
            if ((shared_mem.cmd_a.signal_state != shared_mem.cmd_b.signal_state ||
                 shared_mem.cmd_a.speed_limit != shared_mem.cmd_b.speed_limit) &&
                (shared_mem.cmd_a_out.signal_state == 0 &&
                 shared_mem.cmd_a_out.speed_limit == 0)) {

                stats.mismatches++;
                printf("         ⚠️  MISMATCH DETECTED (contained via safe state)\n");
            }

            printf("\n");
        }

        /* Periodic statistics */
        if ((cycle_count % 20) == 0) {
            printf("┌────── Statistics ──────┐\n");
            printf("│ Total cycles: %u\n", stats.total_cycles);
            printf("│ Agreements: %u (%.0f%%)\n", stats.matches,
                   stats.total_cycles > 0 ?
                   (100.0 * stats.matches / stats.total_cycles) : 0.0);
            printf("│ Mismatches detected: %u\n", stats.mismatches);
            printf("│ Output errors: %u\n", stats.output_errors);
            printf("└────────────────────────┘\n");
            printf("\n");
        }

        usleep(50000);  /* Non-deterministic service */
    }

    return EXIT_SUCCESS;
}

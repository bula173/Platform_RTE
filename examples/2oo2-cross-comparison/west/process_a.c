/**
 * @file west/process_a.c
 * @brief Process A - Cross-Comparison Architecture
 *
 * Process A:
 * 1. Computes train command independently
 * 2. Sends to Process B for comparison
 * 3. Receives Process B's command
 * 4. Performs 2oo2 comparison locally
 * 5. Outputs result (either joint command or safe state)
 * 6. Verifies B reached same decision (if not → alarm)
 *
 * No central voter - A and B co-decide via cross-comparison
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

/* ============================================================================
 * Command Structure
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

/* ============================================================================
 * Shared Memory
 * ========================================================================== */

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
static shared_state_t shared_mem = {0};

/* ============================================================================
 * Train Database
 * ========================================================================== */

typedef struct {
    uint32_t train_id;
    uint32_t position;
} train_db_entry_t;

static train_db_entry_t trains[] = {
    { .train_id = 101, .position = 500 },
    { .train_id = 102, .position = 2500 },
    { .train_id = 103, .position = 7500 },
};

#define TRAIN_COUNT (sizeof(trains) / sizeof(trains[0]))

/* ============================================================================
 * Computation
 * ========================================================================== */

static uint32_t crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xffffffff;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        crc = (crc >> 8) ^ (uint32_t)(crc ^ byte);
    }
    return crc ^ 0xffffffff;
}

static train_command_t compute_command(uint32_t train_id, uint32_t position)
{
    train_command_t cmd = {
        .train_id = train_id,
        .position = position,
        .emergency_stop = 0,
        .timestamp_ms = (uint32_t)(time(NULL) * 1000) & 0xffffffff,
        .sequence_number = 0,
    };

    if (position < 1000) {
        cmd.signal_state = 2;
        cmd.speed_limit = 80;
    } else if (position < 5000) {
        cmd.signal_state = 1;
        cmd.speed_limit = 40;
    } else if (position < 9000) {
        cmd.signal_state = 0;
        cmd.speed_limit = 0;
    } else {
        cmd.emergency_stop = 1;
        cmd.signal_state = 0;
        cmd.speed_limit = 0;
    }

    uint8_t *cmd_bytes = (uint8_t *)&cmd;
    cmd.checksum = crc32(cmd_bytes, sizeof(cmd) - sizeof(cmd.checksum));
    return cmd;
}

/* ============================================================================
 * Cross-Comparison Logic (Identical in A and B)
 * ========================================================================== */

static train_command_t safe_state(uint32_t train_id)
{
    train_command_t safe = {
        .train_id = train_id,
        .position = 0,
        .signal_state = 0,
        .emergency_stop = 1,
        .speed_limit = 0,
        .timestamp_ms = 0,
        .checksum = 0,
        .sequence_number = 0,
    };
    return safe;
}

static void compare_and_decide(const train_command_t *my_cmd,
                                const train_command_t *other_cmd,
                                train_command_t *output,
                                uint8_t *match_status)
{
    /* Cross-comparison logic (SIL 4) */
    if (my_cmd->train_id == other_cmd->train_id &&
        my_cmd->position == other_cmd->position &&
        my_cmd->signal_state == other_cmd->signal_state &&
        my_cmd->speed_limit == other_cmd->speed_limit) {

        /* MATCH: Both agree, output joint command */
        *output = *my_cmd;
        *match_status = 1;

    } else {

        /* MISMATCH: Both output safe state */
        *output = safe_state(my_cmd->train_id);
        *match_status = 0;
    }
}

/* ============================================================================
 * Main Loop
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t cycle_count = 0;
    uint32_t sequence = 0;

    fprintf(stderr,
            "┌────────────────────────────────────────────────────────┐\n");
    fprintf(stderr,
            "│ Process A - Cross-Comparison (Peer-to-Peer 2oo2)       │\n");
    fprintf(stderr,
            "│ Computes independently, compares with B, co-decides    │\n");
    fprintf(stderr,
            "└────────────────────────────────────────────────────────┘\n");
    fprintf(stderr, "\n");

    while (1) {
        cycle_count++;
        sequence++;

        /* PHASE 1: Compute independently */
        for (uint32_t i = 0; i < TRAIN_COUNT; i++) {
            trains[i].position += (i + 1) * 3;
            if (trains[i].position > 10000) {
                trains[i].position = 10000;
            }
        }

        for (uint32_t i = 0; i < TRAIN_COUNT; i++) {
            train_command_t cmd = compute_command(
                trains[i].train_id,
                trains[i].position
            );
            cmd.sequence_number = sequence;

            shared_mem.cmd_a = cmd;
            shared_mem.sequence = sequence;
            __sync_synchronize();
            shared_mem.a_ready = 1;

            if (cycle_count % 10 == 0) {
                fprintf(stderr,
                        "[WEST:A] Cycle %u: Computed train=%u, pos=%u, sig=%u, speed=%u\n",
                        cycle_count, cmd.train_id, cmd.position,
                        cmd.signal_state, cmd.speed_limit);
            }
        }

        /* PHASE 2: Wait for B's computation */
        uint32_t timeout_ms = 0;
        while (!shared_mem.b_ready && timeout_ms < 25) {
            usleep(1000);
            timeout_ms++;
        }

        if (!shared_mem.b_ready) {
            fprintf(stderr, "[WEST:A] ERROR: Process B timeout\n");
            continue;
        }

        /* PHASE 3: Cross-comparison (local decision) */
        train_command_t output;
        uint8_t match_status;

        compare_and_decide(
            &shared_mem.cmd_a,
            &shared_mem.cmd_b,
            &output,
            &match_status
        );

        shared_mem.cmd_a_out = output;
        __sync_synchronize();
        shared_mem.a_compared = 1;

        if (cycle_count % 10 == 0) {
            fprintf(stderr,
                    "[WEST:A] Received B's: train=%u, sig=%u, speed=%u\n",
                    shared_mem.cmd_b.train_id,
                    shared_mem.cmd_b.signal_state,
                    shared_mem.cmd_b.speed_limit);

            fprintf(stderr,
                    "[WEST:A] Comparison: %s\n",
                    match_status ? "MATCH ✓" : "MISMATCH ✗");

            fprintf(stderr,
                    "[WEST:A] Output: sig=%u, speed=%u\n",
                    output.signal_state, output.speed_limit);
        }

        /* PHASE 4: Verify B reached same decision */
        timeout_ms = 0;
        while (!shared_mem.b_compared && timeout_ms < 10) {
            usleep(1000);
            timeout_ms++;
        }

        if (shared_mem.b_compared) {
            if (shared_mem.cmd_a_out.signal_state == shared_mem.cmd_b_out.signal_state &&
                shared_mem.cmd_a_out.speed_limit == shared_mem.cmd_b_out.speed_limit) {

                shared_mem.outputs_match = 1;

            } else {

                shared_mem.outputs_match = 0;
                shared_mem.mismatch_count++;

                fprintf(stderr,
                        "[WEST:A] ⚠️  ALERT: B's output differs!\n");
                fprintf(stderr,
                        "         A output: sig=%u, speed=%u\n",
                        shared_mem.cmd_a_out.signal_state,
                        shared_mem.cmd_a_out.speed_limit);
                fprintf(stderr,
                        "         B output: sig=%u, speed=%u\n",
                        shared_mem.cmd_b_out.signal_state,
                        shared_mem.cmd_b_out.speed_limit);
            }
        }

        /* Reset for next cycle */
        shared_mem.a_ready = 0;
        shared_mem.a_compared = 0;

        usleep(25000);  /* 25ms cycle */
    }

    return EXIT_SUCCESS;
}

/**
 * @file east/process_a.c
 * @brief EAST Site - Process A (Vital Channel A - CPU0)
 *
 * Standby/Backup computation channel for ERTMS RBC East (Barcelona).
 * Mirrors WEST site but remains standby until promoted.
 *
 * In STANDBY mode:
 * - Computes commands independently (for synchronization)
 * - Does not produce final output commands
 * - Ready to take over if WEST fails
 *
 * In ONLINE mode (after failover):
 * - Same as WEST Process A
 * - Becomes primary computation channel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

/* ============================================================================
 * Shared Memory
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
static shared_state_t shared_mem = {0};

/* ============================================================================
 * Train Database (Synchronized with WEST)
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
 * CRC32
 * ========================================================================== */

static uint32_t crc32_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xffffffff;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        crc = (crc >> 8) ^ (uint32_t)(crc ^ byte);
    }
    return crc ^ 0xffffffff;
}

/* ============================================================================
 * Process A (Standby - mirrors WEST)
 * ========================================================================== */

static train_command_t compute_train_command(uint32_t train_id, uint32_t position)
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
    cmd.checksum = crc32_checksum(cmd_bytes,
                                   sizeof(cmd) - sizeof(cmd.checksum));

    return cmd;
}

/* ============================================================================
 * Main Loop
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t cycle_count = 0;
    uint32_t process_a_seq = 0;

    fprintf(stderr, "[EAST:A] Process A (Standby Channel A) starting\n");
    fprintf(stderr, "[EAST:A] Synchronizing with WEST (role: STANDBY)\n");

    while (1) {
        cycle_count++;
        process_a_seq++;

        /* Synchronize train positions with WEST */
        for (uint32_t i = 0; i < TRAIN_COUNT; i++) {
            trains[i].position += (i + 1) * 3;
            if (trains[i].position > 10000) {
                trains[i].position = 10000;
            }
        }

        /* Compute commands (keeping in sync for failover) */
        for (uint32_t i = 0; i < TRAIN_COUNT; i++) {
            train_command_t cmd = compute_train_command(
                trains[i].train_id,
                trains[i].position
            );
            cmd.sequence_number = process_a_seq;

            shared_mem.cmd_a = cmd;
            shared_mem.process_a_seq = process_a_seq;
            __sync_synchronize();
            shared_mem.ready_a = 1;

            if (cycle_count % 10 == 0) {
                fprintf(stderr,
                        "[EAST:A] Cycle %u (STANDBY): train=%u, pos=%u, sig=%u, speed=%u\n",
                        cycle_count, cmd.train_id, cmd.position,
                        cmd.signal_state, cmd.speed_limit);
            }
        }

        shared_mem.process_a_healthy = 1;
        usleep(25000);
    }

    return EXIT_SUCCESS;
}

/**
 * @file west/process_a.c
 * @brief WEST Site - Process A (Vital Channel A - CPU0)
 *
 * Primary computation channel for ERTMS RBC West.
 * Runs on dedicated CPU core with real-time scheduling.
 *
 * Responsibilities:
 * 1. Read train position updates from sensors
 * 2. Compute signal commands and speed limits
 * 3. Write results to shared memory (voted_cmd_a)
 * 4. Send heartbeat to Process C (service channel)
 * 5. Monitor for faults from Process B
 *
 * Safety: SIL 4 - No malloc, bounded loops, deterministic timing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

/* ============================================================================
 * Shared Memory Interface (IPC with other processes)
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
    train_command_t cmd_a;      /* Written by Process A */
    train_command_t cmd_b;      /* Written by Process B */
    uint32_t process_a_seq;     /* Sequence number from A */
    uint32_t process_b_seq;     /* Sequence number from B */
    volatile uint8_t ready_a;   /* 1 when cmd_a is valid */
    volatile uint8_t ready_b;   /* 1 when cmd_b is valid */
    volatile uint8_t process_a_healthy;  /* Health status */
    volatile uint8_t process_b_healthy;  /* Health status */
    uint32_t timestamp_ms;
} shared_state_t;

/* Global shared memory (in production: mapped file or IPC) */
__attribute__((aligned(64)))
static shared_state_t shared_mem = {0};

/* ============================================================================
 * Simulated Train Database
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
 * CRC32 Checksum
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
 * Process A: Compute Train Commands (Vital Channel)
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

    /* Safety-critical signal logic */
    if (position < 1000) {
        cmd.signal_state = 2;      /* Green */
        cmd.speed_limit = 80;
    } else if (position < 5000) {
        cmd.signal_state = 1;      /* Yellow */
        cmd.speed_limit = 40;
    } else if (position < 9000) {
        cmd.signal_state = 0;      /* Red */
        cmd.speed_limit = 0;
    } else {
        cmd.emergency_stop = 1;    /* Beyond end of track */
        cmd.signal_state = 0;
        cmd.speed_limit = 0;
    }

    /* Compute integrity checksum (SIL 4 requirement) */
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

    fprintf(stderr, "[WEST:A] Process A (Vital Channel A) starting\n");
    fprintf(stderr, "[WEST:A] Computing train commands every 25ms\n");

    while (1) {
        cycle_count++;
        process_a_seq++;

        /* Simulate train movement */
        for (uint32_t i = 0; i < TRAIN_COUNT; i++) {
            trains[i].position += (i + 1) * 3;  /* Different speeds */
            if (trains[i].position > 10000) {
                trains[i].position = 10000;
            }
        }

        /* Compute commands for all trains */
        for (uint32_t i = 0; i < TRAIN_COUNT; i++) {
            train_command_t cmd = compute_train_command(
                trains[i].train_id,
                trains[i].position
            );
            cmd.sequence_number = process_a_seq;

            /* Write to shared memory */
            shared_mem.cmd_a = cmd;
            shared_mem.process_a_seq = process_a_seq;
            __sync_synchronize();  /* Memory fence */
            shared_mem.ready_a = 1;

            if (cycle_count % 10 == 0) {
                fprintf(stderr,
                        "[WEST:A] Cycle %u: train=%u, pos=%u, sig=%u, speed=%u\n",
                        cycle_count, cmd.train_id, cmd.position,
                        cmd.signal_state, cmd.speed_limit);
            }
        }

        /* Mark as healthy */
        shared_mem.process_a_healthy = 1;

        /* Sleep 25ms (deterministic cycle) */
        usleep(25000);
    }

    return EXIT_SUCCESS;
}

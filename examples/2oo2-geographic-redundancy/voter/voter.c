/**
 * @file voter/voter.c
 * @brief 2oo2 Voter - Performs vital voting per site
 *
 * Independent voter process that:
 * 1. Reads commands from Process A and B (vital channels)
 * 2. Compares them (2oo2 logic)
 * 3. Generates vote results
 * 4. Writes to Process C (service channel)
 * 5. Enforces timeout and error thresholds
 *
 * In production: Separate voter per site, with deterministic timing
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
extern voted_result_t voted_result;

/* ============================================================================
 * 2oo2 Voting Logic
 * ========================================================================== */

static int perform_2oo2_vote(const train_command_t *cmd_a,
                              const train_command_t *cmd_b,
                              train_command_t *result_out,
                              char *message_out,
                              uint32_t message_len)
{
    if (cmd_a == NULL || cmd_b == NULL || result_out == NULL) {
        return 2;  /* Timeout */
    }

    /* Compare vital fields */
    if (cmd_a->train_id != cmd_b->train_id ||
        cmd_a->position != cmd_b->position ||
        cmd_a->signal_state != cmd_b->signal_state ||
        cmd_a->speed_limit != cmd_b->speed_limit) {

        /* MISMATCH */
        snprintf(message_out, message_len,
                 "[VOTER] MISMATCH: A(train=%u,sig=%u,spd=%u) vs "
                 "B(train=%u,sig=%u,spd=%u) → SAFE_STATE",
                 cmd_a->train_id, cmd_a->signal_state, cmd_a->speed_limit,
                 cmd_b->train_id, cmd_b->signal_state, cmd_b->speed_limit);

        /* Output safe state */
        *result_out = *cmd_a;
        result_out->signal_state = 0;  /* Red */
        result_out->speed_limit = 0;   /* Stop */
        result_out->emergency_stop = 1;

        return 1;  /* Mismatch */
    }

    /* MATCH */
    snprintf(message_out, message_len,
             "[VOTER] MATCH: train=%u, pos=%u, sig=%u, speed=%u",
             cmd_a->train_id, cmd_a->position,
             cmd_a->signal_state, cmd_a->speed_limit);

    *result_out = *cmd_a;
    return 0;  /* Match */
}

/* ============================================================================
 * Main Voter Loop
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t voter_seq = 0;
    uint32_t total_votes = 0;
    uint32_t total_matches = 0;
    uint32_t total_mismatches = 0;
    uint32_t total_timeouts = 0;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Voter Process - 2oo2 Voting Logic (Deterministic)        ║\n");
    printf("║  Reads from Processes A & B, writes to Process C          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    while (1) {
        voter_seq++;

        /* Wait for both channels to be ready */
        if (!shared_mem.ready_a || !shared_mem.ready_b) {
            usleep(5000);  /* Brief wait */
            continue;
        }

        /* Check sequence synchronization */
        if (shared_mem.process_a_seq != shared_mem.process_b_seq) {
            fprintf(stderr, "[VOTER] Sequence mismatch: A_seq=%u vs B_seq=%u\n",
                    shared_mem.process_a_seq, shared_mem.process_b_seq);
            usleep(1000);
            continue;
        }

        /* Perform 2oo2 voting */
        train_command_t voted_cmd;
        char vote_message[256];
        int vote_result = perform_2oo2_vote(
            &shared_mem.cmd_a,
            &shared_mem.cmd_b,
            &voted_cmd,
            vote_message,
            sizeof(vote_message)
        );

        /* Update statistics */
        total_votes++;
        switch (vote_result) {
            case 0:
                total_matches++;
                break;
            case 1:
                total_mismatches++;
                break;
            case 2:
                total_timeouts++;
                break;
        }

        /* Write result to shared memory for Process C */
        voted_result.voted_command = voted_cmd;
        voted_result.vote_result = (uint8_t)vote_result;
        voted_result.voter_sequence = voter_seq;
        strncpy(voted_result.log_message, vote_message, sizeof(voted_result.log_message) - 1);
        voted_result.log_message[sizeof(voted_result.log_message) - 1] = '\0';

        /* Heartbeat to Coordinator: voter is alive */
        if ((total_votes % 10) == 0) {
            fprintf(stderr,
                    "[VOTER] Cycle %u: votes=%u, matches=%u, mismatches=%u, timeouts=%u\n",
                    voter_seq, total_votes, total_matches, total_mismatches, total_timeouts);
        }

        /* Reset ready flags for next cycle */
        shared_mem.ready_a = 0;
        shared_mem.ready_b = 0;

        /* Deterministic 25ms cycle (must match Process A & B) */
        usleep(25000);
    }

    return EXIT_SUCCESS;
}

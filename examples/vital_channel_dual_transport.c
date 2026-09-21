/**
 * @file vital_channel_dual_transport.c
 * @brief Example: Voted Channel with Dual Transport (Shared Memory + TCP/IP)
 *
 * Demonstrates how to:
 * 1. Create two redundant communication links using different transports
 * 2. Register them into a rte_voter_t for 2oo2 voting/redundancy (ADR-025)
 * 3. Support user selection of online/standby channel configurations
 * 4. Monitor health for fault detection
 *
 * This pattern enables:
 * - Online process communicates via fast shared memory to local standby
 * - Online process communicates via TCP to remote standby (fallback)
 * - Voting ensures data consistency across both standbies
 * - If one channel fails, voting detects and triggers safe-state
 *
 * ADR-025 split what used to be a single "vital channel" type (one
 * transport handle + built-in N-way voting) into rte_channel (one
 * link) plus rte_voter (voting across N registered links). This
 * example registers one rte_channel_t per transport into one
 * rte_voter_t configured for RTE_VOTING_2OO2.
 *
 * Compile:
 *   gcc -std=c99 -Wall -Wextra -o vital_dual_transport \
 *       vital_channel_dual_transport.c \
 *       -I../include -L../build -lsafeapi_channels -lsafeapi_oal \
 *       -lsafeapi_core
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "safeapi/channel_link/rte_channel.h"
#include "safeapi/voter/rte_voter.h"
#include "safeapi/status/rte_status.h"

/* ============================================================================
 * Transport OSAdapter 1: Shared Memory Queue (Simulated)
 * ========================================================================== */

typedef struct {
    uint32_t write_idx;
    uint32_t read_idx;
    uint32_t count;
    uint8_t data[10][256];  /* Queue of 10 messages, each 256 bytes */
} shm_queue_t;

/* For example purposes: one shared queue per channel */
static shm_queue_t shm_queue_0 = {0};

static rte_status_t shm_send(shm_queue_t *q, const void *data, size_t size)
{
    if (q->count >= 10) {
        return RTE_STATUS_TIMEOUT;  /* Queue full */
    }

    if (size > 256) {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }

    memcpy(q->data[q->write_idx], data, size);
    q->write_idx = (q->write_idx + 1) % 10;
    q->count++;

    return RTE_STATUS_OK;
}

static rte_status_t shm_recv(shm_queue_t *q, void *data, size_t size)
{
    if (q->count == 0) {
        return RTE_STATUS_TIMEOUT;  /* Queue empty */
    }

    if (size < 256) {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }

    memcpy(data, q->data[q->read_idx], 256);
    q->read_idx = (q->read_idx + 1) % 10;
    q->count--;

    return RTE_STATUS_OK;
}

/* ============================================================================
 * Transport OSAdapter 2: TCP/IP Socket (Simulated)
 * ========================================================================== */

typedef struct {
    int socket;
    uint32_t packets_sent;
    uint32_t packets_recv;
    bool connected;
} tcp_channel_t;

static tcp_channel_t tcp_ch_0 = {.socket = 0, .connected = true};

static rte_status_t tcp_send(tcp_channel_t *ch, const void *data, size_t size)
{
    if (ch->socket < 0 || !ch->connected) {
        return RTE_STATUS_HARDWARE_FAULT;
    }

    if (size > 256) {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }

    /* In real implementation:
     * - Add message framing header (length prefix)
     * - Call socket send() with timeout
     * - Check for EAGAIN (queue full)
     */

    ch->packets_sent++;
    return RTE_STATUS_OK;
}

static rte_status_t tcp_recv(tcp_channel_t *ch, void *data, size_t size)
{
    if (ch->socket < 0 || !ch->connected) {
        return RTE_STATUS_HARDWARE_FAULT;
    }

    if (size < 256) {
        return RTE_STATUS_RESOURCE_EXHAUSTED;
    }

    /* In real implementation:
     * - Call socket recv() with timeout
     * - Parse message framing
     * - Handle partial receives
     */

    ch->packets_recv++;
    return RTE_STATUS_OK;
}

/* ============================================================================
 * Multi-Transport OSAdapter Callbacks (each rte_channel_t uses one of these)
 * ========================================================================== */

/**
 * @brief Opaque channel type that hides whether it's SHM or TCP
 *
 * This allows each rte_channel_t to wrap ANY transport. The callback
 * functions dispatch based on channel type.
 */
typedef enum {
    CHANNEL_TYPE_SHARED_MEMORY,
    CHANNEL_TYPE_TCP
} channel_type_t;

typedef struct {
    channel_type_t type;
    union {
        shm_queue_t *shm;
        tcp_channel_t *tcp;
    } impl;
} multi_transport_channel_t;

static multi_transport_channel_t g_channel_0 = {
    .type = CHANNEL_TYPE_SHARED_MEMORY,
    .impl.shm = &shm_queue_0,
};

static multi_transport_channel_t g_channel_1 = {
    .type = CHANNEL_TYPE_TCP,
    .impl.tcp = &tcp_ch_0,
};

/**
 * @brief OSAdapter send callback for rte_channel (dispatches to transport)
 *
 * Called by rte_channel_send() for one link.
 * The callback hides the specific transport implementation.
 */
static rte_status_t channel_backend_send(void *channel_handle, const void *data, size_t size)
{
    multi_transport_channel_t *ch = (multi_transport_channel_t *)channel_handle;

    if (ch == NULL) {
        return RTE_STATUS_INVALID_PARAM;
    }

    switch (ch->type) {
    case CHANNEL_TYPE_SHARED_MEMORY:
        return shm_send(ch->impl.shm, data, size);

    case CHANNEL_TYPE_TCP:
        return tcp_send(ch->impl.tcp, data, size);

    default:
        return RTE_STATUS_INVALID_PARAM;
    }
}

/**
 * @brief OSAdapter receive callback for rte_channel (dispatches to transport)
 *
 * Called by rte_channel_receive() for one link.
 * The callback hides the specific transport implementation.
 */
static rte_status_t channel_backend_recv(void *channel_handle, void *data, size_t size,
                                           uint32_t timeout_ms __attribute__((unused)))
{
    multi_transport_channel_t *ch = (multi_transport_channel_t *)channel_handle;

    if (ch == NULL) {
        return RTE_STATUS_INVALID_PARAM;
    }

    switch (ch->type) {
    case CHANNEL_TYPE_SHARED_MEMORY:
        return shm_recv(ch->impl.shm, data, size);

    case CHANNEL_TYPE_TCP:
        return tcp_recv(ch->impl.tcp, data, size);

    default:
        return RTE_STATUS_INVALID_PARAM;
    }
}

/* ============================================================================
 * Application Example: Train Command Distribution
 * ========================================================================== */

/**
 * Train command data structure
 */
typedef struct {
    uint32_t command_id;
    uint32_t speed_limit_kmh;
    uint32_t target_distance_m;
    uint8_t emergency_stop;
} train_command_t;

#define CHANNEL_COUNT 2U

/**
 * @brief Example: Initialize dual-transport channels plus a 2oo2 voter
 *
 * Configuration:
 * - Channel 0: Shared Memory (fast, local standby)
 * - Channel 1: TCP/IP (remote standby, reliable backup)
 * - Voting: 2oo2 (both must agree)
 */
static rte_status_t example_setup_dual_redundancy(rte_channel_t channels[CHANNEL_COUNT],
                                                     rte_voter_t *voter)
{
    static void * const osadapter_handles[CHANNEL_COUNT] = { &g_channel_0, &g_channel_1 };
    rte_voter_config_t voter_cfg = {0};
    rte_status_t rc;
    uint32_t i;

    voter_cfg.voting_strategy = RTE_VOTING_2OO2;
    voter_cfg.channel_timeout_ms = 1000;
    voter_cfg.log_disagreements = true;
    voter_cfg.on_disagreement = NULL;
    voter_cfg.disagreement_context = NULL;

    rc = rte_voter_init(voter, &voter_cfg);
    if (rc != RTE_STATUS_OK) {
        printf("ERROR: Failed to initialize voter: %d\n", rc);
        return rc;
    }

    for (i = 0; i < CHANNEL_COUNT; i++) {
        rte_channel_config_t chan_cfg = {0};

        chan_cfg.channel_handle = osadapter_handles[i];
        chan_cfg.send = channel_backend_send;
        chan_cfg.recv = channel_backend_recv;

        rc = rte_channel_init(&channels[i], &chan_cfg);
        if (rc != RTE_STATUS_OK) {
            printf("ERROR: Failed to initialize channel %u: %d\n", i, rc);
            return rc;
        }

        rc = rte_voter_register_channel(voter, &channels[i]);
        if (rc != RTE_STATUS_OK) {
            printf("ERROR: Failed to register channel %u: %d\n", i, rc);
            return rc;
        }
    }

    printf("Voter initialized with 2oo2 voting over 2 channels\n");
    printf("  Channel 0: Shared Memory (local standby)\n");
    printf("  Channel 1: TCP/IP (remote standby)\n");
    return RTE_STATUS_OK;
}

/**
 * @brief Send a train command to every registered channel (atomic broadcast)
 */
static rte_status_t send_train_command(rte_voter_t *voter, const train_command_t *cmd)
{
    printf("\n>>> Broadcasting train command:\n");
    printf("    Command ID: %u\n", cmd->command_id);
    printf("    Speed Limit: %u km/h\n", cmd->speed_limit_kmh);
    printf("    Distance: %u m\n", cmd->target_distance_m);
    printf("    E-Stop: %s\n", cmd->emergency_stop ? "YES" : "NO");

    rte_status_t rc = rte_voter_send(voter, cmd, sizeof(*cmd));
    if (rc != RTE_STATUS_OK) {
        printf("Send failed: %d (channel fault detected)\n", rc);
        return rc;
    }

    printf("Sent to both channels (atomic broadcast)\n");
    return RTE_STATUS_OK;
}

/**
 * @brief Receive a train command via the voter (with 2oo2 voting)
 */
static rte_status_t receive_train_command(rte_voter_t *voter, train_command_t *cmd_out)
{
    rte_voting_result_t vote_result;
    size_t bytes_received = 0;

    printf("\n<<< Receiving train command (with voting)...\n");

    rte_status_t rc = rte_voter_receive(voter, cmd_out, sizeof(*cmd_out), &vote_result,
                                           &bytes_received);

    if (rc != RTE_STATUS_OK) {
        printf("Receive failed: %d (voting result: %d)\n", rc, vote_result);
        return rc;
    }

    printf("Both channels agreed on data:\n");
    printf("    Command ID: %u\n", cmd_out->command_id);
    printf("    Speed Limit: %u km/h\n", cmd_out->speed_limit_kmh);
    return RTE_STATUS_OK;
}

/**
 * @brief Monitor channel health and detect faults
 */
static void monitor_channel_health(rte_channel_t channels[CHANNEL_COUNT], rte_voter_t *voter)
{
    printf("\n=== Channel Health Report ===\n");

    for (uint32_t i = 0; i < CHANNEL_COUNT; i++) {
        rte_channel_health_t health;
        rte_status_t rc = rte_channel_get_health(&channels[i], &health);

        if (rc != RTE_STATUS_OK) {
            printf("  Channel %u: Error reading health\n", i);
            continue;
        }

        printf("  Channel %u:\n", i);
        printf("    Healthy: %s\n", health.is_healthy ? "YES" : "NO");
        printf("    Sends: %u (errors: %u)\n", health.send_count, health.send_error_count);
        printf("    Recvs: %u (errors: %u)\n", health.receive_count, health.receive_error_count);
    }

    uint32_t healthy_count = 0;
    uint32_t total_disagreements = 0;
    rte_voter_get_aggregated_health(voter, &healthy_count, &total_disagreements);
    printf("  Total healthy: %u/%u\n", healthy_count, CHANNEL_COUNT);
    printf("  Total disagreements: %u\n", total_disagreements);

    if (healthy_count < CHANNEL_COUNT) {
        printf("  WARNING: Redundancy compromised!\n");
    }
}

/**
 * @brief Main example
 */
int main(void)
{
    printf("========================================\n");
    printf("Voted Channel: Dual Transport Example\n");
    printf("========================================\n");

    rte_channel_t channels[CHANNEL_COUNT];
    rte_voter_t voter;

    if (example_setup_dual_redundancy(channels, &voter) != RTE_STATUS_OK) {
        return EXIT_FAILURE;
    }

    /* Example 1: Normal operation (both channels working) */
    printf("\n--- Test 1: Normal Operation ---\n");
    train_command_t cmd = {
        .command_id = 1001,
        .speed_limit_kmh = 80,
        .target_distance_m = 5000,
        .emergency_stop = 0,
    };

    if (send_train_command(&voter, &cmd) != RTE_STATUS_OK) {
        printf("Send failed\n");
    }

    monitor_channel_health(channels, &voter);

    /* Example 2: Simulate channel degradation */
    printf("\n--- Test 2: One Channel Healthy ---\n");
    tcp_ch_0.connected = false;  /* TCP connection lost */
    printf("TCP connection lost\n");

    cmd.command_id = 1002;
    if (send_train_command(&voter, &cmd) != RTE_STATUS_OK) {
        printf("Send failed (expected, channel down)\n");
    }

    monitor_channel_health(channels, &voter);

    /* Example 3: Restore and verify recovery */
    printf("\n--- Test 3: Recovery ---\n");
    tcp_ch_0.connected = true;  /* TCP reconnected */
    printf("TCP connection restored\n");

    cmd.command_id = 1003;
    if (send_train_command(&voter, &cmd) != RTE_STATUS_OK) {
        printf("Send failed\n");
    }

    monitor_channel_health(channels, &voter);

    (void)receive_train_command; /* demonstrates the receive-side API; not driven in this example */

    /* Cleanup */
    rte_voter_destroy(&voter);
    for (uint32_t i = 0; i < CHANNEL_COUNT; i++) {
        rte_channel_destroy(&channels[i]);
    }
    printf("\n========================================\n");
    printf("Example completed\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}

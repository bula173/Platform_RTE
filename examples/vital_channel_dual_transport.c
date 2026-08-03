/**
 * @file vital_channel_dual_transport.c
 * @brief Example: Vital Channel with Dual Transport (Shared Memory + TCP/IP)
 *
 * Demonstrates how to:
 * 1. Create two redundant communication channels using different transports
 * 2. Wrap them in a vital_channel for voting/redundancy
 * 3. Support user selection of online/standby channel configurations
 * 4. Monitor health for fault detection
 *
 * This pattern enables:
 * - Online process communicates via fast shared memory to local standby
 * - Online process communicates via TCP to remote standby (fallback)
 * - Voting ensures data consistency across both standbies
 * - If one channel fails, voting detects and triggers safe-state
 *
 * Compile:
 *   gcc -std=c99 -Wall -Wextra -o vital_dual_transport \
 *       vital_channel_dual_transport.c \
 *       -I../include -L../build -lsafeapi_vital_channel -lsafeapi_safestate -lsafeapi_log -lsafeapi_status
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "safeapi/vital_channel/sapi_vital_channel.h"
#include "safeapi/status/sapi_status.h"

/* ============================================================================
 * Transport Backend 1: Shared Memory Queue (Simulated)
 * ========================================================================== */

typedef struct {
    uint32_t write_idx;
    uint32_t read_idx;
    uint32_t count;
    uint8_t data[10][256];  /* Queue of 10 messages, each 256 bytes */
} shm_queue_t;

/* For example purposes: one shared queue per channel */
static shm_queue_t shm_queue_0 = {0};
static shm_queue_t shm_queue_1 = {0};

static sapi_status_t shm_send(shm_queue_t *q, const void *data, size_t size)
{
    if (q->count >= 10) {
        return SAPI_STATUS_TIMEOUT;  /* Queue full */
    }

    if (size > 256) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    memcpy(q->data[q->write_idx], data, size);
    q->write_idx = (q->write_idx + 1) % 10;
    q->count++;

    return SAPI_STATUS_OK;
}

static sapi_status_t shm_recv(shm_queue_t *q, void *data, size_t size)
{
    if (q->count == 0) {
        return SAPI_STATUS_TIMEOUT;  /* Queue empty */
    }

    if (size < 256) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    memcpy(data, q->data[q->read_idx], 256);
    q->read_idx = (q->read_idx + 1) % 10;
    q->count--;

    return SAPI_STATUS_OK;
}

/* ============================================================================
 * Transport Backend 2: TCP/IP Socket (Simulated)
 * ========================================================================== */

typedef struct {
    int socket;
    uint32_t packets_sent;
    uint32_t packets_recv;
    bool connected;
} tcp_channel_t;

static tcp_channel_t tcp_ch_0 = {.socket = -1, .connected = false};
static tcp_channel_t tcp_ch_1 = {.socket = -1, .connected = false};

static sapi_status_t tcp_send(tcp_channel_t *ch, const void *data, size_t size)
{
    if (ch->socket < 0 || !ch->connected) {
        return SAPI_STATUS_HARDWARE_FAULT;
    }

    if (size > 256) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    /* In real implementation:
     * - Add message framing header (length prefix)
     * - Call socket send() with timeout
     * - Check for EAGAIN (queue full)
     */

    ch->packets_sent++;
    return SAPI_STATUS_OK;
}

static sapi_status_t tcp_recv(tcp_channel_t *ch, void *data, size_t size)
{
    if (ch->socket < 0 || !ch->connected) {
        return SAPI_STATUS_HARDWARE_FAULT;
    }

    if (size < 256) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    /* In real implementation:
     * - Call socket recv() with timeout
     * - Parse message framing
     * - Handle partial receives
     */

    ch->packets_recv++;
    return SAPI_STATUS_OK;
}

/* ============================================================================
 * Multi-Transport Backend Callbacks (Vital Channel uses these)
 * ========================================================================== */

/**
 * @brief Opaque channel type that hides whether it's SHM or TCP
 *
 * This allows vital_channel to work with ANY mix of transports.
 * The callback functions dispatch based on channel type.
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

/**
 * @brief Backend send callback for vital_channel (dispatches to transport)
 *
 * Called by sapi_vital_channel_send() for each redundant channel.
 * The callback hides the specific transport implementation.
 */
static sapi_status_t vital_backend_send(void *channel, const void *data, size_t size)
{
    multi_transport_channel_t *ch = (multi_transport_channel_t *)channel;

    if (ch == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    switch (ch->type) {
    case CHANNEL_TYPE_SHARED_MEMORY:
        return shm_send(ch->impl.shm, data, size);

    case CHANNEL_TYPE_TCP:
        return tcp_send(ch->impl.tcp, data, size);

    default:
        return SAPI_STATUS_INVALID_PARAM;
    }
}

/**
 * @brief Backend receive callback for vital_channel (dispatches to transport)
 *
 * Called by sapi_vital_channel_receive() for each redundant channel.
 * The callback hides the specific transport implementation.
 */
static sapi_status_t vital_backend_recv(void *channel, void *data, size_t size,
                                        uint32_t timeout_ms __attribute__((unused)))
{
    multi_transport_channel_t *ch = (multi_transport_channel_t *)channel;

    if (ch == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    switch (ch->type) {
    case CHANNEL_TYPE_SHARED_MEMORY:
        return shm_recv(ch->impl.shm, data, size);

    case CHANNEL_TYPE_TCP:
        return tcp_recv(ch->impl.tcp, data, size);

    default:
        return SAPI_STATUS_INVALID_PARAM;
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

/**
 * @brief Example: Initialize dual-transport vital channel for train commands
 *
 * Configuration:
 * - Channel 0: Shared Memory (fast, local standby)
 * - Channel 1: TCP/IP (remote standby, reliable backup)
 * - Voting: 2oo2 (both must agree)
 */
static sapi_status_t example_setup_dual_redundancy(
    sapi_vital_channel_t *vital_handle)
{
    sapi_vital_channel_storage_t *storage = vital_handle;
    sapi_vital_channel_config_t config = {0};

    /* Create multi-transport channel wrappers */
    static multi_transport_channel_t channel_0 = {
        .type = CHANNEL_TYPE_SHARED_MEMORY,
        .impl.shm = &shm_queue_0,
    };

    static multi_transport_channel_t channel_1 = {
        .type = CHANNEL_TYPE_TCP,
        .impl.tcp = &tcp_ch_0,
    };

    /* Configure vital channel for 2oo2 voting */
    config.voting_strategy = SAPI_VOTING_2OO2;
    config.channel_count = 2;
    config.channel_timeout_ms = 1000;
    config.log_disagreements = true;
    config.on_disagreement = NULL;
    config.context = NULL;
    config.backend_send = vital_backend_send;   /* Dispatch to appropriate transport */
    config.backend_recv = vital_backend_recv;   /* Dispatch to appropriate transport */

    /* Create the vital channel with both transports */
    void *channels[2] = {
        &channel_0,  /* Shared memory transport */
        &channel_1,  /* TCP/IP transport */
    };

    sapi_status_t rc = sapi_vital_channel_init(storage, &config, channels, 2);
    if (rc != SAPI_STATUS_OK) {
        printf("ERROR: Failed to initialize vital channel: %d\n", rc);
        return rc;
    }

    printf("✓ Vital Channel initialized with 2oo2 voting\n");
    printf("  Channel 0: Shared Memory (local standby)\n");
    printf("  Channel 1: TCP/IP (remote standby)\n");
    return SAPI_STATUS_OK;
}

/**
 * @brief Send a train command via vital channel (broadcast to both standbies)
 */
static sapi_status_t send_train_command(
    sapi_vital_channel_t *vital_channel,
    const train_command_t *cmd)
{
    printf("\n>>> Broadcasting train command:\n");
    printf("    Command ID: %u\n", cmd->command_id);
    printf("    Speed Limit: %u km/h\n", cmd->speed_limit_kmh);
    printf("    Distance: %u m\n", cmd->target_distance_m);
    printf("    E-Stop: %s\n", cmd->emergency_stop ? "YES" : "NO");

    sapi_status_t rc = sapi_vital_channel_send(vital_channel, cmd, sizeof(*cmd));
    if (rc != SAPI_STATUS_OK) {
        printf("✗ Send failed: %d (channel fault detected)\n", rc);
        return rc;
    }

    printf("✓ Sent to both channels (atomic broadcast)\n");
    return SAPI_STATUS_OK;
}

/**
 * @brief Receive train command via vital channel (with voting)
 */
static sapi_status_t receive_train_command(
    sapi_vital_channel_t *vital_channel,
    train_command_t *cmd_out,
    sapi_voting_result_t *vote_result)
{
    printf("\n<<< Receiving train command (with voting)...\n");

    sapi_status_t rc = sapi_vital_channel_receive(
        vital_channel, cmd_out, sizeof(*cmd_out), vote_result, NULL);

    if (rc != SAPI_STATUS_OK) {
        printf("✗ Receive failed: %d\n", rc);
        if (vote_result != NULL) {
            printf("   Voting result: %d\n", *vote_result);
        }
        return rc;
    }

    printf("✓ Both channels agreed on data:\n");
    printf("    Command ID: %u\n", cmd_out->command_id);
    printf("    Speed Limit: %u km/h\n", cmd_out->speed_limit_kmh);
    return SAPI_STATUS_OK;
}

/**
 * @brief Monitor channel health and detect faults
 */
static void monitor_channel_health(sapi_vital_channel_t *vital_channel)
{
    printf("\n=== Channel Health Report ===\n");

    for (uint32_t i = 0; i < 2; i++) {
        sapi_vital_channel_health_t health;
        sapi_status_t rc = sapi_vital_channel_get_health(vital_channel, i, &health);

        if (rc != SAPI_STATUS_OK) {
            printf("  Channel %u: Error reading health\n", i);
            continue;
        }

        printf("  Channel %u:\n", i);
        printf("    Healthy: %s\n", health.is_healthy ? "YES" : "NO");
        printf("    Sends: %u (errors: %u)\n", health.send_count, health.send_error_count);
        printf("    Recvs: %u (errors: %u)\n", health.receive_count, health.receive_error_count);
        printf("    Disagreements: %u\n", health.disagreement_count);
    }

    uint32_t healthy_count = 0;
    uint32_t total_disagreements = 0;
    sapi_vital_channel_get_aggregated_health(vital_channel, &healthy_count,
                                             &total_disagreements);
    printf("  Total healthy: %u/2\n", healthy_count);
    printf("  Total disagreements: %u\n", total_disagreements);

    if (healthy_count < 2) {
        printf("  ⚠️  WARNING: Redundancy compromised!\n");
    }
}

/**
 * @brief Main example
 */
int main(void)
{
    printf("========================================\n");
    printf("Vital Channel: Dual Transport Example\n");
    printf("========================================\n");

    /* Initialize the vital channel with mixed transports */
    sapi_vital_channel_storage_t vital_storage = {0};
    sapi_vital_channel_t *vital = &vital_storage;

    if (example_setup_dual_redundancy(vital) != SAPI_STATUS_OK) {
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

    if (send_train_command(vital, &cmd) != SAPI_STATUS_OK) {
        printf("Send failed\n");
    }

    monitor_channel_health(vital);

    /* Example 2: Simulate channel degradation */
    printf("\n--- Test 2: One Channel Healthy ---\n");
    tcp_ch_0.connected = false;  /* TCP connection lost */
    printf("⚠️  TCP connection lost\n");

    cmd.command_id = 1002;
    if (send_train_command(vital, &cmd) != SAPI_STATUS_OK) {
        printf("Send failed (expected, channel down)\n");
    }

    monitor_channel_health(vital);

    /* Example 3: Restore and verify recovery */
    printf("\n--- Test 3: Recovery ---\n");
    tcp_ch_0.connected = true;  /* TCP reconnected */
    printf("✓ TCP connection restored\n");

    cmd.command_id = 1003;
    if (send_train_command(vital, &cmd) != SAPI_STATUS_OK) {
        printf("Send failed\n");
    }

    monitor_channel_health(vital);

    /* Cleanup */
    sapi_vital_channel_destroy(vital);
    printf("\n========================================\n");
    printf("Example completed\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}

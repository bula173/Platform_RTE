/**
 * @file channel_configuration_example.c
 * @brief Example: How users configure channels for online/standby redundancy
 *
 * This example shows:
 * 1. Creating different channel types with user-specified parameters
 * 2. TCP/IP for remote standby communication (primary use case)
 * 3. Optional fallback channels (UDP, shared memory for local)
 * 4. Wrapping in vital_channel for voting/redundancy
 * 5. Dispatcher pattern for mixed transport types
 *
 * User responsibility: Provide configuration values (IPs, ports, paths)
 * Framework responsibility: Voting, health tracking, safe-state
 * OS integrator responsibility: Actual send/recv implementations
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "safeapi/vital_channel/sapi_vital_channel.h"
#include "safeapi/status/sapi_status.h"

/* ============================================================================
 * Channel Configuration Structures (User fills these in)
 * ========================================================================== */

/**
 * TCP/IP channel configuration
 * User specifies: IP address, port, timeouts
 */
typedef struct {
    const char *name;
    const char *remote_ip;           // e.g., "192.168.1.100"
    uint16_t remote_port;            // e.g., 5000
    size_t message_size;             // e.g., 256
    uint32_t timeout_ms;             // e.g., 1000
    bool server_mode;                // true = listen, false = connect
} tcp_channel_config_t;

/**
 * UDP channel configuration
 * User specifies: IP addresses, ports, timeout
 */
typedef struct {
    const char *name;
    const char *local_ip;            // e.g., "192.168.1.50"
    uint16_t local_port;             // e.g., 5001
    const char *remote_ip;           // e.g., "192.168.1.100"
    uint16_t remote_port;            // e.g., 5001
    size_t message_size;             // e.g., 256
    uint32_t timeout_ms;             // e.g., 500
} udp_channel_config_t;

/**
 * Shared memory channel configuration
 * User specifies: descriptor path, queue size, timeout
 */
typedef struct {
    const char *name;
    const char *descriptor_path;     // e.g., "/dev/shm/vital_channel"
    size_t message_size;             // e.g., 256
    size_t queue_depth;              // e.g., 10
    uint32_t timeout_ms;             // e.g., 100
} shm_channel_config_t;

/**
 * FIFO channel configuration
 * User specifies: FIFO path, message size, timeout
 */
typedef struct {
    const char *name;
    const char *fifo_path;           // e.g., "/tmp/vital_channel_fifo"
    size_t message_size;             // e.g., 256
    uint32_t timeout_ms;             // e.g., 500
    bool blocking;                   // true = blocking, false = non-blocking
} fifo_channel_config_t;

/* ============================================================================
 * Channel Type Identification (for dispatcher)
 * ========================================================================== */

typedef enum {
    CHANNEL_TYPE_TCP,
    CHANNEL_TYPE_UDP,
    CHANNEL_TYPE_SHM,
    CHANNEL_TYPE_FIFO,
} channel_type_t;

/**
 * Opaque channel wrapper with type identification
 * This allows vital_channel to work with ANY transport
 */
typedef struct {
    channel_type_t type;
    void *impl;  // Opaque pointer to actual channel (TCP/UDP/SHM/FIFO)
    uint32_t timeout_ms;
} channel_wrapper_t;

/* ============================================================================
 * Dispatcher Callbacks (User implements these)
 * ========================================================================== */

/**
 * @brief Dispatcher send callback
 * Routes to appropriate transport based on channel type
 */
static sapi_status_t app_backend_send(void *channel, const void *data, size_t size)
{
    channel_wrapper_t *wrapper = (channel_wrapper_t *)channel;

    if (wrapper == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    switch (wrapper->type) {
    case CHANNEL_TYPE_TCP:
        // TODO: Call actual TCP send
        // return tcp_send(wrapper->impl, data, size, wrapper->timeout_ms);
        printf("[TCP] Sending %zu bytes\n", size);
        return SAPI_STATUS_OK;

    case CHANNEL_TYPE_UDP:
        // TODO: Call actual UDP send
        // return udp_send(wrapper->impl, data, size, wrapper->timeout_ms);
        printf("[UDP] Sending %zu bytes\n", size);
        return SAPI_STATUS_OK;

    case CHANNEL_TYPE_SHM:
        // TODO: Call actual SHM send
        // return shm_send(wrapper->impl, data, size, wrapper->timeout_ms);
        printf("[SHM] Sending %zu bytes\n", size);
        return SAPI_STATUS_OK;

    case CHANNEL_TYPE_FIFO:
        // TODO: Call actual FIFO send
        // return fifo_send(wrapper->impl, data, size, wrapper->timeout_ms);
        printf("[FIFO] Sending %zu bytes\n", size);
        return SAPI_STATUS_OK;

    default:
        return SAPI_STATUS_INVALID_PARAM;
    }
}

/**
 * @brief Dispatcher receive callback
 * Routes to appropriate transport based on channel type
 */
static sapi_status_t app_backend_recv(void *channel, void *data, size_t size,
                                      uint32_t timeout_ms)
{
    channel_wrapper_t *wrapper = (channel_wrapper_t *)channel;

    if (wrapper == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    switch (wrapper->type) {
    case CHANNEL_TYPE_TCP:
        // TODO: Call actual TCP recv
        // return tcp_recv(wrapper->impl, data, size, timeout_ms);
        printf("[TCP] Receiving up to %zu bytes (timeout %ums)\n", size, timeout_ms);
        return SAPI_STATUS_OK;

    case CHANNEL_TYPE_UDP:
        // TODO: Call actual UDP recv
        // return udp_recv(wrapper->impl, data, size, timeout_ms);
        printf("[UDP] Receiving up to %zu bytes (timeout %ums)\n", size, timeout_ms);
        return SAPI_STATUS_OK;

    case CHANNEL_TYPE_SHM:
        // TODO: Call actual SHM recv
        // return shm_recv(wrapper->impl, data, size, timeout_ms);
        printf("[SHM] Receiving up to %zu bytes (timeout %ums)\n", size, timeout_ms);
        return SAPI_STATUS_OK;

    case CHANNEL_TYPE_FIFO:
        // TODO: Call actual FIFO recv
        // return fifo_recv(wrapper->impl, data, size, timeout_ms);
        printf("[FIFO] Receiving up to %zu bytes (timeout %ums)\n", size, timeout_ms);
        return SAPI_STATUS_OK;

    default:
        return SAPI_STATUS_INVALID_PARAM;
    }
}

/* ============================================================================
 * Example 1: Online/Standby with TCP/IP (Primary Use Case)
 * ========================================================================== */

void example_tcp_based_redundancy(void)
{
    printf("\n=== Example 1: TCP/IP Based Online/Standby ===\n");

    /* ========================================================================
     * ONLINE HOST CONFIGURATION
     * ======================================================================== */

    printf("\n[ONLINE HOST] Configuration:\n");

    // User specifies TCP configuration for online host
    tcp_channel_config_t tcp_config_online = {
        .name = "online_to_standby_tcp",
        .remote_ip = "192.168.1.100",        // User provides standby IP
        .remote_port = 5000,                 // User chooses port
        .message_size = 256,
        .timeout_ms = 1000,
        .server_mode = false,                // Connect to standby (client)
    };

    printf("  ✓ TCP Config: %s → %s:%u\n", tcp_config_online.name,
           tcp_config_online.remote_ip, tcp_config_online.remote_port);
    printf("    Timeout: %ums\n", tcp_config_online.timeout_ms);

    // Optional: Add UDP for fast heartbeat
    udp_channel_config_t udp_config_online = {
        .name = "online_heartbeat_udp",
        .local_ip = "192.168.1.50",          // Online host IP
        .local_port = 5001,
        .remote_ip = "192.168.1.100",        // Standby host IP
        .remote_port = 5001,
        .message_size = 256,
        .timeout_ms = 500,
    };

    printf("  ✓ UDP Config: %s (heartbeat)\n", udp_config_online.name);
    printf("    Local: %s:%u\n", udp_config_online.local_ip, udp_config_online.local_port);
    printf("    Remote: %s:%u\n", udp_config_online.remote_ip, udp_config_online.remote_port);

    // Create wrapper channels for vital_channel
    static channel_wrapper_t online_ch0 = {
        .type = CHANNEL_TYPE_TCP,
        .impl = (void *)0x1000,  // Placeholder (actual handle from OS backend)
        .timeout_ms = 1000,
    };

    static channel_wrapper_t online_ch1 = {
        .type = CHANNEL_TYPE_UDP,
        .impl = (void *)0x2000,  // Placeholder
        .timeout_ms = 500,
    };

    // Configure vital channel for 2oo2 voting (both must agree)
    sapi_vital_channel_config_t vital_cfg_online = {
        .voting_strategy = SAPI_VOTING_2OO2,
        .channel_count = 2,
        .channel_timeout_ms = 1000,
        .log_disagreements = true,
        .on_disagreement = NULL,
        .context = NULL,
        .backend_send = app_backend_send,    // User's dispatcher
        .backend_recv = app_backend_recv,
    };

    sapi_vital_channel_storage_t vital_online = {0};
    void *online_channels[2] = { &online_ch0, &online_ch1 };

    sapi_status_t rc = sapi_vital_channel_init(&vital_online, &vital_cfg_online,
                                               online_channels, 2);
    if (rc == SAPI_STATUS_OK) {
        printf("  ✓ Vital Channel (2oo2) initialized\n");
    } else {
        printf("  ✗ Failed to initialize vital channel\n");
        return;
    }

    /* ========================================================================
     * STANDBY HOST CONFIGURATION
     * ======================================================================== */

    printf("\n[STANDBY HOST] Configuration:\n");

    // Standby listens for TCP connections
    tcp_channel_config_t tcp_config_standby = {
        .name = "standby_listen_tcp",
        .remote_ip = "0.0.0.0",              // Listen on all interfaces
        .remote_port = 5000,                 // Same port as online's target
        .message_size = 256,
        .timeout_ms = 1000,
        .server_mode = true,                 // Listen (server mode)
    };

    printf("  ✓ TCP Config: %s (listening)\n", tcp_config_standby.name);
    printf("    Listen: %s:%u\n", tcp_config_standby.remote_ip, tcp_config_standby.remote_port);

    // Standby also receives UDP
    udp_channel_config_t udp_config_standby = {
        .name = "standby_heartbeat_udp",
        .local_ip = "192.168.1.100",         // Standby IP
        .local_port = 5001,
        .remote_ip = "192.168.1.50",         // Online IP (for feedback)
        .remote_port = 5001,
        .message_size = 256,
        .timeout_ms = 500,
    };

    printf("  ✓ UDP Config: %s (receiving)\n", udp_config_standby.name);
    printf("    Listen: %s:%u\n", udp_config_standby.local_ip, udp_config_standby.local_port);

    // Create channels with same configuration
    static channel_wrapper_t standby_ch0 = {
        .type = CHANNEL_TYPE_TCP,
        .impl = (void *)0x3000,
        .timeout_ms = 1000,
    };

    static channel_wrapper_t standby_ch1 = {
        .type = CHANNEL_TYPE_UDP,
        .impl = (void *)0x4000,
        .timeout_ms = 500,
    };

    sapi_vital_channel_storage_t vital_standby = {0};
    void *standby_channels[2] = { &standby_ch0, &standby_ch1 };

    rc = sapi_vital_channel_init(&vital_standby, &vital_cfg_online,
                                 standby_channels, 2);
    if (rc == SAPI_STATUS_OK) {
        printf("  ✓ Vital Channel (2oo2) initialized\n");
    }

    /* ========================================================================
     * SIMULATION: Voting Communication
     * ======================================================================== */

    printf("\n[COMMUNICATION] Simulated voting:\n");

    uint8_t command[256] = {0x42};  // Dummy command

    printf("  1. Online sends command via vital_channel...\n");
    rc = sapi_vital_channel_send(&vital_online, command, 256);
    printf("     Result: %d (broadcasts via TCP + UDP)\n", rc);

    printf("  2. Standby receives and votes...\n");
    uint8_t received[256] = {0};
    sapi_voting_result_t vote = SAPI_VOTING_AGREED;
    rc = sapi_vital_channel_receive(&vital_standby, received, 256, &vote, NULL);
    printf("     Result: %d, Voting: %d\n", rc, vote);

    printf("  3. Check health...\n");
    sapi_vital_channel_health_t health;
    rc = sapi_vital_channel_get_health(&vital_online, 0, &health);
    printf("     Channel 0 health: sends=%u, errors=%u\n",
           health.send_count, health.send_error_count);
}

/* ============================================================================
 * Example 2: Local Shared Memory Channels (Optional)
 * ========================================================================== */

void example_local_redundancy(void)
{
    printf("\n=== Example 2: Local Shared Memory (Optional For Same-Board) ===\n");

    // For same-board redundancy: Online A and Standby B on same CPU
    shm_channel_config_t shm_config = {
        .name = "vital_ab_shared",
        .descriptor_path = "/dev/shm/rail_vital_ab",  // User specifies path
        .message_size = 256,
        .queue_depth = 10,
        .timeout_ms = 100,  // Much tighter for local
    };

    printf("  Shared Memory Config:\n");
    printf("    Path: %s\n", shm_config.descriptor_path);
    printf("    Queue: %zu messages × %zu bytes\n",
           shm_config.queue_depth, shm_config.message_size);
    printf("    Timeout: %ums\n", shm_config.timeout_ms);

    static channel_wrapper_t shm_ch = {
        .type = CHANNEL_TYPE_SHM,
        .impl = (void *)0x5000,
        .timeout_ms = 100,
    };

    printf("  ✓ Channel created\n");
}

/* ============================================================================
 * Example 3: Hybrid Configuration
 * ========================================================================== */

void example_hybrid_configuration(void)
{
    printf("\n=== Example 3: Hybrid (TCP Primary, Local SHM Fallback) ===\n");
    printf("  For maximum flexibility: use TCP for remote, SHM for local\n");
    printf("  Dispatcher handles both transparently\n");

    // This allows:
    // - Channel 0: TCP to remote standby (50-100ms)
    // - Channel 1: SHM to local standby (< 1µs)
    // - Vital channel votes on both
    // - If TCP times out, still have SHM
    // - If both fail, safe-state triggered
}

int main(void)
{
    printf("========================================\n");
    printf("Channel Configuration Examples\n");
    printf("========================================\n");

    example_tcp_based_redundancy();
    example_local_redundancy();
    example_hybrid_configuration();

    printf("\n========================================\n");
    printf("Configuration Complete\n");
    printf("========================================\n");
    printf("\nKey Points:\n");
    printf("  ✓ User provides configuration values (IPs, ports, paths)\n");
    printf("  ✓ Framework handles voting and health tracking\n");
    printf("  ✓ OS integrator implements actual I/O (send/recv)\n");
    printf("  ✓ Dispatcher routes to correct transport\n");
    printf("  ✓ Same vital_channel code works with ANY transport\n");

    return 0;
}

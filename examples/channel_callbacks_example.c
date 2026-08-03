/**
 * @file channel_callbacks_example.c
 * @brief Example: Channel naming and callback-based event handling
 *
 * Demonstrates:
 * 1. Naming channels for identification
 * 2. Registering callbacks on channels
 * 3. Event-driven processing (callbacks instead of polling)
 * 4. Channel registry for managing multiple channels
 * 5. Identifying which channel has data in logs
 *
 * This is more efficient than polling: callbacks are invoked when data arrives.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "safeapi/status/sapi_status.h"

/* ============================================================================
 * Channel Callback Definition
 * ========================================================================== */

/**
 * @brief Callback invoked when data is available on a channel
 *
 * @param channel_name  Unique identifier (e.g., "online_to_standby_vital")
 * @param data          Received data buffer
 * @param data_size     Size of received data
 * @param context       User-provided context (e.g., app state)
 *
 * Called by OS backend when data arrives on the channel.
 * User implements this to handle incoming data.
 */
typedef void (*sapi_channel_callback_t)(const char *channel_name,
                                        const void *data,
                                        size_t data_size,
                                        void *context);

/* ============================================================================
 * Application Context and Data Types
 * ========================================================================== */

typedef struct {
    uint32_t command_id;
    uint32_t speed_kmh;
    uint8_t emergency_stop;
} train_command_t;

typedef struct {
    uint32_t status_code;
    bool is_healthy;
    uint32_t error_count;
} standby_status_t;

typedef struct {
    // Channel metrics
    uint32_t vital_commands_received;
    uint32_t feedback_messages_received;
    uint32_t telemetry_messages_received;
    uint32_t config_updates_applied;

    // State
    train_command_t last_command;
    standby_status_t standby_status;
    bool running;
} app_context_t;

/* ============================================================================
 * Channel Registry (for managing named channels)
 * ========================================================================== */

#define MAX_CHANNELS 16

typedef struct {
    const char *name;
    sapi_channel_callback_t callback;
    void *context;
    // In real implementation: sapi_ipc_handle_t handle;
} channel_registry_entry_t;

typedef struct {
    channel_registry_entry_t channels[MAX_CHANNELS];
    uint32_t count;
} channel_registry_t;

static channel_registry_t g_channel_registry = {0};

/**
 * @brief Register a named channel with callback
 */
sapi_status_t channel_registry_register(const char *name,
                                        sapi_channel_callback_t callback,
                                        void *context)
{
    if (g_channel_registry.count >= MAX_CHANNELS) {
        return SAPI_STATUS_RESOURCE_EXHAUSTED;
    }

    channel_registry_entry_t *entry = &g_channel_registry.channels[g_channel_registry.count];
    entry->name = name;
    entry->callback = callback;
    entry->context = context;

    g_channel_registry.count++;
    printf("✓ Registered channel: %s\n", name);
    return SAPI_STATUS_OK;
}

/**
 * @brief Find channel by name
 */
channel_registry_entry_t *channel_registry_find(const char *name)
{
    for (uint32_t i = 0; i < g_channel_registry.count; i++) {
        if (strcmp(g_channel_registry.channels[i].name, name) == 0) {
            return &g_channel_registry.channels[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Callback Implementations (User provides these)
 * ========================================================================== */

/**
 * @brief Callback: Vital command received on channel
 *
 * This is invoked by the OS backend when data arrives on the
 * "online_to_standby_vital" channel. The callback identifies the
 * channel by name and processes the incoming data.
 */
void on_vital_command_received(const char *channel_name,
                               const void *data,
                               size_t data_size,
                               void *context)
{
    app_context_t *app = (app_context_t *)context;

    printf("\n[CALLBACK] Channel: %s\n", channel_name);
    printf("           Data size: %zu bytes\n", data_size);

    // Validate
    if (data_size != sizeof(train_command_t)) {
        printf("✗ ERROR: Invalid command size on %s\n", channel_name);
        return;
    }

    // Parse
    const train_command_t *cmd = (const train_command_t *)data;
    printf("           Command ID: %u\n", cmd->command_id);
    printf("           Speed: %u km/h\n", cmd->speed_kmh);
    printf("           E-Stop: %s\n", cmd->emergency_stop ? "YES" : "NO");

    // Process
    app->last_command = *cmd;
    app->vital_commands_received++;

    // Log for debugging/monitoring
    printf("✓ Vital command processed (total received: %u)\n",
           app->vital_commands_received);
}

/**
 * @brief Callback: Standby feedback/health received
 *
 * Invoked on "standby_to_online_feedback" channel.
 * Tracks standby health and detects anomalies.
 */
void on_standby_feedback_received(const char *channel_name,
                                  const void *data,
                                  size_t data_size,
                                  void *context)
{
    app_context_t *app = (app_context_t *)context;

    printf("\n[CALLBACK] Channel: %s\n", channel_name);
    printf("           Data size: %zu bytes\n", data_size);

    if (data_size != sizeof(standby_status_t)) {
        printf("✗ ERROR: Invalid status size on %s\n", channel_name);
        return;
    }

    const standby_status_t *status = (const standby_status_t *)data;
    printf("           Status code: %u\n", status->status_code);
    printf("           Healthy: %s\n", status->is_healthy ? "YES" : "NO");
    printf("           Error count: %u\n", status->error_count);

    // Update state
    app->standby_status = *status;
    app->feedback_messages_received++;

    // Health check
    if (!status->is_healthy) {
        printf("⚠️  WARNING: Standby unhealthy on %s\n", channel_name);
    }

    if (status->error_count > 10) {
        printf("⚠️  WARNING: High error count on %s\n", channel_name);
    }

    printf("✓ Feedback processed (total received: %u)\n",
           app->feedback_messages_received);
}

/**
 * @brief Callback: Telemetry received
 *
 * Invoked on "online_to_diag_telemetry" channel.
 * Non-blocking, non-vital: failures are logged but not critical.
 */
void on_telemetry_received(const char *channel_name,
                          const void *data,
                          size_t data_size,
                          void *context)
{
    app_context_t *app = (app_context_t *)context;

    printf("\n[CALLBACK] Channel: %s\n", channel_name);
    printf("           Telemetry size: %zu bytes\n", data_size);

    // Non-vital: just log and continue
    app->telemetry_messages_received++;
    printf("✓ Telemetry logged (total received: %u)\n",
           app->telemetry_messages_received);
}

/**
 * @brief Callback: Configuration update received
 *
 * Invoked on "config_update_channel".
 * Validates and applies new configuration.
 */
void on_config_update_received(const char *channel_name,
                              const void *data,
                              size_t data_size,
                              void *context)
{
    app_context_t *app = (app_context_t *)context;

    printf("\n[CALLBACK] Channel: %s\n", channel_name);
    printf("           Config size: %zu bytes\n", data_size);

    // Validate
    if (data_size < 4) {
        printf("✗ ERROR: Invalid config on %s\n", channel_name);
        return;
    }

    // Would apply configuration here
    app->config_updates_applied++;
    printf("✓ Configuration applied from %s (total: %u)\n",
           channel_name, app->config_updates_applied);
}

/* ============================================================================
 * Simulated Event Loop
 * ========================================================================== */

/**
 * @brief Simulate data arriving on a channel
 *
 * In real implementation, OS backend would call the callback when
 * actual data arrives. Here we simulate it.
 */
void simulate_channel_data(const char *channel_name,
                          const void *data,
                          size_t size)
{
    printf("\n=== DATA ARRIVING ON CHANNEL ===\n");

    // Look up channel in registry
    channel_registry_entry_t *ch = channel_registry_find(channel_name);
    if (!ch) {
        printf("✗ Channel not found: %s\n", channel_name);
        return;
    }

    // Invoke callback (this is what OS backend does)
    if (ch->callback) {
        ch->callback(channel_name, data, size, ch->context);
    }
}

/* ============================================================================
 * Application Example
 * ========================================================================== */

int main(void)
{
    printf("========================================\n");
    printf("Channel Callbacks Example\n");
    printf("========================================\n");

    // Initialize app context
    app_context_t app = {
        .vital_commands_received = 0,
        .feedback_messages_received = 0,
        .telemetry_messages_received = 0,
        .config_updates_applied = 0,
        .running = true,
    };

    /* ====================================================================
     * STEP 1: Register named channels with callbacks at startup
     * ==================================================================== */

    printf("\n--- STEP 1: Register Named Channels ---\n");

    // Vital command channel
    channel_registry_register(
        "online_to_standby_vital",
        on_vital_command_received,
        &app
    );

    // Feedback channel
    channel_registry_register(
        "standby_to_online_feedback",
        on_standby_feedback_received,
        &app
    );

    // Telemetry channel
    channel_registry_register(
        "online_to_diag_telemetry",
        on_telemetry_received,
        &app
    );

    // Configuration channel
    channel_registry_register(
        "config_update_channel",
        on_config_update_received,
        &app
    );

    printf("\nAll channels registered. Waiting for events...\n");

    /* ====================================================================
     * STEP 2: Simulate events (in real system, OS backend triggers these)
     * ==================================================================== */

    printf("\n--- STEP 2: Simulate Channel Events ---\n");

    // Event 1: Vital command arrives
    train_command_t cmd = {
        .command_id = 1001,
        .speed_kmh = 80,
        .emergency_stop = 0,
    };
    simulate_channel_data("online_to_standby_vital", &cmd, sizeof(cmd));

    // Event 2: Standby feedback arrives
    standby_status_t status = {
        .status_code = 0,
        .is_healthy = true,
        .error_count = 0,
    };
    simulate_channel_data("standby_to_online_feedback", &status, sizeof(status));

    // Event 3: Telemetry arrives
    uint8_t telemetry_data[100] = {0x42, 0x43};
    simulate_channel_data("online_to_diag_telemetry", telemetry_data, sizeof(telemetry_data));

    // Event 4: Another vital command
    cmd.command_id = 1002;
    cmd.speed_kmh = 100;
    simulate_channel_data("online_to_standby_vital", &cmd, sizeof(cmd));

    // Event 5: Configuration update
    uint8_t config_data[32] = {0xAA, 0xBB};
    simulate_channel_data("config_update_channel", config_data, sizeof(config_data));

    // Event 6: Standby degradation
    status.is_healthy = false;
    status.error_count = 15;
    simulate_channel_data("standby_to_online_feedback", &status, sizeof(status));

    /* ====================================================================
     * STEP 3: Report metrics
     * ==================================================================== */

    printf("\n--- STEP 3: Metrics Summary ---\n");
    printf("Vital commands received: %u\n", app.vital_commands_received);
    printf("Feedback messages received: %u\n", app.feedback_messages_received);
    printf("Telemetry messages received: %u\n", app.telemetry_messages_received);
    printf("Config updates applied: %u\n", app.config_updates_applied);

    printf("\nLast command: ID=%u, Speed=%u km/h, E-Stop=%s\n",
           app.last_command.command_id,
           app.last_command.speed_kmh,
           app.last_command.emergency_stop ? "YES" : "NO");

    printf("Standby status: Healthy=%s, Errors=%u\n",
           app.standby_status.is_healthy ? "YES" : "NO",
           app.standby_status.error_count);

    printf("\n========================================\n");
    printf("Key Takeaways\n");
    printf("========================================\n");
    printf("✓ Each channel has a unique name\n");
    printf("✓ Callbacks are registered at startup\n");
    printf("✓ OS backend calls callback when data arrives\n");
    printf("✓ Callback identifies channel by name\n");
    printf("✓ No polling needed (event-driven)\n");
    printf("✓ Easy to add/remove channels\n");
    printf("✓ Easy to log and monitor by channel name\n");

    return 0;
}

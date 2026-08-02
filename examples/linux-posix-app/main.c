/**
 * @file main.c
 * @brief Linux POSIX example application demonstrating safeAPIFramework
 *
 * This example shows a simple railway message processor running on Linux with POSIX OAL.
 * It demonstrates:
 *   - Timer management (periodic message processing)
 *   - Task scheduling and IPC
 *   - Safe state transitions
 *   - Structured logging
 *   - Error handling with status codes
 *
 * Use case: Simple train communication handler
 * - Main task processes incoming messages
 * - Timer callback generates heartbeats
 * - All operations logged with safe state management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "safeapi/status.h"
#include "safeapi/types.h"
#include "safeapi/buffer.h"
#include "safeapi/string.h"
#include "safeapi/log.h"
#include "safeapi/timer.h"
#include "safeapi/safestate.h"

/* ============================================================================
 * Application Configuration
 * ========================================================================== */

#define APP_NAME "railway-message-processor"
#define APP_VERSION "1.0.0"
#define MESSAGE_BUFFER_SIZE 256
#define MAX_MESSAGES 10

/* Application state machine */
typedef enum {
    STATE_INIT,
    STATE_RUNNING,
    STATE_ERROR,
    STATE_SHUTDOWN
} app_state_t;

typedef struct {
    app_state_t state;
    uint32_t message_count;
    uint32_t heartbeat_count;
    uint32_t error_count;
    volatile int running;
} app_context_t;

static app_context_t g_app = {
    .state = STATE_INIT,
    .message_count = 0,
    .heartbeat_count = 0,
    .error_count = 0,
    .running = 1
};

/* ============================================================================
 * Helper Functions
 * ========================================================================== */

/**
 * @brief Initialize the application context
 */
static sapi_status_t app_init(void)
{
    SAPI_LOG_INFO("Initializing %s v%s", APP_NAME, APP_VERSION);

    sapi_status_t status = sapi_log_initialize();
    if (status != SAPI_STATUS_OK) {
        fprintf(stderr, "Failed to initialize logging: %d\n", status);
        return status;
    }

    g_app.state = STATE_RUNNING;
    SAPI_LOG_INFO("Application initialized successfully");
    return SAPI_STATUS_OK;
}

/**
 * @brief Process a single message (simulated)
 * @param msg Message data
 * @param len Message length
 */
static sapi_status_t process_message(const uint8_t *msg, size_t len)
{
    SAPI_ASSERT(msg != NULL);
    SAPI_ASSERT(len > 0);
    SAPI_ASSERT(len <= MESSAGE_BUFFER_SIZE);

    SAPI_LOG_DEBUG("Processing message [%zu bytes]", len);

    /* Example: Parse first byte as message type */
    uint8_t msg_type = msg[0];

    switch (msg_type) {
        case 0x01: /* HEARTBEAT */
            SAPI_LOG_INFO("Received HEARTBEAT from train");
            break;

        case 0x02: /* POSITION_UPDATE */
            if (len >= 5) {
                uint32_t position;
                memcpy(&position, &msg[1], sizeof(position));
                SAPI_LOG_INFO("Train position update: %u", position);
            } else {
                SAPI_LOG_WARN("Invalid POSITION_UPDATE message (len=%zu)", len);
                return SAPI_STATUS_ERROR;
            }
            break;

        case 0x03: /* COMMAND_ACK */
            SAPI_LOG_INFO("Received command acknowledgment");
            break;

        default:
            SAPI_LOG_WARN("Unknown message type: 0x%02x", msg_type);
            return SAPI_STATUS_ERROR;
    }

    g_app.message_count++;
    return SAPI_STATUS_OK;
}

/**
 * @brief Timer callback - generates periodic heartbeats
 * @param timer_id Timer identifier
 * @param user_context Application context
 */
static void heartbeat_callback(sapi_timer_id_t timer_id, void *user_context)
{
    (void)timer_id;
    (void)user_context;

    g_app.heartbeat_count++;

    /* Log heartbeat every 5th beat (reduce log spam) */
    if ((g_app.heartbeat_count % 5) == 0) {
        SAPI_LOG_INFO("Heartbeat #%u - Messages processed: %u, Errors: %u",
                      g_app.heartbeat_count,
                      g_app.message_count,
                      g_app.error_count);
    }
}

/**
 * @brief Simulate receiving a message from train
 */
static sapi_status_t simulate_train_message(void)
{
    /* Simulate different message types in sequence */
    static uint8_t sequence[] = {0x01, 0x02, 0x01, 0x03};
    static size_t seq_idx = 0;

    uint8_t message[MESSAGE_BUFFER_SIZE];
    size_t msg_len = 0;

    message[0] = sequence[seq_idx % 4];

    /* Add position data for POSITION_UPDATE messages */
    if (message[0] == 0x02) {
        uint32_t position = 1000 + (g_app.message_count * 50);
        memcpy(&message[1], &position, sizeof(position));
        msg_len = 5;
    } else {
        msg_len = 1;
    }

    seq_idx++;
    return process_message(message, msg_len);
}

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        SAPI_LOG_INFO("Received signal %d, shutting down...", sig);
        g_app.running = 0;
    }
}

/**
 * @brief Safe state transition
 */
static sapi_status_t transition_to_state(app_state_t new_state)
{
    const char *state_names[] = {
        "INIT", "RUNNING", "ERROR", "SHUTDOWN"
    };

    if (new_state >= 4) {
        return SAPI_STATUS_ERROR;
    }

    if (g_app.state == new_state) {
        return SAPI_STATUS_OK; /* Already in this state */
    }

    SAPI_LOG_INFO("State transition: %s -> %s",
                  state_names[g_app.state],
                  state_names[new_state]);

    g_app.state = new_state;
    return SAPI_STATUS_OK;
}

/* ============================================================================
 * Main Application
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    sapi_status_t status;

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize application */
    status = app_init();
    if (status != SAPI_STATUS_OK) {
        fprintf(stderr, "Initialization failed\n");
        return EXIT_FAILURE;
    }

    /* Simulate timer callback directly (no real timer in this simple example) */
    /* In a real app, use sapi_timer_start() with proper backend registration */
    SAPI_LOG_INFO("Starting message processing loop (Ctrl+C to exit)");

    uint32_t loop_count = 0;
    while (g_app.running && loop_count < 50) {
        loop_count++;

        /* Simulate receiving a message */
        status = simulate_train_message();
        if (status != SAPI_STATUS_OK) {
            g_app.error_count++;
            SAPI_LOG_ERROR("Message processing failed: %d", status);
        }

        /* Simulate timer callback every 10 iterations */
        if ((loop_count % 10) == 0) {
            heartbeat_callback(0, NULL);
        }

        /* Small delay to prevent busy-waiting */
        usleep(100000); /* 100ms */
    }

    /* Shutdown */
    SAPI_LOG_INFO("Shutting down...");
    transition_to_state(STATE_SHUTDOWN);

    /* Print summary */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("%s Summary\n", APP_NAME);
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Messages processed:  %u\n", g_app.message_count);
    printf("Heartbeats sent:     %u\n", g_app.heartbeat_count);
    printf("Errors encountered:  %u\n", g_app.error_count);
    printf("Final state:         SHUTDOWN\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    sapi_log_shutdown();
    return EXIT_SUCCESS;
}

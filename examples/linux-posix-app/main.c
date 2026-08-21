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
 *
 * No timer/log backend is registered in this minimal example (a real
 * integration wires one per ADR-005 - see safeAPIRBC2oo2's
 * src/posix_backend/ for a full POSIX backend), so sapi_log_write() is a
 * documented no-op here (REQ-OAL-LOG-001: a missing backend never blocks
 * or fails the caller) and the timer callback is invoked directly rather
 * than through a real sapi_timer_create()/_start() - see the main loop
 * below.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"
#include "safeapi/buffer/sapi_buffer.h"
#include "safeapi/string/sapi_string.h"
#include "safeapi/log/sapi_log.h"
#include "safeapi/timer/sapi_timer.h"
#include "safeapi/safestate/sapi_safestate.h"

/* ============================================================================
 * Application Configuration
 * ========================================================================== */

#define APP_NAME "railway-message-processor"
#define APP_VERSION "1.0.0"
#define MESSAGE_BUFFER_SIZE 256U
#define LOG_LINE_MAX 128U

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
 * @brief Logs one already-formatted line at the given level.
 *
 * sapi_log_write() takes a fixed message string, deliberately no varargs
 * (MISRA C:2012 Rule 17.1 - see its own header) - callers below build the
 * line with snprintf() into a fixed-size buffer first, at the call site,
 * rather than through a variadic wrapper here, to keep that same
 * no-varargs discipline visible in this example rather than reintroducing
 * it one layer down.
 */
static void log_line(sapi_log_level_t level, const char *line)
{
    sapi_log_write(level, APP_NAME, line);
}

/**
 * @brief Initialize the application context
 */
static sapi_status_t app_init(void)
{
    char line[LOG_LINE_MAX];
    sapi_status_t status = sapi_log_init();
    if (status != SAPI_STATUS_OK) {
        (void)fprintf(stderr, "Failed to initialize logging: %d\n", (int)status);
        return status;
    }

    (void)snprintf(line, sizeof(line), "Initializing %s v%s", APP_NAME, APP_VERSION);
    log_line(SAPI_LOG_LEVEL_INFO, line);
    g_app.state = STATE_RUNNING;
    log_line(SAPI_LOG_LEVEL_INFO, "Application initialized successfully");
    return SAPI_STATUS_OK;
}

/**
 * @brief Process a single message (simulated)
 * @param msg Message data
 * @param len Message length
 */
static sapi_status_t process_message(const uint8_t *msg, size_t len)
{
    char line[LOG_LINE_MAX];
    uint8_t msg_type;

    SAPI_ASSERT(msg != NULL);
    SAPI_ASSERT(len > 0U);
    SAPI_ASSERT(len <= MESSAGE_BUFFER_SIZE);

    (void)snprintf(line, sizeof(line), "Processing message [%zu bytes]", len);
    log_line(SAPI_LOG_LEVEL_DEBUG, line);

    /* Example: Parse first byte as message type */
    msg_type = msg[0];

    switch (msg_type) {
        case 0x01: /* HEARTBEAT */
            log_line(SAPI_LOG_LEVEL_INFO, "Received HEARTBEAT from train");
            break;

        case 0x02: /* POSITION_UPDATE */
            if (len >= 5U) {
                uint32_t position;
                (void)memcpy(&position, &msg[1], sizeof(position));
                (void)snprintf(line, sizeof(line), "Train position update: %u", position);
                log_line(SAPI_LOG_LEVEL_INFO, line);
            } else {
                (void)snprintf(line, sizeof(line), "Invalid POSITION_UPDATE message (len=%zu)", len);
                log_line(SAPI_LOG_LEVEL_WARNING, line);
                return SAPI_STATUS_INVALID_PARAM;
            }
            break;

        case 0x03: /* COMMAND_ACK */
            log_line(SAPI_LOG_LEVEL_INFO, "Received command acknowledgment");
            break;

        default:
            (void)snprintf(line, sizeof(line), "Unknown message type: 0x%02x", msg_type);
            log_line(SAPI_LOG_LEVEL_WARNING, line);
            return SAPI_STATUS_INVALID_PARAM;
    }

    g_app.message_count++;
    return SAPI_STATUS_OK;
}

/**
 * @brief Timer callback - generates periodic heartbeats
 * @param handle       Timer that expired (unused - invoked directly in this
 *                     minimal example, not through a real timer backend)
 * @param user_context Application context
 */
static void heartbeat_callback(sapi_timer_handle_t handle, void *user_context)
{
    (void)handle;
    (void)user_context;

    g_app.heartbeat_count++;

    /* Log heartbeat every 5th beat (reduce log spam) */
    if ((g_app.heartbeat_count % 5U) == 0U) {
        char line[LOG_LINE_MAX];
        (void)snprintf(line, sizeof(line),
                        "Heartbeat #%u - Messages processed: %u, Errors: %u",
                        g_app.heartbeat_count,
                        g_app.message_count,
                        g_app.error_count);
        log_line(SAPI_LOG_LEVEL_INFO, line);
    }
}

/**
 * @brief Simulate receiving a message from train
 */
static sapi_status_t simulate_train_message(void)
{
    /* Simulate different message types in sequence */
    static const uint8_t sequence[] = {0x01, 0x02, 0x01, 0x03};
    static size_t seq_idx = 0;

    uint8_t message[MESSAGE_BUFFER_SIZE];
    size_t msg_len;

    message[0] = sequence[seq_idx % 4U];

    /* Add position data for POSITION_UPDATE messages */
    if (message[0] == 0x02U) {
        uint32_t position = 1000U + (g_app.message_count * 50U);
        (void)memcpy(&message[1], &position, sizeof(position));
        msg_len = 5U;
    } else {
        msg_len = 1U;
    }

    seq_idx++;
    return process_message(message, msg_len);
}

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int sig)
{
    if ((sig == SIGINT) || (sig == SIGTERM)) {
        g_app.running = 0;
    }
}

/**
 * @brief Safe state transition
 */
static sapi_status_t transition_to_state(app_state_t new_state)
{
    static const char *const state_names[] = {
        "INIT", "RUNNING", "ERROR", "SHUTDOWN"
    };

    if ((uint32_t)new_state >= 4U) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (g_app.state == new_state) {
        return SAPI_STATUS_OK; /* Already in this state */
    }

    {
        char line[LOG_LINE_MAX];
        (void)snprintf(line, sizeof(line), "State transition: %s -> %s",
                        state_names[g_app.state], state_names[new_state]);
        log_line(SAPI_LOG_LEVEL_INFO, line);
    }

    g_app.state = new_state;
    return SAPI_STATUS_OK;
}

/* ============================================================================
 * Main Application
 * ========================================================================== */

int main(int argc, char *argv[])
{
    sapi_status_t status;
    uint32_t loop_count;

    (void)argc;
    (void)argv;

    /* Set up signal handlers */
    (void)signal(SIGINT, signal_handler);
    (void)signal(SIGTERM, signal_handler);

    /* Initialize application */
    status = app_init();
    if (status != SAPI_STATUS_OK) {
        (void)fprintf(stderr, "Initialization failed\n");
        return EXIT_FAILURE;
    }

    /* No real timer backend is registered in this minimal example - the
     * heartbeat callback is invoked directly from the loop below every
     * 10th iteration instead of through sapi_timer_create()/_start(). A
     * real integration would register a backend (ADR-005) and use the
     * real timer API. */
    log_line(SAPI_LOG_LEVEL_INFO, "Starting message processing loop (Ctrl+C to exit)");

    loop_count = 0U;
    while ((g_app.running != 0) && (loop_count < 50U)) {
        loop_count++;

        /* Simulate receiving a message */
        status = simulate_train_message();
        if (status != SAPI_STATUS_OK) {
            char line[LOG_LINE_MAX];
            g_app.error_count++;
            (void)snprintf(line, sizeof(line), "Message processing failed: %d", (int)status);
            log_line(SAPI_LOG_LEVEL_ERROR, line);
        }

        /* Simulate timer callback every 10 iterations */
        if ((loop_count % 10U) == 0U) {
            heartbeat_callback(NULL, NULL);
        }

        /* Small delay to prevent busy-waiting */
        (void)usleep(100000); /* 100ms */
    }

    /* Shutdown */
    log_line(SAPI_LOG_LEVEL_INFO, "Shutting down...");
    (void)transition_to_state(STATE_SHUTDOWN);

    /* Print summary */
    (void)printf("\n");
    (void)printf("===================================================================\n");
    (void)printf("%s Summary\n", APP_NAME);
    (void)printf("===================================================================\n");
    (void)printf("Messages processed:  %u\n", g_app.message_count);
    (void)printf("Heartbeats sent:     %u\n", g_app.heartbeat_count);
    (void)printf("Errors encountered:  %u\n", g_app.error_count);
    (void)printf("Final state:         SHUTDOWN\n");
    (void)printf("===================================================================\n");
    (void)printf("\n");

    return EXIT_SUCCESS;
}

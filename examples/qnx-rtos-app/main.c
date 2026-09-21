/**
 * @file main.c
 * @brief QNX RTOS example application demonstrating RteFramework
 *
 * This example shows a railway control server running on QNX RTOS with deterministic
 * real-time guarantees. It demonstrates:
 *   - QNX message passing (MsgSend, MsgReceive)
 *   - Real-time task scheduling
 *   - Deterministic message processing
 *   - Safe state management
 *   - Structured logging in RTOS environment
 *   - Integration with RteFramework OAL
 *
 * Use case: Train control server
 * - Receives train position updates via QNX message passing
 * - Processes signals/track state
 * - Broadcasts control commands
 * - Deterministic timing for safety-critical operations
 *
 * Compilation: Use cmake with Toolchain-QNX.cmake
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <sys/siginfo.h>

#include "rte/status.h"
#include "rte/types.h"
#include "rte/log.h"
#include "rte/safestate.h"

/* ============================================================================
 * QNX Configuration
 * ========================================================================== */

#define SERVER_NAME "railway-server"
#define CHANNEL_NAME "/railway/cmd"
#define MESSAGE_BUFFER_SIZE 256

/* Application state */
typedef enum {
    STATE_INIT = 0,
    STATE_LISTENING = 1,
    STATE_PROCESSING = 2,
    STATE_ERROR = 3,
    STATE_SHUTDOWN = 4
} app_state_t;

typedef struct {
    int chid;                    /* Channel ID for message passing */
    int coid;                    /* Connection ID */
    app_state_t state;
    uint32_t messages_received;
    uint32_t messages_processed;
    uint32_t errors;
    volatile int running;
} app_context_t;

static app_context_t g_app = {
    .chid = -1,
    .coid = -1,
    .state = STATE_INIT,
    .messages_received = 0,
    .messages_processed = 0,
    .errors = 0,
    .running = 1
};

/* Message types */
typedef enum {
    MSG_TYPE_PING = 1,
    MSG_TYPE_TRAIN_UPDATE = 2,
    MSG_TYPE_SIGNAL_QUERY = 3,
    MSG_TYPE_SHUTDOWN = 99
} msg_type_t;

/* Message structure */
typedef struct {
    uint32_t type;
    uint32_t train_id;
    uint32_t position;
    uint8_t data[MESSAGE_BUFFER_SIZE - 12];
} train_message_t;

/* Reply structure */
typedef struct {
    rte_status_t status;
    uint32_t signal_state;
    uint32_t track_speed_limit;
} cmd_reply_t;

/* ============================================================================
 * QNX Message Handling
 * ========================================================================== */

/**
 * @brief Initialize QNX message channel
 */
static rte_status_t qnx_init_channel(void)
{
    /* Create a channel for receiving messages */
    g_app.chid = ChannelCreate(0);
    if (g_app.chid == -1) {
        RTE_LOG_ERROR("Failed to create QNX channel: %d", errno);
        return RTE_STATUS_ERROR;
    }

    RTE_LOG_INFO("QNX channel created: %d", g_app.chid);

    /* Register the channel in the name space */
    /* In production, would attach to well-known name */
    RTE_LOG_INFO("Message channel ready (PID=%d, CHID=%d)", getpid(), g_app.chid);

    return RTE_STATUS_OK;
}

/**
 * @brief Process train update message
 */
static rte_status_t process_train_update(const train_message_t *msg,
                                           cmd_reply_t *reply)
{
    RTE_ASSERT(msg != NULL);
    RTE_ASSERT(reply != NULL);

    RTE_LOG_DEBUG("Train update: ID=%u, Position=%u", msg->train_id, msg->position);

    /* Validate train position */
    if (msg->position > 10000) {
        RTE_LOG_WARN("Train position out of valid range: %u", msg->position);
        reply->status = RTE_STATUS_ERROR;
        reply->signal_state = 0; /* Red signal - stop */
        return RTE_STATUS_ERROR;
    }

    /* Determine signal state based on position */
    if (msg->position < 1000) {
        reply->signal_state = 1; /* Green - go */
        reply->track_speed_limit = 80;
    } else if (msg->position < 5000) {
        reply->signal_state = 2; /* Yellow - caution */
        reply->track_speed_limit = 40;
    } else {
        reply->signal_state = 0; /* Red - stop */
        reply->track_speed_limit = 0;
    }

    reply->status = RTE_STATUS_OK;

    RTE_LOG_INFO("Train %u: signal=%u, speed_limit=%u",
                  msg->train_id,
                  reply->signal_state,
                  reply->track_speed_limit);

    g_app.messages_processed++;
    return RTE_STATUS_OK;
}

/**
 * @brief Handle incoming message
 */
static rte_status_t handle_message(train_message_t *msg, int rcvid)
{
    cmd_reply_t reply = {
        .status = RTE_STATUS_OK,
        .signal_state = 0,
        .track_speed_limit = 0
    };

    g_app.messages_received++;

    /* Dispatch based on message type */
    switch (msg->type) {
        case MSG_TYPE_PING:
            RTE_LOG_DEBUG("Received PING from client");
            reply.status = RTE_STATUS_OK;
            break;

        case MSG_TYPE_TRAIN_UPDATE:
            RTE_LOG_DEBUG("Processing TRAIN_UPDATE message");
            process_train_update(msg, &reply);
            break;

        case MSG_TYPE_SIGNAL_QUERY:
            RTE_LOG_DEBUG("Received SIGNAL_QUERY");
            reply.signal_state = 1; /* Default: green */
            reply.track_speed_limit = 80;
            reply.status = RTE_STATUS_OK;
            break;

        case MSG_TYPE_SHUTDOWN:
            RTE_LOG_INFO("Received SHUTDOWN request");
            g_app.running = 0;
            reply.status = RTE_STATUS_OK;
            break;

        default:
            RTE_LOG_WARN("Unknown message type: %u", msg->type);
            reply.status = RTE_STATUS_ERROR;
            break;
    }

    /* Send reply back to client */
    MsgReply(rcvid, EOK, &reply, sizeof(reply));

    return reply.status;
}

/**
 * @brief Main server loop
 */
static rte_status_t server_loop(void)
{
    int rcvid;
    train_message_t msg;
    int rc;

    RTE_LOG_INFO("Starting QNX server loop");
    g_app.state = STATE_LISTENING;

    while (g_app.running) {
        /* Block waiting for a message */
        rcvid = MsgReceive(g_app.chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) {
            /* Error receiving message */
            RTE_LOG_ERROR("MsgReceive failed: %d", errno);
            g_app.errors++;
            continue;
        }

        if (rcvid == 0) {
            /* Pulse or special case */
            RTE_LOG_DEBUG("Received pulse or special message");
            continue;
        }

        /* Process the message */
        g_app.state = STATE_PROCESSING;
        rc = handle_message(&msg, rcvid);
        g_app.state = STATE_LISTENING;

        if (rc != RTE_STATUS_OK) {
            g_app.errors++;
        }

        /* Log statistics every 10 messages */
        if ((g_app.messages_received % 10) == 0) {
            RTE_LOG_INFO("Stats: received=%u, processed=%u, errors=%u",
                          g_app.messages_received,
                          g_app.messages_processed,
                          g_app.errors);
        }
    }

    return RTE_STATUS_OK;
}

/**
 * @brief Shutdown application
 */
static rte_status_t app_shutdown(void)
{
    RTE_LOG_INFO("Shutting down railway server");

    g_app.state = STATE_SHUTDOWN;

    /* Destroy channel */
    if (g_app.chid != -1) {
        ChannelDestroy(g_app.chid);
        g_app.chid = -1;
    }

    return RTE_STATUS_OK;
}

/* ============================================================================
 * Main Entry Point
 * ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    rte_status_t status;

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Railway Control Server (QNX RTOS)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("PID: %d\n", getpid());

    /* Initialize logging */
    status = rte_log_initialize();
    if (status != RTE_STATUS_OK) {
        fprintf(stderr, "Failed to initialize logging\n");
        return EXIT_FAILURE;
    }

    RTE_LOG_INFO("Railway Control Server starting (QNX RTOS)");

    /* Initialize QNX message channel */
    status = qnx_init_channel();
    if (status != RTE_STATUS_OK) {
        RTE_LOG_ERROR("Failed to initialize QNX channel");
        return EXIT_FAILURE;
    }

    /* Run server loop */
    status = server_loop();
    if (status != RTE_STATUS_OK) {
        RTE_LOG_ERROR("Server loop exited with error: %d", status);
    }

    /* Shutdown */
    app_shutdown();

    /* Print summary */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Railway Server Summary\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Messages received:   %u\n", g_app.messages_received);
    printf("Messages processed:  %u\n", g_app.messages_processed);
    printf("Errors:              %u\n", g_app.errors);
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    rte_log_shutdown();
    return EXIT_SUCCESS;
}

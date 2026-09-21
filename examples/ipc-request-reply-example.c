/**
 * @file ipc-request-reply-example.c
 * @brief IPC Request-Reply Pattern Example
 *
 * Demonstrates synchronous RPC-style communication:
 * - Client: Train controller queries signal state from database
 * - Server: Signal database receives query and replies with current state
 *
 * This is a TWO-PROCESS EXAMPLE. In production, you'd run:
 *   Terminal 1: ./signal-server
 *   Terminal 2: ./train-controller
 *
 * Both use IPC Request-Reply for bidirectional communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "rte/status.h"
#include "rte/log.h"
#include "rte/ipc/rte_ipc_request_reply.h"

/* ============================================================================
 * Message Definitions (Shared between client and server)
 * ========================================================================== */

/**
 * @brief Query message: Train asks for signal state
 */
typedef struct {
    uint32_t train_id;      /**< Which train is asking */
    uint32_t location_m;    /**< Train location in meters */
} signal_query_t;

/**
 * @brief Reply message: Signal state and limits
 */
typedef struct {
    rte_status_t status;           /**< Query result status */
    uint8_t signal_state;           /**< 0=RED, 1=YELLOW, 2=GREEN */
    uint32_t track_speed_limit;     /**< Speed limit in km/h */
    char description[32];           /**< Human-readable state (optional) */
} signal_reply_t;

#define SIGNAL_RED    0
#define SIGNAL_YELLOW 1
#define SIGNAL_GREEN  2

/* ============================================================================
 * SIGNAL SERVER (Responder)
 * ========================================================================== */

/**
 * @brief Signal database query logic
 *
 * Simulates a real signal database that determines signal state based on
 * track conditions, occupancy, etc.
 */
static rte_status_t query_signal_database(const signal_query_t *query,
                                            signal_reply_t *reply)
{
    RTE_LOG_DEBUG("Database query: train=%u, location=%u m",
                   query->train_id, query->location_m);

    /* Simple rules based on location */
    if (query->location_m < 1000) {
        reply->signal_state = SIGNAL_GREEN;
        reply->track_speed_limit = 100;
        strcpy(reply->description, "Clear ahead");
    } else if (query->location_m < 5000) {
        reply->signal_state = SIGNAL_YELLOW;
        reply->track_speed_limit = 50;
        strcpy(reply->description, "Caution");
    } else {
        reply->signal_state = SIGNAL_RED;
        reply->track_speed_limit = 0;
        strcpy(reply->description, "Stop");
    }

    reply->status = RTE_STATUS_OK;
    return RTE_STATUS_OK;
}

/**
 * @brief Run signal server (responder)
 *
 * Creates an IPC request-reply server, waits for queries, and responds.
 */
static int run_signal_server(void)
{
    rte_ipc_rr_server_t server;
    rte_ipc_rr_request_t request;
    rte_status_t status;

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Signal Database Server (Request-Reply)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    RTE_LOG_INFO("Starting signal server");

    /* Create server channel */
    rte_ipc_rr_server_config_t server_config = {
        .name = "signal-database",
        .request_size = sizeof(signal_query_t),
        .reply_size = sizeof(signal_reply_t),
        .queue_depth = 10
    };

    status = rte_ipc_rr_server_create(&server, &server_config);
    if (status != RTE_STATUS_OK) {
        RTE_LOG_ERROR("Failed to create server: %d", status);
        return EXIT_FAILURE;
    }

    RTE_LOG_INFO("Server ready, waiting for queries...");

    /* Process 10 queries then shutdown */
    for (int i = 0; i < 10; i++) {
        /* Wait for query (5 second timeout) */
        status = rte_ipc_rr_receive_request(&server, &request, 5000);

        if (status == RTE_STATUS_TIMEOUT) {
            RTE_LOG_WARN("No query received (timeout)");
            continue;
        }

        if (status != RTE_STATUS_OK) {
            RTE_LOG_ERROR("Failed to receive request: %d", status);
            break;
        }

        /* Process query */
        signal_query_t *query = (signal_query_t *)request.request_data;
        signal_reply_t reply = {0};

        RTE_LOG_INFO("Received query #%d from train %u (location=%u m)",
                      i + 1, query->train_id, query->location_m);

        /* Query database */
        query_signal_database(query, &reply);

        /* Send reply */
        status = rte_ipc_rr_send_reply(&server, request.request_id,
                                         &reply, sizeof(reply));

        if (status == RTE_STATUS_OK) {
            RTE_LOG_INFO("Replied: signal=%u, speed=%u km/h, \"%s\"",
                          reply.signal_state, reply.track_speed_limit,
                          reply.description);
        } else {
            RTE_LOG_ERROR("Failed to send reply: %d", status);
        }
    }

    /* Cleanup */
    rte_ipc_rr_server_destroy(&server);

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Signal Server shutdown\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    return EXIT_SUCCESS;
}

/* ============================================================================
 * TRAIN CONTROLLER CLIENT (Requester)
 * ========================================================================== */

/**
 * @brief Simulate train moving and querying signal state
 */
static int run_train_controller(void)
{
    rte_ipc_rr_client_t client;
    rte_status_t status;

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Train Controller (Request-Reply Client)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    RTE_LOG_INFO("Starting train controller");

    /* Connect to signal server */
    rte_ipc_rr_client_config_t client_config = {
        .server_name = "signal-database",
        .request_size = sizeof(signal_query_t),
        .reply_size = sizeof(signal_reply_t)
    };

    status = rte_ipc_rr_client_create(&client, &client_config);
    if (status != RTE_STATUS_OK) {
        RTE_LOG_ERROR("Failed to connect to signal server: %d", status);
        return EXIT_FAILURE;
    }

    RTE_LOG_INFO("Connected to signal server");

    /* Simulate train moving and querying signal state */
    uint32_t train_id = 42;
    uint32_t position = 0;

    for (int i = 0; i < 10; i++) {
        /* Move train */
        position += 500;

        /* Query signal state */
        signal_query_t query = {
            .train_id = train_id,
            .location_m = position
        };

        signal_reply_t reply = {0};

        RTE_LOG_INFO("Query #%d: Train %u at location %u m",
                      i + 1, query.train_id, query.location_m);

        /* Send request and wait for reply (5 second timeout) */
        status = rte_ipc_rr_request(&client,
                                      &query, sizeof(query),
                                      &reply, sizeof(reply),
                                      5000);

        if (status == RTE_STATUS_OK) {
            const char *signal_names[] = {"RED", "YELLOW", "GREEN"};
            RTE_LOG_INFO("Reply: signal=%s, speed=%u km/h (\"%s\")",
                          signal_names[reply.signal_state],
                          reply.track_speed_limit,
                          reply.description);

            /* React to signal state */
            switch (reply.signal_state) {
                case SIGNAL_GREEN:
                    printf("  → Train accelerating to %u km/h\n",
                           reply.track_speed_limit);
                    break;
                case SIGNAL_YELLOW:
                    printf("  → Train preparing to stop\n");
                    break;
                case SIGNAL_RED:
                    printf("  → Train EMERGENCY STOP\n");
                    break;
            }
        } else if (status == RTE_STATUS_TIMEOUT) {
            RTE_LOG_ERROR("Server did not reply in time!");
            return EXIT_FAILURE;
        } else {
            RTE_LOG_ERROR("RPC failed: %d", status);
            return EXIT_FAILURE;
        }

        /* Simulate travel time */
        usleep(500000); /* 500ms */
    }

    /* Cleanup */
    rte_ipc_rr_client_destroy(&client);

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Train Controller shutdown\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    return EXIT_SUCCESS;
}

/* ============================================================================
 * Main Entry Point
 * ========================================================================== */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <server|client>\n", argv[0]);
        printf("  server  - Run as signal database server\n");
        printf("  client  - Run as train controller client\n");
        printf("\nExample:\n");
        printf("  Terminal 1: %s server\n", argv[0]);
        printf("  Terminal 2: %s client\n", argv[0]);
        return EXIT_FAILURE;
    }

    rte_log_initialize();

    if (strcmp(argv[1], "server") == 0) {
        return run_signal_server();
    } else if (strcmp(argv[1], "client") == 0) {
        return run_train_controller();
    } else {
        printf("Unknown mode: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
}

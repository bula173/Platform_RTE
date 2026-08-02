/**
 * @file ipc-pubsub-example.c
 * @brief IPC Publish-Subscribe Pattern Example
 *
 * Demonstrates one-to-many event broadcasting:
 * - Publisher: Track database publishes track status changes
 * - Subscribers: Multiple controllers (signal, speed, route) react independently
 *
 * Architecture:
 * @code
 *   Track Database (Publisher)
 *        │
 *     publish()
 *        │
 *        ├─→ Signal Manager (Subscriber 1) → Adjusts signals
 *        ├─→ Speed Manager (Subscriber 2)  → Updates speed limits
 *        └─→ Route Manager (Subscriber 3)  → Plans routes
 * @endcode
 *
 * All subscribers receive same message, act independently.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "safeapi/status.h"
#include "safeapi/log.h"
#include "safeapi/ipc/sapi_ipc_pubsub.h"

/* ============================================================================
 * Message Definitions
 * ========================================================================== */

/**
 * @brief Track status event (published to all subscribers)
 */
typedef struct {
    uint32_t track_id;          /**< Which track changed */
    uint8_t occupancy;          /**< 0=EMPTY, 1=OCCUPIED, 2=UNKNOWN */
    uint32_t speed_limit;       /**< Current speed limit (km/h) */
    char description[32];       /**< Status description */
} track_status_t;

#define TRACK_EMPTY     0
#define TRACK_OCCUPIED  1
#define TRACK_UNKNOWN   2

/* ============================================================================
 * Track Database (Publisher)
 * ========================================================================== */

/**
 * @brief Simulate track status changes and publish
 */
static void* track_publisher_thread(void *arg)
{
    sapi_ipc_pubsub_topic_t *topic = (sapi_ipc_pubsub_topic_t *)arg;

    printf("[Publisher] Track status publisher starting\n");

    /* Simulate track status changes */
    track_status_t statuses[] = {
        {.track_id = 1, .occupancy = TRACK_EMPTY, .speed_limit = 100, .description = "Clear"},
        {.track_id = 2, .occupancy = TRACK_OCCUPIED, .speed_limit = 40, .description = "Train present"},
        {.track_id = 3, .occupancy = TRACK_EMPTY, .speed_limit = 80, .description = "Clear"},
        {.track_id = 1, .occupancy = TRACK_OCCUPIED, .speed_limit = 50, .description = "Train arriving"},
        {.track_id = 2, .occupancy = TRACK_EMPTY, .speed_limit = 100, .description = "Train left"},
    };

    for (int i = 0; i < 5; i++) {
        sleep(1);

        track_status_t *status = &statuses[i];

        printf("[Publisher] Publishing track %u status: %s\n",
               status->track_id, status->description);

        sapi_status_t result = sapi_ipc_pubsub_publish(*topic, status, sizeof(*status));

        if (result != SAPI_STATUS_OK) {
            SAPI_LOG_ERROR("Publish failed: %d", result);
        }
    }

    printf("[Publisher] Publisher done\n");
    return NULL;
}

/* ============================================================================
 * Subscribers (Consumers)
 * ========================================================================== */

/**
 * @brief Signal Manager - Reacts to track status by updating signals
 */
static void* signal_manager_thread(void *arg)
{
    sapi_ipc_pubsub_subscriber_t *subscriber = (sapi_ipc_pubsub_subscriber_t *)arg;

    printf("[Signal Manager] Started\n");

    for (int i = 0; i < 5; i++) {
        track_status_t status = {0};

        sapi_status_t result = sapi_ipc_pubsub_receive(
            *subscriber,
            &status, sizeof(status),
            2000  /* 2 second timeout */
        );

        if (result == SAPI_STATUS_OK) {
            printf("[Signal Manager] Track %u: ", status.track_id);

            if (status.occupancy == TRACK_OCCUPIED) {
                printf("Setting signal to RED (stop)\n");
            } else if (status.speed_limit >= 80) {
                printf("Setting signal to GREEN (go)\n");
            } else {
                printf("Setting signal to YELLOW (caution)\n");
            }
        } else if (result == SAPI_STATUS_TIMEOUT) {
            printf("[Signal Manager] No update received (timeout)\n");
        }
    }

    printf("[Signal Manager] Done\n");
    return NULL;
}

/**
 * @brief Speed Manager - Reacts to track status by updating speed limits
 */
static void* speed_manager_thread(void *arg)
{
    sapi_ipc_pubsub_subscriber_t *subscriber = (sapi_ipc_pubsub_subscriber_t *)arg;

    printf("[Speed Manager] Started\n");

    for (int i = 0; i < 5; i++) {
        track_status_t status = {0};

        sapi_status_t result = sapi_ipc_pubsub_receive(
            *subscriber,
            &status, sizeof(status),
            2000
        );

        if (result == SAPI_STATUS_OK) {
            printf("[Speed Manager] Track %u: Speed limit = %u km/h\n",
                   status.track_id, status.speed_limit);
        } else if (result == SAPI_STATUS_TIMEOUT) {
            printf("[Speed Manager] No update received (timeout)\n");
        }
    }

    printf("[Speed Manager] Done\n");
    return NULL;
}

/**
 * @brief Route Manager - Reacts to track status by planning routes
 */
static void* route_manager_thread(void *arg)
{
    sapi_ipc_pubsub_subscriber_t *subscriber = (sapi_ipc_pubsub_subscriber_t *)arg;

    printf("[Route Manager] Started\n");

    for (int i = 0; i < 5; i++) {
        track_status_t status = {0};

        sapi_status_t result = sapi_ipc_pubsub_receive(
            *subscriber,
            &status, sizeof(status),
            2000
        );

        if (result == SAPI_STATUS_OK) {
            if (status.occupancy == TRACK_OCCUPIED) {
                printf("[Route Manager] Track %u occupied - rerouting trains\n",
                       status.track_id);
            } else {
                printf("[Route Manager] Track %u available - planning route\n",
                       status.track_id);
            }
        } else if (result == SAPI_STATUS_TIMEOUT) {
            printf("[Route Manager] No update received (timeout)\n");
        }
    }

    printf("[Route Manager] Done\n");
    return NULL;
}

/* ============================================================================
 * Main Application
 * ========================================================================== */

int main(void)
{
    sapi_log_initialize();

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("IPC Publish-Subscribe Example\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    SAPI_LOG_INFO("Creating pub-sub topic...");

    /* Create topic */
    sapi_ipc_pubsub_topic_config_t topic_config = {
        .topic_name = "track_status",
        .message_size = sizeof(track_status_t),
        .max_subscribers = 5
    };

    sapi_ipc_pubsub_topic_t topic;
    sapi_status_t status = sapi_ipc_pubsub_topic_create(&topic, &topic_config);

    if (status != SAPI_STATUS_OK) {
        SAPI_LOG_ERROR("Failed to create topic: %d", status);
        return EXIT_FAILURE;
    }

    SAPI_LOG_INFO("Creating subscribers...");

    /* Create subscribers */
    sapi_ipc_pubsub_subscriber_t signal_subscriber, speed_subscriber, route_subscriber;

    sapi_ipc_pubsub_subscriber_config_t signal_config = {
        .subscriber_name = "signal_manager",
        .topic = topic,
        .queue_depth = 5
    };
    sapi_ipc_pubsub_subscribe(&signal_subscriber, &signal_config);

    sapi_ipc_pubsub_subscriber_config_t speed_config = {
        .subscriber_name = "speed_manager",
        .topic = topic,
        .queue_depth = 5
    };
    sapi_ipc_pubsub_subscribe(&speed_subscriber, &speed_config);

    sapi_ipc_pubsub_subscriber_config_t route_config = {
        .subscriber_name = "route_manager",
        .topic = topic,
        .queue_depth = 5
    };
    sapi_ipc_pubsub_subscribe(&route_subscriber, &route_config);

    SAPI_LOG_INFO("Starting publisher and subscribers...");

    /* Start threads */
    pthread_t pub_thread, sig_thread, spd_thread, rte_thread;

    pthread_create(&pub_thread, NULL, track_publisher_thread, &topic);
    pthread_create(&sig_thread, NULL, signal_manager_thread, &signal_subscriber);
    pthread_create(&spd_thread, NULL, speed_manager_thread, &speed_subscriber);
    pthread_create(&rte_thread, NULL, route_manager_thread, &route_subscriber);

    /* Wait for threads */
    pthread_join(pub_thread, NULL);
    pthread_join(sig_thread, NULL);
    pthread_join(spd_thread, NULL);
    pthread_join(rte_thread, NULL);

    /* Cleanup */
    SAPI_LOG_INFO("Cleaning up...");
    sapi_ipc_pubsub_unsubscribe(&signal_subscriber);
    sapi_ipc_pubsub_unsubscribe(&speed_subscriber);
    sapi_ipc_pubsub_unsubscribe(&route_subscriber);
    sapi_ipc_pubsub_topic_destroy(&topic);

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Example Complete\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    printf("Key Observations:\n");
    printf("✓ Publisher (Track DB) sends status changes\n");
    printf("✓ Three subscribers (Signal, Speed, Route) receive same message\n");
    printf("✓ Each subscriber reacts independently\n");
    printf("✓ Decoupled: Publisher doesn't know about subscribers\n");
    printf("✓ Event-driven: Subscribers wake up when status changes\n");
    printf("\n");

    return EXIT_SUCCESS;
}

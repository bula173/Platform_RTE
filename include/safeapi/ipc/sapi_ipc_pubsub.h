/**
 * @file sapi_ipc_pubsub.h
 * @brief IPC Publish-Subscribe Pattern (one-to-many broadcasting)
 * @ingroup IPC
 *
 * Implements publish-subscribe communication where one publisher sends
 * messages to multiple subscribers on a topic. Subscribers are registered
 * at initialization time (static, no dynamic registration).
 *
 * Safety Properties:
 * - Static subscriber registration (no surprise subscribers)
 * - Decouples publishers from subscribers
 * - Bounded queues per subscriber (no unbounded growth)
 * - Delivery guarantees (at-least-once)
 * - Type-safe message handling
 *
 * Use Case:
 * - Track database publishes state changes (topic="track_status")
 * - Multiple controllers subscribe: signal manager, speed manager, router
 * - Each reacts independently without knowing about others
 *
 * Architecture:
 * @code
 *            Publisher
 *              │
 *         sapi_ipc_publish()
 *              │
 *              ▼
 *    ┌──────────────────┐
 *    │  Topic "track"   │
 *    │   (broadcast)    │
 *    └────┬─┬─┬────────┘
 *         │ │ │
 *    ┌────▼─▼─▼────────────────────┐
 *    │ Subscriber Queues            │
 *    ├──────────────────────────────┤
 *    │ [Queue] Signal Manager       │
 *    │ [Queue] Speed Manager        │
 *    │ [Queue] Route Manager        │
 *    └──────────────────────────────┘
 *         │          │         │
 *         ▼          ▼         ▼
 *    Consumer  Consumer   Consumer
 * @endcode
 */

#ifndef SAFEAPI_IPC_PUBSUB_H
#define SAFEAPI_IPC_PUBSUB_H

#include "safeapi/status/sapi_status.h"
#include "safeapi/types/sapi_types.h"
#include "sapi_ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Publish-Subscribe Topic
 * ========================================================================== */

/**
 * @brief Publish-subscribe topic handle
 */
typedef struct sapi_ipc_pubsub_topic_s *sapi_ipc_pubsub_topic_t;

/**
 * @brief Configuration for a pub-sub topic
 */
typedef struct {
    const char *topic_name;        /**< Topic name (e.g., "track_status") */
    size_t message_size;           /**< Fixed size of all messages on topic */
    size_t max_subscribers;        /**< Max number of subscribers allowed */
} sapi_ipc_pubsub_topic_config_t;

/**
 * @brief Create a publish-subscribe topic
 *
 * Topics are created before any publishers or subscribers are attached.
 *
 * @param topic_out      Receives topic handle (not NULL)
 * @param config         Topic configuration (not NULL)
 * @return SAPI_STATUS_OK on success
 *         SAPI_STATUS_INVALID_PARAM if config is invalid
 *
 * Example:
 * @code
 * sapi_ipc_pubsub_topic_config_t topic_config = {
 *     .topic_name = "track_status",
 *     .message_size = sizeof(track_status_t),
 *     .max_subscribers = 5
 * };
 * sapi_ipc_pubsub_topic_t topic;
 * sapi_ipc_pubsub_topic_create(&topic, &topic_config);
 * @endcode
 */
sapi_status_t sapi_ipc_pubsub_topic_create(sapi_ipc_pubsub_topic_t *topic_out,
                                            const sapi_ipc_pubsub_topic_config_t *config);

/**
 * @brief Publish a message to all subscribers on a topic
 *
 * Message is delivered to all registered subscribers asynchronously.
 * If a subscriber queue is full, behavior depends on overflow strategy.
 *
 * @param topic          Topic handle (not NULL)
 * @param message        Message to publish (not NULL)
 * @param message_size   Size of message
 * @return SAPI_STATUS_OK on success
 *         SAPI_STATUS_INVALID_PARAM if arguments invalid
 *         SAPI_STATUS_ERROR if delivery failed
 *
 * Example:
 * @code
 * track_status_t status = {
 *     .track_id = 1,
 *     .occupancy = OCCUPIED,
 *     .speed_limit = 40
 * };
 * sapi_ipc_pubsub_publish(topic, &status, sizeof(status));
 * @endcode
 */
sapi_status_t sapi_ipc_pubsub_publish(sapi_ipc_pubsub_topic_t topic,
                                       const void *message,
                                       size_t message_size);

/**
 * @brief Destroy a publish-subscribe topic
 *
 * @param topic Topic handle (not NULL)
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_ipc_pubsub_topic_destroy(sapi_ipc_pubsub_topic_t topic);

/* ============================================================================
 * Subscriber (Receiver Side)
 * ========================================================================== */

/**
 * @brief Subscriber handle for receiving from a topic
 */
typedef struct sapi_ipc_pubsub_subscriber_s *sapi_ipc_pubsub_subscriber_t;

/**
 * @brief Configuration for a subscriber
 */
typedef struct {
    const char *subscriber_name;   /**< Subscriber name (diagnostic) */
    sapi_ipc_pubsub_topic_t topic; /**< Topic to subscribe to */
    size_t queue_depth;            /**< Queue depth (max pending messages) */
} sapi_ipc_pubsub_subscriber_config_t;

/**
 * @brief Subscribe to a topic
 *
 * Registers a subscriber to receive messages from a topic.
 * Must be called during initialization, before publishing.
 *
 * @param subscriber_out Receives subscriber handle (not NULL)
 * @param config         Subscriber configuration (not NULL)
 * @return SAPI_STATUS_OK on success
 *         SAPI_STATUS_INVALID_PARAM if config invalid
 *         SAPI_STATUS_ERROR if max subscribers reached
 *
 * Example:
 * @code
 * sapi_ipc_pubsub_subscriber_config_t sub_config = {
 *     .subscriber_name = "signal_manager",
 *     .topic = track_status_topic,
 *     .queue_depth = 10
 * };
 * sapi_ipc_pubsub_subscriber_t subscriber;
 * sapi_ipc_pubsub_subscribe(&subscriber, &sub_config);
 * @endcode
 */
sapi_status_t sapi_ipc_pubsub_subscribe(sapi_ipc_pubsub_subscriber_t *subscriber_out,
                                         const sapi_ipc_pubsub_subscriber_config_t *config);

/**
 * @brief Receive a message from subscribed topic (blocking with timeout)
 *
 * @param subscriber     Subscriber handle (not NULL)
 * @param message_out    Destination buffer for message (not NULL)
 * @param message_size   Max size of message buffer
 * @param timeout_ms     Max wait time; 0 = poll, UINT32_MAX = infinite
 * @return SAPI_STATUS_OK on success (message received)
 *         SAPI_STATUS_TIMEOUT if no message within timeout
 *         SAPI_STATUS_INVALID_PARAM on invalid arguments
 *
 * Example:
 * @code
 * track_status_t status;
 * sapi_status_t result = sapi_ipc_pubsub_receive(
 *     subscriber,
 *     &status, sizeof(status),
 *     1000  // Poll every 1 second
 * );
 *
 * if (result == SAPI_STATUS_OK) {
 *     printf("Track %d status: %d\n", status.track_id, status.occupancy);
 *     // React to status change...
 * }
 * @endcode
 */
sapi_status_t sapi_ipc_pubsub_receive(sapi_ipc_pubsub_subscriber_t subscriber,
                                       void *message_out,
                                       size_t message_size,
                                       sapi_duration_ms_t timeout_ms);

/**
 * @brief Unsubscribe from a topic
 *
 * @param subscriber Subscriber handle (not NULL)
 * @return SAPI_STATUS_OK on success
 */
sapi_status_t sapi_ipc_pubsub_unsubscribe(sapi_ipc_pubsub_subscriber_t subscriber);

#ifdef __cplusplus
}
#endif

#endif /* SAFEAPI_IPC_PUBSUB_H */

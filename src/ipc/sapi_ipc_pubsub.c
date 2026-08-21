/**
 * @file sapi_ipc_pubsub.c
 * @brief IPC Publish-Subscribe implementation
 * @ingroup IPC
 */

#include "safeapi/ipc/sapi_ipc_pubsub.h"
#include "safeapi/lifecycle/sapi_lifecycle.h"
#include "safeapi/log.h"

/* Implementation stubs - actual implementation would use base IPC layer */

sapi_status_t sapi_ipc_pubsub_topic_create(sapi_ipc_pubsub_topic_t *topic_out,
                                            const sapi_ipc_pubsub_topic_config_t *config)
{
    sapi_status_t lifecycle_status;

    if (topic_out == NULL || config == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (config->message_size == 0 || config->max_subscribers == 0) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* REQ-LIFECYCLE-001 (ADR-026): a pub-sub topic is a setup-only resource -
     * refuse once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }

    SAPI_LOG_INFO("Creating pub-sub topic: %s (msg_sz=%zu, max_sub=%zu)",
                  config->topic_name, config->message_size, config->max_subscribers);

    /* TODO: Implement topic creation */
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_pubsub_publish(sapi_ipc_pubsub_topic_t topic,
                                       const void *message,
                                       size_t message_size)
{
    if (topic == NULL || message == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_DEBUG("Publishing message to topic (%zu bytes)", message_size);

    /* TODO: Deliver message to all subscribers */
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_pubsub_topic_destroy(sapi_ipc_pubsub_topic_t topic)
{
    if (topic == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_INFO("Destroying pub-sub topic");
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_pubsub_subscribe(sapi_ipc_pubsub_subscriber_t *subscriber_out,
                                         const sapi_ipc_pubsub_subscriber_config_t *config)
{
    if (subscriber_out == NULL || config == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    if (config->topic == NULL || config->queue_depth == 0) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_INFO("Subscribing: %s to topic (queue_depth=%zu)",
                  config->subscriber_name, config->queue_depth);

    /* TODO: Register subscriber and create queue */
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_pubsub_receive(sapi_ipc_pubsub_subscriber_t subscriber,
                                       void *message_out,
                                       size_t message_size,
                                       sapi_duration_ms_t timeout_ms)
{
    if (subscriber == NULL || message_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* TODO: Receive from subscriber queue with timeout */
    return SAPI_STATUS_TIMEOUT;
}

sapi_status_t sapi_ipc_pubsub_unsubscribe(sapi_ipc_pubsub_subscriber_t subscriber)
{
    if (subscriber == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_INFO("Unsubscribing from topic");
    return SAPI_STATUS_OK;
}

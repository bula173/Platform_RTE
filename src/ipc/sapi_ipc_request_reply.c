/**
 * @file sapi_ipc_request_reply.c
 * @brief IPC Request-Reply implementation
 * @ingroup IPC
 */

#include "safeapi/ipc/sapi_ipc_request_reply.h"
#include "safeapi/lifecycle/sapi_lifecycle.h"
#include "safeapi/log.h"

/* Implementation stubs - actual implementation would use base IPC layer */

sapi_status_t sapi_ipc_rr_server_create(sapi_ipc_rr_server_t *handle_out,
                                         const sapi_ipc_rr_server_config_t *config)
{
    sapi_status_t lifecycle_status;

    if (handle_out == NULL || config == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* REQ-LIFECYCLE-001 (ADR-026): an RR server is a setup-only resource -
     * refuse once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }

    SAPI_LOG_INFO("Creating RR server: %s (req_sz=%zu, reply_sz=%zu, depth=%zu)",
                  config->name, config->request_size, config->reply_size,
                  config->queue_depth);

    /* TODO: Implement using base IPC layer */
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_rr_receive_request(sapi_ipc_rr_server_t server,
                                           sapi_ipc_rr_request_t *request_out,
                                           sapi_duration_ms_t timeout_ms)
{
    if (server == NULL || request_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* TODO: Implement request receiving with timeout */
    return SAPI_STATUS_TIMEOUT;
}

sapi_status_t sapi_ipc_rr_send_reply(sapi_ipc_rr_server_t server,
                                      uint32_t request_id,
                                      const void *reply_data,
                                      size_t reply_size)
{
    if (server == NULL || reply_data == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_DEBUG("Sending reply to request %u (%zu bytes)", request_id, reply_size);

    /* TODO: Implement reply sending */
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_rr_server_destroy(sapi_ipc_rr_server_t server)
{
    if (server == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_INFO("Destroying RR server");
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_rr_client_create(sapi_ipc_rr_client_t *handle_out,
                                         const sapi_ipc_rr_client_config_t *config)
{
    sapi_status_t lifecycle_status;

    if (handle_out == NULL || config == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    /* REQ-LIFECYCLE-001 (ADR-026): an RR client is a setup-only resource -
     * refuse once the application's setup phase has been locked. */
    lifecycle_status = sapi_lifecycle_check_setup_allowed();
    if (lifecycle_status != SAPI_STATUS_OK)
    {
        return lifecycle_status;
    }

    SAPI_LOG_INFO("Creating RR client for server: %s", config->server_name);

    /* TODO: Implement client connection to server */
    return SAPI_STATUS_OK;
}

sapi_status_t sapi_ipc_rr_request(sapi_ipc_rr_client_t client,
                                   const void *request_data,
                                   size_t request_size,
                                   void *reply_out,
                                   size_t reply_size,
                                   sapi_duration_ms_t timeout_ms)
{
    if (client == NULL || request_data == NULL || reply_out == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_DEBUG("Sending RPC request (%zu bytes), waiting for reply (timeout=%u ms)",
                   request_size, timeout_ms);

    /* TODO: Implement request send + reply receive with timeout */
    return SAPI_STATUS_TIMEOUT;
}

sapi_status_t sapi_ipc_rr_client_destroy(sapi_ipc_rr_client_t client)
{
    if (client == NULL) {
        return SAPI_STATUS_INVALID_PARAM;
    }

    SAPI_LOG_INFO("Destroying RR client");
    return SAPI_STATUS_OK;
}

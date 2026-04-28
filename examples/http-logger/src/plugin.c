/**
 * @file plugin.c
 * @brief Example qcontrol plugin that logs HTTP exchange lifecycle events
 *
 * This plugin demonstrates the structured HTTP callback family through the C
 * SDK. It tracks one small exchange-local state object, logs request and
 * response metadata, and records body byte counts across the full exchange.
 *
 * The example is intended for HTTP-aware hosts that emit the HTTP ABI
 * directly. It is primarily a reference for callback shape and state flow.
 */

#include <stdlib.h>

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <qcontrol/log.h>

QCONTROL_LOGGER(logger);

/**
 * Tracks request and response byte counts for one HTTP exchange.
 */
typedef struct {
    uint64_t exchange_id;
    uint64_t request_body_bytes;
    uint64_t response_body_bytes;
    uint16_t status_code;
} exchange_state_t;

/**
 * Initialize logging when the plugin loads.
 */
static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[http_logger] initializing...\n");
    return 0;
}

/**
 * Flush and release logger resources when the plugin unloads.
 */
static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[http_logger] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

/**
 * Allocate per-exchange state and log the normalized request head.
 */
static qcontrol_http_action_t on_http_request(qcontrol_http_request_event_t* ev) {
    exchange_state_t* state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return QCONTROL_HTTP_BLOCK;
    }

    state->exchange_id = ev->ctx.exchange_id;

    qcontrol_log(&logger, "[http_logger] request exchange=%llu method=%.*s path=%.*s headers=%zu\n",
            (unsigned long long)ev->ctx.exchange_id,
            (int)ev->method_len, ev->method,
            (int)ev->path_len, ev->path,
            ev->header_count);

    return QCONTROL_HTTP_STATE(state);
}

/**
 * Accumulate request body bytes as body callbacks arrive.
 */
static qcontrol_http_action_t on_http_request_body(void* state_ptr, qcontrol_http_body_event_t* ev) {
    exchange_state_t* state = state_ptr;
    if (state != NULL) {
        state->request_body_bytes += ev->bytes_len;
        qcontrol_log(&logger, "[http_logger] request_body exchange=%llu bytes=%zu offset=%llu eos=%d\n",
                (unsigned long long)state->exchange_id,
                ev->bytes_len,
                (unsigned long long)ev->offset,
                ev->end_of_stream);
    }
    return QCONTROL_HTTP_PASS;
}

/**
 * Log the response head and remember the final status code.
 */
static qcontrol_http_action_t on_http_response(void* state_ptr, qcontrol_http_response_event_t* ev) {
    exchange_state_t* state = state_ptr;
    if (state != NULL) {
        state->status_code = ev->status_code;
        qcontrol_log(&logger, "[http_logger] response exchange=%llu status=%u headers=%zu\n",
                (unsigned long long)state->exchange_id,
                ev->status_code,
                ev->header_count);
    }
    return QCONTROL_HTTP_PASS;
}

/**
 * Accumulate response body bytes as body callbacks arrive.
 */
static qcontrol_http_action_t on_http_response_body(void* state_ptr, qcontrol_http_body_event_t* ev) {
    exchange_state_t* state = state_ptr;
    if (state != NULL) {
        state->response_body_bytes += ev->bytes_len;
        qcontrol_log(&logger, "[http_logger] response_body exchange=%llu bytes=%zu offset=%llu eos=%d\n",
                (unsigned long long)state->exchange_id,
                ev->bytes_len,
                (unsigned long long)ev->offset,
                ev->end_of_stream);
    }
    return QCONTROL_HTTP_PASS;
}

/**
 * Log the final exchange outcome and release the per-exchange state.
 */
static void on_http_exchange_close(void* state_ptr, qcontrol_http_exchange_close_event_t* ev) {
    exchange_state_t* state = state_ptr;
    if (state != NULL) {
        qcontrol_log(&logger,
                "[http_logger] close exchange=%llu status=%u request_bytes=%llu response_bytes=%llu reason=%u flags=%u\n",
                (unsigned long long)state->exchange_id,
                state->status_code,
                (unsigned long long)state->request_body_bytes,
                (unsigned long long)state->response_body_bytes,
                ev->reason,
                ev->flags);
        free(state);
    }
}

QCONTROL_PLUGIN("http-logger",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_http_request = on_http_request,
    .on_http_request_body = on_http_request_body,
    .on_http_response = on_http_response,
    .on_http_response_body = on_http_response_body,
    .on_http_exchange_close = on_http_exchange_close,
);

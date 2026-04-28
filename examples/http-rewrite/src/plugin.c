/**
 * @file plugin.c
 * @brief Example qcontrol plugin that rewrites one JSON response body
 *
 * This plugin demonstrates the mutable C HTTP ABI:
 * - request header normalization
 * - response header mutation
 * - explicit buffered-body scheduling from the response callback
 * - full-body replacement from the response body callback
 *
 * The example uses a fixed JSON replacement so it can compile without an
 * external JSON dependency while still showing the intended callback flow for
 * whole-body transforms.
 */

#include <stdlib.h>
#include <string.h>

#include <qcontrol.h>
#include <qcontrol/helpers.h>

/**
 * Tracks whether the current exchange should rewrite its response body.
 */
typedef struct {
    int rewrite_response;
} exchange_state_t;

/**
 * Normalize request headers and decide whether the response should be rewritten.
 */
static qcontrol_http_action_t on_http_request(qcontrol_http_request_event_t* ev) {
    exchange_state_t* state = calloc(1, sizeof(*state));
    qcontrol_http_headers_t* headers = NULL;

    if (state == NULL) {
        return QCONTROL_HTTP_BLOCK;
    }

    if (ev->head != NULL) {
        headers = qcontrol_http_request_headers(ev->head);
        if (headers != NULL) {
            QCONTROL_HTTP_HEADER_REMOVE(headers, "proxy-connection");
            QCONTROL_HTTP_HEADER_SET(headers, "x-qcontrol", "1");
        }
    }

    state->rewrite_response =
            ev->path != NULL &&
            ev->path_len == strlen("/api/profile") &&
            memcmp(ev->path, "/api/profile", ev->path_len) == 0;

    return QCONTROL_HTTP_STATE(state);
}

/**
 * Rewrite response headers and request buffered-body scheduling when needed.
 */
static qcontrol_http_action_t on_http_response(void* state_ptr, qcontrol_http_response_event_t* ev) {
    exchange_state_t* state = state_ptr;
    qcontrol_http_headers_t* headers = NULL;

    if (state == NULL || !state->rewrite_response) {
        return QCONTROL_HTTP_PASS;
    }

    if (ev->head != NULL) {
        headers = qcontrol_http_response_headers(ev->head);
        if (headers != NULL) {
            QCONTROL_HTTP_HEADER_SET(headers, "content-type", "application/json");
        }
    }

    return QCONTROL_HTTP_BUFFER(QCONTROL_HTTP_PASS);
}

/**
 * Replace the buffered JSON body on the terminal response body callback.
 */
static qcontrol_http_action_t on_http_response_body(void* state_ptr, qcontrol_http_body_event_t* ev) {
    static const char replacement[] = "{\"rewritten\":true}";
    exchange_state_t* state = state_ptr;

    if (state == NULL || !state->rewrite_response) {
        return QCONTROL_HTTP_PASS;
    }

    if (!ev->end_of_stream || ev->body == NULL) {
        return QCONTROL_HTTP_PASS;
    }

    qcontrol_buffer_set(ev->body, replacement, sizeof(replacement) - 1);
    return QCONTROL_HTTP_PASS;
}

/**
 * Release exchange-local state once the HTTP lifecycle is complete.
 */
static void on_http_exchange_close(void* state_ptr, qcontrol_http_exchange_close_event_t* ev) {
    (void)ev;
    free(state_ptr);
}

QCONTROL_PLUGIN("http-rewrite",
    .on_http_request = on_http_request,
    .on_http_response = on_http_response,
    .on_http_response_body = on_http_response_body,
    .on_http_exchange_close = on_http_exchange_close,
);

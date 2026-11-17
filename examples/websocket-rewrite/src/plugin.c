/**
 * @file plugin.c
 * @brief Example qcontrol plugin that rewrites logical WebSocket messages
 *
 * This plugin demonstrates the mutable C WebSocket ABI:
 * - stream-scoped state created from `on_websocket_open`
 * - correlation through `connection_id` and `exchange_id`
 * - logical text-message rewrites on send and receive callbacks
 */

#include <stdlib.h>
#include <string.h>

#include <qcontrol.h>
#include <qcontrol/helpers.h>

/**
 * Tracks one upgraded WebSocket stream and how many messages were rewritten.
 */
typedef struct {
    uint64_t connection_id;
    uint64_t exchange_id;
    uint64_t rewritten;
} websocket_state_t;

/**
 * Return whether the current message bytes exactly match one string literal.
 */
static int bytes_equal(const char* value, size_t value_len, const char* expected) {
    size_t expected_len = strlen(expected);
    return value != NULL && value_len == expected_len && memcmp(value, expected, expected_len) == 0;
}

/**
 * Create stream-scoped state after the upgrade handshake succeeds.
 */
static qcontrol_stream_action_t on_websocket_open(qcontrol_websocket_open_event_t* ev) {
    websocket_state_t* state = calloc(1, sizeof(*state));

    if (state == NULL) {
        return QCONTROL_STREAM_BLOCK;
    }

    state->connection_id = ev->ctx.connection_id;
    state->exchange_id = ev->ctx.exchange_id;
    return QCONTROL_STREAM_STATE(state);
}

/**
 * Rewrite one logical text message when it matches one expected payload.
 */
static qcontrol_stream_action_t rewrite_message(
    websocket_state_t* state,
    qcontrol_websocket_message_event_t* ev,
    const char* expected,
    const char* replacement
) {
    if (state == NULL || ev->body == NULL) {
        return QCONTROL_STREAM_PASS;
    }

    if (state->connection_id != ev->ctx.connection_id || state->exchange_id != ev->ctx.exchange_id) {
        return QCONTROL_STREAM_PASS;
    }

    if (ev->kind != QCONTROL_WEBSOCKET_MESSAGE_TEXT ||
        !bytes_equal(ev->bytes, ev->bytes_len, expected)) {
        return QCONTROL_STREAM_PASS;
    }

    state->rewritten += 1;
    qcontrol_buffer_set(ev->body, replacement, strlen(replacement));
    return QCONTROL_STREAM_PASS;
}

/**
 * Rewrite one outgoing logical message payload.
 */
static qcontrol_stream_action_t on_websocket_send(
    void* state_ptr,
    qcontrol_websocket_message_event_t* ev
) {
    return rewrite_message(state_ptr, ev, "ping", "ping from qcontrol");
}

/**
 * Rewrite one incoming logical message payload.
 */
static qcontrol_stream_action_t on_websocket_recv(
    void* state_ptr,
    qcontrol_websocket_message_event_t* ev
) {
    return rewrite_message(state_ptr, ev, "hello", "hello from qcontrol");
}

/**
 * Release stream-scoped state when the upgraded stream closes.
 */
static void on_websocket_close(void* state_ptr, qcontrol_websocket_close_event_t* ev) {
    websocket_state_t* state = state_ptr;
    (void)ev;
    free(state);
}

QCONTROL_PLUGIN("websocket-rewrite",
    .on_websocket_open = on_websocket_open,
    .on_websocket_send = on_websocket_send,
    .on_websocket_recv = on_websocket_recv,
    .on_websocket_close = on_websocket_close,
);

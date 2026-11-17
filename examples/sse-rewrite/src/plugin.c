/**
 * @file plugin.c
 * @brief Example qcontrol plugin that rewrites parsed SSE events
 *
 * This plugin demonstrates the mutable C SSE ABI:
 * - stream-scoped state created from `on_sse_open`
 * - correlation through `connection_id` and `exchange_id`
 * - parsed event mutation through `event_name`, `id`, `retry_ms`, and `data`
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qcontrol.h>
#include <qcontrol/helpers.h>

/**
 * Tracks one SSE stream and how many events were rewritten.
 */
typedef struct {
    uint64_t connection_id;
    uint64_t exchange_id;
    uint64_t rewritten;
} sse_state_t;

/**
 * Return whether the current field exactly matches one string literal.
 */
static int bytes_equal(const char* value, size_t value_len, const char* expected) {
    size_t expected_len = strlen(expected);
    return value != NULL && value_len == expected_len && memcmp(value, expected, expected_len) == 0;
}

/**
 * Create stream-scoped state when the HTTP response becomes SSE.
 */
static qcontrol_stream_action_t on_sse_open(qcontrol_sse_open_event_t* ev) {
    sse_state_t* state = calloc(1, sizeof(*state));

    if (state == NULL) {
        return QCONTROL_STREAM_BLOCK;
    }

    state->connection_id = ev->ctx.connection_id;
    state->exchange_id = ev->ctx.exchange_id;
    return QCONTROL_STREAM_STATE(state);
}

/**
 * Rewrite one parsed SSE event record in place.
 */
static qcontrol_stream_action_t on_sse_event(void* state_ptr, qcontrol_sse_event_record_t* ev) {
    char id_buf[64];
    int id_len = 0;
    qcontrol_sse_event_ops_t* ops = NULL;
    sse_state_t* state = state_ptr;

    if (state == NULL || ev->event_handle == NULL || ev->event_handle->ops == NULL) {
        return QCONTROL_STREAM_PASS;
    }

    if (state->connection_id != ev->ctx.connection_id || state->exchange_id != ev->ctx.exchange_id) {
        return QCONTROL_STREAM_PASS;
    }

    if (!bytes_equal(ev->event_name, ev->event_name_len, "message") ||
        !bytes_equal(ev->data, ev->data_len, "ping")) {
        return QCONTROL_STREAM_PASS;
    }

    ops = (qcontrol_sse_event_ops_t*)ev->event_handle->ops;
    state->rewritten += 1;
    id_len = snprintf(id_buf, sizeof(id_buf), "rewritten-%" PRIu64, state->rewritten);
    if (id_len < 0) {
        return QCONTROL_STREAM_PASS;
    }

    if (ops->set_event_name != NULL) {
        ops->set_event_name(ev->event_handle->context, "status", strlen("status"));
    }
    if (ops->set_id != NULL) {
        ops->set_id(ev->event_handle->context, id_buf, (size_t)id_len);
    }
    if (ops->set_retry_ms != NULL) {
        ops->set_retry_ms(ev->event_handle->context, 1500);
    }
    if (ops->set_data != NULL) {
        ops->set_data(
            ev->event_handle->context,
            "pong from qcontrol",
            strlen("pong from qcontrol")
        );
    }

    return QCONTROL_STREAM_PASS;
}

/**
 * Release stream-scoped state when the SSE response finishes.
 */
static void on_sse_close(void* state_ptr, qcontrol_sse_close_event_t* ev) {
    sse_state_t* state = state_ptr;
    (void)ev;
    free(state);
}

QCONTROL_PLUGIN("sse-rewrite",
    .on_sse_open = on_sse_open,
    .on_sse_event = on_sse_event,
    .on_sse_close = on_sse_close,
);

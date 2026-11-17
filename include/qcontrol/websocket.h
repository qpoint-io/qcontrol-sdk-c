/**
 * @file qcontrol/websocket.h
 * @brief WebSocket operation types for qcontrol SDK
 *
 * Defines the post-upgrade WebSocket plugin model where:
 * - HTTP callbacks still own the handshake request and `101` response
 * - WebSocket callbacks own the post-upgrade message stream
 * - Plugins observe logical reassembled messages rather than raw frames
 * - State can be attached once per upgraded stream and re-used by later
 *   callbacks on that stream
 */

#ifndef QCONTROL_WEBSOCKET_H
#define QCONTROL_WEBSOCKET_H

#include <stddef.h>
#include <stdint.h>

#include "buffer.h"
#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * WebSocket Enums
 * ============================================================================ */

/**
 * Logical WebSocket message kind.
 *
 * The host owns frame parsing and message reassembly. Plugins only observe the
 * resulting logical text or binary messages.
 */
typedef enum {
    QCONTROL_WEBSOCKET_MESSAGE_TEXT = 0,
    QCONTROL_WEBSOCKET_MESSAGE_BINARY = 1,
} qcontrol_websocket_message_type_t;

/* ============================================================================
 * WebSocket Event Structures
 * ============================================================================ */

/**
 * Event passed to on_websocket_open callback.
 */
typedef struct {
    qcontrol_stream_ctx_t ctx;

    /** Negotiated subprotocol, or NULL when none was selected. */
    const char* subprotocol;
    size_t subprotocol_len;

    /** Negotiated extensions header value, or NULL when none were negotiated. */
    const char* extensions;
    size_t extensions_len;
} qcontrol_websocket_open_event_t;

/**
 * Event passed to on_websocket_send and on_websocket_recv callbacks.
 *
 * `bytes` exposes the current logical message payload. Hosts that support
 * mutation provide a non-NULL `body` buffer for message replacement.
 */
typedef struct {
    qcontrol_stream_ctx_t ctx;

    /** Logical text or binary message kind. */
    qcontrol_websocket_message_type_t kind;

    /** Direct read-only message bytes for this callback. */
    const char* bytes;
    size_t bytes_len;

    /**
     * Mutable message buffer for host-backed edits, or NULL when this
     * host/path only supports read-only observation.
     */
    qcontrol_buffer_t* body;
} qcontrol_websocket_message_event_t;

/**
 * Event passed to on_websocket_close callback.
 */
typedef struct {
    qcontrol_stream_ctx_t ctx;

    /** Direction of the first observed close frame, or UNKNOWN when none was observed. */
    qcontrol_stream_direction_t direction;

    /** Close status code, or 0 when none was observed. */
    uint16_t code;

    /** UTF-8 close reason, or NULL when none was carried. */
    const char* reason;
    size_t reason_len;

    /** Non-zero when the close handshake completed cleanly. */
    int clean;
} qcontrol_websocket_close_event_t;

/* ============================================================================
 * WebSocket Callback Signatures
 * ============================================================================ */

/**
 * WebSocket open callback.
 *
 * Called once after the HTTP upgrade handshake succeeds and the host is ready
 * to deliver post-upgrade WebSocket messages.
 *
 * Return:
 * - PASS: continue with normal WebSocket handling
 * - BLOCK: terminate the upgraded stream
 * - STATE: track one stream-scoped state pointer
 */
typedef qcontrol_stream_action_t (*qcontrol_websocket_open_fn)(
    qcontrol_websocket_open_event_t* event
);

/**
 * WebSocket send callback.
 *
 * Called once per logical client-to-server WebSocket message.
 *
 * Return:
 * - PASS: forward the message normally
 * - BLOCK: terminate the upgraded stream
 * - STATE: replace the tracked stream-scoped state pointer
 */
typedef qcontrol_stream_action_t (*qcontrol_websocket_send_fn)(
    void* state,
    qcontrol_websocket_message_event_t* event
);

/**
 * WebSocket receive callback.
 *
 * Called once per logical server-to-client WebSocket message.
 *
 * Return:
 * - PASS: forward the message normally
 * - BLOCK: terminate the upgraded stream
 * - STATE: replace the tracked stream-scoped state pointer
 */
typedef qcontrol_stream_action_t (*qcontrol_websocket_recv_fn)(
    void* state,
    qcontrol_websocket_message_event_t* event
);

/**
 * WebSocket close callback.
 *
 * Called once when the upgraded stream ends.
 */
typedef void (*qcontrol_websocket_close_fn)(
    void* state,
    qcontrol_websocket_close_event_t* event
);

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_WEBSOCKET_H */

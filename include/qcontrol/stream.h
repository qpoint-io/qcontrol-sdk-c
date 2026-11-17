/**
 * @file qcontrol/stream.h
 * @brief Shared stream protocol types for qcontrol SDK
 *
 * Defines the common correlation and action types used by protocol surfaces
 * that are derived from one owning network connection and HTTP exchange.
 */

#ifndef QCONTROL_STREAM_H
#define QCONTROL_STREAM_H

#include <stdint.h>

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Shared Stream Enums
 * ============================================================================ */

/**
 * Normalized HTTP version.
 *
 * This type is shared by the HTTP ABI and by protocol surfaces derived from an
 * HTTP exchange such as SSE or WebSocket.
 */
typedef enum {
    QCONTROL_HTTP_VERSION_UNKNOWN = 0,
    QCONTROL_HTTP_VERSION_1_0 = 1,
    QCONTROL_HTTP_VERSION_1_1 = 2,
    QCONTROL_HTTP_VERSION_2 = 3,
} qcontrol_http_version_t;

/**
 * Derived stream protocol carried by one owning network connection and HTTP
 * exchange.
 */
typedef enum {
    QCONTROL_STREAM_PROTOCOL_WEBSOCKET = 0,
    QCONTROL_STREAM_PROTOCOL_SSE = 1,
} qcontrol_stream_protocol_t;

/**
 * Direction of one derived stream record relative to the client and server.
 */
typedef enum {
    QCONTROL_STREAM_CLIENT_TO_SERVER = 0,
    QCONTROL_STREAM_SERVER_TO_CLIENT = 1,
    /** No directional stream record was observed. */
    QCONTROL_STREAM_DIRECTION_UNKNOWN = 2,
} qcontrol_stream_direction_t;

/**
 * Action types returned by derived stream callbacks.
 */
typedef enum {
    /** Forward the current derived stream record normally. */
    QCONTROL_STREAM_ACTION_PASS = 0,
    /** Terminate the derived stream. */
    QCONTROL_STREAM_ACTION_BLOCK = 1,
    /** Attach plugin-defined state without changing forwarding. */
    QCONTROL_STREAM_ACTION_STATE = 2,
} qcontrol_stream_action_type_t;

/* ============================================================================
 * Shared Stream Structures
 * ============================================================================ */

/**
 * Context shared by callbacks on one derived stream surface.
 *
 * Correlation contract:
 * - `net` carries the normalized connection metadata already used by network
 *   and HTTP callbacks.
 * - `connection_id` identifies the owning network connection.
 * - `exchange_id` identifies the HTTP exchange that created the derived
 *   protocol surface.
 * - `http_version` records the HTTP version of that owning exchange.
 * - `protocol` identifies which derived surface is delivering this callback.
 */
typedef struct {
    qcontrol_net_ctx_t net;
    uint64_t connection_id;
    uint64_t exchange_id;
    qcontrol_http_version_t http_version;
    qcontrol_stream_protocol_t protocol;
} qcontrol_stream_ctx_t;

/**
 * Action returned from one derived stream callback.
 */
typedef struct {
    qcontrol_stream_action_type_t type;
    /**
     * Plugin-defined state carried only when `type` is
     * `QCONTROL_STREAM_ACTION_STATE`.
     */
    void* state;
} qcontrol_stream_action_t;

/* ============================================================================
 * Shared Stream Convenience Macros
 * ============================================================================ */

/** Return PASS action (continue normally). */
#define QCONTROL_STREAM_PASS \
    ((qcontrol_stream_action_t){ .type = QCONTROL_STREAM_ACTION_PASS, .state = NULL })

/** Return BLOCK action (terminate the derived stream). */
#define QCONTROL_STREAM_BLOCK \
    ((qcontrol_stream_action_t){ .type = QCONTROL_STREAM_ACTION_BLOCK, .state = NULL })

/** Return STATE action (track state, no forwarding change). */
#define QCONTROL_STREAM_STATE(s) \
    ((qcontrol_stream_action_t){ .type = QCONTROL_STREAM_ACTION_STATE, .state = (s) })

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_STREAM_H */

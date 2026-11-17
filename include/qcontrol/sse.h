/**
 * @file qcontrol/sse.h
 * @brief SSE operation types for qcontrol SDK
 *
 * Defines the Server-Sent Events plugin model where:
 * - HTTP response callbacks still own status and headers
 * - HTTP response body callbacks still receive decoded body chunks
 * - SSE callbacks own parsed event records inside one `text/event-stream`
 *   response body
 * - Plugins that implement both body and SSE callbacks should choose one
 *   semantic parser per exchange to avoid double-processing
 */

#ifndef QCONTROL_SSE_H
#define QCONTROL_SSE_H

#include <stddef.h>
#include <stdint.h>

#include "http.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct qcontrol_sse_event_ops qcontrol_sse_event_ops_t;
typedef struct qcontrol_sse_event qcontrol_sse_event_t;
typedef struct qcontrol_sse_event_record qcontrol_sse_event_record_t;

/* ============================================================================
 * SSE Handle Structures
 * ============================================================================ */

/**
 * Host callbacks that let plugins inspect and mutate one parsed SSE event.
 */
struct qcontrol_sse_event_ops {
    const char* (*event_name)(void* context);
    size_t (*event_name_len)(void* context);
    int (*set_event_name)(void* context, const char* value, size_t value_len);
    int (*clear_event_name)(void* context);

    const char* (*id)(void* context);
    size_t (*id_len)(void* context);
    int (*set_id)(void* context, const char* value, size_t value_len);
    int (*clear_id)(void* context);

    int (*retry_ms)(void* context, uint64_t* out_retry_ms);
    int (*set_retry_ms)(void* context, uint64_t retry_ms);
    int (*clear_retry_ms)(void* context);

    const char* (*data)(void* context);
    size_t (*data_len)(void* context);
    int (*set_data)(void* context, const char* value, size_t value_len);
    int (*clear_data)(void* context);
};

/**
 * Concrete mutable SSE event handle carried by SSE event callbacks when the
 * host supports SSE event editing.
 */
struct qcontrol_sse_event {
    void* context;
    const qcontrol_sse_event_ops_t* ops;
};

/* ============================================================================
 * SSE Event Structures
 * ============================================================================ */

/**
 * Event passed to on_sse_open callback.
 */
typedef struct {
    qcontrol_stream_ctx_t ctx;

    /** Response content type that identified this stream as SSE. */
    const char* content_type;
    size_t content_type_len;
} qcontrol_sse_open_event_t;

/**
 * Event passed to on_sse_event callback.
 *
 * Read-only snapshots expose the current parsed fields. Hosts that support
 * mutation provide a non-NULL `event_handle`.
 */
struct qcontrol_sse_event_record {
    qcontrol_stream_ctx_t ctx;

    /** Parsed `event:` field, or NULL when absent. */
    const char* event_name;
    size_t event_name_len;

    /** Parsed `id:` field, or NULL when absent. */
    const char* id;
    size_t id_len;

    /** Parsed `retry:` field, or 0 when absent. */
    uint64_t retry_ms;
    int has_retry_ms;

    /** Parsed logical event data with embedded newlines preserved. */
    const char* data;
    size_t data_len;

    /** Mutable host-backed SSE event handle, or NULL. */
    qcontrol_sse_event_t* event_handle;
};

/**
 * Event passed to on_sse_close callback.
 */
typedef struct {
    qcontrol_stream_ctx_t ctx;

    /** Terminal reason for the owning HTTP response stream closing. */
    qcontrol_http_close_reason_t reason;
} qcontrol_sse_close_event_t;

/* ============================================================================
 * SSE Callback Signatures
 * ============================================================================ */

/**
 * SSE open callback.
 *
 * Called once after the host identifies one response body as
 * `text/event-stream`.
 *
 * Return:
 * - PASS: continue with normal SSE handling
 * - BLOCK: terminate the response stream
 * - STATE: track one SSE-stream-scoped state pointer
 */
typedef qcontrol_stream_action_t (*qcontrol_sse_open_fn)(
    qcontrol_sse_open_event_t* event
);

/**
 * SSE event callback.
 *
 * Called once per complete parsed SSE event record.
 *
 * Return:
 * - PASS: forward the event normally
 * - BLOCK: terminate the response stream
 * - STATE: replace the tracked SSE-stream-scoped state pointer
 */
typedef qcontrol_stream_action_t (*qcontrol_sse_event_fn)(
    void* state,
    qcontrol_sse_event_record_t* event
);

/**
 * SSE close callback.
 *
 * Called once when the owning SSE response stream ends.
 */
typedef void (*qcontrol_sse_close_fn)(
    void* state,
    qcontrol_sse_close_event_t* event
);

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_SSE_H */

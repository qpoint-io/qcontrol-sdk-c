/**
 * @file qcontrol/http.h
 * @brief HTTP operation types for qcontrol SDK
 *
 * Defines the exchange-based HTTP plugin model where:
 * - HTTP/1.x and HTTP/2 share one structured callback surface
 * - Request and response starts expose full normalized header blocks
 * - Bodies are streamed incrementally as decoded chunks
 * - Plugin state can be attached once per HTTP exchange
 * - The runtime handles protocol parsing and framing
 * - Independent exchanges may be dispatched concurrently
 *
 * This header exposes one structured HTTP callback surface that supports both
 * read-only inspection and host-backed mutation for request and response data.
 */

#ifndef QCONTROL_HTTP_H
#define QCONTROL_HTTP_H

#include <stddef.h>
#include <stdint.h>

#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * HTTP Action Types
 * ============================================================================ */

/**
 * Action types returned by HTTP callbacks.
 */
typedef enum {
    /** No interception, continue normally */
    QCONTROL_HTTP_ACTION_PASS = 0,
    /** Block the exchange */
    QCONTROL_HTTP_ACTION_BLOCK = 1,
    /** Track per-exchange state only */
    QCONTROL_HTTP_ACTION_STATE = 2,
} qcontrol_http_action_type_t;

/**
 * Message kind within an HTTP exchange.
 */
typedef enum {
    QCONTROL_HTTP_MESSAGE_REQUEST = 0,
    QCONTROL_HTTP_MESSAGE_RESPONSE = 1,
} qcontrol_http_message_kind_t;

/**
 * Body event flags.
 */
typedef enum {
    /** No extra decoding flags */
    QCONTROL_HTTP_BODY_FLAG_NONE = 0,
    /** Transfer framing has been decoded (for example, chunked encoding) */
    QCONTROL_HTTP_BODY_FLAG_TRANSFER_DECODED = 1 << 0,
    /** Content encoding has been decoded (for example, gzip) */
    QCONTROL_HTTP_BODY_FLAG_CONTENT_DECODED = 1 << 1,
} qcontrol_http_body_flag_t;

/**
 * Body scheduling mode requested by one request or response head callback.
 */
typedef enum {
    /**
     * Preserve the host's default body scheduling and framing behavior for
     * this message.
     *
     * This mode is intended for observation-only callbacks that do not expect
     * body mutation to change downstream or upstream framing decisions.
     */
    QCONTROL_HTTP_BODY_MODE_DEFAULT = 0,
    /**
     * Deliver decoded body callbacks incrementally as chunks arrive.
     *
     * Hosts may forward headers before the final logical body is known. On
     * HTTP/1.1 paths, mutable streaming bodies may therefore require
     * chunked-style framing whenever body size can change.
     */
    QCONTROL_HTTP_BODY_MODE_STREAM = 1,
    /**
     * Buffer the decoded logical body before running body callbacks.
     *
     * Hosts should use this mode when plugins need full-body transforms such
     * as JSON parse -> mutate -> encode -> replace.
     *
     * On HTTP/1.1 paths, the intended contract is that the host delays header
     * emission until the final body is known so it can emit correct framing
     * metadata such as Content-Length instead of falling back to chunked
     * transfer coding.
     *
     * HTTP/2 does not use chunked transfer coding, so this mode remains useful
     * there as a full-body scheduling hint even when transport framing does not
     * require delayed header emission for correctness.
     */
    QCONTROL_HTTP_BODY_MODE_BUFFER = 2,
} qcontrol_http_body_mode_t;

/**
 * Exchange-close reason.
 */
typedef enum {
    /** Exchange completed normally */
    QCONTROL_HTTP_CLOSE_COMPLETE = 0,
    /** Exchange ended before protocol completion */
    QCONTROL_HTTP_CLOSE_ABORTED = 1,
    /** Streamer disabled after parse or desync failure */
    QCONTROL_HTTP_CLOSE_PARSE_ERROR = 2,
    /** Underlying connection closed before exchange completion */
    QCONTROL_HTTP_CLOSE_CONNECTION_CLOSED = 3,
    /** Exchange completed by switching protocols into a derived stream */
    QCONTROL_HTTP_CLOSE_UPGRADED = 4,
} qcontrol_http_close_reason_t;

/**
 * Exchange-close flags.
 */
typedef enum {
    /** No completion flags were recorded */
    QCONTROL_HTTP_EXCHANGE_FLAG_NONE = 0,
    /** Request message reached its done callback */
    QCONTROL_HTTP_EXCHANGE_FLAG_REQUEST_DONE = 1 << 0,
    /** Response message reached its done callback */
    QCONTROL_HTTP_EXCHANGE_FLAG_RESPONSE_DONE = 1 << 1,
} qcontrol_http_exchange_flag_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct qcontrol_http_ctx qcontrol_http_ctx_t;
typedef struct qcontrol_http_action qcontrol_http_action_t;
typedef struct qcontrol_http_header qcontrol_http_header_t;
typedef struct qcontrol_http_headers_ops qcontrol_http_headers_ops_t;
typedef struct qcontrol_http_request_head_ops qcontrol_http_request_head_ops_t;
typedef struct qcontrol_http_response_head_ops qcontrol_http_response_head_ops_t;
typedef struct qcontrol_http_headers qcontrol_http_headers_t;
typedef struct qcontrol_http_request_head qcontrol_http_request_head_t;
typedef struct qcontrol_http_response_head qcontrol_http_response_head_t;

/* ============================================================================
 * HTTP Data Structures
 * ============================================================================ */

/**
 * Host callbacks that let plugins read and mutate one structured header block
 * through the handle already carried by HTTP events.
 */
struct qcontrol_http_headers_ops {
    const qcontrol_http_header_t* (*data)(void* context);
    size_t (*count)(void* context);
    int (*add)(
        void* context,
        const char* name,
        size_t name_len,
        const char* value,
        size_t value_len
    );
    int (*set)(
        void* context,
        const char* name,
        size_t name_len,
        const char* value,
        size_t value_len
    );
    size_t (*remove)(
        void* context,
        const char* name,
        size_t name_len
    );
};

/**
 * Concrete mutable header-block handle carried by request, response, and
 * trailers events when the host supports structured header edits.
 */
struct qcontrol_http_headers {
    void* context;
    const qcontrol_http_headers_ops_t* ops;
};

/**
 * Host callbacks that let plugins read and mutate one request head.
 * Rebuild hosts and plugins for original_headers: older ops tables are too short.
 */
struct qcontrol_http_request_head_ops {
    const char* (*raw_target)(void* context);
    size_t (*raw_target_len)(void* context);
    const char* (*method)(void* context);
    size_t (*method_len)(void* context);
    int (*set_method)(void* context, const char* value, size_t value_len);
    const char* (*scheme)(void* context);
    size_t (*scheme_len)(void* context);
    int (*set_scheme)(void* context, const char* value, size_t value_len);
    const char* (*authority)(void* context);
    size_t (*authority_len)(void* context);
    int (*set_authority)(void* context, const char* value, size_t value_len);
    const char* (*path)(void* context);
    size_t (*path_len)(void* context);
    int (*set_path)(void* context, const char* value, size_t value_len);
    /**
     * Headers received before any request plugin edits. Read-only arrays and
     * bytes remain valid throughout request-head dispatch; NULL is unsupported.
     * This does not change the existing callback or mutable header views.
     */
    const qcontrol_http_header_t* (*original_headers)(void* context, size_t* count);
};

/**
 * Concrete mutable request-head handle carried by request events when the host
 * supports structured request editing.
 */
struct qcontrol_http_request_head {
    void* context;
    const qcontrol_http_request_head_ops_t* ops;
    qcontrol_http_headers_t headers;
};

/**
 * Host callbacks that let plugins read and mutate one response head.
 */
struct qcontrol_http_response_head_ops {
    uint16_t (*status_code)(void* context);
    void (*set_status_code)(void* context, uint16_t status_code);
    const char* (*reason)(void* context);
    size_t (*reason_len)(void* context);
    int (*set_reason)(void* context, const char* value, size_t value_len);
};

/**
 * Concrete mutable response-head handle carried by response events when the
 * host supports structured response editing.
 */
struct qcontrol_http_response_head {
    void* context;
    const qcontrol_http_response_head_ops_t* ops;
    qcontrol_http_headers_t headers;
};

/**
 * Single normalized HTTP header.
 */
struct qcontrol_http_header {
    const char* name;
    size_t name_len;
    const char* value;
    size_t value_len;
};

/**
 * Context shared by all callbacks for one HTTP exchange.
 *
 * Ordering contract:
 * - Callbacks for one exchange follow the normal request/response lifecycle.
 * - Hosts may dispatch other exchanges concurrently, including other streams
 *   on the same HTTP/2 connection.
 */
struct qcontrol_http_ctx {
    /** Underlying network metadata snapshot */
    qcontrol_net_ctx_t net;

    /** Runtime-assigned exchange identifier */
    uint64_t exchange_id;

    /** Native HTTP/2 stream identifier, or 0 for HTTP/1.x */
    uint32_t stream_id;

    /** Normalized HTTP version */
    qcontrol_http_version_t version;
};

/**
 * Action result returned from HTTP callbacks.
 */
struct qcontrol_http_action {
    qcontrol_http_action_type_t type;
    qcontrol_http_body_mode_t body_mode;
    union {
        /**
         * State pointer for STATE.
         *
         * The pointed-to state is scoped to one logical HTTP exchange unless
         * the plugin chooses to share it. Plugins remain responsible for
         * synchronizing any shared mutable state they store or reference here.
         */
        void* state;
    };
};

/* ============================================================================
 * HTTP Action Convenience Macros
 * ============================================================================ */

/** Return PASS action (continue normally) */
#define QCONTROL_HTTP_PASS \
    ((qcontrol_http_action_t){ .type = QCONTROL_HTTP_ACTION_PASS, .body_mode = QCONTROL_HTTP_BODY_MODE_DEFAULT })

/** Return BLOCK action (reject the exchange) */
#define QCONTROL_HTTP_BLOCK \
    ((qcontrol_http_action_t){ .type = QCONTROL_HTTP_ACTION_BLOCK, .body_mode = QCONTROL_HTTP_BODY_MODE_DEFAULT })

/** Return STATE action (track per-exchange state) */
#define QCONTROL_HTTP_STATE(s) \
    ((qcontrol_http_action_t){ .type = QCONTROL_HTTP_ACTION_STATE, .body_mode = QCONTROL_HTTP_BODY_MODE_DEFAULT, .state = (s) })

/**
 * Apply one body scheduling request to an existing HTTP action.
 *
 * Request callbacks apply the selected mode to the request body for that
 * exchange. Response callbacks apply the selected mode to the response body
 * for that exchange. Other callbacks should leave the mode at DEFAULT.
 *
 * Mode guidance:
 *
 * - DEFAULT: observation-only, preserve normal host framing behavior
 * - STREAM: body may change while the message is streaming
 * - BUFFER: wait for the full logical body before forwarding the message
 */
static inline qcontrol_http_action_t qcontrol_http_action_with_body_mode(
    qcontrol_http_action_t action,
    qcontrol_http_body_mode_t body_mode
) {
    action.body_mode = body_mode;
    return action;
}

/* ============================================================================
 * HTTP Event Structures
 * ============================================================================ */

/**
 * Event passed to on_http_request callback.
 */
typedef struct {
    qcontrol_http_ctx_t ctx;

    /** Raw request-target as seen on the wire */
    const char* raw_target;
    size_t raw_target_len;

    /** Normalized request method */
    const char* method;
    size_t method_len;

    /** Normalized request scheme, or NULL if unavailable */
    const char* scheme;
    size_t scheme_len;

    /** Normalized request authority, or NULL if unavailable */
    const char* authority;
    size_t authority_len;

    /** Normalized request path */
    const char* path;
    size_t path_len;

    /** Runtime-owned header array */
    const qcontrol_http_header_t* headers;
    size_t header_count;

    /** Mutable request head handle for host-backed edits, or NULL. */
    qcontrol_http_request_head_t* head;
} qcontrol_http_request_event_t;

/**
 * Event passed to on_http_response callback.
 */
typedef struct {
    qcontrol_http_ctx_t ctx;

    /** Response status code */
    uint16_t status_code;

    /** Response reason phrase, or NULL if unavailable */
    const char* reason;
    size_t reason_len;

    /** Runtime-owned header array */
    const qcontrol_http_header_t* headers;
    size_t header_count;

    /** Mutable response head handle for host-backed edits, or NULL. */
    qcontrol_http_response_head_t* head;
} qcontrol_http_response_event_t;

/**
 * Event passed to request/response body callbacks.
 *
 * The host owns body scheduling and forwarding semantics. Plugins should treat
 * `bytes` as the decoded input view for this callback and `body` as an
 * optional mutable output buffer supplied by hosts that support body editing.
 */
typedef struct {
    qcontrol_http_ctx_t ctx;

    /** Request or response body */
    qcontrol_http_message_kind_t kind;

    /** Direct read-only body bytes for this callback */
    const char* bytes;
    size_t bytes_len;

    /**
     * Mutable body buffer for host-backed edits, or NULL when this host/path
     * only supports read-only observation.
     */
    qcontrol_buffer_t* body;

    /** Decoded body offset within this message */
    uint64_t offset;

    /** qcontrol_http_body_flag_t bitset */
    uint32_t flags;

    /**
     * Non-zero when this callback carries the terminal body chunk for the
     * current message body.
     */
    int end_of_stream;
} qcontrol_http_body_event_t;

/**
 * Event passed to request/response trailers callbacks.
 */
typedef struct {
    qcontrol_http_ctx_t ctx;

    /** Request or response trailers */
    qcontrol_http_message_kind_t kind;

    /** Runtime-owned trailer array */
    const qcontrol_http_header_t* headers;
    size_t header_count;

    /** Mutable trailer block for host-backed edits, or NULL. */
    qcontrol_http_headers_t* header_block;
} qcontrol_http_trailers_event_t;

/**
 * Event passed to request/response done callbacks.
 */
typedef struct {
    qcontrol_http_ctx_t ctx;

    /** Request or response completion */
    qcontrol_http_message_kind_t kind;

    /** Total decoded body bytes observed for this message */
    uint64_t body_bytes;
} qcontrol_http_message_done_event_t;

/**
 * Event passed to on_http_exchange_close callback.
 */
typedef struct {
    qcontrol_http_ctx_t ctx;

    /** Terminal reason for exchange close */
    qcontrol_http_close_reason_t reason;

    /** qcontrol_http_exchange_flag_t bitset */
    uint32_t flags;
} qcontrol_http_exchange_close_event_t;

/* ============================================================================
 * HTTP Callback Signatures
 * ============================================================================ */

/**
 * HTTP request callback - request start and normalized request headers.
 *
 * Called once per HTTP exchange before any request body or trailers.
 * Return:
 * - PASS: observe only
 * - BLOCK: reject the exchange
 * - STATE: track per-exchange state
 *
 * When the returned action sets `body_mode`, that mode applies to the request
 * body for this exchange:
 *
 * - DEFAULT keeps the host's normal framing behavior
 * - STREAM allows body mutation while headers may already be in flight
 * - BUFFER requests host-managed full-body buffering before forwarding
 *
 * Hosts may invoke this callback concurrently for independent exchanges.
 */
typedef qcontrol_http_action_t (*qcontrol_http_request_fn)(
    qcontrol_http_request_event_t* event
);

/**
 * HTTP request body callback - decoded request body chunk.
 *
 * Hosts that support body mutation provide a non-NULL `event->body` buffer.
 * Full-body rewrite requires host-managed body scheduling so every plugin can
 * observe the intended input without being affected by earlier mutations.
 *
 * For one request body, callbacks are delivered in decoded offset order. Hosts
 * may still invoke other exchanges concurrently.
 */
typedef qcontrol_http_action_t (*qcontrol_http_request_body_fn)(
    void* state,
    qcontrol_http_body_event_t* event
);

/**
 * HTTP request trailers callback - complete request trailers block.
 *
 * Hosts may invoke this callback concurrently for independent exchanges.
 */
typedef qcontrol_http_action_t (*qcontrol_http_request_trailers_fn)(
    void* state,
    qcontrol_http_trailers_event_t* event
);

/**
 * HTTP request done callback - final request message completion.
 *
 * This callback runs after the final body or trailers callback for the request
 * and is intended for bookkeeping and cleanup rather than body replacement.
 * Hosts may invoke this callback concurrently for independent exchanges.
 */
typedef void (*qcontrol_http_request_done_fn)(
    void* state,
    qcontrol_http_message_done_event_t* event
);

/**
 * HTTP response callback - response start and normalized response headers.
 *
 * When the returned action sets `body_mode`, that mode applies to the
 * response body for this exchange:
 *
 * - DEFAULT keeps the host's normal framing behavior
 * - STREAM allows body mutation while headers may already be in flight
 * - BUFFER requests host-managed full-body buffering before forwarding
 *
 * Hosts may invoke this callback concurrently for independent exchanges.
 */
typedef qcontrol_http_action_t (*qcontrol_http_response_fn)(
    void* state,
    qcontrol_http_response_event_t* event
);

/**
 * HTTP response body callback - decoded response body chunk.
 *
 * Hosts that support body mutation provide a non-NULL `event->body` buffer.
 * Full-body rewrite requires host-managed body scheduling so every plugin can
 * observe the intended input without being affected by earlier mutations.
 *
 * For one response body, callbacks are delivered in decoded offset order.
 * Hosts may still invoke other exchanges concurrently.
 */
typedef qcontrol_http_action_t (*qcontrol_http_response_body_fn)(
    void* state,
    qcontrol_http_body_event_t* event
);

/**
 * HTTP response trailers callback - complete response trailers block.
 *
 * Hosts may invoke this callback concurrently for independent exchanges.
 */
typedef qcontrol_http_action_t (*qcontrol_http_response_trailers_fn)(
    void* state,
    qcontrol_http_trailers_event_t* event
);

/**
 * HTTP response done callback - final response message completion.
 *
 * This callback runs after the final body or trailers callback for the
 * response and is intended for bookkeeping and cleanup rather than body
 * replacement.
 * Hosts may invoke this callback concurrently for independent exchanges.
 */
typedef void (*qcontrol_http_response_done_fn)(
    void* state,
    qcontrol_http_message_done_event_t* event
);

/**
 * HTTP exchange close callback - terminal cleanup hook for exchange state.
 *
 * Called exactly once for tracked exchanges, including abnormal termination.
 * Plugin is responsible for freeing exchange state here if it did not already
 * do so in an earlier done callback.
 * Hosts may invoke this callback concurrently for independent exchanges.
 */
typedef void (*qcontrol_http_exchange_close_fn)(
    void* state,
    qcontrol_http_exchange_close_event_t* event
);

/* ============================================================================
 * HTTP Head and Header Accessors
 * ============================================================================ */

/**
 * Return the mutable header collection for one request head.
 */
static inline qcontrol_http_headers_t* qcontrol_http_request_headers(
    qcontrol_http_request_head_t* head
) {
    if (!head) {
        return NULL;
    }
    return &head->headers;
}

/**
 * Return the mutable header collection for one response head.
 */
static inline qcontrol_http_headers_t* qcontrol_http_response_headers(
    qcontrol_http_response_head_t* head
) {
    if (!head) {
        return NULL;
    }
    return &head->headers;
}

/**
 * Return the current request raw-target pointer.
 */
static inline const char* qcontrol_http_request_raw_target(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->raw_target) {
        return NULL;
    }
    return head->ops->raw_target(head->context);
}

/**
 * Return the current request raw-target length.
 */
static inline size_t qcontrol_http_request_raw_target_len(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->raw_target_len) {
        return 0;
    }
    return head->ops->raw_target_len(head->context);
}

/**
 * Return the current request method pointer.
 */
static inline const char* qcontrol_http_request_method(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->method) {
        return NULL;
    }
    return head->ops->method(head->context);
}

/**
 * Return the current request method length.
 */
static inline size_t qcontrol_http_request_method_len(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->method_len) {
        return 0;
    }
    return head->ops->method_len(head->context);
}

/**
 * Replace the request method.
 *
 * Returns 0 on success and non-zero when the host rejects the replacement.
 */
static inline int qcontrol_http_request_set_method(
    qcontrol_http_request_head_t* head,
    const char* value,
    size_t value_len
) {
    if (!head || !head->ops || !head->ops->set_method) {
        return 1;
    }
    return head->ops->set_method(head->context, value, value_len);
}

/**
 * Return the current request scheme pointer.
 */
static inline const char* qcontrol_http_request_scheme(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->scheme) {
        return NULL;
    }
    return head->ops->scheme(head->context);
}

/**
 * Return the current request scheme length.
 */
static inline size_t qcontrol_http_request_scheme_len(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->scheme_len) {
        return 0;
    }
    return head->ops->scheme_len(head->context);
}

/**
 * Replace the request scheme.
 *
 * Returns 0 on success and non-zero when the host rejects the replacement.
 */
static inline int qcontrol_http_request_set_scheme(
    qcontrol_http_request_head_t* head,
    const char* value,
    size_t value_len
) {
    if (!head || !head->ops || !head->ops->set_scheme) {
        return 1;
    }
    return head->ops->set_scheme(head->context, value, value_len);
}

/**
 * Return the current request authority pointer.
 */
static inline const char* qcontrol_http_request_authority(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->authority) {
        return NULL;
    }
    return head->ops->authority(head->context);
}

/**
 * Return the current request authority length.
 */
static inline size_t qcontrol_http_request_authority_len(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->authority_len) {
        return 0;
    }
    return head->ops->authority_len(head->context);
}

/**
 * Replace the request authority.
 *
 * Returns 0 on success and non-zero when the host rejects the replacement.
 */
static inline int qcontrol_http_request_set_authority(
    qcontrol_http_request_head_t* head,
    const char* value,
    size_t value_len
) {
    if (!head || !head->ops || !head->ops->set_authority) {
        return 1;
    }
    return head->ops->set_authority(head->context, value, value_len);
}

/**
 * Return the current request path pointer.
 */
static inline const char* qcontrol_http_request_path(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->path) {
        return NULL;
    }
    return head->ops->path(head->context);
}

/**
 * Return the current request path length.
 */
static inline size_t qcontrol_http_request_path_len(
    const qcontrol_http_request_head_t* head
) {
    if (!head || !head->ops || !head->ops->path_len) {
        return 0;
    }
    return head->ops->path_len(head->context);
}

/**
 * Replace the request path.
 *
 * Returns 0 on success and non-zero when the host rejects the replacement.
 */
static inline int qcontrol_http_request_set_path(
    qcontrol_http_request_head_t* head,
    const char* value,
    size_t value_len
) {
    if (!head || !head->ops || !head->ops->set_path) {
        return 1;
    }
    return head->ops->set_path(head->context, value, value_len);
}

/**
 * Return the current response status code.
 */
static inline uint16_t qcontrol_http_response_status_code(
    const qcontrol_http_response_head_t* head
) {
    if (!head || !head->ops || !head->ops->status_code) {
        return 0;
    }
    return head->ops->status_code(head->context);
}

/**
 * Replace the response status code.
 */
static inline void qcontrol_http_response_set_status_code(
    qcontrol_http_response_head_t* head,
    uint16_t status_code
) {
    if (!head || !head->ops || !head->ops->set_status_code) {
        return;
    }
    head->ops->set_status_code(head->context, status_code);
}

/**
 * Return the current response reason pointer.
 */
static inline const char* qcontrol_http_response_reason(
    const qcontrol_http_response_head_t* head
) {
    if (!head || !head->ops || !head->ops->reason) {
        return NULL;
    }
    return head->ops->reason(head->context);
}

/**
 * Return the current response reason length.
 */
static inline size_t qcontrol_http_response_reason_len(
    const qcontrol_http_response_head_t* head
) {
    if (!head || !head->ops || !head->ops->reason_len) {
        return 0;
    }
    return head->ops->reason_len(head->context);
}

/**
 * Replace the response reason phrase.
 *
 * Returns 0 on success and non-zero when the host rejects the replacement.
 */
static inline int qcontrol_http_response_set_reason(
    qcontrol_http_response_head_t* head,
    const char* value,
    size_t value_len
) {
    if (!head || !head->ops || !head->ops->set_reason) {
        return 1;
    }
    return head->ops->set_reason(head->context, value, value_len);
}

/**
 * Return the runtime-owned contiguous header view for the current callback.
 */
static inline const qcontrol_http_header_t* qcontrol_http_headers_data(
    const qcontrol_http_headers_t* headers
) {
    if (!headers || !headers->ops || !headers->ops->data) {
        return NULL;
    }
    return headers->ops->data(headers->context);
}

/**
 * Return the number of headers in the current callback view.
 */
static inline size_t qcontrol_http_headers_count(
    const qcontrol_http_headers_t* headers
) {
    if (!headers || !headers->ops || !headers->ops->count) {
        return 0;
    }
    return headers->ops->count(headers->context);
}

/**
 * Append one header without removing any existing headers of the same name.
 *
 * Returns 0 on success and non-zero when the host rejects the mutation.
 */
static inline int qcontrol_http_headers_add(
    qcontrol_http_headers_t* headers,
    const char* name,
    size_t name_len,
    const char* value,
    size_t value_len
) {
    if (!headers || !headers->ops || !headers->ops->add) {
        return 1;
    }
    return headers->ops->add(headers->context, name, name_len, value, value_len);
}

/**
 * Replace all existing headers with the same name with one new header value.
 *
 * Returns 0 on success and non-zero when the host rejects the mutation.
 */
static inline int qcontrol_http_headers_set(
    qcontrol_http_headers_t* headers,
    const char* name,
    size_t name_len,
    const char* value,
    size_t value_len
) {
    if (!headers || !headers->ops || !headers->ops->set) {
        return 1;
    }
    return headers->ops->set(headers->context, name, name_len, value, value_len);
}

/**
 * Remove every header whose name matches the supplied byte sequence.
 *
 * Returns the number of removed headers.
 */
static inline size_t qcontrol_http_headers_remove(
    qcontrol_http_headers_t* headers,
    const char* name,
    size_t name_len
) {
    if (!headers || !headers->ops || !headers->ops->remove) {
        return 0;
    }
    return headers->ops->remove(headers->context, name, name_len);
}

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_HTTP_H */

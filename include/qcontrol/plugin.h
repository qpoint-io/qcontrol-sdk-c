/**
 * @file qcontrol/plugin.h
 * @brief Plugin descriptor for qcontrol SDK
 *
 * Defines the plugin descriptor structure that plugins export.
 * Includes operation-specific headers for the callback types.
 */

#ifndef QCONTROL_PLUGIN_H
#define QCONTROL_PLUGIN_H

#include "common.h"
#include "file.h"
#include "exec.h"
#include "net.h"
#include "stream.h"
#include "http.h"
#include "websocket.h"
#include "sse.h"
#include "netstat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Plugin Descriptor
 * ============================================================================ */

/**
 * Plugin descriptor.
 *
 * Single plugin, multiple operation types. Export as const qcontrol_plugin.
 *
 * Concurrency contract:
 * - Plugins must be thread-safe.
 * - Hosts may invoke callbacks concurrently across independent sessions such
 *   as connections, file sessions, exec sessions, and HTTP exchanges.
 * - Plugins are responsible for synchronizing any shared mutable state they
 *   keep outside the session- or exchange-scoped state pointers carried by
 *   callback events and action results.
 *
 * Example:
 * @code
 * const qcontrol_plugin_t qcontrol_plugin = {
 *     .version = QCONTROL_PLUGIN_VERSION,
 *     .name = "my-plugin",
 *     .on_file_open = my_file_open,
 *     .on_file_read = my_file_read,
 *     .on_file_close = my_file_close,
 * };
 * @endcode
 */
/**
 * Plugin cleanup callback.
 * Called after callback delivery has quiesced and before plugin unload.
 */
typedef void (*qcontrol_plugin_cleanup_fn)(void);

/**
 * Config was accepted.
 */
#define QCONTROL_CONFIG_OK 0

/**
 * The plugin rejected this config.
 * Any other non-zero value means the callback failed for another reason.
 */
#define QCONTROL_CONFIG_INVALID 1

/*
 * Error reporting shared by on_init and on_config.
 *
 * `error` is a host buffer of `error_cap` bytes, separate from the config.
 * The host zero-initializes it before the call and never writes a terminator
 * after the callback. On failure the plugin may write a NUL-terminated reason
 * using at most `error_cap` bytes, including the NUL. If `error` is NULL or
 * `error_cap` is 0, the plugin writes nothing. The host reads at most
 * `error_cap` bytes, stopping at the first NUL. Messages name a field and
 * reason, never config values.
 *
 * The production loader always passes 1024 bytes. That size is a host
 * choice, not an ABI promise. Plugins honor the `error_cap` of the call.
 */

/**
 * Initialize a plugin once with its effective config, before operational callbacks.
 *
 * `config` points to a borrowed view of UTF-8 JSON. Read exactly `config->len`
 * bytes from `config->data`; NUL termination is not guaranteed. The view and its
 * bytes are valid only during the callback; copy anything retained. Absent
 * configuration is the four bytes "null", not a NULL config pointer.
 *
 * Return QCONTROL_CONFIG_OK, QCONTROL_CONFIG_INVALID for rejected settings,
 * or another nonzero value for initialization failure. On failure the plugin
 * unwinds its own partial resources; the host does not call on_cleanup.
 */
typedef int (*qcontrol_plugin_init_fn)(const qcontrol_string_view_t* config, char* error,
                                       size_t error_cap);

/**
 * Update configuration of an already initialized plugin.
 *
 * The borrowed UTF-8 JSON view has the same lifetime and encoding as on_init.
 * This callback is not called during initial load. The host calls it when the
 * session's configuration is edited, possibly with config identical to an
 * earlier call. Validate a replacement before applying it: rejection leaves the
 * previous config and plugin running. The host serializes lifecycle calls;
 * plugins synchronize shared state with operational callbacks.
 *
 * Return QCONTROL_CONFIG_OK, QCONTROL_CONFIG_INVALID, or another nonzero value
 * for failure. The callback receives no host handle or secret resolver.
 */
typedef int (*qcontrol_plugin_config_fn)(const qcontrol_string_view_t* config, char* error,
                                         size_t error_cap);

typedef struct {
    /** Must be QCONTROL_PLUGIN_VERSION. */
    uint32_t version;

    /** Plugin name for debugging */
    const char* name;

    /* === LIFECYCLE (optional) === */
    /** Initialize once with config; null accepts only absent/null config. */
    qcontrol_plugin_init_fn on_init;
    /** Called before plugin unload */
    qcontrol_plugin_cleanup_fn on_cleanup;

    /* === FILE OPERATIONS (all optional) === */
    qcontrol_file_open_fn on_file_open;
    qcontrol_file_read_fn on_file_read;
    qcontrol_file_write_fn on_file_write;
    qcontrol_file_close_fn on_file_close;
    qcontrol_file_op_fn on_file_op;

    /* === EXEC OPERATIONS (all optional, v1 spec - not yet implemented) === */
    qcontrol_exec_fn on_exec;
    qcontrol_exec_stdin_fn on_exec_stdin;
    qcontrol_exec_stdout_fn on_exec_stdout;
    qcontrol_exec_stderr_fn on_exec_stderr;
    qcontrol_exec_exit_fn on_exec_exit;

    /* === NETWORK OPERATIONS (all optional, v1 spec - not yet implemented) === */
    qcontrol_net_connect_fn on_net_connect;
    qcontrol_net_accept_fn on_net_accept;
    qcontrol_net_tls_fn on_net_tls;
    qcontrol_net_domain_fn on_net_domain;
    qcontrol_net_protocol_fn on_net_protocol;
    qcontrol_net_send_fn on_net_send;
    qcontrol_net_recv_fn on_net_recv;
    qcontrol_net_close_fn on_net_close;

    /* === HTTP OPERATIONS (all optional) === */
    qcontrol_http_request_fn on_http_request;
    qcontrol_http_request_body_fn on_http_request_body;
    qcontrol_http_request_trailers_fn on_http_request_trailers;
    qcontrol_http_request_done_fn on_http_request_done;
    qcontrol_http_response_fn on_http_response;
    qcontrol_http_response_body_fn on_http_response_body;
    qcontrol_http_response_trailers_fn on_http_response_trailers;
    qcontrol_http_response_done_fn on_http_response_done;
    qcontrol_http_exchange_close_fn on_http_exchange_close;

    /* === WEBSOCKET OPERATIONS (all optional) === */
    qcontrol_websocket_open_fn on_websocket_open;
    qcontrol_websocket_send_fn on_websocket_send;
    qcontrol_websocket_recv_fn on_websocket_recv;
    qcontrol_websocket_close_fn on_websocket_close;

    /* === SSE OPERATIONS (all optional) === */
    qcontrol_sse_open_fn on_sse_open;
    qcontrol_sse_event_fn on_sse_event;
    qcontrol_sse_close_fn on_sse_close;

    /* === NETSTAT (optional) === */
    /**
     * Called with one read-only snapshot. The pointer is valid only for
     * the call. A null callback skips the plugin.
     */
    qcontrol_netstat_fn on_netstat;

    /* === CONFIG UPDATES (optional) === */
    /** Update an initialized plugin; null means live updates are unsupported. */
    qcontrol_plugin_config_fn on_config;
} qcontrol_plugin_t;

/**
 * Plugins must export this symbol.
 */
extern const qcontrol_plugin_t qcontrol_plugin;

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_PLUGIN_H */

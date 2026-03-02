/**
 * @file plugin.c
 * @brief Example qcontrol plugin that logs network operations
 *
 * This plugin demonstrates how to use the qcontrol SDK to observe network
 * operations. It uses the descriptor-based plugin model to register
 * callbacks for connect, accept, TLS, domain, protocol, send, recv, and close.
 *
 * NOTE: Network hooks are not yet implemented in the agent, so this plugin
 * will compile but the callbacks won't be invoked at runtime.
 *
 * Environment variables:
 *   QCONTROL_LOG_FILE - Path to log file (default: /tmp/qcontrol.log)
 *
 * Build:
 *   make build   # Build shared library for dynamic loading
 *   make dist    # Build object file for bundling
 *
 * Usage:
 *   QCONTROL_PLUGINS=./dist/net-logger-<arch>.so qcontrol wrap -- ./target
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <qcontrol/log.h>

QCONTROL_LOGGER(logger);

/**
 * Log outbound connect() operations.
 */
static qcontrol_net_action_t on_net_connect(qcontrol_net_connect_event_t* ev) {
    qcontrol_log(&logger, "[net_logger] connect(fd=%d, dst=%s:%u) = %d\n",
            ev->fd, ev->dst_addr, ev->dst_port, ev->result);

    if (ev->src_addr != NULL) {
        qcontrol_log(&logger, "[net_logger]   src=%s:%u\n",
                ev->src_addr, ev->src_port);
    }

    return QCONTROL_NET_PASS;
}

/**
 * Log inbound accept() operations.
 */
static qcontrol_net_action_t on_net_accept(qcontrol_net_accept_event_t* ev) {
    qcontrol_log(&logger, "[net_logger] accept(fd=%d, listen_fd=%d, src=%s:%u) = %d\n",
            ev->fd, ev->listen_fd, ev->src_addr, ev->src_port, ev->result);
    return QCONTROL_NET_PASS;
}

/**
 * Log TLS handshake completion.
 */
static void on_net_tls(void* state, qcontrol_net_tls_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_logger] tls(fd=%d, version=%s)\n",
            ev->fd, ev->version);

    if (ev->cipher != NULL) {
        qcontrol_log(&logger, "[net_logger]   cipher=%s\n", ev->cipher);
    }
}

/**
 * Log domain name discovery.
 */
static void on_net_domain(void* state, qcontrol_net_domain_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_logger] domain(fd=%d, domain=%s)\n",
            ev->fd, ev->domain);
}

/**
 * Log protocol detection.
 */
static void on_net_protocol(void* state, qcontrol_net_protocol_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_logger] protocol(fd=%d, protocol=%s)\n",
            ev->fd, ev->protocol);
}

/**
 * Log send operations.
 */
static qcontrol_net_action_t on_net_send(void* state, qcontrol_net_send_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_logger] send(fd=%d, count=%zu)\n",
            ev->fd, ev->count);
    return QCONTROL_NET_PASS;
}

/**
 * Log recv operations.
 */
static qcontrol_net_action_t on_net_recv(void* state, qcontrol_net_recv_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_logger] recv(fd=%d, count=%zu) = %zd\n",
            ev->fd, ev->count, ev->result);
    return QCONTROL_NET_PASS;
}

/**
 * Log connection close.
 */
static void on_net_close(void* state, qcontrol_net_close_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_logger] close(fd=%d) = %d\n",
            ev->fd, ev->result);
}

/**
 * Plugin initialization - called when the plugin is loaded.
 */
static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[net_logger] initializing...\n");
    return 0;
}

/**
 * Plugin cleanup - called before the plugin is unloaded.
 */
static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[net_logger] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("net-logger",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_net_connect = on_net_connect,
    .on_net_accept = on_net_accept,
    .on_net_tls = on_net_tls,
    .on_net_domain = on_net_domain,
    .on_net_protocol = on_net_protocol,
    .on_net_send = on_net_send,
    .on_net_recv = on_net_recv,
    .on_net_close = on_net_close,
);

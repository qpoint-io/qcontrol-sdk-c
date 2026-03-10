/**
 * @file plugin.c
 * @brief Net transform plugin - demonstrates modifying plaintext network traffic.
 *
 * This example is intended for `qcontrol wrap`.
 * It rewrites simple text responses by replacing:
 *   "hello"  -> "hullo"
 *   "server" -> "client"
 *
 * The transform is deliberately simple and best demonstrated against a local
 * text-based HTTP server such as `../test-net-transform.sh`.
 *
 * Environment variables:
 *   QCONTROL_LOG_FILE - Path to log file (default: /tmp/qcontrol.log)
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <qcontrol/log.h>

QCONTROL_LOGGER(logger);

static const qcontrol_net_pattern_t recv_patterns[] = {
    QCONTROL_PATTERN("hello", "hullo"),
    QCONTROL_PATTERN("server", "client"),
};

static qcontrol_net_rw_config_t recv_config = {
    .replace = recv_patterns,
    .replace_count = sizeof(recv_patterns) / sizeof(recv_patterns[0]),
};

static qcontrol_net_action_t on_net_connect(qcontrol_net_connect_event_t* ev) {
    if (ev->result < 0) {
        return QCONTROL_NET_PASS;
    }

    qcontrol_log(&logger, "[net_transform] intercepting %s:%u\n",
            ev->dst_addr, ev->dst_port);

    qcontrol_net_session_t session = {
        .state = NULL,
        .recv_config = &recv_config,
    };

    return QCONTROL_NET_SESSION(session);
}

static void on_net_domain(void* state, qcontrol_net_domain_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_transform] domain(fd=%d, domain=%s)\n",
            ev->fd, ev->domain);
}

static void on_net_close(void* state, qcontrol_net_close_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[net_transform] close(fd=%d) = %d\n",
            ev->fd, ev->result);
}

static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[net_transform] initializing...\n");
    return 0;
}

static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[net_transform] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("net-transform",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_net_connect = on_net_connect,
    .on_net_domain = on_net_domain,
    .on_net_close = on_net_close,
);

/**
 * @file plugin.c
 * @brief Example qcontrol plugin that logs file operations
 *
 * This plugin demonstrates how to use the qcontrol SDK to observe file
 * operations. It uses the descriptor-based plugin model to register
 * callbacks for open, read, write, and close operations.
 *
 * Environment variables:
 *   QCONTROL_LOG_FILE - Path to log file (default: /tmp/qcontrol.log)
 *
 * Build:
 *   make build   # Build shared library for dynamic loading
 *   make dist    # Build object file for bundling
 *
 * Usage:
 *   QCONTROL_PLUGINS=./dist/file-logger-<arch>.so qcontrol wrap -- ./target
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <qcontrol/log.h>

QCONTROL_LOGGER(logger);

/**
 * Log open() operations after they complete.
 */
static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    if (ev->result >= 0) {
        qcontrol_log(&logger, "[file_logger] open(\"%s\", 0x%x) = %d\n",
                ev->path, ev->flags, ev->result);
    } else {
        qcontrol_log(&logger, "[file_logger] open(\"%s\", 0x%x) = %d (error)\n",
                ev->path, ev->flags, ev->result);
    }
    return QCONTROL_FILE_PASS;
}

/**
 * Log read() operations after they complete.
 */
static qcontrol_file_action_t on_file_read(void* state, qcontrol_file_read_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[file_logger] read(%d, buf, %zu) = %zd\n",
            ev->fd, ev->count, ev->result);
    return QCONTROL_FILE_PASS;
}

/**
 * Log write() operations after they complete.
 */
static qcontrol_file_action_t on_file_write(void* state, qcontrol_file_write_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[file_logger] write(%d, buf, %zu) = %zd\n",
            ev->fd, ev->count, ev->result);
    return QCONTROL_FILE_PASS;
}

/**
 * Log close() operations after they complete.
 */
static void on_file_close(void* state, qcontrol_file_close_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[file_logger] close(%d) = %d\n",
            ev->fd, ev->result);
}

/**
 * Plugin initialization - called when the plugin is loaded.
 */
static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[file_logger] initializing...\n");
    return 0;
}

/**
 * Plugin cleanup - called before the plugin is unloaded.
 */
static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[file_logger] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("file-logger",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_file_open = on_file_open,
    .on_file_read = on_file_read,
    .on_file_write = on_file_write,
    .on_file_close = on_file_close,
);

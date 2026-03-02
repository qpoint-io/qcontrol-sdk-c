/**
 * @file plugin.c
 * @brief Example qcontrol plugin that blocks access to certain paths
 *
 * This plugin demonstrates BLOCK functionality by denying access
 * to files matching "/tmp/secret*".
 *
 * Environment variables:
 *   QCONTROL_LOG_FILE - Path to log file (default: /tmp/qcontrol.log)
 *
 * Build:
 *   make build   # Build shared library for dynamic loading
 *   make dist    # Build object file for bundling
 *
 * Usage:
 *   QCONTROL_PLUGINS=./dist/access-control-<arch>.so qcontrol wrap -- ./target
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <qcontrol/log.h>
#include <string.h>

QCONTROL_LOGGER(logger);

/**
 * Block access to secret files.
 *
 * Note: This runs after the open() syscall completes. If the file was
 * successfully opened, returning BLOCK will close the fd and return
 * an error to the caller.
 */
static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    /* Block access to /tmp/secret* */
    if (strncmp(ev->path, "/tmp/secret", 11) == 0) {
        qcontrol_log(&logger, "[access_control] BLOCKED: %s\n", ev->path);
        return QCONTROL_FILE_BLOCK;
    }
    return QCONTROL_FILE_PASS;
}

/**
 * Plugin initialization - called when the plugin is loaded.
 */
static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[access_control] initializing - blocking /tmp/secret*\n");
    return 0;
}

/**
 * Plugin cleanup - called before the plugin is unloaded.
 */
static void plugin_cleanup(void) {
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("access-control",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_file_open = on_file_open,
);

/**
 * @file plugin.c
 * @brief Example qcontrol plugin that logs exec operations
 *
 * This plugin demonstrates how to use the qcontrol SDK to observe exec
 * operations. It uses the descriptor-based plugin model to register
 * callbacks for exec, stdin, stdout, stderr, and exit events.
 *
 * NOTE: Exec hooks are not yet implemented in the agent, so this plugin
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
 *   QCONTROL_PLUGINS=./dist/exec-logger-<arch>.so qcontrol wrap -- ./target
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <qcontrol/log.h>

QCONTROL_LOGGER(logger);

/**
 * Log exec() operations - called when a process is spawned.
 */
static qcontrol_exec_action_t on_exec(qcontrol_exec_event_t* ev) {
    qcontrol_log(&logger, "[exec_logger] exec(\"%s\")\n", ev->path);

    /* Log arguments */
    for (size_t i = 0; i < ev->argc; i++) {
        qcontrol_log(&logger, "[exec_logger]   argv[%zu] = \"%s\"\n",
                i, ev->argv[i]);
    }

    /* Log cwd if set */
    if (ev->cwd != NULL) {
        qcontrol_log(&logger, "[exec_logger]   cwd = \"%s\"\n", ev->cwd);
    }

    return QCONTROL_EXEC_PASS;
}

/**
 * Log stdin writes to child process.
 */
static qcontrol_exec_action_t on_exec_stdin(void* state, qcontrol_exec_stdin_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[exec_logger] stdin(pid=%d, count=%zu)\n",
            (int)ev->pid, ev->count);
    return QCONTROL_EXEC_PASS;
}

/**
 * Log stdout reads from child process.
 */
static qcontrol_exec_action_t on_exec_stdout(void* state, qcontrol_exec_stdout_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[exec_logger] stdout(pid=%d, count=%zu) = %zd\n",
            (int)ev->pid, ev->count, ev->result);
    return QCONTROL_EXEC_PASS;
}

/**
 * Log stderr reads from child process.
 */
static qcontrol_exec_action_t on_exec_stderr(void* state, qcontrol_exec_stderr_event_t* ev) {
    (void)state;
    qcontrol_log(&logger, "[exec_logger] stderr(pid=%d, count=%zu) = %zd\n",
            (int)ev->pid, ev->count, ev->result);
    return QCONTROL_EXEC_PASS;
}

/**
 * Log child process exit.
 */
static void on_exec_exit(void* state, qcontrol_exec_exit_event_t* ev) {
    (void)state;
    if (ev->exit_signal == 0) {
        qcontrol_log(&logger, "[exec_logger] exit(pid=%d, code=%d)\n",
                (int)ev->pid, ev->exit_code);
    } else {
        qcontrol_log(&logger, "[exec_logger] exit(pid=%d, signal=%d)\n",
                (int)ev->pid, ev->exit_signal);
    }
}

/**
 * Plugin initialization - called when the plugin is loaded.
 */
static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[exec_logger] initializing...\n");
    return 0;
}

/**
 * Plugin cleanup - called before the plugin is unloaded.
 */
static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[exec_logger] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("exec-logger",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_exec = on_exec,
    .on_exec_stdin = on_exec_stdin,
    .on_exec_stdout = on_exec_stdout,
    .on_exec_stderr = on_exec_stderr,
    .on_exec_exit = on_exec_exit,
);

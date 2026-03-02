/**
 * @file plugin.c
 * @brief Content filter plugin - redacts sensitive data in .txt and .log files
 *
 * Demonstrates session configuration with buffer transforms.
 * Uses QCONTROL_FILE_SESSION with read config for pattern replacement.
 *
 * Environment variables:
 *   QCONTROL_LOG_FILE - Path to log file (default: /tmp/qcontrol.log)
 *
 * Build:
 *   make build   # Build shared library for dynamic loading
 *   make dist    # Build object file for bundling
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <qcontrol/log.h>
#include <stdlib.h>
#include <string.h>

QCONTROL_LOGGER(logger);

/**
 * Per-file state for tracking filter activity.
 */
typedef struct {
    char* path;
} filter_state_t;

static filter_state_t* filter_state_create(const char* path, size_t path_len) {
    filter_state_t* state = malloc(sizeof(filter_state_t));
    if (!state) return NULL;

    state->path = malloc(path_len + 1);
    if (!state->path) {
        free(state);
        return NULL;
    }

    memcpy(state->path, path, path_len);
    state->path[path_len] = '\0';

    return state;
}

static void filter_state_destroy(filter_state_t* state) {
    if (state) {
        free(state->path);
        free(state);
    }
}

/**
 * Check if path ends with suffix.
 */
static int ends_with(const char* path, size_t path_len, const char* suffix) {
    size_t suffix_len = strlen(suffix);
    if (path_len < suffix_len) return 0;
    return memcmp(path + path_len - suffix_len, suffix, suffix_len) == 0;
}

/**
 * Pattern replacements for sensitive data.
 */
static const qcontrol_file_pattern_t redaction_patterns[] = {
    QCONTROL_PATTERN("password", "********"),
    QCONTROL_PATTERN("secret", "[REDACTED]"),
    QCONTROL_PATTERN("api_key", "[HIDDEN]"),
    QCONTROL_PATTERN("token", "[HIDDEN]"),
};

/**
 * Read config for filtered files.
 */
static qcontrol_file_rw_config_t read_config = {
    .prefix = "[FILTERED]\n",
    .prefix_len = 11,
    .replace = redaction_patterns,
    .replace_count = sizeof(redaction_patterns) / sizeof(redaction_patterns[0]),
};

/**
 * Filter .txt and .log files with sensitive data redaction.
 */
static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    /* Only filter successfully opened files */
    if (ev->result < 0) {
        return QCONTROL_FILE_PASS;
    }

    /* Only filter .txt and .log files */
    int is_txt = ends_with(ev->path, ev->path_len, ".txt");
    int is_log = ends_with(ev->path, ev->path_len, ".log");

    if (!is_txt && !is_log) {
        return QCONTROL_FILE_PASS;
    }

    /* Create state to track this file */
    filter_state_t* state = filter_state_create(ev->path, ev->path_len);
    if (!state) {
        return QCONTROL_FILE_PASS;
    }

    qcontrol_log(&logger, "[content_filter] filtering: %s\n", state->path);

    /* Return session with read transforms */
    qcontrol_file_session_t session = {
        .state = state,
        .read = &read_config,
    };

    return QCONTROL_FILE_SESSION(session);
}

/**
 * Cleanup state on close.
 */
static void on_file_close(void* state, qcontrol_file_close_event_t* ev) {
    (void)ev;
    if (state) {
        filter_state_t* filter_state = (filter_state_t*)state;
        qcontrol_log(&logger, "[content_filter] closed: %s\n", filter_state->path);
        filter_state_destroy(filter_state);
    }
}

static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[content_filter] initializing...\n");
    return 0;
}

static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[content_filter] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("content-filter",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_file_open = on_file_open,
    .on_file_close = on_file_close,
    /* Note: on_file_read/on_file_write not needed - transforms are
     * handled declaratively via the session config returned from on_file_open */
);

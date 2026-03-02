/**
 * @file plugin.c
 * @brief Byte counter plugin - tracks bytes read/written per file
 *
 * Demonstrates per-file state tracking without transforms.
 * Uses QCONTROL_FILE_STATE to attach custom state to each file.
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
 * Per-file statistics tracked from open to close.
 */
typedef struct {
    char* path;
    size_t bytes_read;
    size_t bytes_written;
    size_t read_calls;
    size_t write_calls;
} file_stats_t;

static file_stats_t* file_stats_create(const char* path, size_t path_len) {
    file_stats_t* stats = malloc(sizeof(file_stats_t));
    if (!stats) return NULL;

    stats->path = malloc(path_len + 1);
    if (!stats->path) {
        free(stats);
        return NULL;
    }

    memcpy(stats->path, path, path_len);
    stats->path[path_len] = '\0';
    stats->bytes_read = 0;
    stats->bytes_written = 0;
    stats->read_calls = 0;
    stats->write_calls = 0;

    return stats;
}

static void file_stats_destroy(file_stats_t* stats) {
    if (stats) {
        free(stats->path);
        free(stats);
    }
}

/**
 * Track file open - attach state to successfully opened files.
 */
static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    /* Only track successfully opened files */
    if (ev->result < 0) {
        return QCONTROL_FILE_PASS;
    }

    /* Skip common paths to reduce noise */
    if (strncmp(ev->path, "/proc/", 6) == 0 ||
        strncmp(ev->path, "/sys/", 5) == 0 ||
        strncmp(ev->path, "/dev/", 5) == 0) {
        return QCONTROL_FILE_PASS;
    }

    /* Create state to track this file */
    file_stats_t* stats = file_stats_create(ev->path, ev->path_len);
    if (!stats) {
        return QCONTROL_FILE_PASS;
    }

    qcontrol_log(&logger, "[byte_counter] tracking: %s\n", stats->path);

    /* Return STATE to track without transforms */
    return QCONTROL_FILE_STATE(stats);
}

/**
 * Count bytes read.
 */
static qcontrol_file_action_t on_file_read(void* state, qcontrol_file_read_event_t* ev) {
    if (state && ev->result > 0) {
        file_stats_t* stats = (file_stats_t*)state;
        stats->bytes_read += (size_t)ev->result;
        stats->read_calls++;
    }
    return QCONTROL_FILE_PASS;
}

/**
 * Count bytes written.
 */
static qcontrol_file_action_t on_file_write(void* state, qcontrol_file_write_event_t* ev) {
    if (state && ev->result > 0) {
        file_stats_t* stats = (file_stats_t*)state;
        stats->bytes_written += (size_t)ev->result;
        stats->write_calls++;
    }
    return QCONTROL_FILE_PASS;
}

/**
 * Report statistics and cleanup state on close.
 */
static void on_file_close(void* state, qcontrol_file_close_event_t* ev) {
    (void)ev;
    if (state) {
        file_stats_t* stats = (file_stats_t*)state;

        qcontrol_log(&logger,
            "[byte_counter] %s: read %zu bytes (%zu calls), wrote %zu bytes (%zu calls)\n",
            stats->path,
            stats->bytes_read,
            stats->read_calls,
            stats->bytes_written,
            stats->write_calls);

        file_stats_destroy(stats);
    }
}

static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[byte_counter] initializing...\n");
    return 0;
}

static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[byte_counter] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("byte-counter",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_file_open = on_file_open,
    .on_file_read = on_file_read,
    .on_file_write = on_file_write,
    .on_file_close = on_file_close,
);

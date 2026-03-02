/**
 * @file plugin.c
 * @brief Text transform plugin - demonstrates custom buffer manipulation
 *
 * Uses RwConfig.transform for advanced buffer operations.
 * Applies different transforms based on file extension:
 *   .upper    - Convert text to uppercase
 *   .rot13    - Apply ROT13 encoding
 *   .bracket  - Wrap content in brackets
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
 * Transform mode for the file.
 */
typedef enum {
    TRANSFORM_UPPER,
    TRANSFORM_ROT13,
    TRANSFORM_BRACKET,
} transform_mode_t;

/**
 * Per-file state tracking transform mode.
 */
typedef struct {
    char* path;
    transform_mode_t mode;
} transform_state_t;

static transform_state_t* transform_state_create(const char* path, size_t path_len, transform_mode_t mode) {
    transform_state_t* state = malloc(sizeof(transform_state_t));
    if (!state) return NULL;

    state->path = malloc(path_len + 1);
    if (!state->path) {
        free(state);
        return NULL;
    }

    memcpy(state->path, path, path_len);
    state->path[path_len] = '\0';
    state->mode = mode;

    return state;
}

static void transform_state_destroy(transform_state_t* state) {
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

static const char* mode_name(transform_mode_t mode) {
    switch (mode) {
        case TRANSFORM_UPPER: return "upper";
        case TRANSFORM_ROT13: return "rot13";
        case TRANSFORM_BRACKET: return "bracket";
        default: return "unknown";
    }
}

/**
 * Custom transform function - modifies buffer based on transform mode.
 */
static qcontrol_file_action_t read_transform(
    void* state_ptr,
    qcontrol_file_ctx_t* ctx,
    qcontrol_buffer_t* buf
) {
    (void)ctx;

    if (!state_ptr) {
        return QCONTROL_FILE_PASS;
    }

    transform_state_t* state = (transform_state_t*)state_ptr;
    size_t len = qcontrol_buffer_len(buf);
    if (len == 0) {
        return QCONTROL_FILE_PASS;
    }

    switch (state->mode) {
        case TRANSFORM_UPPER: {
            /* Convert lowercase to uppercase using single-char replacements */
            const char* data = qcontrol_buffer_data(buf);
            for (size_t i = 0; i < len; i++) {
                char c = data[i];
                if (c >= 'a' && c <= 'z') {
                    char lower[2] = {c, '\0'};
                    char upper[2] = {(char)(c - 32), '\0'};
                    qcontrol_buffer_replace(buf, lower, 1, upper, 1);
                    /* Re-fetch data pointer after modification */
                    data = qcontrol_buffer_data(buf);
                }
            }
            break;
        }
        case TRANSFORM_ROT13: {
            /* ROT13: rotate letters by 13 positions */
            const char* data = qcontrol_buffer_data(buf);
            char* transformed = malloc(len);
            if (!transformed) break;

            for (size_t i = 0; i < len; i++) {
                char c = data[i];
                if (c >= 'a' && c <= 'z') {
                    transformed[i] = 'a' + ((c - 'a' + 13) % 26);
                } else if (c >= 'A' && c <= 'Z') {
                    transformed[i] = 'A' + ((c - 'A' + 13) % 26);
                } else {
                    transformed[i] = c;
                }
            }

            qcontrol_buffer_set(buf, transformed, len);
            free(transformed);
            break;
        }
        case TRANSFORM_BRACKET:
            /* Wrap content in brackets */
            qcontrol_buffer_prepend(buf, "[[[ ", 4);
            qcontrol_buffer_append(buf, " ]]]", 4);
            break;
    }

    return QCONTROL_FILE_PASS;
}

/* Static read config - transform function is set per-file */
static qcontrol_file_rw_config_t read_config = {
    .transform = read_transform,
};

/**
 * Determine transform mode and configure session.
 */
static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    if (ev->result < 0) {
        return QCONTROL_FILE_PASS;
    }

    /* Determine transform mode based on extension */
    transform_mode_t mode;
    if (ends_with(ev->path, ev->path_len, ".upper")) {
        mode = TRANSFORM_UPPER;
    } else if (ends_with(ev->path, ev->path_len, ".rot13")) {
        mode = TRANSFORM_ROT13;
    } else if (ends_with(ev->path, ev->path_len, ".bracket")) {
        mode = TRANSFORM_BRACKET;
    } else {
        return QCONTROL_FILE_PASS;
    }

    transform_state_t* state = transform_state_create(ev->path, ev->path_len, mode);
    if (!state) {
        return QCONTROL_FILE_PASS;
    }

    qcontrol_log(&logger, "[text_transform] filtering %s with mode %s\n",
            state->path, mode_name(mode));

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
        transform_state_t* transform_state = (transform_state_t*)state;
        qcontrol_log(&logger, "[text_transform] closed: %s\n", transform_state->path);
        transform_state_destroy(transform_state);
    }
}

static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[text_transform] initializing...\n");
    return 0;
}

static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[text_transform] cleanup complete\n");
    qcontrol_log_cleanup(&logger);
}

QCONTROL_PLUGIN("text-transform",
    .on_init = plugin_init,
    .on_cleanup = plugin_cleanup,
    .on_file_open = on_file_open,
    .on_file_close = on_file_close,
    /* Note: on_file_read/on_file_write not needed - the custom transform
     * function is called automatically via the session config */
);

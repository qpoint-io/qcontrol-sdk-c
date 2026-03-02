/**
 * @file qcontrol/helpers.h
 * @brief Convenience macros for idiomatic C plugin development
 *
 * This header provides helper macros for defining plugins.
 * It is C-specific and NOT synced from the Zig SDK.
 */

#ifndef QCONTROL_HELPERS_H
#define QCONTROL_HELPERS_H

#include "common.h"
#include "file.h"
#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Plugin Definition Macros
 * ============================================================================ */

/**
 * Define a plugin descriptor with automatic version.
 *
 * Example:
 * @code
 * QCONTROL_PLUGIN("my-plugin",
 *     .on_init = plugin_init,
 *     .on_file_open = on_file_open,
 * );
 * @endcode
 */
#define QCONTROL_PLUGIN(plugin_name, ...) \
    const qcontrol_plugin_t qcontrol_plugin = { \
        .version = QCONTROL_PLUGIN_VERSION, \
        .name = (plugin_name), \
        __VA_ARGS__ \
    }

/* ============================================================================
 * String Helper Macros
 * ============================================================================ */

/**
 * String literal with length for buffer operations.
 *
 * Example:
 * @code
 * qcontrol_buffer_contains(buf, QCONTROL_STR("needle"));
 * @endcode
 */
#define QCONTROL_STR(s) (s), (sizeof(s) - 1)

/**
 * Pattern replacement initializer.
 *
 * Example:
 * @code
 * static const qcontrol_file_pattern_t patterns[] = {
 *     QCONTROL_PATTERN("foo", "bar"),
 *     QCONTROL_PATTERN("old", "new"),
 * };
 * @endcode
 */
#define QCONTROL_PATTERN(needle, replacement) \
    { (needle), sizeof(needle) - 1, (replacement), sizeof(replacement) - 1 }

/* ============================================================================
 * File Action Macros
 * ============================================================================ */

/**
 * Create a SESSION action for file interception.
 *
 * Example:
 * @code
 * qcontrol_file_session_t session = { .state = my_state, .read = &read_cfg };
 * return QCONTROL_FILE_SESSION(session);
 * @endcode
 */
#define QCONTROL_FILE_SESSION(sess) \
    ((qcontrol_file_action_t){ .type = QCONTROL_FILE_ACTION_SESSION, .session = (sess) })

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_HELPERS_H */

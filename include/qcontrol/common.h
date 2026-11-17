/**
 * @file qcontrol/common.h
 * @brief Common types and constants for qcontrol SDK
 */

#ifndef QCONTROL_COMMON_H
#define QCONTROL_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plugin descriptor version for the current public plugin ABI.
 */
#define QCONTROL_PLUGIN_VERSION 1

/**
 * Borrowed, length-delimited string bytes shared without allocation.
 *
 * `data` is readable for `len` bytes; NUL termination is not guaranteed.
 * The API using the view defines the encoding and how long the bytes remain
 * valid. Copy any bytes that must outlive that lifetime.
 */
typedef struct {
    const char* data;
    size_t len;
} qcontrol_string_view_t;

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_COMMON_H */

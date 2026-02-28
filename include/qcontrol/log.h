/**
 * @file log.h
 * @brief Thread-safe logging utilities for qcontrol plugins
 *
 * This header provides a simple, thread-safe logging API for plugins.
 * It is header-only for easy integration with both shared libraries and
 * object files.
 *
 * Features:
 * - Thread-safe via mutex
 * - Reentrancy protection (prevents infinite recursion from logging syscalls)
 * - Auto-flushes after each write
 * - Supports QCONTROL_LOG_FILE environment variable
 *
 * Usage:
 * @code
 * #include <qcontrol.h>
 * #include <qcontrol/log.h>
 *
 * QCONTROL_LOGGER(logger);
 *
 * int qcontrol_plugin_init(void) {
 *     qcontrol_log_init(&logger, NULL);  // NULL = use QCONTROL_LOG_FILE env var
 *     qcontrol_log(&logger, "[plugin] started\n");
 *     return 0;
 * }
 *
 * static qcontrol_status_t on_file_open_leave(qcontrol_file_open_ctx_t* ctx) {
 *     qcontrol_log(&logger, "[plugin] open(%s) = %d\n", ctx->path, ctx->result);
 *     return QCONTROL_STATUS_CONTINUE;
 * }
 *
 * void qcontrol_plugin_cleanup(void) {
 *     qcontrol_log_cleanup(&logger);
 * }
 * @endcode
 */

#ifndef QCONTROL_LOG_H
#define QCONTROL_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Logger Types
 * ============================================================================ */

/**
 * Logger state structure.
 *
 * Use QCONTROL_LOGGER() macro to declare and initialize.
 */
typedef struct {
    FILE* file;
    int fd;
    pthread_mutex_t mutex;
} qcontrol_logger_t;

/**
 * Declare and initialize a static logger instance.
 *
 * Example:
 *   QCONTROL_LOGGER(my_logger);
 */
#define QCONTROL_LOGGER(name) \
    static qcontrol_logger_t name = { \
        .file = NULL, \
        .fd = -1, \
        .mutex = PTHREAD_MUTEX_INITIALIZER \
    }

/**
 * Default log file path when QCONTROL_LOG_FILE is not set.
 */
#define QCONTROL_LOG_DEFAULT_PATH "/tmp/qcontrol.log"

/* ============================================================================
 * Reentrancy Protection
 * ============================================================================ */

/**
 * Thread-local flag to prevent recursive logging.
 *
 * When a plugin logs during a file operation callback, that logging
 * might trigger additional file operations (write to log file), which
 * would cause infinite recursion. This flag prevents that.
 */
static __thread int qcontrol_log_in_progress_ = 0;

/* ============================================================================
 * Logger API
 * ============================================================================ */

/**
 * Initialize a logger.
 *
 * @param logger Pointer to logger instance (from QCONTROL_LOGGER)
 * @param path   Log file path, or NULL to use QCONTROL_LOG_FILE env var
 *               (defaults to /tmp/qcontrol.log if env var not set)
 */
static inline void qcontrol_log_init(qcontrol_logger_t* logger, const char* path) {
    if (logger->file != NULL) {
        return;  /* Already initialized */
    }

    if (path == NULL) {
        path = getenv("QCONTROL_LOG_FILE");
        if (path == NULL || path[0] == '\0') {
            path = QCONTROL_LOG_DEFAULT_PATH;
        }
    }

    logger->file = fopen(path, "a");
    if (logger->file != NULL) {
        logger->fd = fileno(logger->file);
    }
}

/**
 * Clean up a logger.
 *
 * Closes the log file and resets state.
 *
 * @param logger Pointer to logger instance
 */
static inline void qcontrol_log_cleanup(qcontrol_logger_t* logger) {
    if (logger->file != NULL) {
        fclose(logger->file);
        logger->file = NULL;
        logger->fd = -1;
    }
}

/**
 * Check if a file descriptor is the logger's log file.
 *
 * Use this to skip logging operations on the log file itself.
 *
 * @param logger Pointer to logger instance
 * @param fd     File descriptor to check
 * @return       Non-zero if fd is the log file, 0 otherwise
 */
static inline int qcontrol_log_is_log_fd(qcontrol_logger_t* logger, int fd) {
    return logger->fd >= 0 && fd == logger->fd;
}

/**
 * Write a formatted message to the log.
 *
 * Thread-safe and reentrant-safe. Messages are flushed immediately.
 *
 * @param logger Pointer to logger instance
 * @param fmt    printf-style format string
 * @param ...    Format arguments
 */
static inline void qcontrol_log(qcontrol_logger_t* logger, const char* fmt, ...) {
    /* Skip if logger not initialized or we're already logging */
    if (logger->fd < 0 || qcontrol_log_in_progress_) {
        return;
    }

    /* Set reentrancy guard */
    qcontrol_log_in_progress_ = 1;

    pthread_mutex_lock(&logger->mutex);

    va_list args;
    va_start(args, fmt);
    vfprintf(logger->file, fmt, args);
    va_end(args);
    fflush(logger->file);

    pthread_mutex_unlock(&logger->mutex);

    /* Clear reentrancy guard */
    qcontrol_log_in_progress_ = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_LOG_H */

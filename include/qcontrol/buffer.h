/**
 * @file qcontrol/buffer.h
 * @brief Shared mutable byte-buffer handle for qcontrol SDK callbacks
 *
 * Defines the common buffer ABI used by file, net, exec, and HTTP callbacks.
 * Hosts provide one concrete `context + ops` handle, and plugins use the
 * inline helpers in this header to query or mutate bytes without depending on
 * process-global helper symbols.
 */

#ifndef QCONTROL_BUFFER_H
#define QCONTROL_BUFFER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Shared Buffer Handle
 * ============================================================================ */

typedef struct qcontrol_buffer_ops qcontrol_buffer_ops_t;
typedef struct qcontrol_buffer qcontrol_buffer_t;

/**
 * Host callbacks that let plugins inspect and mutate one shared byte buffer
 * without depending on process-global helper exports.
 */
struct qcontrol_buffer_ops {
    size_t (*len)(void* context);
    const char* (*data)(void* context);
    int (*contains)(
        void* context,
        const char* needle,
        size_t needle_len
    );
    int (*starts_with)(
        void* context,
        const char* prefix,
        size_t prefix_len
    );
    int (*ends_with)(
        void* context,
        const char* suffix,
        size_t suffix_len
    );
    size_t (*index_of)(
        void* context,
        const char* needle,
        size_t needle_len
    );
    void (*prepend)(
        void* context,
        const char* data,
        size_t len
    );
    void (*append)(
        void* context,
        const char* data,
        size_t len
    );
    int (*replace)(
        void* context,
        const char* needle,
        size_t needle_len,
        const char* replacement,
        size_t replacement_len
    );
    size_t (*replace_all)(
        void* context,
        const char* needle,
        size_t needle_len,
        const char* replacement,
        size_t replacement_len
    );
    int (*remove)(
        void* context,
        const char* needle,
        size_t needle_len
    );
    size_t (*remove_all)(
        void* context,
        const char* needle,
        size_t needle_len
    );
    void (*clear)(void* context);
    void (*set)(
        void* context,
        const char* data,
        size_t len
    );
    void (*insert_at)(
        void* context,
        size_t pos,
        const char* data,
        size_t len
    );
    void (*remove_range)(
        void* context,
        size_t start,
        size_t end
    );
};

/**
 * Concrete mutable byte-buffer handle shared across file, net, exec, and HTTP
 * plugin callbacks.
 */
struct qcontrol_buffer {
    void* context;
    const qcontrol_buffer_ops_t* ops;
};

/* ============================================================================
 * Buffer Operations
 * ============================================================================ */

/**
 * Get the current buffer length.
 */
static inline size_t qcontrol_buffer_len(const qcontrol_buffer_t* buf) {
    if (!buf || !buf->ops || !buf->ops->len) {
        return 0;
    }
    return buf->ops->len(buf->context);
}

/**
 * Get a pointer to the current buffer data.
 */
static inline const char* qcontrol_buffer_data(const qcontrol_buffer_t* buf) {
    if (!buf || !buf->ops || !buf->ops->data) {
        return NULL;
    }
    return buf->ops->data(buf->context);
}

/**
 * Check if buffer contains needle.
 * @return 1 if found, 0 otherwise
 */
static inline int qcontrol_buffer_contains(
    const qcontrol_buffer_t* buf,
    const char* needle,
    size_t needle_len
) {
    if (!buf || !buf->ops || !buf->ops->contains) {
        return 0;
    }
    return buf->ops->contains(buf->context, needle, needle_len);
}

/**
 * Check if buffer starts with prefix.
 * @return 1 if true, 0 otherwise
 */
static inline int qcontrol_buffer_starts_with(
    const qcontrol_buffer_t* buf,
    const char* prefix,
    size_t prefix_len
) {
    if (!buf || !buf->ops || !buf->ops->starts_with) {
        return 0;
    }
    return buf->ops->starts_with(buf->context, prefix, prefix_len);
}

/**
 * Check if buffer ends with suffix.
 * @return 1 if true, 0 otherwise
 */
static inline int qcontrol_buffer_ends_with(
    const qcontrol_buffer_t* buf,
    const char* suffix,
    size_t suffix_len
) {
    if (!buf || !buf->ops || !buf->ops->ends_with) {
        return 0;
    }
    return buf->ops->ends_with(buf->context, suffix, suffix_len);
}

/**
 * Find index of needle in buffer.
 * @return Index if found, SIZE_MAX if not found
 */
static inline size_t qcontrol_buffer_index_of(
    const qcontrol_buffer_t* buf,
    const char* needle,
    size_t needle_len
) {
    if (!buf || !buf->ops || !buf->ops->index_of) {
        return (size_t)-1;
    }
    return buf->ops->index_of(buf->context, needle, needle_len);
}

/**
 * Prepend data to buffer.
 */
static inline void qcontrol_buffer_prepend(
    qcontrol_buffer_t* buf,
    const char* data,
    size_t len
) {
    if (!buf || !buf->ops || !buf->ops->prepend) {
        return;
    }
    buf->ops->prepend(buf->context, data, len);
}

/**
 * Append data to buffer.
 */
static inline void qcontrol_buffer_append(
    qcontrol_buffer_t* buf,
    const char* data,
    size_t len
) {
    if (!buf || !buf->ops || !buf->ops->append) {
        return;
    }
    buf->ops->append(buf->context, data, len);
}

/**
 * Replace first occurrence of needle with replacement.
 * @return 1 if replaced, 0 if not found
 */
static inline int qcontrol_buffer_replace(
    qcontrol_buffer_t* buf,
    const char* needle,
    size_t needle_len,
    const char* replacement,
    size_t replacement_len
) {
    if (!buf || !buf->ops || !buf->ops->replace) {
        return 0;
    }
    return buf->ops->replace(
        buf->context,
        needle,
        needle_len,
        replacement,
        replacement_len
    );
}

/**
 * Replace all occurrences of needle with replacement.
 * @return Number of replacements made
 */
static inline size_t qcontrol_buffer_replace_all(
    qcontrol_buffer_t* buf,
    const char* needle,
    size_t needle_len,
    const char* replacement,
    size_t replacement_len
) {
    if (!buf || !buf->ops || !buf->ops->replace_all) {
        return 0;
    }
    return buf->ops->replace_all(
        buf->context,
        needle,
        needle_len,
        replacement,
        replacement_len
    );
}

/**
 * Remove first occurrence of needle.
 * @return 1 if removed, 0 if not found
 */
static inline int qcontrol_buffer_remove(
    qcontrol_buffer_t* buf,
    const char* needle,
    size_t needle_len
) {
    if (!buf || !buf->ops || !buf->ops->remove) {
        return 0;
    }
    return buf->ops->remove(buf->context, needle, needle_len);
}

/**
 * Remove all occurrences of needle.
 * @return Number of removals
 */
static inline size_t qcontrol_buffer_remove_all(
    qcontrol_buffer_t* buf,
    const char* needle,
    size_t needle_len
) {
    if (!buf || !buf->ops || !buf->ops->remove_all) {
        return 0;
    }
    return buf->ops->remove_all(buf->context, needle, needle_len);
}

/**
 * Clear buffer contents.
 */
static inline void qcontrol_buffer_clear(qcontrol_buffer_t* buf) {
    if (!buf || !buf->ops || !buf->ops->clear) {
        return;
    }
    buf->ops->clear(buf->context);
}

/**
 * Set buffer contents to new data.
 */
static inline void qcontrol_buffer_set(
    qcontrol_buffer_t* buf,
    const char* data,
    size_t len
) {
    if (!buf || !buf->ops || !buf->ops->set) {
        return;
    }
    buf->ops->set(buf->context, data, len);
}

/**
 * Insert data at position.
 */
static inline void qcontrol_buffer_insert_at(
    qcontrol_buffer_t* buf,
    size_t pos,
    const char* data,
    size_t len
) {
    if (!buf || !buf->ops || !buf->ops->insert_at) {
        return;
    }
    buf->ops->insert_at(buf->context, pos, data, len);
}

/**
 * Remove range from buffer.
 */
static inline void qcontrol_buffer_remove_range(
    qcontrol_buffer_t* buf,
    size_t start,
    size_t end
) {
    if (!buf || !buf->ops || !buf->ops->remove_range) {
        return;
    }
    buf->ops->remove_range(buf->context, start, end);
}

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_BUFFER_H */

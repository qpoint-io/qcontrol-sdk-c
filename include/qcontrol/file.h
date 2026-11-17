/**
 * @file qcontrol/file.h
 * @brief File operation types for qcontrol SDK
 *
 * Defines the session-based file plugin model where:
 * - Configuration happens per-file at open time (not globally)
 * - State flows automatically between operations on the same fd
 * - Declarative transforms (prefix, suffix, replace) require zero code
 * - The agent handles heavy lifting; SDKs are thin wrappers
 *
 * Concurrency (see also plugin.h):
 * - Hosts may invoke file callbacks concurrently for **independent** file
 *   sessions (distinct host-tracked handles / streams).
 * - Hosts may serialize callbacks for a **single** session so multi-plugin
 *   chains and transforms stay ordered.
 * - Plugins must synchronize any shared mutable state they keep outside the
 *   session state pointer returned from open.
 */

#ifndef QCONTROL_FILE_H
#define QCONTROL_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * File Constants
 * ============================================================================ */

/**
 * Sentinel value for set_flags to indicate "no change".
 * Use this instead of magic number -1 for clarity.
 */
#define QCONTROL_FILE_FLAGS_UNCHANGED (-1)

/* ============================================================================
 * File Action Types
 * ============================================================================ */

/**
 * Action types returned by file operation callbacks.
 */
typedef enum {
    /** No interception, continue normally */
    QCONTROL_FILE_ACTION_PASS = 0,
    /** Block the operation with EACCES */
    QCONTROL_FILE_ACTION_BLOCK = 1,
    /** Block the operation with a specific errno */
    QCONTROL_FILE_ACTION_BLOCK_ERRNO = 2,
    /** Intercept with full session config */
    QCONTROL_FILE_ACTION_SESSION = 3,
    /** Track state only, no transforms */
    QCONTROL_FILE_ACTION_STATE = 4,
} qcontrol_file_action_type_t;

/* ============================================================================
 * File Pattern Replacement
 * ============================================================================ */

/**
 * Pattern for string replacement in transform pipeline.
 */
typedef struct {
    const char* needle;
    size_t needle_len;
    const char* replacement;
    size_t replacement_len;
} qcontrol_file_pattern_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct qcontrol_file_ctx qcontrol_file_ctx_t;
typedef struct qcontrol_file_rw_config qcontrol_file_rw_config_t;
typedef struct qcontrol_file_session qcontrol_file_session_t;
typedef struct qcontrol_file_action qcontrol_file_action_t;
typedef struct qcontrol_file_op_action qcontrol_file_op_action_t;

/* ============================================================================
 * File Policy Operation Types
 * ============================================================================ */

/**
 * Coarse policy class used to map filesystem operations onto OpenShell-style
 * read-only versus read-write policy buckets.
 */
typedef enum {
    /** Operation only reads path metadata or directory structure */
    QCONTROL_FILE_POLICY_CLASS_READ = 0,
    /** Operation mutates path state or permissions */
    QCONTROL_FILE_POLICY_CLASS_WRITE = 1,
} qcontrol_file_policy_class_t;

/**
 * Path-oriented filesystem operations that do not fit the stream session model.
 */
typedef enum {
    QCONTROL_FILE_OP_ACCESS = 0,
    QCONTROL_FILE_OP_STAT = 1,
    QCONTROL_FILE_OP_READLINK = 2,
    QCONTROL_FILE_OP_OPENDIR = 3,
    QCONTROL_FILE_OP_RENAME = 4,
    QCONTROL_FILE_OP_UNLINK = 5,
    QCONTROL_FILE_OP_MKDIR = 6,
    QCONTROL_FILE_OP_RMDIR = 7,
    QCONTROL_FILE_OP_LINK = 8,
    QCONTROL_FILE_OP_SYMLINK = 9,
    QCONTROL_FILE_OP_CHMOD = 10,
    QCONTROL_FILE_OP_CHOWN = 11,
} qcontrol_file_op_kind_t;

/**
 * Action types returned by generic path-operation callbacks.
 */
typedef enum {
    /** Allow the filesystem operation to continue */
    QCONTROL_FILE_OP_ACTION_PASS = 0,
    /** Block the filesystem operation with EACCES */
    QCONTROL_FILE_OP_ACTION_BLOCK = 1,
    /** Block the filesystem operation with a specific errno */
    QCONTROL_FILE_OP_ACTION_BLOCK_ERRNO = 2,
} qcontrol_file_op_action_type_t;

/* ============================================================================
 * File Transform Function Types
 * ============================================================================ */

/**
 * Transform function - called during read/write to modify buffer.
 *
 * @param state Plugin-defined state (from session)
 * @param ctx File context (fd, path, flags)
 * @param buf Buffer containing data to transform
 * @return Action indicating whether to continue or block
 */
typedef qcontrol_file_action_t (*qcontrol_file_transform_fn)(
    void* state,
    qcontrol_file_ctx_t* ctx,
    qcontrol_buffer_t* buf
);

/**
 * Dynamic prefix function - returns prefix to prepend.
 *
 * @param state Plugin-defined state
 * @param ctx File context
 * @param out_len Output parameter for prefix length
 * @return Prefix string (plugin-owned, must remain valid until close)
 */
typedef const char* (*qcontrol_file_prefix_fn)(
    void* state,
    qcontrol_file_ctx_t* ctx,
    size_t* out_len
);

/**
 * Dynamic suffix function - returns suffix to append.
 *
 * @param state Plugin-defined state
 * @param ctx File context
 * @param out_len Output parameter for suffix length
 * @return Suffix string (plugin-owned, must remain valid until close)
 */
typedef const char* (*qcontrol_file_suffix_fn)(
    void* state,
    qcontrol_file_ctx_t* ctx,
    size_t* out_len
);

/* ============================================================================
 * File Configuration Structures
 * ============================================================================ */

/**
 * Read/Write configuration for a file session.
 *
 * Transform order: prefix -> replace -> transform -> suffix
 */
struct qcontrol_file_rw_config {
    /** Static prefix to prepend (or NULL) */
    const char* prefix;
    size_t prefix_len;

    /** Static suffix to append (or NULL) */
    const char* suffix;
    size_t suffix_len;

    /** Dynamic prefix function (or NULL) */
    qcontrol_file_prefix_fn prefix_fn;

    /** Dynamic suffix function (or NULL) */
    qcontrol_file_suffix_fn suffix_fn;

    /** Pattern replacements array (or NULL) */
    const qcontrol_file_pattern_t* replace;
    size_t replace_count;

    /** Custom transform function (or NULL) */
    qcontrol_file_transform_fn transform;
};

/**
 * Session configuration for a file.
 * Returned from on_file_open to configure read/write behavior.
 */
struct qcontrol_file_session {
    /** Plugin-defined state (opaque, plugin owns memory) */
    void* state;

    /* === MODIFICATIONS (NULL/sentinel = no change) === */

    /** Replace file path (NULL = no change) */
    const char* set_path;

    /** Replace open flags (-1 = no change) */
    int set_flags;

    /** Replace file mode (0 = no change) */
    unsigned int set_mode;

    /* === I/O TRANSFORM CONFIGS === */

    /** Read transform config (NULL if no read transforms) */
    qcontrol_file_rw_config_t* read;

    /** Write transform config (NULL if no write transforms) */
    qcontrol_file_rw_config_t* write;
};

/**
 * Action result returned from file callbacks.
 */
struct qcontrol_file_action {
    qcontrol_file_action_type_t type;
    union {
        /** errno value for BLOCK_ERRNO */
        int errno_val;
        /** Session config for SESSION */
        qcontrol_file_session_t session;
        /** State pointer for STATE (no config, state only) */
        void* state;
    };
};

/**
 * Action result returned from generic file path-operation callbacks.
 */
struct qcontrol_file_op_action {
    qcontrol_file_op_action_type_t type;
    union {
        /** errno value for BLOCK_ERRNO */
        int errno_val;
    };
};

/* ============================================================================
 * File Action Convenience Macros
 * ============================================================================ */

/** Return PASS action (continue normally) */
#define QCONTROL_FILE_PASS \
    ((qcontrol_file_action_t){ .type = QCONTROL_FILE_ACTION_PASS })

/** Return BLOCK action (reject with EACCES) */
#define QCONTROL_FILE_BLOCK \
    ((qcontrol_file_action_t){ .type = QCONTROL_FILE_ACTION_BLOCK })

/** Return BLOCK_ERRNO action (reject with specific errno) */
#define QCONTROL_FILE_BLOCK_WITH(e) \
    ((qcontrol_file_action_t){ .type = QCONTROL_FILE_ACTION_BLOCK_ERRNO, .errno_val = (e) })

/** Return STATE action (track state, no transforms) */
#define QCONTROL_FILE_STATE(s) \
    ((qcontrol_file_action_t){ .type = QCONTROL_FILE_ACTION_STATE, .state = (s) })

/** Return PASS action for generic path operations */
#define QCONTROL_FILE_OP_PASS \
    ((qcontrol_file_op_action_t){ .type = QCONTROL_FILE_OP_ACTION_PASS })

/** Return BLOCK action for generic path operations */
#define QCONTROL_FILE_OP_BLOCK \
    ((qcontrol_file_op_action_t){ .type = QCONTROL_FILE_OP_ACTION_BLOCK })

/** Return BLOCK_ERRNO action for generic path operations */
#define QCONTROL_FILE_OP_BLOCK_WITH(e) \
    ((qcontrol_file_op_action_t){ .type = QCONTROL_FILE_OP_ACTION_BLOCK_ERRNO, .errno_val = (e) })

/* ============================================================================
 * File Event Structures
 * ============================================================================ */

/**
 * Event passed to on_file_open callback.
 */
typedef struct {
    /** Path being opened */
    const char* path;
    size_t path_len;

    /** Open flags (O_RDONLY, O_WRONLY, etc.) */
    int flags;

    /** File mode for O_CREAT */
    unsigned int mode;

    /** Result: fd on success, -errno on failure */
    int result;
} qcontrol_file_open_event_t;

/**
 * Event passed to on_file_read callback.
 */
typedef struct {
    /** File descriptor */
    int fd;

    /** Buffer containing read data (read-only for observation) */
    void* buf;

    /** Number of bytes requested */
    size_t count;

    /** Bytes actually read, or -errno on error */
    ssize_t result;
} qcontrol_file_read_event_t;

/**
 * Event passed to on_file_write callback.
 */
typedef struct {
    /** File descriptor */
    int fd;

    /** Buffer containing data to write */
    const void* buf;

    /** Number of bytes to write */
    size_t count;

    /** Bytes actually written, or -errno on error */
    ssize_t result;
} qcontrol_file_write_event_t;

/**
 * Event passed to on_file_close callback.
 */
typedef struct {
    /** File descriptor */
    int fd;

    /** Result: 0 on success, -errno on failure */
    int result;
} qcontrol_file_close_event_t;

/**
 * Event passed to on_file_op callback.
 *
 * These operations are path-oriented rather than fd-stream-oriented, so they
 * use a dedicated event family instead of the session transform model.
 */
typedef struct {
    /** Specific filesystem operation being performed */
    qcontrol_file_op_kind_t kind;

    /** Coarse policy bucket for read-only versus read-write policy mapping */
    qcontrol_file_policy_class_t policy_class;

    /** Primary path argument for the operation */
    const char* path;
    size_t path_len;

    /** Secondary path for two-path operations like rename/link/symlink */
    const char* secondary_path;
    size_t secondary_path_len;

    /** dirfd context for the primary path on *at-style operations, or -1 */
    int dirfd;

    /** AT_* flags or 0 when the operation has no flags */
    int at_flags;

    /** Result: 0 on success, -errno on failure */
    int result;

    /**
     * dirfd context for the secondary path on two-path *at-style operations,
     * or -1 when not applicable
     */
    int secondary_dirfd;
} qcontrol_file_op_event_t;

/**
 * File context passed to transform functions.
 */
struct qcontrol_file_ctx {
    /** File descriptor */
    int fd;

    /** Path (may be NULL if fd wasn't tracked from open) */
    const char* path;
    size_t path_len;

    /** Original open flags */
    int flags;
};

/* ============================================================================
 * File Callback Signatures
 * ============================================================================ */

/**
 * File open callback - determines session configuration.
 *
 * Called after open() syscall completes. Return:
 * - PASS: no interception for this file
 * - BLOCK: reject the open (close fd, return error)
 * - SESSION: intercept with read/write config
 * - STATE: track state only, no transforms
 *
 * Hosts may invoke this concurrently for independent opens/sessions.
 */
typedef qcontrol_file_action_t (*qcontrol_file_open_fn)(
    qcontrol_file_open_event_t* event
);

/**
 * File read callback - observe or block reads.
 *
 * Called after read() syscall completes. Can block but not modify.
 * Modification happens via session read config transforms.
 *
 * Hosts may invoke this concurrently for independent sessions. The same
 * session may be serialized by the host.
 */
typedef qcontrol_file_action_t (*qcontrol_file_read_fn)(
    void* state,
    qcontrol_file_read_event_t* event
);

/**
 * File write callback - observe or block writes.
 *
 * Called before write() syscall executes. Can block.
 * Modification happens via session write config transforms.
 *
 * Hosts may invoke this concurrently for independent sessions. The same
 * session may be serialized by the host.
 */
typedef qcontrol_file_action_t (*qcontrol_file_write_fn)(
    void* state,
    qcontrol_file_write_event_t* event
);

/**
 * File close callback - cleanup state.
 *
 * Called after close() syscall completes.
 * Plugin is responsible for freeing state here.
 *
 * Hosts may invoke this concurrently for independent sessions.
 */
typedef void (*qcontrol_file_close_fn)(
    void* state,
    qcontrol_file_close_event_t* event
);

/**
 * Generic path-operation callback for filesystem actions outside the stream
 * lifecycle.
 *
 * Hosts may invoke this concurrently for independent path-ops.
 */
typedef qcontrol_file_op_action_t (*qcontrol_file_op_fn)(
    qcontrol_file_op_event_t* event
);

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_FILE_H */

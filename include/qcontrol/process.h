/**
 * @file qcontrol/process.h
 * @brief Process identity types shared across qcontrol SDK surfaces
 *
 * Defines the stable process identity shape used by plugin callbacks that need
 * to attribute runtime activity back to the originating executable and its
 * ancestor chain.
 */

#ifndef QCONTROL_PROCESS_H
#define QCONTROL_PROCESS_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Process Identity Types
 * ============================================================================ */

/**
 * One ancestor executable in the attributed process lineage.
 *
 * Entries are ordered nearest ancestor first and exclude the current process,
 * which is carried directly by qcontrol_process_identity_t.
 */
typedef struct {
    /** Ancestor process ID */
    pid_t pid;

    /** Ancestor executable path */
    const char* exec_path;
    size_t exec_path_len;
} qcontrol_process_lineage_entry_t;

/**
 * Process identity attributed to one observed runtime action.
 *
 * Carries the current process executable plus its ancestor executable chain so
 * policy code can evaluate caller identity without issuing a second host query.
 */
typedef struct {
    /** Current process ID */
    pid_t pid;

    /** Current executable path */
    const char* exec_path;
    size_t exec_path_len;

    /** Ordered ancestor lineage (nearest ancestor first, may be NULL) */
    const qcontrol_process_lineage_entry_t* lineage;
    size_t lineage_len;
} qcontrol_process_identity_t;

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_PROCESS_H */

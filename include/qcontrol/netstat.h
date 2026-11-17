/**
 * @file qcontrol/netstat.h
 * @brief Netstat snapshot callback for qcontrol SDK
 *
 * A netstat snapshot is a read-only measurement of one publishing
 * interval: the process, how long the interval lasted, process-wide
 * traffic, and the listed connections. The host calls `on_netstat`
 * after it has accepted the snapshot. Pointers are valid only for that
 * call.
 *
 * `window` traffic covers the time since the previous snapshot.
 * `total` traffic covers the time since the publisher started in this
 * process. Bytes per second is window bytes times 1e9 divided by
 * `duration_ns`, or zero when `duration_ns` is zero.
 */

#ifndef QCONTROL_NETSTAT_H
#define QCONTROL_NETSTAT_H

#include <stddef.h>
#include <stdint.h>

#include "process.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TCP or UDP of a listed flow.
 */
typedef enum {
    QCONTROL_NETSTAT_PROTO_TCP = 0,
    QCONTROL_NETSTAT_PROTO_UDP = 1,
} qcontrol_netstat_protocol_t;

/**
 * Bytes and transfer failures for one interval.
 *
 * A send or receive failure is a transfer that returned an error other
 * than would-block or interrupted.
 */
typedef struct qcontrol_netstat_traffic {
    /** Bytes written on sockets in this interval. */
    uint64_t bytes_sent;
    /** Bytes read on sockets in this interval. */
    uint64_t bytes_recv;
    /** Send calls that returned a real error in this interval. */
    uint64_t send_fail;
    /** Receive calls that returned a real error in this interval. */
    uint64_t recv_fail;
} qcontrol_netstat_traffic_t;

/**
 * One listed flow in this snapshot.
 *
 * A TCP row is one socket. A connected UDP socket is one five-tuple.
 * An unconnected UDP socket is one row per remote peer that socket
 * talked to. Rows are connections open at the end of the window, plus
 * ones that closed or failed during it, ordered by bytes moved in the
 * window (largest first). A short-lived connection that opened and
 * closed inside the window appears here with its final totals.
 *
 * Addresses are presentation IP strings (`"192.0.2.1"`, `"::1"`),
 * pointer plus length, NUL-terminated. Unknown address or port is
 * NULL / 0.
 */
typedef struct qcontrol_netstat_connection {
    /** TCP or UDP. */
    qcontrol_netstat_protocol_t protocol;
    /** Local IP string, or NULL if unknown. */
    const char* local_addr;
    size_t local_addr_len;
    /** Local port, host order; 0 if unknown. */
    uint16_t local_port;
    /** Remote IP string, or NULL if unknown. */
    const char* remote_addr;
    size_t remote_addr_len;
    /** Remote port, host order; 0 if unknown. */
    uint16_t remote_port;
    /** Traffic on this flow during the window. */
    qcontrol_netstat_traffic_t window;
    /** Traffic on this flow since it was recorded. */
    qcontrol_netstat_traffic_t total;
} qcontrol_netstat_connection_t;

/**
 * One snapshot delivered to `on_netstat`.
 *
 * `connections` has `connection_count` entries. A null pointer with a
 * zero count means the publisher listed no rows. Process-wide traffic
 * includes flows that have no row.
 */
typedef struct qcontrol_netstat_snapshot {
    /**
     * Count of snapshot attempts from this publisher, starting at 1.
     * A skipped number means a snapshot was dropped.
     */
    uint64_t sequence;
    /**
     * Process whose sockets were counted. After a fork, the child
     * publishes under its own pid. Path and lineage may be empty.
     */
    qcontrol_process_identity_t process;
    /**
     * Measured length of this interval, in nanoseconds. Bytes-per-second
     * uses this as the divisor.
     */
    uint64_t duration_ns;
    /** Process-wide traffic during this interval. */
    qcontrol_netstat_traffic_t window;
    /** Process-wide traffic since the publisher started. */
    qcontrol_netstat_traffic_t total;
    /**
     * Listed connection rows. Null with `connection_count` 0 means
     * nothing listed. Valid only for the callback.
     */
    const qcontrol_netstat_connection_t* connections;
    /** Number of entries in `connections`. */
    size_t connection_count;
} qcontrol_netstat_snapshot_t;

/**
 * Observe one snapshot.
 *
 * The host calls this after it has accepted the snapshot. Pointers are
 * invalid after the callback returns.
 */
typedef void (*qcontrol_netstat_fn)(const qcontrol_netstat_snapshot_t* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_NETSTAT_H */

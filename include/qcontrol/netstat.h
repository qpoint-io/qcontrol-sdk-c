/**
 * @file qcontrol/netstat.h
 * @brief Netstat event callback for qcontrol SDK
 *
 * A netstat plugin observes what a process's sockets did, as a stream of
 * events delivered in batches to `on_netstat`. The events mean the
 * same on every operating system, and the plugin builds whatever view it
 * wants from them. Pointers are valid only for the call.
 */

#ifndef QCONTROL_NETSTAT_H
#define QCONTROL_NETSTAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TCP or UDP.
 */
typedef enum {
    QCONTROL_NETSTAT_PROTO_TCP = 0,
    QCONTROL_NETSTAT_PROTO_UDP = 1,
} qcontrol_netstat_protocol_t;

/**
 * Direction of a flow's traffic.
 */
typedef enum {
    /** Bytes the process sent. */
    QCONTROL_NETSTAT_DIR_SEND = 1,
    /** Bytes the process received. */
    QCONTROL_NETSTAT_DIR_RECV = 2,
} qcontrol_netstat_direction_t;

/*
 * Netstat events
 *
 * A stream of events says what an observed process's sockets did: created,
 * connected, sent, received, closed. The events mean the same thing on every
 * operating system. A plugin that sets `on_netstat` receives them in batches
 * and builds whatever view it wants from them.
 *
 * Terms:
 * - A socket instance is one socket from its creation until it is gone. Its
 *   `instance` number is never given to another socket, even when the
 *   operating system reuses the descriptor number or the process id. A
 *   socket the program connects again to somewhere new is a new instance.
 * - A name is a number the program uses for a socket: a file descriptor, or
 *   a handle on Windows. A socket can have several names (after `dup`, for
 *   example) and stays open until its last name is closed.
 *
 * Guarantees:
 * - Delivery timing is arbitrary. When a batch is handed over, and how
 *   many events it holds, may change from run to run. A plugin must not
 *   rely on it; batch boundaries carry no meaning.
 * - Within one pid, events arrive in the order they happened, except that a
 *   `LATE` transfer arrives after the stretch it belongs to was reported
 *   unknown.
 * - A `WATERMARK` at time T promises that no event for that pid at or
 *   before T follows, except `LATE` ones. Watermarks never go backward. A
 *   plugin that waits for the watermark before judging a stretch of time
 *   never mistakes "not delivered yet" for "nothing happened".
 * - `PROCESS_ENDED` is a pid's last event.
 * - Times are nanoseconds of a monotonic clock, comparable across every
 *   event a plugin receives.
 * - `instance` is meaningful only for socket events.
 * - Each type uses only the fields named beside it below; every other byte
 *   of the record is zero.
 */

/** What a netstat event says. */
typedef enum {
    /** The program created a TCP or UDP socket: `body.opened`. */
    QCONTROL_NETSTAT_EVENT_OPENED = 1,
    /** A connection is being made: `body.connect` (peer when HAS_PEER). */
    QCONTROL_NETSTAT_EVENT_CONNECTING = 2,
    /** The socket is connected: `body.connect` (peer when HAS_PEER). */
    QCONTROL_NETSTAT_EVENT_CONNECTED = 3,
    /** The connection attempt failed: `body.connect` (peer when HAS_PEER). */
    QCONTROL_NETSTAT_EVENT_CONNECT_FAILED = 4,
    /** The attempt ended with no connection and no failure the program saw. */
    QCONTROL_NETSTAT_EVENT_CONNECT_ABANDONED = 5,
    /** The socket's own address and port, once known (the operating system
     *  usually assigns them during a connect): `body.endpoint`. */
    QCONTROL_NETSTAT_EVENT_LOCAL_ENDPOINT = 6,
    /** The other side's address and port, once known when it was not given
     *  at connect (an accepted or already-open socket): `body.endpoint`. */
    QCONTROL_NETSTAT_EVENT_PEER_ENDPOINT = 7,
    /** Bytes moved: `body.transfer` (peer when HAS_PEER). */
    QCONTROL_NETSTAT_EVENT_TRANSFERRED = 8,
    /** A send or receive failed: `body.transfer` with zero bytes. */
    QCONTROL_NETSTAT_EVENT_TRANSFER_FAILED = 9,
    /** The program closed the socket's last name. A send or receive started
     *  before the close can still complete and be reported before `ENDED`. */
    QCONTROL_NETSTAT_EVENT_NAMES_CLOSED = 10,
    /** The socket instance is gone and no event about it follows:
     *  `body.ended`. */
    QCONTROL_NETSTAT_EVENT_ENDED = 11,
    /** Observation of the pid began; its first event. */
    QCONTROL_NETSTAT_EVENT_PROCESS_ARMED = 12,
    /** The pid exited or stopped being observed; its last event. Every socket
     *  instance still open ends with it. */
    QCONTROL_NETSTAT_EVENT_PROCESS_ENDED = 13,
    /** No event for the pid at or before `at_ns` follows: `body.watermark`. */
    QCONTROL_NETSTAT_EVENT_WATERMARK = 14,
    /** Nothing is known about the pid for a stretch of time, because reports
     *  were lost: `body.unknown`. */
    QCONTROL_NETSTAT_EVENT_UNKNOWN = 15,
    /** Running totals of the pid's reports: submitted, and dropped for each
     *  reason before qcontrol received them: `body.losses`. */
    QCONTROL_NETSTAT_EVENT_LOSSES = 16,
    /** Observation of the pid stopped. */
    QCONTROL_NETSTAT_EVENT_STOPPED = 17,
} qcontrol_netstat_event_type_t;

/** A normalized address family. */
typedef enum {
    /** No family, or one that is not an IP family. */
    QCONTROL_NETSTAT_ADDR_UNSPEC = 0,
    /** IPv4. */
    QCONTROL_NETSTAT_ADDR_IPV4 = 1,
    /** IPv6. */
    QCONTROL_NETSTAT_ADDR_IPV6 = 2,
} qcontrol_netstat_addr_family_t;

/** Why an instance ended. */
typedef enum {
    /** The program closed it, and no send, receive, or connect still in flight
     *  refers to it. */
    QCONTROL_NETSTAT_INSTANCE_CLOSED = 0,
    /** The program reused the socket for a new connection, which continues as
     *  a new instance. */
    QCONTROL_NETSTAT_INSTANCE_REPLACED = 1,
} qcontrol_netstat_instance_end_t;

/** Why a stretch is unknown. */
typedef enum {
    /** Reports of the pid's socket activity were lost before qcontrol
     *  received them. */
    QCONTROL_NETSTAT_UNKNOWN_RECORDS_LOST = 0,
} qcontrol_netstat_unknown_cause_t;

/*
 * Bits of qcontrol_netstat_event_t.flags.
 */
/** Connect and transfer events: `peer` is set. */
#define QCONTROL_NETSTAT_EVENT_HAS_PEER (1u << 0)
/** TRANSFERRED: arrived after its stretch of time was reported unknown. */
#define QCONTROL_NETSTAT_EVENT_LATE (1u << 1)
/** TRANSFERRED: completed through a name the program had already closed. */
#define QCONTROL_NETSTAT_EVENT_NAME_NOT_OPEN (1u << 2)
/** OPENED: the socket was already open when observation began, so its
     *  creation was not seen. */
#define QCONTROL_NETSTAT_EVENT_PREEXISTING (1u << 3)

/**
 * An endpoint: 20 bytes. IPv4 uses the first 4 address bytes in network
 * order; IPv6 uses all 16. `port` is in host byte order.
 */
typedef struct qcontrol_netstat_endpoint {
    /** A `qcontrol_netstat_addr_family_t`. */
    uint8_t family;
    uint8_t reserved;
    uint16_t port;
    uint8_t addr[16];
} qcontrol_netstat_endpoint_t;

/**
 * One event: 64 bytes, fixed. The fields before `body` are common to every
 * type; `body` holds the fields of the type named by `type`.
 */
typedef struct qcontrol_netstat_event {
    /** A `qcontrol_netstat_event_type_t`. */
    uint16_t type;
    /** `QCONTROL_NETSTAT_EVENT_*` flag bits. */
    uint16_t flags;
    /** The pid of the observed process the event is about. */
    uint32_t pid;
    /** When the event happened, in monotonic nanoseconds. */
    uint64_t at_ns;
    /** Socket events only; meaningless for process and stream events. */
    uint64_t instance;
    union {
        /** OPENED. */
        struct {
            /** A `qcontrol_netstat_protocol_t`. */
            uint8_t protocol;
            /** A `qcontrol_netstat_addr_family_t`. */
            uint8_t family;
        } opened;
        /** CONNECTING, CONNECTED, CONNECT_FAILED. */
        struct {
            qcontrol_netstat_endpoint_t peer;
        } connect;
        /** LOCAL_ENDPOINT, PEER_ENDPOINT. */
        struct {
            qcontrol_netstat_endpoint_t endpoint;
        } endpoint;
        /** TRANSFERRED, TRANSFER_FAILED (bytes 0). */
        struct {
            uint64_t bytes;
            /** A `qcontrol_netstat_direction_t`. */
            uint8_t direction;
            uint8_t reserved[3];
            qcontrol_netstat_endpoint_t peer;
        } transfer;
        /** ENDED. */
        struct {
            /** A `qcontrol_netstat_instance_end_t`. */
            uint8_t reason;
        } ended;
        /** WATERMARK. */
        struct {
            /** Wall-clock time of the instant `at_ns` names, in nanoseconds
             *  since 1970-01-01 UTC. */
            uint64_t wall_ns;
        } watermark;
        /** UNKNOWN. */
        struct {
            uint64_t from_ns;
            uint64_t to_ns;
            /** A `qcontrol_netstat_unknown_cause_t`. */
            uint8_t cause;
        } unknown;
        /** LOSSES: running totals of the pid's reports, submitted and dropped. */
        struct {
            uint64_t submitted;
            uint64_t ring_full;
            uint64_t too_large;
            uint64_t payload_pool_exhausted;
            uint64_t invalid;
        } losses;
    } body;
} qcontrol_netstat_event_t;

/**
 * Receives one batch of events, in order (see "Guarantees" above).
 *
 * `events` points at `count` records, valid only during the call; `count`
 * is never zero. The plugin observes and cannot change the program's
 * behavior through this callback.
 */
typedef void (*qcontrol_netstat_fn)(const qcontrol_netstat_event_t* events, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* QCONTROL_NETSTAT_H */

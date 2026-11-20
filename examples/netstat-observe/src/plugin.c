/**
 * @file plugin.c
 * @brief Example plugin that registers the netstat event callback.
 *
 * The callback counts the events of each type in a batch and prints one line
 * per batch. Batch boundaries carry no meaning, so a plugin that wants a
 * measurement aggregates across batches. The batch is valid only for this
 * call.
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <stdio.h>
#include <string.h>

/** Size of the per-type tables: one slot per type number, the highest being STOPPED. */
#define EVENT_TYPE_COUNT (QCONTROL_NETSTAT_EVENT_STOPPED + 1)

/** Name printed for each event type, indexed by type number. */
static const char* const EVENT_NAMES[EVENT_TYPE_COUNT] = {
    [QCONTROL_NETSTAT_EVENT_OPENED] = "opened",
    [QCONTROL_NETSTAT_EVENT_CONNECTING] = "connecting",
    [QCONTROL_NETSTAT_EVENT_CONNECTED] = "connected",
    [QCONTROL_NETSTAT_EVENT_CONNECT_FAILED] = "connect_failed",
    [QCONTROL_NETSTAT_EVENT_CONNECT_ABANDONED] = "connect_abandoned",
    [QCONTROL_NETSTAT_EVENT_LOCAL_ENDPOINT] = "local_endpoint",
    [QCONTROL_NETSTAT_EVENT_PEER_ENDPOINT] = "peer_endpoint",
    [QCONTROL_NETSTAT_EVENT_TRANSFERRED] = "transferred",
    [QCONTROL_NETSTAT_EVENT_TRANSFER_FAILED] = "transfer_failed",
    [QCONTROL_NETSTAT_EVENT_NAMES_CLOSED] = "names_closed",
    [QCONTROL_NETSTAT_EVENT_ENDED] = "ended",
    [QCONTROL_NETSTAT_EVENT_PROCESS_ARMED] = "process_armed",
    [QCONTROL_NETSTAT_EVENT_PROCESS_ENDED] = "process_ended",
    [QCONTROL_NETSTAT_EVENT_WATERMARK] = "watermark",
    [QCONTROL_NETSTAT_EVENT_UNKNOWN] = "unknown",
    [QCONTROL_NETSTAT_EVENT_LOSSES] = "losses",
    [QCONTROL_NETSTAT_EVENT_STOPPED] = "stopped",
};

static void on_netstat(const qcontrol_netstat_event_t* events, size_t count) {
    size_t counts[EVENT_TYPE_COUNT];
    memset(counts, 0, sizeof(counts));
    for (size_t index = 0; index < count; index++) {
        if (events[index].type < EVENT_TYPE_COUNT) {
            counts[events[index].type]++;
        }
    }
    fprintf(stderr, "netstat events=%zu", count);
    for (size_t type = 0; type < EVENT_TYPE_COUNT; type++) {
        if (counts[type] != 0) {
            fprintf(stderr, " %s=%zu", EVENT_NAMES[type], counts[type]);
        }
    }
    fputc('\n', stderr);
}

QCONTROL_PLUGIN("netstat-observe", .on_netstat = on_netstat, );

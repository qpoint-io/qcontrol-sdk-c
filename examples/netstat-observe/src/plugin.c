/**
 * @file plugin.c
 * @brief Example plugin that registers the netstat snapshot callback.
 *
 * The callback prints sequence, process pid, duration, and connection
 * count. Pointers are valid only for this call.
 */

#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <stdio.h>

static void on_netstat(const qcontrol_netstat_snapshot_t* snapshot) {
    if (snapshot == NULL) {
        return;
    }
    fprintf(stderr, "netstat sequence=%llu pid=%ld duration_ns=%llu connections=%zu\n",
            (unsigned long long)snapshot->sequence, (long)snapshot->process.pid,
            (unsigned long long)snapshot->duration_ns, snapshot->connection_count);
}

QCONTROL_PLUGIN("netstat-observe", .on_netstat = on_netstat, );

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <time.h>

/* Central state, shared between the network side and the D-Bus side.
 * Single-threaded (driven by sd-event), so no locking needed. If you ever
 * add worker threads, protect this with a mutex and note that in your
 * threat model / design doc. */
struct telemetry_state {
    int      listen_fd;          /* listening socket                     */
    uint32_t active_connections; /* exposed as a D-Bus property          */
    uint64_t messages_total;     /* exposed via GetStatus                */
    time_t   started_at;
    void    *tls_ctx;            /* opaque TLS context (see tls.c)       */
};

/* Called when a message is received, so counters stay in one place. */
void telemetry_record_message(struct telemetry_state *st, size_t bytes);

#endif /* TELEMETRY_H */

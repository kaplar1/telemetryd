#ifndef NET_H
#define NET_H

#include <systemd/sd-event.h>
#include "telemetry.h"

/* Set up the listening socket and register it with the event loop.
 * Prefers a socket-activated fd from systemd (SD_LISTEN_FDS_START); falls
 * back to binding TELEMETRYD_PORT itself when run outside systemd. */
int  net_listen_init(sd_event *event, struct telemetry_state *st);
void net_listen_cleanup(struct telemetry_state *st);

#define TELEMETRYD_PORT 9099

#endif /* NET_H */

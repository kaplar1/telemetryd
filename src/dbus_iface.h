#ifndef DBUS_IFACE_H
#define DBUS_IFACE_H

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include "telemetry.h"

/* Publishes com.example.Telemetry on the system bus and attaches the bus to
 * the shared event loop. */
int dbus_iface_init(sd_bus *bus, sd_event *event, struct telemetry_state *st);

#define TELEMETRY_BUS_NAME  "com.example.Telemetry"
#define TELEMETRY_OBJ_PATH  "/com/example/Telemetry"
#define TELEMETRY_IFACE     "com.example.Telemetry1"

#endif /* DBUS_IFACE_H */

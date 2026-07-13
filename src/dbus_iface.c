#include <stdio.h>
#include <string.h>

#include "dbus_iface.h"

/* --- Method: GetStatus() -> (u active, t messages) ---------------------- */
static int method_get_status(sd_bus_message *m, void *userdata, sd_bus_error *err) {
    (void)err;
    struct telemetry_state *st = userdata;
    return sd_bus_reply_method_return(m, "ut",
                                      st->active_connections,
                                      st->messages_total);
}

/* --- Method: Reset() -> () ---------------------------------------------- */
static int method_reset(sd_bus_message *m, void *userdata, sd_bus_error *err) {
    (void)err;
    struct telemetry_state *st = userdata;
    st->messages_total = 0;
    /* TODO: emit the CountersReset signal here with sd_bus_emit_signal(). */
    return sd_bus_reply_method_return(m, "");
}

/* --- Property: ActiveConnections (read-only u) -------------------------- */
static int prop_active(sd_bus *bus, const char *path, const char *iface,
                       const char *property, sd_bus_message *reply,
                       void *userdata, sd_bus_error *err) {
    (void)bus; (void)path; (void)iface; (void)property; (void)err;
    struct telemetry_state *st = userdata;
    return sd_bus_message_append(reply, "u", st->active_connections);
}

/* The interface definition. This vtable is the single source of truth that
 * `busctl introspect com.example.Telemetry /com/example/Telemetry` will show. */
static const sd_bus_vtable telemetry_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetStatus", "", "ut", method_get_status, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Reset",     "", "",   method_reset,      SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("ActiveConnections", "u", prop_active, 0,
                    SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_SIGNAL("CountersReset", "", 0),
    SD_BUS_VTABLE_END
};

int dbus_iface_init(sd_bus *bus, sd_event *event, struct telemetry_state *st) {
    int r = sd_bus_add_object_vtable(bus, NULL,
                                     TELEMETRY_OBJ_PATH, TELEMETRY_IFACE,
                                     telemetry_vtable, st);
    if (r < 0) return r;

    r = sd_bus_request_name(bus, TELEMETRY_BUS_NAME, 0);
    if (r < 0) return r;

    /* Let the shared sd-event loop drive the bus (no separate thread). */
    return sd_bus_attach_event(bus, event, SD_EVENT_PRIORITY_NORMAL);
}

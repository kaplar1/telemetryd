/*
 * telemetryd - a small secure network telemetry daemon.
 *
 *   - C in Linux user space
 *   - a TCP/IP network service (net.c) with TLS (tls.c)
 *   - systemd integration: socket activation + sd_notify readiness + journald
 *   - a D-Bus control interface via sd-bus (dbus_iface.c)
 *
 * This is a SCAFFOLD. Functions with `TODO` are where you add the real logic
 * during the two-week plan. It is structured to compile once the TODOs and
 * libraries are wired up; treat compiler errors as your worklist.
 */

#define _POSIX_C_SOURCE 200809L /* sigemptyset/sigaddset/sigprocmask/SIG_BLOCK under -std=c11 */

#include <errno.h>
#include <signal.h>   /* sigset_t, sigemptyset/sigaddset, sigprocmask, SIG_BLOCK */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <systemd/sd-daemon.h>   /* sd_notify, sd_listen_fds */
#include <systemd/sd-event.h>    /* event loop            */
#include <systemd/sd-bus.h>      /* D-Bus                 */

#include "net.h"
#include "dbus_iface.h"
#include "telemetry.h"

static int on_signal(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
    (void)userdata;
    sd_event *e = sd_event_source_get_event(s);
    fprintf(stderr, "telemetryd: caught signal %u, shutting down\n", si->ssi_signo);
    sd_event_exit(e, 0);
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    sd_event *event = NULL;
    sd_bus   *bus   = NULL;
    struct telemetry_state state = {0};
    int r;

    /* 1. Event loop -------------------------------------------------------- */
    r = sd_event_default(&event);
    if (r < 0) { fprintf(stderr, "sd_event_default: %s\n", strerror(-r)); goto out; }

    /* Clean shutdown on SIGTERM/SIGINT (systemd sends SIGTERM on stop). */
    sigset_t ss;
    sigemptyset(&ss); sigaddset(&ss, SIGTERM); sigaddset(&ss, SIGINT);
    sigprocmask(SIG_BLOCK, &ss, NULL);
    sd_event_add_signal(event, NULL, SIGTERM, on_signal, NULL);
    sd_event_add_signal(event, NULL, SIGINT,  on_signal, NULL);

    /* 2. Network listener (socket-activated if systemd passed us an fd) ---- */
    r = net_listen_init(event, &state);   /* see net.c */
    if (r < 0) { fprintf(stderr, "net_listen_init: %s\n", strerror(-r)); goto out; }

    /* 3. D-Bus control interface ------------------------------------------ */
    r = sd_bus_open_system(&bus);
    if (r < 0) { fprintf(stderr, "sd_bus_open_system: %s\n", strerror(-r)); goto out; }
    r = dbus_iface_init(bus, event, &state);   /* see dbus_iface.c */
    if (r < 0) { fprintf(stderr, "dbus_iface_init: %s\n", strerror(-r)); goto out; }

    /* 4. Tell systemd we are ready (Type=notify in the .service) ---------- */
    sd_notify(0, "READY=1\n"
                 "STATUS=Listening for telemetry connections\n");

    /* 5. Run -------------------------------------------------------------- */
    r = sd_event_loop(event);

out:
    sd_notify(0, "STOPPING=1\n");
    bus   = sd_bus_flush_close_unref(bus);
    event = sd_event_unref(event);
    net_listen_cleanup(&state);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

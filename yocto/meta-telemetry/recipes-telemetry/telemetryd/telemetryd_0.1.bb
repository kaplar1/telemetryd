SUMMARY = "Secure network telemetry daemon"
DESCRIPTION = "A small C daemon: TCP/TLS service, systemd socket activation, \
D-Bus control interface. Demo project for embedded Linux network work."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# The actual sources live at the telemetryd repo root, four directories up
# from this recipe (yocto/meta-telemetry/recipes-telemetry/telemetryd/ ->
# repo root). Point bitbake's file:// search there instead of duplicating
# src/, Makefile, systemd/, dbus/ into this layer.
FILESEXTRAPATHS:prepend := "${THISDIR}/../../../../:"

# Pull the build tree in. For real work you'd use SRC_URI = "git://..." with
# SRCREV; for local iteration, file:// pointing at the sources works.
SRC_URI = " \
    file://src/ \
    file://Makefile \
    file://systemd/telemetryd.service \
    file://systemd/telemetryd.socket \
    file://dbus/com.example.Telemetry.conf \
"

S = "${WORKDIR}"

# libsystemd provides sd-bus / sd-event / sd-daemon. openssl provides
# libssl/libcrypto for tls.c. Runtime RDEPENDS on the actual shared-lib
# packages is worked out automatically by OE's shlibs scanning of the
# compiled binary -- no manual RDEPENDS needed here.
DEPENDS = "systemd openssl"

# Pull in systemd handling (installs + enables units). Also inherit
# pkgconfig: a bare custom-Makefile recipe doesn't get a working
# cross pkg-config for free -- without this, `pkg-config` inside the
# Makefile's $(shell ...) calls is either missing entirely or resolves
# against the wrong sysroot, silently producing empty output (no error)
# instead of `-lsystemd`, which is what caused the "undefined reference
# to sd_bus_*/sd_event_*" link failures.
# Also inherit useradd: telemetryd.service runs as a fixed system user
# (not DynamicUser -- confirmed by on-target testing that DynamicUser's
# ephemeral UID breaks the D-Bus RequestName handshake, while a fixed
# user works). useradd.bbclass creates this user at image-build/rootfs
# time, so it exists on first boot without any manual `useradd` step.
inherit systemd pkgconfig useradd

USERADD_PACKAGES = "${PN}"
USERADD_PARAM:${PN} = "--system --no-create-home --shell /sbin/nologin --user-group telemetryd"

SYSTEMD_SERVICE:${PN} = "telemetryd.socket telemetryd.service"
SYSTEMD_AUTO_ENABLE = "enable"

# This daemon needs systemd in the image.
REQUIRED_DISTRO_FEATURES = "systemd"

# No LDLIBS here: bitbake doesn't define that variable itself (unlike
# CC/CFLAGS), so 'LDLIBS=${LDLIBS}' used to reach make as a literal
# unexpanded string -- and make's own ${...} syntax then read it as
# LDLIBS defined in terms of itself, i.e. "Recursive variable 'LDLIBS'
# references itself". Not needed anyway: the Makefile computes LDLIBS
# itself via pkg-config, which already resolves against this recipe's
# sysroot because of DEPENDS = "systemd" above.
EXTRA_OEMAKE = "'CC=${CC}' 'CFLAGS=${CFLAGS}'"

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/telemetryd ${D}${bindir}/telemetryd

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/systemd/telemetryd.service ${D}${systemd_system_unitdir}/
    install -m 0644 ${S}/systemd/telemetryd.socket  ${D}${systemd_system_unitdir}/

    install -d ${D}${sysconfdir}/dbus-1/system.d
    install -m 0644 ${S}/dbus/com.example.Telemetry.conf \
        ${D}${sysconfdir}/dbus-1/system.d/
}

FILES:${PN} += "${systemd_system_unitdir} ${sysconfdir}/dbus-1/system.d"

# /etc/telemetryd/tls/{server.crt,server.key,ca.crt} are NOT provisioned by
# this recipe -- see telemetryd-devcerts for that (a separate, explicitly
# dev-only recipe: baking real TLS key material into the image identically
# on every build isn't something to shortcut silently here). telemetryd
# fails closed with a clear log message if the files aren't present on
# target (see net_listen_init() in net.c) regardless of which provisioning
# path supplies them. Flag the dev-cert shortcut in THREAT_MODEL.md
# alongside the devkeys/wifi ones (Stage F).

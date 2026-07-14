SUMMARY = "Secure network telemetry daemon"
DESCRIPTION = "A small C daemon: TCP/TLS service, systemd socket activation, \
D-Bus control interface. Demo project for embedded Linux network work."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
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

# libsystemd provides sd-bus / sd-event / sd-daemon. Add openssl once tls.c
# is implemented.
DEPENDS = "systemd"
# DEPENDS += "openssl"

# Pull in systemd handling (installs + enables units).
inherit systemd

SYSTEMD_SERVICE:${PN} = "telemetryd.socket telemetryd.service"
SYSTEMD_AUTO_ENABLE = "enable"

# This daemon needs systemd in the image.
REQUIRED_DISTRO_FEATURES = "systemd"

EXTRA_OEMAKE = "'CC=${CC}' 'CFLAGS=${CFLAGS}' 'LDLIBS=${LDLIBS}'"

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

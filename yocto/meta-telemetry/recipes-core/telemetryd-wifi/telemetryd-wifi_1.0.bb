SUMMARY = "Dev-only: bake in WiFi so the Pi gets an IP without Ethernet"
DESCRIPTION = "Static wpa_supplicant config + systemd-networkd DHCP config \
for wlan0, so a freshly-flashed image associates to a known AP and gets \
an address on boot -- same convenience rationale as telemetryd-devkeys. \
The PRD prefers wired Ethernet for stable Wireshark captures during \
testing; this is for general reachability/dev convenience, not a \
replacement for that. Baking a plaintext PSK into the image is another \
dev-only shortcut worth a line in THREAT_MODEL.md alongside the SSH key."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://wpa_supplicant-wlan0.conf \
    file://25-wlan0.network \
"

S = "${WORKDIR}"

RDEPENDS:${PN} = "wpa-supplicant"

do_install() {
    install -d ${D}${sysconfdir}/wpa_supplicant
    install -m 0600 ${WORKDIR}/wpa_supplicant-wlan0.conf \
        ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf

    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${WORKDIR}/25-wlan0.network \
        ${D}${sysconfdir}/systemd/network/25-wlan0.network
}

FILES:${PN} += " \
    ${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf \
    ${sysconfdir}/systemd/network/25-wlan0.network \
"

# Enabling wpa_supplicant@wlan0.service itself happens in a .bbappend on
# the wpa-supplicant recipe (recipes-connectivity/wpa-supplicant/), not
# here -- bitbake's systemd class only looks for a SYSTEMD_SERVICE unit
# among the files the *same* recipe installs, and wpa_supplicant@.service
# is shipped by wpa-supplicant, not by this recipe.

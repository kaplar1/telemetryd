# Enable the wlan0 instance of the wpa_supplicant@.service template unit
# that wpa-supplicant already ships. This is the recipe that actually
# packages wpa_supplicant@.service, so this is where SYSTEMD_SERVICE has
# to be extended -- see the comment in telemetryd-wifi_1.0.bb.
SYSTEMD_SERVICE:${PN} += "wpa_supplicant@wlan0.service"

# Upstream wpa-supplicant_2.10.bb sets SYSTEMD_AUTO_ENABLE = "disable",
# which presets *every* unit in SYSTEMD_SERVICE to disabled -- including
# the wlan0 instance added above. Override it so the image boots with the
# unit enabled. Side effect: the plain dbus-activated wpa_supplicant.service
# gets enabled too; with systemd-networkd as net manager it just idles.
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

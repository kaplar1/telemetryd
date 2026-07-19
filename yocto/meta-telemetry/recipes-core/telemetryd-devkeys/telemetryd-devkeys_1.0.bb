SUMMARY = "Dev-only: bake the workstation's SSH public key into root's authorized_keys"
DESCRIPTION = "Convenience recipe so freshly-flashed images are reachable over \
SSH immediately, without a console/useradd workaround. NOT a production \
pattern: baking a specific dev machine's key (and open root login) into \
an image is fine for this interview-prep project's own iteration loop, but \
would need revisiting (per-device provisioning, no root login, etc.) for \
anything real. Worth a line in THREAT_MODEL.md."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://authorized_keys"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${ROOT_HOME}/.ssh
    install -m 0600 ${WORKDIR}/authorized_keys ${D}${ROOT_HOME}/.ssh/authorized_keys
}

FILES:${PN} += "${ROOT_HOME}/.ssh/authorized_keys"

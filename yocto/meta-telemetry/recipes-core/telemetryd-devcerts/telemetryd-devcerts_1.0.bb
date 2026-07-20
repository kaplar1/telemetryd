SUMMARY = "Dev-only: bake a self-signed TLS cert/key/CA for telemetryd into the image"
DESCRIPTION = "Convenience recipe so freshly-flashed dev images have a working \
telemetryd TLS listener without a manual on-target provisioning step. NOT a \
production pattern: every image built from this layer gets an identical \
CA/server keypair baked in, and the private key ships world-readable in the \
rootfs (see do_install below). Fine for this interview-prep project's own \
iteration loop -- same posture as telemetryd-devkeys (baked SSH key) and \
telemetryd-wifi (baked PSK) -- but would need real per-device provisioning \
(or short-lived certs from an actual CA) for anything real. Flagged in \
THREAT_MODEL.md alongside those."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = "openssl-native"

S = "${WORKDIR}"

# Same recipe as scripts/gen_test_certs.sh in the telemetryd repo root, just
# run at build time instead of by hand. ECDSA P-256 to match tls.c's
# ECDHE-only cipher list. Long-dated (10y) since these aren't meant to be
# rotated -- they're a dev shortcut, not a real PKI.
do_compile() {
    openssl ecparam -name prime256v1 -genkey -noout -out ca.key
    openssl req -x509 -new -key ca.key -days 3650 -out ca.crt \
        -subj "/O=telemetryd-dev/CN=telemetryd-dev-CA"

    openssl ecparam -name prime256v1 -genkey -noout -out server.key
    openssl req -new -key server.key -out server.csr \
        -subj "/O=telemetryd-dev/CN=telemetryd"
    # bitbake's shell-function parser doesn't support process substitution
    # (<(...)), unlike scripts/gen_test_certs.sh which this otherwise
    # mirrors -- use a real temp file for the SAN extension instead.
    printf "subjectAltName=DNS:telemetryd,DNS:localhost,IP:127.0.0.1" > server.ext
    openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
        -days 3650 -out server.crt -extfile server.ext
}

# server.key is installed 0644 (world-readable), not owned by/restricted to
# the telemetryd group: the telemetryd system user is created by
# telemetryd_0.1.bb's useradd.bbclass, and there's no reliable ordering
# between two separate recipes' do_install to chown against a group that
# might not exist yet. World-readable is already consistent with this
# recipe's dev-only posture (a cert/key pair baked identically into every
# image isn't meaningfully more "secret" for being group-restricted).
do_install() {
    install -d ${D}${sysconfdir}/telemetryd/tls
    install -m 0644 ${WORKDIR}/ca.crt     ${D}${sysconfdir}/telemetryd/tls/ca.crt
    install -m 0644 ${WORKDIR}/server.crt ${D}${sysconfdir}/telemetryd/tls/server.crt
    install -m 0644 ${WORKDIR}/server.key ${D}${sysconfdir}/telemetryd/tls/server.key
}

FILES:${PN} += "${sysconfdir}/telemetryd/tls"

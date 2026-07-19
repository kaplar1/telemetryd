#!/usr/bin/env bash
# Generate a throwaway CA + server + client cert/key set for LOCAL TESTING
# of telemetryd's TLS support only. Not for any real deployment: the CA
# private key sits unencrypted on disk right next to everything it signed.
#
# Usage: scripts/gen_test_certs.sh [output-dir]   (default: ./certs)
set -euo pipefail

OUT="${1:-certs}"
DAYS=365

mkdir -p "$OUT"
cd "$OUT"

echo "== dev CA =="
openssl ecparam -name prime256v1 -genkey -noout -out ca.key
openssl req -x509 -new -key ca.key -days "$DAYS" -out ca.crt \
    -subj "/O=telemetryd-dev/CN=telemetryd-dev-CA"

echo "== server cert (CN=localhost, SAN=localhost/127.0.0.1) =="
openssl ecparam -name prime256v1 -genkey -noout -out server.key
openssl req -new -key server.key -out server.csr \
    -subj "/O=telemetryd-dev/CN=localhost"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -days "$DAYS" -out server.crt \
    -extfile <(printf "subjectAltName=DNS:localhost,IP:127.0.0.1")

echo "== client cert (CN=test-client) =="
openssl ecparam -name prime256v1 -genkey -noout -out client.key
openssl req -new -key client.key -out client.csr \
    -subj "/O=telemetryd-dev/CN=test-client"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -days "$DAYS" -out client.crt

rm -f server.csr client.csr ca.srl
chmod 600 ca.key server.key client.key

echo
echo "Done. Files in $OUT/:"
ls -1 .
echo
echo "Run the server against these with:"
echo "  TELEMETRYD_TLS_CERT=$OUT/server.crt \\"
echo "  TELEMETRYD_TLS_KEY=$OUT/server.key \\"
echo "  TELEMETRYD_TLS_CA=$OUT/ca.crt \\"
echo "  ./telemetryd"

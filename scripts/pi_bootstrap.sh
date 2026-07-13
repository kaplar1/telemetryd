#!/bin/sh
# Run this on the Raspberry Pi (over the VS Code Remote-SSH terminal, or
# plain ssh) to install everything the telemetryd PRD's native build path
# (Section 8) needs: build tooling, TLS/systemd dev headers, and the
# test/analysis tools for Stages A-C.
#
# Usage: chmod +x pi_bootstrap.sh && ./pi_bootstrap.sh

set -e

echo "== Updating package lists =="
sudo apt-get update

echo "== Build tooling =="
sudo apt-get install -y \
    build-essential \
    pkg-config \
    git

echo "== Library headers (systemd sd-bus/sd-event/sd-daemon, TLS) =="
sudo apt-get install -y \
    libsystemd-dev \
    libssl-dev

echo "== Test & analysis tools =="
sudo apt-get install -y \
    wireshark \
    tcpdump \
    gdb \
    valgrind \
    openssl \
    netcat-openbsd

# busctl ships with systemd, which Raspberry Pi OS already has — nothing
# extra to install there.

echo "== Optional: let your user capture packets without sudo =="
if getent group wireshark >/dev/null; then
    sudo usermod -aG wireshark "$USER" || true
    echo "Added $USER to the wireshark group — log out/in (or reboot) for it to take effect."
fi

echo
echo "Done. Versions installed:"
gcc --version | head -1
pkg-config --modversion libsystemd
openssl version
echo
echo "Next: clone the telemetryd repo here and run 'make'."

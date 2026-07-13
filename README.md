A small secure network telemetry daemon


## Directory structure

```
telemetryd/
├── README.md
├── Makefile                     # native dev build (pkg-config libsystemd)
├── src/
│   ├── main.c                   # entry: sd-event loop, sd_notify, wiring
│   ├── net.c / net.h            # TCP listener, socket activation, accept/read
│   ├── tls.c / tls.h            # TLS wrapper (OpenSSL or mbedTLS) — you implement
│   ├── dbus_iface.c / .h        # sd-bus interface: GetStatus, Reset, property
│   └── telemetry.c / telemetry.h# shared state + counters
├── systemd/
│   ├── telemetryd.socket        # socket activation
│   └── telemetryd.service       # Type=notify + sandbox hardening
├── dbus/
│   └── com.example.Telemetry.conf   # system-bus policy
└── yocto/
    └── meta-telemetry/          # your custom Yocto layer
        ├── conf/layer.conf
        └── recipes-telemetry/telemetryd/telemetryd_0.1.bb
```

## Build order

1. **Stage A/B — native build + systemd.** On any Linux box with
   `libsystemd-dev`:
   ```
   make
   sudo cp telemetryd /usr/bin/
   sudo cp systemd/telemetryd.* /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl start telemetryd.socket
   echo hello | nc localhost 9099        # triggers socket activation
   journalctl -u telemetryd -f           # watch journald logs
   ```

2. **Stage C — D-Bus.** With the service running:
   ```
   busctl introspect com.example.Telemetry /com/example/Telemetry
   busctl call com.example.Telemetry /com/example/Telemetry \
       com.example.Telemetry1 GetStatus
   ```

3. **Stage D — Yocto.** In a poky checkout (e.g. `scarthgap`):
   ```
   bitbake-layers add-layer /path/to/telemetryd/yocto/meta-telemetry
   # add telemetryd to your image, e.g. in local.conf:
   #   IMAGE_INSTALL:append = " telemetryd"
   #   DISTRO_FEATURES:append = " systemd"
   bitbake core-image-minimal
   runqemu qemux86-64 nographic
   # inside QEMU: systemctl status telemetryd.socket
   ```

4. **Stage E — TLS.** Implement `tls.c` (OpenSSL or mbedTLS), enable it in
   `net.c`, add `openssl`/`mbedtls` to `DEPENDS` and the Makefile `PKGS`. Test:
   ```
   openssl s_client -connect localhost:9099
   ```

5. **Stage F — threat model.** Write a one-page STRIDE model for the network
   feature (spoofing → mutual TLS; tampering → TLS integrity; info disclosure →
   encryption + least privilege; DoS → connection limits/timeouts; elevation →
   DynamicUser + syscall filter). Keep it in this repo as `THREAT_MODEL.md`.

## Prerequisites
- A Linux dev machine (or VM) with `gcc`, `make`, `pkg-config`, `libsystemd-dev`
- `openssl` or `mbedtls` dev packages for Stage E
- A Yocto/poky checkout for Stage D (or a Raspberry Pi with a BSP layer if you
  prefer real hardware)

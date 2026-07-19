# Some Raspberry Pi 4 board revisions ship a BCM43456 WiFi chip instead of
# the BCM43455 the linux-firmware-bcm43455 package expects. BCM43456 is the
# same silicon and is firmware/NVRAM-compatible with 43455 -- it just
# reports a different chip ID over SDIO, so brcmfmac looks for
# "brcmfmac43456-sdio.*" filenames that don't exist in the package.
# Raspberry Pi's own firmware repo works around this with symlinks;
# do the same here rather than shipping a duplicate copy of the blobs.

do_install:append() {
    cd ${D}${nonarch_base_libdir}/firmware/brcm

    if [ -e brcmfmac43455-sdio.bin ]; then
        ln -sf brcmfmac43455-sdio.bin brcmfmac43456-sdio.bin
    fi
    if [ -e brcmfmac43455-sdio.clm_blob ]; then
        ln -sf brcmfmac43455-sdio.clm_blob brcmfmac43456-sdio.clm_blob
    fi

    for f in brcmfmac43455-sdio.*.txt; do
        [ -e "$f" ] || continue
        newname=$(echo "$f" | sed 's/^brcmfmac43455-sdio\./brcmfmac43456-sdio./')
        ln -sf "$f" "$newname"
    done

    # brcmfmac's board-specific NVRAM lookup is keyed off a DT compatible
    # string that doesn't match any of the board names this firmware
    # package ships calibration data for (only per-board .txt files exist,
    # never a suffix-less one) -- so after the specific-name lookup misses,
    # its generic no-suffix fallback ("brcmfmac<chip>-sdio.txt") also 404s
    # and the driver aborts entirely (no wlan0 device at all). Give it a
    # generic fallback pointed at the Pi 4 Model B calibration file, which
    # is what this hardware actually is regardless of what compatible
    # string the driver derived.
    if [ -e "brcmfmac43455-sdio.raspberrypi,4-model-b.txt" ]; then
        ln -sf "brcmfmac43455-sdio.raspberrypi,4-model-b.txt" brcmfmac43455-sdio.txt
    fi
    if [ -e "brcmfmac43456-sdio.raspberrypi,4-model-b.txt" ]; then
        ln -sf "brcmfmac43456-sdio.raspberrypi,4-model-b.txt" brcmfmac43456-sdio.txt
    fi
}

FILES:${PN}-bcm43455 += " \
    ${nonarch_base_libdir}/firmware/brcm/brcmfmac43455-sdio.txt \
    ${nonarch_base_libdir}/firmware/brcm/brcmfmac43456-sdio.bin \
    ${nonarch_base_libdir}/firmware/brcm/brcmfmac43456-sdio.clm_blob \
    ${nonarch_base_libdir}/firmware/brcm/brcmfmac43456-sdio.txt \
    ${nonarch_base_libdir}/firmware/brcm/brcmfmac43456-sdio.*.txt \
"

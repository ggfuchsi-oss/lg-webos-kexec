#!/bin/sh
# rexos-boot-hook.sh — OPTIONAL webOSbrew startup.sh snippet.
#
# NOT installed by default. RexOS-TV's rule is "nothing hardcoded on the TV";
# this hook is the single exception, and even it only RE-TRIGGERS A RAM LOAD —
# it never stores the OS image on disk. On every boot it fetches the kernel +
# initramfs (from a USB stick or network share) into /tmp and kexecs.
#
# To enable (only if you want auto-takeover at power-on):
#   append the body below to /var/lib/webosbrew/startup.sh on the TV.
# Everything it touches is in /tmp (tmpfs) and is gone on power cycle.

# --- begin snippet ---
# RexOS-TV auto-kexec (RAM-resident; no eMMC writes).
# Source the artifacts from wherever you stage them (USB mount shown):
REXOS_DIR="/tmp/rexos"
mkdir -p "$REXOS_DIR"
# Example: pull from a USB stick mounted at /media/*/rexos-tv/out
SRC="$(ls -d /media/*/rexos-tv/out 2>/dev/null | head -1)"
if [ -n "$SRC" ] && [ -f "$SRC/rexos-kernel.zImage" ]; then
  cp "$SRC"/rexos-kernel.zImage "$SRC"/rexos-initramfs.cpio.gz /tmp/
  cp /media/*/rexos-tv/boot/kexec-arm-fixed /tmp/ 2>/dev/null
  # then run the same stage+fire sequence as boot/rexos-kexec (inline or sourced)
  echo "RexOS-TV: artifacts present in RAM, kexec pending."
fi
# --- end snippet ---

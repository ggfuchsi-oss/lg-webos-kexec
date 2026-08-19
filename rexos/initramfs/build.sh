#!/usr/bin/env bash
# initramfs/build.sh — build a busybox-based initramfs for RexOS-TV.
#
# Output: $OUT/rexos-initramfs.cpio.gz  (RAM-resident rootfs, pushed to /tmp)
#
# busybox is cross-built static with the musl armv7 toolchain so it runs inside
# our kexec'd kernel with no shared libs. The init (/init) pets the micom so the
# box does not reset, then brings up a netcat debug shell + breadcrumb heartbeat.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${OUT:-$HERE/../out}"
MUSL_DIR="${MUSL_DIR:-$HOME/lgtv-toolkit/kexec-poc/arm-linux-musleabihf-cross}"
BB_VER="1.36.1"
BB_SRC="$HERE/busybox-$BB_VER"
ROOTFS="$HERE/rootfs"
BUSYBOX_BIN="$BB_SRC/busybox"

[[ -d "$MUSL_DIR" ]] || { echo "musl cross missing: $MUSL_DIR" >&2; exit 1; }
export PATH="$MUSL_DIR/bin:$PATH"
CC=arm-linux-musleabihf-gcc

mkdir -p "$OUT" "$ROOTFS"

# --- fetch + build busybox (static) ---
if [[ ! -f "$BUSYBOX_BIN" ]]; then
  echo "==> fetching busybox $BB_VER"
  curl -sL -o "$HERE/busybox.tar.bz2" \
    "https://busybox.net/downloads/busybox-$BB_VER.tar.bz2"
  tar xjf "$HERE/busybox.tar.bz2" -C "$HERE"
  cd "$BB_SRC"
  echo "==> configuring busybox (static, minimal)"
  make defconfig >/dev/null 2>&1
  # force static + the applets we need
  sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
  for opt in INIT SH IFCONFIG UDHCPC NC DEVMEM HTTPD UDPD CAT GREP MOUNT \
             FREERAMDISK REBOOT POWEROFF HWCLOCK; do
    sed -i "s/^# CONFIG_${opt} is not set/CONFIG_${opt}=y/" .config
  done
echo "    CONFIG_STATIC=$(grep -c 'CONFIG_STATIC=y' .config)"
  echo "==> building busybox (cross, static)"
  make -j"$(nproc)" CROSS_COMPILE=arm-linux-musleabihf- 2>&1 | tail -15
fi
echo "==> busybox: $(file "$BUSYBOX_BIN" | cut -d: -f2-)"

# --- build the micom-pet daemon (static) ---
echo "==> building micom-pet"
"$CC" -static -Os -o "$HERE/micom-pet" "$HERE/micom-pet.c"
arm-linux-musleabihf-strip "$HERE/micom-pet"

# --- assemble the rootfs ---
echo "==> assembling rootfs"
rm -rf "$ROOTFS" && mkdir -p "$ROOTFS"/{bin,sbin,etc,proc,sys,dev,tmp,usr,var}
cp "$BUSYBOX_BIN" "$ROOTFS/bin/busybox"
cp "$HERE/micom-pet" "$ROOTFS/sbin/micom-pet"
cp "$HERE/init" "$ROOTFS/init"
chmod +x "$ROOTFS/init"
# busybox applet symlinks
for a in sh ls cp mv rm cat grep mount umount init reboot poweroff \
         ifconfig nc devmem httpd free; do
  ln -sf busybox "$ROOTFS/bin/$a" 2>/dev/null || true
done
ln -sf busybox "$ROOTFS/sbin/init" 2>/dev/null || true

# --- copy captured TV binaries/libs (wifi support) ---
if [ -d "$HERE/files" ]; then
  echo "==> adding wifi/files extras"
  mkdir -p "$ROOTFS/lib/modules" "$ROOTFS/usr/sbin" "$ROOTFS/usr/lib" \
           "$ROOTFS/usr/share/udhcpc" "$ROOTFS/etc"
  [ -d "$HERE/files/lib" ]        && cp -a "$HERE/files/lib/."        "$ROOTFS/lib/"        2>/dev/null
  [ -d "$HERE/files/usr/lib" ]    && cp -a "$HERE/files/usr/lib/."    "$ROOTFS/usr/lib/"    2>/dev/null
  [ -f "$HERE/files/wlan_mt7663.ko" ] && cp "$HERE/files/wlan_mt7663.ko" "$ROOTFS/lib/modules/"
  if [ -f "$HERE/files/wpa_supplicant" ]; then
    cp "$HERE/files/wpa_supplicant" "$ROOTFS/usr/sbin/"; chmod +x "$ROOTFS/usr/sbin/wpa_supplicant"
  fi
  [ -f "$HERE/files/wpa_supplicant.conf" ] && cp "$HERE/files/wpa_supplicant.conf" "$ROOTFS/etc/"
  if [ -f "$HERE/files/udhcpc.script" ]; then
    cp "$HERE/files/udhcpc.script" "$ROOTFS/usr/share/udhcpc/default.script"
    chmod +x "$ROOTFS/usr/share/udhcpc/default.script"
  fi
fi

# --- pack cpio.gz ---
echo "==> packing $OUT/rexos-initramfs.cpio.gz"
( cd "$ROOTFS" && find . | cpio -o -H newc 2>/dev/null ) | gzip -9 > "$OUT/rexos-initramfs.cpio.gz"
echo "    $(du -h "$OUT/rexos-initramfs.cpio.gz" | cut -f1)  -> $OUT/rexos-initramfs.cpio.gz"

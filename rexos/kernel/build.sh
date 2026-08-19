#!/usr/bin/env bash
# kernel/build.sh — build the RTD2875 RexOS kernel (zImage) into $OUT.
#
# Source tree: the LG K7LP GPL linux-4.4.3 at $KERNEL_SRC (default
# ~/lgtv-toolkit/kernel-src/kernel/linux-4.4.3). We capture the TV's live
# .config once (from /proc/config.gz), apply the modern-toolchain patches, and
# build with arm-none-eabi-gcc + ccache.
#
# Output: $OUT/rexos-kernel.zImage  (loaded by ../boot/rexos-kexec, RAM only)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${OUT:-$HERE/../out}"
KERNEL_SRC="${KERNEL_SRC:-$HOME/lgtv-toolkit/kernel-src/kernel/linux-4.4.3}"
PATCHED="$HERE/.patched"
CONFIG="$HERE/rexos-tv.config"

[[ -d "$KERNEL_SRC" ]] || { echo "KERNEL_SRC missing: $KERNEL_SRC" >&2; exit 1; }
mkdir -p "$OUT"

# --- 1. config (capture from TV once, then cache) ---
TV="${REXOS_TV:-192.168.2.103}"
KEY="${REXOS_TV_KEY:-$HOME/.ssh/tv_key}"
SSH_OPTS="-i $KEY -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o BatchMode=yes -o ConnectTimeout=10"

if [[ ! -f "$CONFIG" ]]; then
  echo "==> capturing .config from TV $TV:/proc/config.gz"
  if ! ssh $SSH_OPTS "root@$TV" 'cat /proc/config.gz' 2>/dev/null | gunzip > "$CONFIG"; then
    echo "    could not reach TV — drop a .config at $CONFIG manually" >&2
    echo "    (e.g. scp root@$TV:/proc/config.gz - | gunzip > $CONFIG)" >&2
    exit 1
  fi
fi
cp "$CONFIG" "$KERNEL_SRC/.config"

# --- force the options RexOS needs (may introduce NEW dependent symbols) ---
( cd "$KERNEL_SRC" && \
  scripts/config --enable BLK_DEV_INITRD --enable RD_GZIP --enable RD_BZIP2 \
                --enable KALLSYMS --enable DEVMEM --enable BLK_DEV_RAM \
                --disable RTK_KDRV_SB2 --disable LOCALVERSION_AUTO ) || true
# LOCALVERSION must be EMPTY: stock modules (e.g. wlan_mt7663.ko) carry
# vermagic "4.4.84 SMP preempt mod_unload ARMv7" with NO suffix. Any suffix
# (e.g. -rexos) makes insmod fail with "Invalid module format". Rename only
# after networking is proven.
( cd "$KERNEL_SRC" && scripts/config --set-str LOCALVERSION "" ) || true
( cd "$KERNEL_SRC" && scripts/config --set-str INITRAMFS_SOURCE "" ) || true

# --- resolve ALL new symbols non-interactively (old 4.4 kconfig) ---
( cd "$KERNEL_SRC" && yes "" | make ARCH=arm oldconfig ) || true

# --- 3. modern-toolchain patches (idempotent) ---
if [[ ! -f "$PATCHED" ]]; then
  echo "==> applying toolchain patches"
  cd "$KERNEL_SRC"
  # remove -rdynamic from CFLAGS_ABI (modern ld rejects it there)
  sed -i 's/-rdynamic//g' arch/arm/Makefile
  # new GAS section syntax: #alloc/#execinstr -> "a"/"ax"
  find arch/arm -name '*.S' -exec sed -i \
    's/,#alloc,#execinstr/,"ax"/g; s/,#alloc/,"a"/g; s/,#execinstr/,"x"/g' {} +
  # header-guard typo warnings -> not errors
  touch "$PATCHED"
fi

# --- 4. build ---
echo "==> building zImage (arm-none-eabi-gcc + ccache)"
cd "$KERNEL_SRC"
# re-resolve in case anything shifted, then compile
( yes "" | make ARCH=arm oldconfig ) >/dev/null 2>&1 || true
make ARCH=arm CROSS_COMPILE="ccache arm-none-eabi-" \
     HOSTCFLAGS="-fcommon -w" \
     KCFLAGS="-w -Wno-error -Wno-error=scalar-storage-order" \
     zImage -j"$(nproc)"

cp arch/arm/boot/zImage "$OUT/rexos-kernel.zImage"
echo "zImage -> $OUT/rexos-kernel.zImage  ($(du -h "$OUT/rexos-kernel.zImage" | cut -f1))"

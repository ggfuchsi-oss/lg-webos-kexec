#!/usr/bin/env bash
# run-poc.sh - reproduce the kexec custom-boot proof-of-concept, safely.
#
# Proves that an arbitrary kernel can be STAGED for boot from userspace on the
# rooted TV, with no bootloader / secure-boot involvement. It loads the TV's own
# current kernel (the only kernel we know boots this hardware), confirms the
# kernel accepted the stage (/sys/kernel/kexec_loaded -> 1), then UNLOADS it.
#
#   *** This never executes the staged kernel. No `kexec -e`, no reboot. ***
#
# Executing would be the real custom boot; that belongs in a deliberate, screen-
# attended step, not this PoC.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LGTV="$HERE/../bin/lgtv"
KEXEC="$HERE/kexec-arm"          # static armv7 kexec-tools built by build.sh

[[ -f "$KEXEC" ]] || { echo "missing $KEXEC - run build.sh first"; exit 1; }

echo "==> pushing kexec + extracting the current kernel on the TV"
"$LGTV" push "$KEXEC" /tmp/kexec >/dev/null
"$LGTV" shell 'bash -c "
  chmod +x /tmp/kexec
  # p21 is the A-slot kernel uImage (magic 27051956, load 0x108000).
  dd if=/dev/mmcblk0p21 of=/tmp/kernel.uImage bs=1M count=10 2>/dev/null
  cp /sys/firmware/fdt /tmp/tv.dtb
  echo \"  kexec: \$(/tmp/kexec -v 2>&1 | head -1)\"
  echo \"  staged before: \$(cat /sys/kernel/kexec_loaded)\"
"'

echo "==> STAGE (load only) then UNLOAD"
"$LGTV" shell 'bash -c "
  CMDLINE=\$(cat /proc/cmdline)
  /tmp/kexec -l /tmp/kernel.uImage --dtb=/tmp/tv.dtb --command-line=\"\$CMDLINE\"
  echo \"  staged after load : \$(cat /sys/kernel/kexec_loaded)   (1 = kernel accepted)\"
  /tmp/kexec -u
  echo \"  staged after unload: \$(cat /sys/kernel/kexec_loaded)   (0 = cleared)\"
  rm -f /tmp/kexec /tmp/kernel.uImage /tmp/tv.dtb
"'
echo "==> done. Custom-boot primitive confirmed; TV left untouched."

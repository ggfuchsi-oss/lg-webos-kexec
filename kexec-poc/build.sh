#!/usr/bin/env bash
# build.sh - cross-build a static armv7 kexec-tools for the TV (RTD2875, aarch32).
#
# Uses a self-contained prebuilt musl cross-toolchain (no host packages, no
# sudo). Output: ./kexec-arm, a static ARM binary that runs on the TV as-is.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

TC_DIR="$HERE/arm-linux-musleabihf-cross"
KX_VER="2.0.28"
KX_DIR="$HERE/kexec-tools-$KX_VER"

if [[ ! -d "$TC_DIR" ]]; then
  echo "==> fetching musl armv7 cross-toolchain (~98MB, one time)"
  curl -sL -o tc.tgz https://musl.cc/arm-linux-musleabihf-cross.tgz
  tar xzf tc.tgz && rm tc.tgz
fi
export PATH="$TC_DIR/bin:$PATH"

if [[ ! -d "$KX_DIR" ]]; then
  echo "==> fetching kexec-tools $KX_VER"
  curl -sL -o kx.tgz \
    "https://mirrors.edge.kernel.org/pub/linux/utils/kernel/kexec/kexec-tools-$KX_VER.tar.gz"
  tar xzf kx.tgz && rm kx.tgz
fi

cd "$KX_DIR"
echo "==> configure (static, arm, no zlib/lzma)"
# LDFLAGS=-static here is baked into the Makefile for the main binary; do NOT
# repeat it on the make line or it clobbers purgatory's own link flags
# (-nostartfiles -nostdlib -e purgatory_start -r) and the PIE startfile breaks
# the freestanding purgatory link.
LDFLAGS="-static" ./configure --host=arm-linux-musleabihf \
  --without-zlib --without-lzma >/dev/null
echo "==> make"
make clean >/dev/null 2>&1 || true
make >/dev/null
arm-linux-musleabihf-strip build/sbin/kexec
cp build/sbin/kexec "$HERE/kexec-arm"
echo "==> built $HERE/kexec-arm  ($(du -h "$HERE/kexec-arm" | cut -f1))"
file "$HERE/kexec-arm"

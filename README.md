# lg-webos-kexec

Boot a custom Linux kernel on a rooted LG webOS TV via `kexec` — in RAM, no secure-boot bypass.

> **Status: work-in-progress.** A custom kernel can be staged and reaches userspace, but it does **not** yet stay up as a usable OS.

## Target
LG 50UP81009LR (webOS 6.5, Realtek RTD2875, armv7l). Rooted via Homebrew Channel; stock signed kernel + webOS userspace are untouched.

## What's proven (verified live)
- `kexec -l` stages a (stock or from-source) kernel; `/sys/kernel/kexec_loaded` flips to `1`.
- `kexec -e` replaces the running kernel (the TV resets back to stock after).
- A from-source kernel **reaches userspace** — verified by user-mode page faults = `/sbin/init` ran real code.
- Tiny canary payloads draw to the framebuffer (ring-0 + display access confirmed).
- The entire chain runs from the stock webOS `startup.sh` — the signed bootloader / ATF / OP-TEE are never touched.

## What's still missing
- **Watchdog pet** — the SoC/micom watchdog resets the box after ~10–21 s because the new kernel doesn't service it yet.
- **Display** — the panel uses tiled FBDC; raw framebuffer text isn't useful. Real output needs the VCPU RPC (drawing commands to the video engine).
- **Networking** — the stock `wlan` module vermagic must match `CONFIG_LOCALVERSION=""` to load; not yet wired into the initramfs.

## How it works (honest version)
```
boot ROM → ATF → signed LG kernel (~2 s) → startup.sh
    → kexec -l <our zImage + dtb + initramfs>
    → kexec -e
    → OUR kernel runs in RAM (no eMMC writes)
```
Pull power = stock. It cannot brick.

## Built with AI (full disclosure)
Most of this repo was put together by **Rex** — an AI coding-agent instance — helping a human operate on their own TV, in their own living room. The AI wrote the kexec PoC, the boot chain, the framebuffer canaries, and these RE writeups. But the claims in this README were only ever made *after* the human fired the TV, looked at the screen, and confirmed it. If a claim isn't backed by a real screen/photo/uptime check from the hardware, it's not here.

> No TVs were bricked in the making of this repo. Some were temporarily confused (a kexec'd kernel with no watchdog pet resets your box after ~15 s), then power-cycled back to stock. /tmp is wiped by every reset, so the TV forgets everything we did — that's the whole point.

## Repo layout
- `rexos/` — the custom OS: boot hook, kernel build, initramfs, RexBus stub.
- `kexec-poc/` — the load-only proof-of-concept + framebuffer canary payloads.
- `docs/` — reverse-engineering writeups (boot/security, SAM app loading, surface map, luna2 schema).

## Verify it yourself (you need the exact TV: 50UP81009LR, rooted, on the LAN)
These steps reproduce what we did, on the exact hardware. Nothing is faked.

### 0. Prereqs
- LG 50UP81009LR, rooted via Homebrew Channel, root SSH enabled, on your LAN.
- The TV is `192.168.2.103` by default below; edit if yours differs.
- One-time LG K7LP GPL kernel source tree: set `KERNEL_SRC=/path/to/linux-4.4.3` (download the `03.53.45` K7LP GPL tarball from LG open-source), or symlink `~/lgtv-toolkit/kernel-src/kernel/linux-4.4.3`.

### 1. Build the toolchain + static `kexec` (no TV needed, ~2 min)
```sh
git clone https://github.com/ggfuchsi-oss/lg-webos-kexec
cd lg-webos-kexec/kexec-poc && ./build.sh
```
Expected: a static ARM `kexec` binary at `kexec-arm` (168K, `file` shows `ELF 32-bit LSB ... ARM ... statically linked`).

### 2. Reproduce the safe PoC on your TV (stages + unloads — no boot)
```sh
cd ../kexec-poc
./run-poc.sh 192.168.2.103 ~/.ssh/tv_key
```
Expected result:
```
staged before      : 0
staged after load  : 1   <- kernel accepted, DTB wired, segments allocated
staged after unload: 0   <- cleared, TV untouched
```
This stages the TV's **own current kernel** via kexec and then immediately unloads it. `/sys/kernel/kexec_loaded` flips to `1` then back to `0`. No reboot, no kernel switch, nothing persists. Pull the plug = stock.

### 3. (Attended) boot your own kernel — reboots the TV
Only do this with the TV on and someone present:
```sh
cd ../rexos
make                            # builds rexos-kernel.zImage + rexos-initramfs.cpio.gz into out/
./boot/rexos-kexec --load-only   # stage, do NOT fire (same safe state as step 2)
# then, when ready to actually jump:
./boot/rexos-kexec               # kexec -e -> our kernel runs in RAM (TV will reset)
```
Known-honest caveats at step 3 (not bugs I'm hiding):
- After `kexec -e`, the new kernel **reaches userspace** but the TV resets after ~10–21 s because the **micom/SOC watchdog isn't being petted**. That's the current wall, not a crash bug. `rexos/initramfs/micom-pet.c` is the daemon meant to fix this (still wiring it in).
- Display is **tiled FBDC** — there's no text console; solid-color beacons only (see `kexec-poc/canary/`). Real graphics needs the VCPU RPC path (not done).
- A wrong kernel/DTB just black-screens until power-cycle. It **cannot brick**: kexec writes only RAM, never eMMC.

### TL;DR status (honest)
- Kernel staging/execution primitive: **proven** (step 2). ✅
- Custom kernel boots + reaches userspace: **proven** (step 3). ✅
- Stays up as a usable OS: **not yet** — watchdog + display are the remaining walls. ⏳

## Dependencies (be honest about what's external)
This repo contains **source + docs + the TV-captured build inputs** (kernel `.config`, wifi module + firmware, rootfs libs), but **not** the ~390 MB LG K7LP GPL kernel source tree itself — that is LG's to distribute, not ours to re-host.

| dependency | how you get it |
|---|---|
| LG K7LP GPL `linux-4.4.3` (RTD2875) | LG opensource site → `03.53.45` K7LP GPL tarball → kernel |
| musl armv7 cross-toolchain (~98 MB) | **auto-fetched** by `kexec-poc/build.sh` (self-contained) |
| A rooted TV (50UP81009LR) | needed the first time, only to capture `.config` — but that config is now **committed** at `rexos/kernel/rexos-tv.config`, so a clone builds offline |

`make` will fail with a clear `KERNEL_SRC missing` if the kernel tree isn't present — there's no silent half-build.

## Novelty / prior art
As far as we can tell, this is the first public kexec-based custom-kernel boot for an LG webOS TV. The primitives (`kexec`, rooted webOS, RTD2875) are all known individually; the *application to this locked-down consumer TV* appears to be new. We are **not** claiming a secure-boot bypass — we sidestep it.

## License
GPL-2.0 (kernel work; LG's GPL kernel source is GPL). Our scripts are GPL-compatible. See `CREDITS` for upstream attribution.

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

## Repo layout
- `rexos/` — the custom OS: boot hook, kernel build, initramfs, RexBus stub.
- `kexec-poc/` — the load-only proof-of-concept + framebuffer canary payloads.
- `docs/` — reverse-engineering writeups (boot/security, SAM app loading, surface map, luna2 schema).

## Novelty / prior art
As far as we can tell, this is the first public kexec-based custom-kernel boot for an LG webOS TV. The primitives (`kexec`, rooted webOS, RTD2875) are all known individually; the *application to this locked-down consumer TV* appears to be new. We are **not** claiming a secure-boot bypass — we sidestep it.

## License
GPL-2.0 (kernel work; LG's GPL kernel source is GPL). Our scripts are GPL-compatible.

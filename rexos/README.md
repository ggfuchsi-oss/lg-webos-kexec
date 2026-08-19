# RexOS-TV

Our own OS for rex's rooted LG 50UP81009LR (webOS 6.5, firmware 03.53.45,
Realtek **RTD2875** — quad Cortex-A55 running a 32-bit armv7l kernel 4.4.84).

## The one rule (from rex, 2026-08-18)

> **Nothing gets hardcoded on the TV. The whole OS stays in RAM.**

RexOS-TV is **RAM-resident**: the kernel + initramfs + kexec loader are pushed
into the TV's `/tmp` (tmpfs) over SSH and `kexec -e`'d into memory. **No writes
to the TV's eMMC / rootfs / bootloader.** The webOS userspace is replaced *in
memory only*. Pull the power and it's a stock TV again. Zero brick risk, fully
reversible.

The only optional persistent artifact is the webOSbrew `startup.sh` kexec hook
(`boot/rexos-boot-hook.sh`) — and even that just *re-triggers a RAM load* (from
USB/network) at boot; it never stores the OS image on disk. It is **not
installed by default**.

## Why this is possible (the hard part is already done)

We do **not** fight secure boot. The achievable flagship:

```
[LG bootloader] -> [stock SIGNED kernel] -> [webOSbrew startup.sh]
        -> kexec -> [RexOS kernel 4.4.3 / RTD2875] -> [initramfs]
                       ├─ micom-pet daemon   (keep the box alive)
                       ├─ RexBus  (native IPC = carried luna2 schema)
                       ├─ framebuffer driver  (VCPU RPC path, later)
                       └─ app runtime -> rexos-flex UI (later)
```

Proven on this exact TV (see `/run/media/rex/009C-1660/lgtv-backup-20260818/`):
- kexec custom-boot works **end to end** (load + execute + visible output).
- Our from-source kernel **reaches userspace** — 3752 user-mode page faults =
  `/sbin/init` ran real user code.
- The "silent death at handoff" was self-inflicted instrumentation (a physical
  address written after a user mm was installed → fault → silent hang). Fix =
  use the kernel virtual mapping `0xFE060110`, not phys `0x18060110`.

## Boot flow (what `./boot/rexos-kexec` does)

1. Build (or reuse) `out/rexos-kernel.zImage` + `out/rexos-initramfs.cpio.gz`
   (see `kernel/`, `initramfs/`).
2. `scp` all three artifacts (kernel, initramfs, `kexec-arm-fixed`) to the
   TV's **`/tmp`** — RAM only.
3. Over SSH, on the TV:
   - `export KEXEC_FIXED_BASE=0x100000` (deterministic load, avoids the
     locate_hole intermittency).
   - `kexec -l` our zImage + live DTB (`/sys/firmware/fdt`) + cmdline.
   - Disarm the **SB2** hardware write-monitors (`/sys/realtek_boards/sb2_dbg`,
     sets 1–16) — stock arms them; our kernel's driver is disabled.
   - Clear the **arm_wrapper** memory monitors via `devmem` (`0x1805C0xx`).
   - Widen the **SoC watchdog** window (`devmem 0x18062210`, TCWOV).
   - Offline secondary CPUs 1–3 (parked in stock code we'd otherwise clobber).
   - `kexec -e` → RexOS takes over in RAM.

## Build

```sh
make            # builds kernel + initramfs into ./out
make boot       # build then fire via ./boot/rexos-kexec
```

Requires (all present on rex's box): `arm-none-eabi-gcc`, `mkimage`, `bc`,
`ccache`, and the musl armv7 cross at
`~/lgtv-toolkit/kexec-poc/arm-linux-musleabihf-cross`. The kernel source is the
LG K7LP GPL tree at `~/lgtv-toolkit/kernel-src/kernel/linux-4.4.3`.

The kernel needs a `.config`. First build captures it from the running TV
(`/proc/config.gz`); afterwards it's cached at `kernel/rexos-tv.config`.

## Current status

| Piece | Status |
|---|---|
| kexec custom-boot | ✅ proven end-to-end |
| kernel → userspace | ✅ reached (3752 user faults) |
| RAM-resident loader (`rexos-kexec`) | ✅ scaffolded |
| initramfs + micom-pet init | ✅ scaffolded |
| reliable persistent boot | ⏳ needs a fired boot to confirm |
| real graphics (VCPU RPC / FBDC) | ❌ not started (display is tiled) |
| RexBus native IPC | 🟡 schema carried in, stub only |
| app runtime / rexos-flex UI | ❌ later |

## References

- USB backup: `/run/media/rex/009C-Used0/lgtv-backup-20260818/` — read its
  `README.md` + `memory/webos-boot-security.md` **first**.
- Toolkit: `~/lgtv-toolkit/` (kernel-src, kexec-poc, luna2).
- TV: `root@192.168.2.103:22`, key `~/.ssh/tv_key`. `/tmp` is wiped by every
  watchdog reset, so re-push before every fire.

## Testing etiquette

The TV is in rex's living space. No audio / backlight / power side effects when
testing. Firing a kexec boot resets the TV — that's expected, but do it when
rex is around / not watching something.

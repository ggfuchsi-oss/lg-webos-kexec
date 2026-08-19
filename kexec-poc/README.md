# kexec-poc — custom-boot primitive, proven

Roadmap step **B**: prove the TV can stage and boot an arbitrary kernel from
userspace, sidestepping the ATF/secure-boot chain entirely — the foundation for
a persistent custom OS via kexec-chainload.

## Result

```
staged before      : 0
staged after load  : 1   (kernel accepted, DTB wired, segments allocated)
staged after unload: 0   (cleared, TV untouched)
```

`kexec -l` staged the TV's current kernel successfully; `kexec -u` cleared it.
The custom-boot primitive is **live**. The one step intentionally not taken is
`kexec -e` (execute → reboot into the staged kernel).

## Why this matters

Secure boot walls off *persistent bootloader/kernel replacement* (see
`../` boot-security notes). kexec doesn't fight that wall — it runs *after* the
signed kernel is already up. Combined with the homebrew hook that runs every
boot (`/var/lib/webosbrew/startup.sh`), the persistent-custom-OS architecture is:

```
boot ROM → ATF → signed LG kernel boots (~2s) → startup.sh: kexec -l <ours> && kexec -e → our kernel/OS
```

No secure-boot break required. The stock kernel becomes a shim bootloader we own.

## Files

| file | role |
|---|---|
| `build.sh` | cross-build static armv7 `kexec-tools` (self-contained musl toolchain, no sudo) |
| `kexec-arm` | the built static ARM binary (168K), produced by `build.sh` — run `./build.sh` first |
| `run-poc.sh` | reproduce the safe load → verify → unload (staged then unloaded, no boot) |
| *(toolchain is fetched, not committed)* | `build.sh` downloads the musl armv7 toolchain once (~98MB) into `arm-linux-musleabihf-cross/` |

## Safety

`run-poc.sh` never executes the staged kernel and always unloads. `kexec -l`
only stages into kernel memory for a *future* boot; nothing runs until `kexec -e`
or a reboot. Even kexec execution is RAM-only and non-persistent: a failed boot
recovers with a power cycle — it cannot brick, because eMMC is never written.

## Target facts (for step C)

- SoC Realtek RTD2875, Cortex-A55 (ARMv8) run as aarch32; kernel 4.4.84.
- current kernel: uImage at `/dev/mmcblk0p21`, ARM, uncompressed, load `0x108000`,
  entry `0x108040`, ~9.7 MB.
- live DTB: `/sys/firmware/fdt` (7 KB) — extracted, the hardware description a
  custom kernel needs.
- kernel is unlocked: `CONFIG_KEXEC=y`, unsigned modules, `/dev/mem`, kallsyms.

## Next (step C)

Build a kernel *for* the RTD2875 (LG/Realtek GPL source + our DTB), then
`kexec -e` into it — attended, with the screen on, since a wrong kernel/DTB
black-screens until a power cycle. First light of a custom OS on this hardware.

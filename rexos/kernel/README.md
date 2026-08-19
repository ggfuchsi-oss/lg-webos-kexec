# RexOS-TV kernel build

Source: LG K7LP GPL `linux-4.4.3` (RTD2875) at
`~/lgtv-toolkit/kernel-src/kernel/linux-4.4.3`.

## First build captures the config

`build.sh` ssh's to the TV and grabs `/proc/config.gz` (the exact running
config) once, caching it at `kernel/rexos-tv.config`. After that, builds are
offline. If the TV isn't reachable, drop a `.config` there manually:

```sh
scp -i ~/.ssh/tv_key root@192.168.2.103:/proc/config.gz - | gunzip > rexos-tv.config
```

## What build.sh forces on top of the TV config

| Option | Why |
|---|---|
| `BLK_DEV_INITRD`, `RD_GZIP` | load the initramfs we pass via `kexec --initrd` |
| `KALLSYMS` | resolve initcall fn pointers against System.map when debugging |
| `DEVMEM` (STRICT_DEVMEM off) | `/dev/mem` for `micom-pet` + crumb reads |
| `RTK_KDRV_SB2=n` | `rtk_sb2_driver_init` hangs; we disarm SB2 in hardware in the loader instead |

`gdma_init_module` is blacklisted on the cmdline (`initcall_blacklist=`) rather
than disabled, so no rebuild is needed to toggle it.

## Toolchain patches (applied once, idempotent)

- `arch/arm/Makefile`: drop `-rdynamic` from `CFLAGS_ABI` (modern `ld` rejects).
- `arch/arm/**/*.S`: convert `#alloc`/`#execinstr` section flags to `"a"`/`"ax"`
  (new GAS syntax).
- Build with `KCFLAGS="-w -Wno-error -Wno-error=scalar-storage-order"` so GCC16's
  many warnings can't fail the build.

## Notes

- Build loop is ~20s with `ccache` + `CONFIG_DEBUG_INFO=n` (set that in the
  cached `.config` for fast iteration).
- We boot the **zImage** via the patched kexec (the proven userspace-reaching
  path). `KEXEC_FIXED_BASE=0x100000` (entry 0x108000) is forced by
  `kexec-arm-fixed`. If you switch to the raw `Image` wrapped with `mkimage
  -a/-e 0x00108000`, the kexec zImage-loader patch must also cover the uImage
  path.
- Display is **tiled (FBDC)** — raw framebuffer text is infeasible; real UI
  needs the VCPU `DrawGraphicWin` RPC (phase 4). Solid color = the reliable
  beacon channel for now.

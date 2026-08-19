---
name: webos-boot-security
description: "rex's LG TV boot chain, secure world, and custom-OS feasibility (software RE)"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 502c0e03-103c-42cf-b877-b81bcd0b7466
  modified: 2026-08-17T20:09:39.033Z
---

Deep software RE of rex's LG 50UP81009LR ([[lgtv-toolkit]], [[webos-tv-surface]]).
Goal is a persistent custom OS / "better than luna" stack, software-only, no
hardware mods, no DRM/piracy (rex confirmed: exploring limits, not piracy).

SoC: Realtek RTD2875, quad **Cortex-A55** (ARMv8/64-bit capable) but LG ships a
32-bit armv7l kernel 4.4.84. So aarch64 is a theoretical ceiling LG doesn't use.

Kernel is effectively unlocked (from /proc/config.gz): CONFIG_KEXEC=y,
CONFIG_MODULES=y with MODULE_SIG NOT set + MODULE_FORCE_LOAD (unsigned modules
load), CONFIG_DEVMEM=y + DEVKMEM=y + STRICT_DEVMEM not set (/dev/mem = full
physmem/register R/W), CONFIG_KALLSYMS=y. modules_disabled=0.

Boot chain (the wall for PERSISTENT bootloader/kernel replacement):
boot ROM -> ATF (CONFIG_RTK_ATF_BOOTING) -> OP-TEE secure world (CONFIG_OPTEE)
+ U-Boot 2012.07 (Realtek RTD287o, has `Verify_SHA256_hash_HW` + do_verify) ->
Linux. Secure-boot machinery is present and wired. Whether the enforcement fuse
is actually blown is UNKNOWN and can't be answered safely software-only:
options are eFuse read via /dev/otp (raw Realtek ioctl, key-adjacent, holding),
a test-write of a modified kernel (brick risk), or the U-Boot `Realtek>` console
on UART ttyS0 @115200 (hardware, rex declined). So: don't attempt persistent
bootloader/kernel replacement blind.

U-Boot env = mmcblk0p11, PLAINTEXT, no visible signature (CRC only):
  bootcmd=cp2ram kernel 0x108000 && go all;  bootdelay=0
  kernel uImage = p21/p22 (A/B, magic 27051956). rootfs squashfs p6/p27.
The env being unsigned doesn't defeat a SHA256-verified kernel.

THE ACHIEVABLE FLAGSHIP (avoids the secure-boot fight entirely):
stock signed kernel boots -> homebrew /var/lib/webosbrew/startup.sh kexec's into
our own kernel+userspace every boot. Persistent custom OS, LG bootloader
untouched. Needs a kernel built for RTD2875 + the live DTB. Real driver work =
display/GPU/tuner.

STEP B DONE (2026-08-17): kexec custom-boot primitive PROVEN. Cross-built static
armv7 kexec-tools 2.0.28 (~/lgtv-toolkit/kexec-poc/, via self-contained musl
toolchain - no host install). `kexec -l` the current kernel (p21 uImage, load
0x108000) + /sys/firmware/fdt dtb -> /sys/kernel/kexec_loaded flips 0->1 (kernel
accepted), `kexec -u` -> 0. LOAD-ONLY, never executed. Reproduce:
kexec-poc/run-poc.sh. kexec is not on the TV by default and /tmp is tmpfs, so
push kexec-poc/kexec-arm each time (or persist to /media/developer).
Build gotcha: don't pass `make LDFLAGS=-static` (clobbers purgatory's
-nostartfiles link); let ./configure bake it in.

KEXEC-EXECUTE ROOT CAUSE FOUND (2026-08-17, via LG K7LP GPL source):
kexec -l works; kexec -e resets to stock. NOT the CPU teardown (PSCI cpu hotplug
works: echo 0>/sys/devices/system/cpu/cpuN/online succeeds), NOT the entry
protocol (bootloader uses standard do_bootm_linux - source confirmed in
bootcode rtapi_rtboot.c rtk_plat_boot_go). The cause: **SB2 hardware memory
write-monitor**. drivers/rtk_kdriver/hw_monitor/rtk_sb2.c + arch/arm/mach-rtd2875
program an SB2 monitor "set 6" write-protecting the kernel region
0x00104000-0x012dc904 (regs: START_5 phys 0x1801A46C=0x00104000, END_5
0x1801A48C, per-slot CTRL at 0x1801A498+slot*4). kexec's purgatory copies the new
kernel to 0x108000 (inside that region) -> SB2 trap -> Micom external watchdog
(dmesg "[WDT] Micom") hard-resets the SoC -> stock boot. Regs readable via
`devmem` at PHYSICAL 0x1801Axxx (NOT virtual 0xb801Axxx - that returns garbage).
FIX (driver's own sysfs, format documented in rtk_sb2.c ~line 2535):
  echo "set6 trap-toggle" > /sys/realtek_boards/sb2_dbg   # violation warns not resets
  echo "set6 clear"       > /sys/realtek_boards/sb2_dbg   # remove kernel monitor
PROGRESS (tested, night of 2026-08-17): cleared SB2 set6 (kexec-sb2-fire.sh) ->
boot got FURTHER (reset later) confirming SB2 was A trap. Then cleared ALL 16 SB2
sets (kexec-allclear-fire.sh) AND all arm_wrapper monitors via devmem
(kexec-nomon-fire.sh, recipe: SCPU_MEM_TRASH_DBG CTRL 0x1805C040+id*4 <- BIT1
disable, START 0x1805C020+id*4 <-0, END 0x1805C030+id*4 <-0). STILL resets to
stock. So MEMORY MONITORS FULLY ELIMINATED as the cause. Screen behavior (rex
watching): off -> LG logo -> normal boot = new kernel dies BEFORE display init,
full SoC reset, no output.
ELIMINATED: entry protocol (standard do_bootm_linux), PSCI cpu teardown (hotplug
works), SB2 monitors, arm_wrapper monitors.
LEADING REMAINING SUSPECT: watchdog. kexec skips the bootloader which normally
pets the Micom, so new kernel may start with a partly-elapsed watchdog window ->
times out in early boot. TWO watchdogs: SoC-internal (watchdog_enable() via ioctl
MISC_SET_WATCHDOG_ENABLE misc.c:1543, CONFIG_RTK_KDRV_WATCHDOG, rtk_watchdog.h)
and external Micom MCU (sys-intmicom / rtk_wdt_func.c). 
WATCHDOG ANALYSIS DONE (2026-08-17, source-confirmed), theory corrected:
Two candidates, NOT equal. (1) SoC-internal WDOG = the real suspect: the running
kernel ACTIVELY auto-kicks it (rtk_watchdog.c, WDIOS_ENABLE_WITH_KERNEL_AUTO_KICK,
rtk_wdog_kick_by_ap), so it fires if kicking stops during the kexec gap.
Authoritative disable = the driver's own On=0 branch of watchdog_enable():
  devmem 0x18062208 32 0x01                      # TCWTR clear timer
  devmem 0x18062204 -> (cur & 0xFFFFFF00)|0xa5    # TCWCR wden field = 0xa5 STOP magic
(wden=0xa5 is the *stopped* magic; enable ends by clearing wden to 0x00 - the
helper is_watchdog_enable()'s name is backwards, trust the On=0 branch. rtd_maskl
(o,a,v)=writel(o,(readl(o)&a)|v), wden_mask=0xFF. USE PHYSICAL 0x18062xxx.) These
are register writes => PERSIST across kexec -e. Must ALSO stop the kernel kicker
first (printf V >/dev/watchdog, nowayout off) or it re-arms before we fire.
(2) External Micom = WEAKER suspect than previously thought: grep of the whole
K7LP driver tree finds NO SoC->Micom watchdog-pet path - the kernel only RECEIVES
Micom->SCPU debug interrupts (rtk_wdt_func.c). If the kernel never pets it, Micom
isn't what fires specifically during the kexec gap.
NEW SCRIPT READY: ~/lgtv-toolkit/kexec-poc/kexec-wdt-fire.sh - stages stock
kernel, clears all SB2+arm_wrapper monitors (no regression), stops the kicker,
writes the authoritative SoC-watchdog disable, adds wdt=off to the new kernel's
cmdline, prints TCWCR/TCWTR before+after (low byte should read 0x..a5), fires.
FIRED 2026-08-17 (kexec-wdt-fire.sh). RESULT = STATE CHANGE / milestone:
the TV FROZE (hung, needed manual power-off) instead of the prior clean reset-to-
stock. A freeze is NOT a watchdog reset -> the watchdog wall was PASSED this time.
What actually changed the outcome: TCWOV widened 0x066FF380->0x10237880 (~4.0s->
~10s window at 27MHz) + kernel auto-kicker stopped. (Note: TCWCR read 0x000000A5
BOTH before and after our devmem write -> 0xA5 is the ARMED state and the driver's
On=0 branch writing 0xa5 does NOT truly clear enable; is_watchdog_enable()==0xa5 is
correct. So the win came from the widened window+stopped kicker, not a true disable.)
No brick: recovered fine on power cycle (kexec never writes eMMC).
NEW DIAGNOSIS: we kexec the STOCK p21 uImage into itself, yet it hangs in early
boot before display/userspace -> the problem is now the post-kexec HARDWARE STATE
(peripherals/clocks/IRQs/secure-world left running), the classic kexec-on-ARM
handoff issue - NOT the kernel image and NOT a memory monitor. We are BLIND: no
display that early, RTD earlyprintk goes to UART only.
VISIBILITY PIVOT (2026-08-17, rex's idea "make OUR kernel output the error"):
UART declined (hardware) + pstore not compiled (CONFIG_PSTORE not set) + LG's
mmcoops/wdtlog need eMMC-up (ll_mmc_ready) so they can't capture an EARLY hang.
So we get a software-only output channel instead: WRITE THE FRAMEBUFFER directly.
fbinfo probe (built w/ the musl cross tc, ~/lgtv-toolkit/kexec-poc/canary/fbinfo)
read the live fb0: PHYS 0x39000000, len 0x1030000, 1920x1080x2 double-buffered,
32bpp, byte order [R,G,B,A] (green word=0xFF00FF00). Also fb1 exists (512x4320).
Built canary.bin: 112-byte bare-metal armv7 PIC payload (canary/canary.S) that
cycles fb 0x39000000 green->red->blue forever. kexec zImage path accepts ANY blob
(zImage_arm_probe returns 0; magic optional) - loads at a located hole, jumps to
offset 0 with MMU/caches OFF. Fire script kexec-canary-fire.sh (clears SB2+
arm_wrapper since purgatory WRITES the payload into a hole that may be monitored;
stops wdt kicker + widens TCWOV; kexecs canary.bin not the stock kernel).
STAGED on TV (/tmp/canary.bin,/tmp/kexec,/tmp/kexec-canary-fire.sh), NOT YET FIRED
(needs rex at the screen). READ = SCREEN CYCLING COLORS -> kexec transfers control
to our code, prior freeze was the stock kernel hanging LATER; STATIC/reset ->
control transfer (purgatory/jump) itself fails.
CANARY FIRED x2 (2026-08-17): NO color cycling, same freeze->reset (once auto-
watchdog-reset, once hung needing power-button recovery). Instrumented v2 writes
staged markers to STB_WDOG_DATA10 (phys 0x18060124, standby always-on domain).
RESULT: after recovery DATA10=0x00000000 (=baseline/unwritten), while neighbors
DATA11=0x2379BEEF & DATA12=0x80002000 matched baseline EXACTLY -> the standby
domain SURVIVES the reset, so our marker would have persisted if written.
*** SMP STALL FOUND + PAST IT (2026-08-17): reading the source (rex's call, not
emulation - QEMU has no RTD2875 model + the stalls ARE the hw-register polls an
emulator can't reproduce, so emulation is out) found arch/arm/mach-rtd2875/platsmp.c
FULL of UNBOUNDED hw-poll loops: while((rtd_inl(0xB805B83C)&(1<<6))); etc (lines
97-298, per secondary CPU). Post-kexec the secondaries aren't in the expected
WFI/SMP-disabled state -> those while() spin forever -> kernel hangs in smp_init
(which runs BEFORE the intmicom device_initcall pet) -> 10s watchdog reset.
FIXES (fire-custom-kernel.sh, no rebuild): (a) offline cpu1-3 pre-kexec (echo 0 >
cpuN/online) AND (b) nr_cpus=1 maxcpus=1 on cmdline (skip secondary bringup
entirely). BOTH independently got boot from ~10s -> ~21s (past SMP). So SMP was A
wall, now cleared. But a SECOND stall sits at ~21s (both fixes plateau there).
=> NEXT SESSION: comprehensive MULTI-BEACON instrument to pinpoint the 21s crash.
Beacon stages, each a distinct doubled-565 solid fill + a Micom/SoC pet:
 - GREEN=decompressor entry (compressed/head.S, DONE)
 - CYAN=__enter_kernel (compressed/head.S, DONE)
 - add BLUE in arch/arm/kernel/head.S (REAL kernel entry, MMU-OFF phase - phys fb
   0x39000000 writable) = "kernel head reached"
 - MMU-ON beacons (start_kernel init/main.c, and progressively later initcalls up
   to intmicom) need the kernel's mapped register access (rtd_outl on ioremap'd
   regs, or a fixed ioremap of 0x39000000) since phys isn't directly writable once
   paging is on. Place a pet+color at: start_kernel, end of start_kernel, first
   device_initcall, right before intmicom_init. LAST color shown = where it dies.
Fire from FRESH cold boot to see colors. head.S beacon/pet code + fire script all
in place. THIS is the debuggable custom boot - just needs the beacon rig extended
into the kernel proper.

*** BOOT MAPPED (2026-08-17): green->cyan->reset. GREEN=decompressor entry,
CYAN(shows striped on stale display but distinct, after green)=__enter_kernel
(decompression DONE, jumping to the real kernel), then RESET ~10s after cyan.
=> decompression + the jump into the decompressed kernel WORK. The stall is in the
KERNEL PROPER (arch/arm/kernel/head.S onward / start_kernel / early initcalls) -
it ran ~10s then the watchdog got it (didn't reach intmicom device_initcall pet in
the window). head.S now has: entry Micom pet + wdog widen + green fill + one pet
(no infinite loop) + at __enter_kernel a pet + cyan fill (preserves r1/r2/r4).
NEXT: beacon INSIDE the kernel. Easiest MMU-off spot = arch/arm/kernel/head.S
(the REAL kernel's head, runs MMU-off before enabling paging) - add a fb fill +
pet there (BLUE) to confirm kernel-head ran. Then need MMU-ON beacons/pets in
start_kernel/early initcall using the kernel's OWN register access (rtd_outl on
mapped regs) since 0x39000000 phys isn't directly writable once MMU is on. Goal:
find the exact stall (likely a driver probe spinning on post-kexec hw state) OR
just add an early kernel pet so it survives to intmicom_wdt_thread. Fire from a
FRESH cold boot each time to see colors.

*** PETTING WORKS (2026-08-17): head.S diagnostic PET LOOP (pet SoC wdog TCWTR=1
+ Micom mailbox cmd 0x01000605/data 0xB1 every ~few ms, holding green, never
booting) kept our custom kernel ALIVE 40s+ (vs the 10s reset). So the ~10s reset
IS a pettable watchdog and our pet sequence is correct. The whole approach works.
=> REAL PROBLEM CONFIRMED: our kernel wasn't reaching its own micom_wdt_thread
(intmicom device_initcall, starts 500ms petting) within the ~10s window when we
only petted ONCE at entry - it's HUNG or STALLED in early boot before that initcall
(likely post-kexec hardware state: a driver probe spinning on a peripheral the
bootloader would've reset). Stock survives because it boots to that initcall in a
few seconds from a fresh (bootloader-petted) window.
NEXT: boot for real (single entry pet for the window) + BEACONS at stages
(green=decompressor entry DONE; add CYAN after decompression/kernel entry; BLUE at
start_kernel) to find WHERE it stalls. If it stalls before intmicom initcall, either
sprinkle pets through early boot (head.S/early init) to extend the window, OR move
micom_wdt_thread earlier, OR quiesce the stalling peripheral pre-kexec. Pet recipe
+ all beacon/watchdog code is in arch/arm/boot/compressed/head.S.

WATCHDOG-BEAT IN PROGRESS (2026-08-17): our custom kernel shows GREEN then the
SoC watchdog resets it at ~10s. head.S beacon block writes TCWCR wden and TCWOV.
Tried: (a) wden=0xA5 + TCWOV=0x10237880 -> reset at exactly 10s (0xA5 ARMS it,
10s window). (b) bic wden (clear enable) + TCWOV=0xFFFFFFFF(max) -> STILL reset
~10s AND TV powered off (didn't auto-recover). CONCLUSION: writing TCWOV directly
does NOT take - the RTD wdog needs the wden UNLOCK (0xa5) written BEFORE TCWOV can
change (driver enable path: rtd_maskl(TCWCR,~wden,0xa5) THEN rtd_outl(TCWOV,val)).
So bic-wden without re-arm kept the OLD 10s window. Reset-reason: TCWCR wd_rst_src
field (bits17-18) 0x1=SW 0x2=WDT else power (rtk_wdog_getbootstatus); STB_WDOG_DATA4
0x1806010C cleared by stock on boot so unreadable post-hoc.
NEXT WATCHDOG ATTEMPT: proper sequence in head.S -> (1) TCWTR=0x01 clear timer,
(2) TCWCR: maskl wden=0xa5 (unlock/arm), (3) TCWOV=0xFFFFFFFF (now it takes), so
~159s window. That gives boot time; kernel's own kick thread then manages it.
Also have the Micom pet (0xB1 to mailbox 0x18060500/504, poll CTRL 0x18060574) at
head.S entry but Micom was NOT the ~10s resetter (it's the SoC wdog). Kernel builds
via kbuild2.log now (kbuild.log got tangled from pkill churn). YT: fetch-hook bypass
(restore-fetch.js) did NOT fix the mod's 2min death, adblock-off didn't either =>
NOT the injected scripts; it's the mod APP itself. Plan: inject minimal adblock into
the STOCK yt app (works) via lgtv eval; rex downloaded stock yt.

*** CUSTOM KERNEL RUNS ON THE SOC (2026-08-17) - THE MILESTONE ***
Fired our cross-compiled zImage via fire-custom-kernel.sh (KEXEC_FIXED_BASE=0x100000
-> entry 0x108000, live dtb, snapshot/resume-stripped cmdline, SB2+arm_wrapper
cleared) from a COLD-BOOTED TV -> SCREEN WENT GREEN = our custom kernel's
decompressor (arch/arm/boot/compressed/head.S beacon) EXECUTED on the SoC. First
known custom kernel booting on a secure-boot webOS TV. head.S has rex's inline
green-fill beacon at label 1: + an inline SoC-watchdog disable (TCWCR<-0xA5,
TCWOV<-0x10237880). NOTE rex's earlier RED fill above it is a no-op (r5 high half
not set until after). Extra source fix needed for the build: forward-declare
`static int micom_wdt_thread(void*);` near line 52 of
drivers/rtk_kdriver/platform/tv006/intmicom.c (used at ~1061, defined at ~1269).
NEXT: add STAGED beacons deeper in boot (after decompression jumps to the kernel
proper; at start_kernel; per early subsystem) each a distinct doubled-565 solid
color, to see how far the kernel gets before any hang = fully debuggable custom
boot. Then real driver/rootfs work toward a persistent custom OS.

*** CUSTOM KERNEL BUILDS (2026-08-17): cross-compiled LG's K7LP RTD2875 kernel
(4.4.84) from GPL source to a VALID 9.5MB arch/arm/boot/zImage (file confirms
"Linux kernel ARM boot executable zImage", magic 0x016f2818) + rtd2875.dtb.
RECIPE (in ~/lgtv-toolkit/kernel-src/kernel/linux-4.4.3):
  toolchain = arm-none-eabi-gcc 16 (pacman: arm-none-eabi-gcc/binutils; the
  bundled musl armhf tc FAILS - hard-float rejects -march=armv7-a -> kernel falls
  back to armv5t -> 'isb' assembler error). Also need bc (+uboot-tools for mkimage).
  config = the TV's live /proc/config.gz copied to .config, then `make oldconfig`.
  Source PATCHES required for modern GCC/binutils:
   - arch/arm/Makefile: remove `-rdynamic` from CFLAGS_ABI (line ~134).
   - arch/arm mm/boot .S: `,#alloc[,#execinstr]` -> `,"a"`/`,"ax"` (new GAS syntax).
   - header-guard typos handled by -Wno-error=header-guard (or fix system.h typo).
  BUILD CMD (the -w kills all warnings so GCC16's many -Werror can't fire):
   make ARCH=arm CROSS_COMPILE=arm-none-eabi- HOSTCFLAGS="-fcommon -w" \
        KCFLAGS="-w -Wno-error -Wno-error=scalar-storage-order" zImage -j$(nproc)
  => EXIT=0, arch/arm/boot/zImage produced (2544 objs).
*** CUSTOM KERNEL FIRED (2026-08-17 ~17:30): SCREEN TURNED GREEN. The green
beacon at arch/arm/boot/compressed/head.S:start+1: fired, proving our cross-
compiled RTD2875 zImage transferred control and its decompressor ran. TV went
unreachable afterward (likely watchdog reset or early-boot hang - expected first
try). This is the DE FACTO custom-boot milestone: custom code from our own
cross-compiled kernel runs on the SoC post-kexec. Build recipe, fire script, and
KEXEC_FIXED_BASE patch all confirmed working.
NEXT: add more beacons (RED at post-decompress, YELLOW at start_kernel, etc.)
to stage how far it gets; also try disabling the watchdog from the beacon so it
can run longer. Still to do: stock-YouTube works -> mod's non-adblock hooks kill
playback ~1-2min; bisect hooks/fetch.ts/sponsorblock/player_api.

*** BREAKTHROUGH 2026-08-17: 3rd canary fire => SCREEN FLASHED GREEN/cycling,
then watchdog-reset (~10s) back to stock. kexec -e DOES transfer control to our
bare-metal payload, and the framebuffer 0x39000000 IS a working software output
channel. STEP B kexec custom-boot = PROVEN END TO END (load + execute + visible
output). The DATA10 register breadcrumb was MISLEADING: stock zeroes DATA10 on
boot (that's why it read 0x00000000 despite the code running) - the SCREEN is the
source of truth, not the standby reg. So the stock-kernel "freeze->reset" is the
stock kernel getting control but hanging LATER in its own early boot (original
hypothesis restored), NOT a transfer failure.
The ~10s reset = our widened TCWOV SoC watchdog firing (we never TRULY disabled
it; TCWCR stays 0xA5 armed, we only stopped the kernel kicker + widened TCWOV).
FIX for indefinite runtime: our payload can PET the SoC watchdog itself - write
WDOG_TCWTR 0x18062208 = 0x01 every loop iteration to keep the SoC alive.
CANARY V3 = the software UART (framebuffer text console), built in freestanding
C: canary/{start.S,canary3.c,link.ld,gen_font.py->font.h}, kexec-canary3-fire.sh.
Build: arm-linux-musleabihf-gcc -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard
-O2 -ffreestanding -fno-pic -fno-pie -nostdlib; link -static -no-pie -Wl,-N
-T link.ld; objcopy -O binary. HARD-WON LESSONS from the v3 bring-up:
 1. CACHES ARE ON at kexec entry -> writes go to cache not DRAM (garbled screen)
    AND watchdog-pet writes never reach the register. start.S MUST disable MMU+
    I/D caches (clear SCTLR M/C/I + ICIALLU/BPIALL/TLBIALL) first thing. DONE.
 2. The "striped/grid" framebuffer was NOT tiling and NOT our code - it was STALE
    DISPLAY STATE from stacking kexecs without a clean reboot. From a FRESHLY
    COLD-REBOOTED stock TV the fb is clean LINEAR and a solid fill shows perfect
    solid color. Geometry (1920x1080, stride 7680, 32bpp [R,G,B,A], fb@0x39000000)
    is correct as-is. => Always fire the fb console from a fresh reboot.
 3. Hardfloat toolchain needs -mfpu even with no floats; watch -O2 NEON autovec
    (none emitted here, but check: NEON is off at entry, would fault).
 4. The ~10s reset that survives SoC-wdt petting = the EXTERNAL MICOM watchdog
    (separate MCU, can't pet via SoC regs). Gives ~10s of runtime - enough to
    display/read/photograph. Beating it (for indefinite runtime / a custom kernel
    that must survive) = future work: find how stock pets Micom (GPIO/I2C heartbeat).
CRITICAL FINDING (2026-08-17, after many fires): kexec -e TRANSFER IS INTERMITTENT.
The SAME solid-green binary yields DIFFERENT results across fires: clean green once,
striped twice, nothing/freeze twice. Not a payload bug - the transfer/relocate is
marginal on this locked SoC. Likely cause: kexec's zImage loader uses locate_hole()
to pick a DIFFERENT physical load address each fire; some land in secure/monitored/
bad regions -> wedge before _start; some work. Also display-controller state after a
Micom reset (vs full cold boot) affects whether output renders linear vs striped.
=> BEFORE more framebuffer-console work, make the transfer DETERMINISTIC: force a
FIXED, known-safe load address instead of locate_hole (investigate kexec --load-addr
or reserve/pin a region; the crashdump path uses a fixed base as a model), and always
cold power-cycle (not Micom-reset) before firing so the display is clean linear.
Only then iterate on readable text. The striping vs clean is display-state; the
nothing/freeze is the flaky transfer.
FIXED-ADDRESS PATCH = the determinism fix (WORKS): patched kexec-tools zImage
loader (kexec-tools-2.0.28/kexec/arch/arm/kexec-zImage-arm.c) to honor env
KEXEC_FIXED_BASE instead of locate_hole(). Rebuilt binary = kexec-poc/kexec-arm-fixed
(prints "[patched] forcing load base=..."). Fire with KEXEC_FIXED_BASE=0x100000 =>
entry 0x108000 (the bootloader's own kernel addr, cp2ram kernel 0x108000). With
this, a solid-green payload rendered CLEAN & reliably (vs random before). So the
locate_hole random offset WAS the intermittency. Fire scripts: fire-fixed-green.sh,
fire-fixed-text.sh (both export KEXEC_FIXED_BASE, clear SB2+arm_wrapper).
DISPLAY MECHANISM (from drivers/rtk_kdriver/rtk_venusfb.c): the panel is driven by
a separate VIDEO co-processor (VCPU), NOT ARM MMIO. ARM configures the graphics
plane by send_rpc_command(RPC_VIDEO, RPCCMD_DRAW_GRAPHIC_WIN/CONFIG_GRAPHIC_CANVAS)
carrying pImage[0]=phyAddr, imgPitch[0]=pitch, compressed flag. The VCPU keeps
compositing our fb (0x39000000) across our ARM kexec = why our DRAM writes show.
STRIPING = the VCPU left configured with a non-standard pitch/format by an
interrupted/Micom-reset state; our linear writes then composite wrong. A full COLD
POWER-CYCLE resets the VCPU to standard linear 1920x1080 pitch 7680 compressed=0 ->
clean. So DISCIPLINE: cold-boot -> single fire = clean. Robust future fix: have the
payload send its OWN RPC_VIDEO DrawGraphicWin to force pitch/addr/compressed=0
(RPC = shared-mem ring + VCPU interrupt; deep).
TEXT-CONSOLE STATUS (photo /home/rex/Downloads/20260817_124123.jpg analyzed):
whole screen = a UNIFORM FINE purple/white MICRO-GRID edge to edge, NOT sheared
text and NOT solid. So the blocker is NOT pitch and NOT visible-page - it is the
OSD plane's PIXEL FORMAT / TILING. Our linear 32bpp bytes (purple COL_BG=0xFF101018
= repeating 18 10 10 FF) get composited as that regular grid. Solid GREEN looked
"clean" only because 00 FF 00 FF is a trivial pattern reading as green; ANY
structured content (text) dissolves into the micro-grid. Two fixes already landed
and hold: (a) FONT/.rodata linking - link.ld now `. = 0x108000` so absolute refs
resolve (was garbage); (b) KEXEC_FIXED_BASE determinism. The REMAINING wall is the
plane format: must drive it via VCPU RPC, not raw linear writes.
BLIND-POKE DEAD END (confirmed after ~30 fires): every SPATIALLY-STRUCTURED draw
dissolves into a fine pixel pattern regardless of format guess - 32bpp=grid,
16bpp RGB565=positioned writes invisible/black, 8 horizontal color bands rendered
as fine VERTICAL purple/black stripes. Only a UNIFORM full-screen fill of a simple
color (green 0xFF00FF00) renders clean; positioned graphics never land. So we
CANNOT derive the plane format/geometry from verbal feedback. Also confirmed: it's
NOT a 16-vs-32-bit write issue (32-bit positioned block also invisible) and NOT
just page offset. The real scanout base/pitch/format/tiling is unknown and must be
READ from the hardware, not guessed.
=> NON-BLIND NEXT STEP (do this instead of more fires): on the RUNNING stock TV,
find the display/OSD plane SCANOUT registers (base addr + pitch + format) in the
source (drivers/rtk_kdriver/tvscalercontrol/vo/rtk_vo.c + gal/rtk_osdcomp_driver.c
+ rtk_venusfb.c) and READ them live via devmem while the webOS UI is on screen -
that gives the ACTUAL base/pitch/format the panel uses NOW. Draw with those exact
values. The OSD plane likely uses COMPRESSION (rtk_osdcomp) and/or tiling, which
is why raw linear writes scramble. If so, raw framebuffer drawing may be
infeasible without the compression format -> then either disable osdcomp or use
the RPC path below.
DEFINITIVE (photo 20260817_160733.jpg): posmap test = 8 CONTIGUOUS 4MB memory
chunks of distinct doubled-565 colors rendered as a UNIFORM FINE STRIPE pattern
(not 8 regions) => consecutive memory does NOT map to consecutive screen pixels =>
the OSD plane is TILED/INTERLEAVED (FBDC). Full solid fill works only because all
pixels equal. Raw-linear text is INFEASIBLE without the tile/compression mapping,
which is NOT in the GPL source (auto-gen GDMA/FBDC reg headers not shipped). Two
real paths remain for graphics: (A) RPC DrawGraphicWin compressed=0 -> VCPU sets a
LINEAR plane on our buffer (the venusfb path; needs RPC ring RE); (B) reverse the
FBDC tile mapping. Both substantial. For the CUSTOM-KERNEL goal, solid-color
beacons are sufficient and reliable NOW.
ALTERNATIVELY the reliable primitive that WORKS today: uniform full-screen solid
color. For the real goal (see where a chain-loaded stock kernel hangs) encode
boot-stage as SOLID COLOR changes - 100% reliable, format-invariant.
=> RPC PATH: implement send_rpc_command(RPC_VIDEO, RPCCMD_DRAW_GRAPHIC_WIN)
from our payload to set pImage=our buf, imgPitch, width/height, compressed=0 AND
the correct color format, so the VCPU composites our buffer as plain linear RGBA.
Reverse the RPC transport from rtk_venusfb.c + the RPC ring (shared mem + VCPU
interrupt doorbell); find the format enum the stock fbdev uses. Until then, only
SOLID full-screen color is a reliable channel (pitch/format-invariant) - could
encode boot-stage as color codes as a fallback. Pitch-finder build (4 labels)=
canary3.c current; drew nothing readable (confirms format not pitch). Files wipe
/tmp each reboot: re-scp kexec-arm-fixed->/tmp/kexec + canary3.bin + fire-fixed-
text.sh every time; fire from cold boot. SoC can boot aarch64
(bootcode CONFIG_JUMP_TO_64BIT). Source at ~/lgtv-toolkit/kernel-src/.

SOURCE: full LG webOS 6.0 K7LP GPL downloaded to ~/lgtv-toolkit/kernel-src/
(klp_1/2/3.tar.gz, ~5.5GB; releaseId 510320 via reverse-engineered LG portal
download API - POST /download/releaseFileDownloadUrl osSeq/fileType=Op/fileIdx/
modelName=55UP81009LR). Extracted: kernel/linux-4.4.3 (mach-rtd2875), bootcode/.
2021 source vs 2025 firmware (03.53.45) - boot code stable, fine for this.
No public prior art: nobody has custom-booted a secure-boot webOS TV (community
stops at root+modules). RESCUE-from-USB boot path exists (rtk_plat_run_rescue_
from_USB) as an unexplored alt vector.

Deepest reachable software boundary for novel bugs: normal->secure world.
/dev/tee0 + /dev/teepriv0 are world-accessible (crw-rw-rw-); OP-TEE SMC/TEEC
interface is the least-audited surface. Auditing it (not key extraction) is the
"something unheard of" frontier.

"Better than luna": luna = ls-hubd, JSON over unix socket + permission gate,
~302 services, all introspectable (ls-monitor -i). A typed/schema-generated
low-latency RPC layer over it is buildable NOW in userspace, zero exploits,
carries into the custom OS later.

## 2026-08-18 session: custom kernel boot debugging (major progress)

Breadcrumb method WORKS and is the primary debug tool. Register **0x18060110**
survives the watchdog WARM reset; stock never writes it (control test: seed
0xEE, plain `reboot`, still reads 0xEE). Read it after each fire = exact last
checkpoint. Only 0x18060110 is safe (0x114/0x120 hold stock values).

Build loop fixed (was ~6min, now ~20s):
- `CONFIG_DEBUG_INFO=n` -> vmlinux 177MB -> 29MB (link/objcopy/gzip was the cost).
  Backup of old config at .config.bak-debuginfo.
- ccache installed; ALWAYS build with `CROSS_COMPILE="ccache arm-none-eabi-"`
  (changing the CROSS_COMPILE string invalidates kbuild cmd cache = full rebuild).
- Helper scripts: kexec-poc/fire.sh (push+fire+wait+read crumb),
  kexec-poc/iterate.sh (build+fire+read). Beware: piping their output can swallow
  it; read the breadcrumb with a direct ssh devmem instead.

BUGS FOUND AND FIXED (all were self-inflicted or kexec-specific):
1. **Our own debug write killed the boot.** A leftover `str` to 0x18061C00 in
   head-common.S ran right after MMU-enable with no mapping for it -> data abort.
   Fixed by identity-mapping the 0x18000000 register section in
   __create_page_tables (device, XN). This was the "crashes between MMU-on and
   mm_init" mystery for hours.
2. **Decompressor destroyed the DTB pointer.** Our injected pet/beacon block at
   label 1: in arch/arm/boot/compressed/head.S runs BEFORE `mov r7,r1 / mov r8,r2`
   and its delay loop counts **r2 down to 0** -> kernel got r2=0 ->
   setup_machine_fdt(0)=NULL. Fixed: stash r1/r2 in r10/r11 at the top of the
   block, restore before the decompressor saves them.
3. **Kernel overwrote our DTB with an empty built-in stub.** CONFIG_BUILD_ARM_
   APPENDED_DTB_IMAGE_EXT=y makes setup_arch memcpy __dtb_start over
   phys_to_virt(__atags_pointer). The built-in DTB's /memory node is **zero-sized**
   (bootloader normally fills it from ATAGs via atags_to_fdt; kexec passes a DTB,
   not ATAGs). Result: memblock empty -> **arm_lowmem_limit = 0** -> map_lowmem()
   breaks on the first region and maps NOTHING -> kernel text/data unmapped, boot
   dies on the first page-table walk. Fixed in arch/arm/kernel/setup.c: only do the
   memcpy when __atags_pointer does NOT already point at OF_DT_HEADER.
   (The live /sys/firmware/fdt correctly declares 1.5GB: 0x0+512MB, 0x20000000+1GB.)

OPEN / CURRENT BLOCKER (where the session ended):
Boot now reaches __turn_mmu_on (crumb 0x20) but never __mmap_switched (0x21).
PROVEN by diagnostics: page tables are CORRECT (PMD for VA 0xa1300000 =
0x01311C0E -> PA 0x01300000 cacheable RW; device PMD = 0x18000C12), and the jump
target is correct. BUT **literal-pool reads return stale garbage**: `ldr r13,
=__mmap_switched` returned **0x20** while vmlinux has 0xa13002e0 at that word
(a01081d4) -- adjacent pool words (a01081d8/dc) read fine. Hardened r13 and
`ldr r6,=(_end-1)` to movw/movt immediates and hardcoded the __turn_mmu_on jump
target, but it still stops at 0x20, so more literal loads are affected.
Root-cause hypothesis: **stale RAM / cache incoherency from the kexec handoff** --
kexec loads the zImage at 0x108000 and the kernel decompresses to 0x108000
(source and destination OVERLAP). Untested next step: set KEXEC_FIXED_BASE well
clear of 0x108000 (e.g. 0x4000000) in kexec-poc/fire-custom-kernel.sh -- no
rebuild needed -- so the decompressor never writes over its own source.

TV: 192.168.2.103, root, port 22, key ~/.ssh/tv_key. /tmp is wiped by every
watchdog reset, so re-push kexec + zImage + fire script before EVERY fire.
Kernel layout: PAGE_OFFSET=0xA0000000, PHYS_OFFSET=0, zreladdr=0x108000,
_text=0xa0108000, _stext=0xa0200000, __init_end=0xa1400000.

rex's stated end goal for this: once the kernel boots to userspace, the FIRST
thing is permanently installing the fixed YouTube app -- that was the original
point, the kernel is just the vehicle.

## 2026-08-18 (later): CUSTOM KERNEL REACHES USERSPACE

From-source linux-4.4.3 kexec'd on the TV now runs start_kernel to completion,
mounts root, and execs /sbin/init. Confirmed by crumb 0x92 (init/main.c, the
line before try_to_run_init_process) with the 0x93 "no working init" panic
crumb never reached, and last initcall = alsa_sound_last_init (the final
late_initcall, so do_basic_setup completed).

**The multi-hour "MMU bug" was a ghost.** Crumb value 0x20 was ambiguous: our
asm crumb in __turn_mmu_on AND a pre-existing boot_breadcrumb(0x20) at
init/main.c:1041, which is the FIRST LINE OF kernel_init() -- i.e. after all of
start_kernel. Reading 0x20 always meant "booted fine", not "died at MMU".
Lesson: crumb values must be unique tree-wide; grep before assigning.

Real blockers found (each named by stamping the initcall fn pointer):
- rtk_sb2_driver_init (drivers/rtk_kdriver/hw_monitor/rtk_sb2.c, via
  module_platform_driver) hangs. Fixed: CONFIG_RTK_KDRV_SB2=n.
- GDMA_init_module (drivers/rtk_kdriver/gal/rtk_gdma.c) hangs. Skipped via
  cmdline, no rebuild needed.

### Method that worked
- Boot the RAW uncompressed Image, not zImage: wrap with
  `mkimage -A arm -O linux -T kernel -C none -a 0x00108000 -e 0x00108000`.
  kexec strips the uImage header and loads at 0x108000 = link address, so the
  decompressor (and all its self-relocation bugs) is removed from the picture.
  TEXT_OFFSET is 0x00108000 for RTK2875, so zreladdr is 0x108000 regardless of
  load base.
- CONFIG_KALLSYMS=y, so `initcall_blacklist=fn1,fn2` on the cmdline skips
  hanging drivers WITHOUT a rebuild. ~2 min iteration instead of ~4.
- Surviving storage across the watchdog reset is exactly TWO words:
  0x18060110 (stage) and 0x1806010C (payload, e.g. the initcall fn pointer,
  resolved against System.map). 0x18060108/0x1806011C/0x18060124 get cleared by
  stock; 0x18060118/0x18060128 hold stock magic 0x2379BEEF -- do not touch.
- DRAM does NOT survive as a log buffer. Addresses in System RAM get reused by
  stock on reboot; the unclaimed holes (0x16000000, 0x1A900000, 0x1B200000,
  0x40000000) contain firmware/vector-table code that is reloaded each boot.
  So "log every fault at once" is not available -- two words is the budget.
- objdump/System.map answer most questions for free. Disassemble before firing.

### Next
Kernel boots to init but the box still watchdog-resets (webOS stack does not
come up: SB2/GDMA disabled, nothing pets the micom from userspace). Next step
is a minimal userspace/init that pets the micom, not more kernel debugging.

### Micom watchdog: NOT the blocker (measured)
The stock micom keepalive already exists and RUNS in our kernel:
drivers/rtk_kdriver/platform/tv006/intmicom.c :: micom_wdt_thread, a kthread
started by intmicom_init_module (device_initcall, CONFIG_REALTEK_INT_MICOM=y).
It calls do_intMicomShareMemory({0xB1} /* CP_READ_MICOM_STATUS */, 1, WRITE)
every 500ms forever. Instrumented it with a pet counter in 0x1806010C and read
0x11C10006 after the reset = 7 successful pets ~= 3.5s of petting. So the micom
is being fed correctly and is NOT what kills the boot.

Corroborating: jiffies stamped at the exec-init point showed ~972 ticks past
INITIAL_JIFFIES at CONFIG_HZ=250 = ~3.9s of real timer ticks. Timer interrupts
work fine (an earlier "no timer IRQ" hypothesis of mine was WRONG - SB2 and
GDMA were genuine per-driver hangs, not clock starvation).

CONFIG_PANIC_TIMEOUT=0, so a kernel panic would HANG, not reboot. The box does
reboot => not a panic. Leading suspect is webOS's own /sbin/init deciding the
system is broken (SB2 + GDMA missing) and rebooting deliberately.

**Instrumentation trap hit twice**: do_one_initcall was stamping every initcall
pointer into 0x1806010C, clobbering later markers (intmicom is device_initcall
level 6, alsa_sound_last_init is late_initcall_sync and overwrote it). Only one
component may own the payload word at a time.

### Next test (needs someone physically at the TV)
Fire with `init=/bin/sh` appended to the cmdline (NO rebuild needed) to bypass
webOS init entirely. If the kernel survives, the micom thread pets forever, so
there is NO watchdog reset and the TV will NOT come back by itself -- it needs a
manual power pull. The pet counter in 0x1806010C survives that power cycle, so a
large value afterwards proves the kernel ran userspace for minutes.

### The ~3.5s wall (three fires, identical)
Death is perfectly deterministic at 7 micom pets (payload 0x11C10006, ~3.5s):
  - webOS /sbin/init          -> crumb 0x92, 7 pets
  - init=/bin/sh              -> crumb 0x91, 7 pets
  - init=/bin/sh + blacklist rtk_wdt_init,wdt_log_init -> crumb 0x91, 7 pets
(crumb 0x91 not 0x92 with init= set is correct: execute_command branch returns
before the 0x92 marker. A failed exec would panic->HANG, not reboot, so /bin/sh
really did exec and userspace really ran.)

RULED OUT as the killer: webOS init, rtk_wdt_init/wdt_log_init (SoC watchdog
driver), micom pet thread not running, timer interrupts, kernel panic.

REMAINING HYPOTHESIS: the external micom 8051 MCU. It is already live and
mid-protocol when we kexec; our kernel re-runs intmicom_init_module against it
and do_intMicomShareMemory likely never lands as a valid keepalive, so the
thread counts pets the MCU never accepts and the MCU resets the board on its own
~4s timer. Next step would be reverse-engineering the micom handshake
(do_intMicomShareMemory + the micom IRQ/ack path), NOT more initcall blacklists.

### The wall characterised precisely: a hard ~3.0s TIME-BASED deadline
Resolution test settles it. Micom keepalive loop at 500ms -> 7 pets (3.5s);
same loop at 100ms -> 30 pets (3.0s). So death is TIME-based (~3s), not
iteration-based. Payload encoding used: 0x11C1<count><ret> in 0x1806010C.

Also proven: do_intMicomShareMemory returns 0 (SUCCESS) on every pet, so the
micom I2C keepalive genuinely works. The micom is an I2C SLAVE at address 0x29
on channel 0 (LG_MICOM_ADDRESS/LG_MICOM_I2C_CHANNEL in rtk_emcu_export.h) --
NOT the raw register pokes at 0x18060500/0x504 we had been using; those were
never a real pet.

NOW RULED OUT as the killer (all give identical ~3s death):
  - micom I2C keepalive (succeeds, ret=0)
  - SoC watchdog TCWCR/TCWOV/TCWTR (unlocked, widened to 0x0FFFFFFF, kicked
    every loop from a thread proven to run)
  - webOS /sbin/init (init=/bin/sh identical)
  - rtk_wdt_init / wdt_log_init (blacklisted, identical)
  - kernel panic (PANIC_TIMEOUT=0 would hang, not reboot)
  - GPIO heartbeat: none exists. PIN_SYSTEM_DEBUG is an INPUT listener, and
    that code path is not even compiled with CONFIG_REALTEK_INT_MICOM=y.
  - wdt_detect_rtk/wakeup_wdt_thread is a CRASH LOGGER, not a keepalive.

LEADING REMAINING HYPOTHESIS (untested): the secondary CPUs. The fire script
offlines CPU1-3 and passes nr_cpus=1 maxcpus=1, so they park in STOCK kernel
code (cpu_die/wfi) at physical addresses our kernel then overwrites with its own
image. Any IPI / broadcast timer waking them executes garbage -> bus fault ->
board reset. A ~3s periodic tick fits. Testing this means parking the
secondaries somewhere our image does not clobber, or holding them in a reserved
trampoline before kexec -e.

### Source-reading pass (bootcode + kernel) -- corrections and eliminations
CORRECTION, important: TCWCR polarity is INVERTED from what earlier notes said.
From LG bootcode uboot/examples/instant_boot/board_merlin.c:
    WATCHDOG_DISABLE(): rtd_maskl(TCWCR, ~0xFF, 0xA5)   -> 0xA5 = DISABLE
    WATCHDOG_ENABLE():  rtd_maskl(TCWCR, ~0xFF, 0x00)   -> 0x00 = ENABLE (WDEN)
    WATCHDOG_KICK():    rtd_outl(TCWTR, 1)
    Regs: TCWCR 0x18062204, TCWTR 0x18062208, TCWNMI 0x1806220c, TCWOV 0x18062210
    Scale: TCWOV 0x08000000 ~= 9.32s (~14.4MHz tick).
Writing 0xa5 (which earlier notes called "unlock") actually DISABLES it.

LIVE STOCK STATE (measured, TV running normally):
    TCWCR=0x000000A5 (DISABLED)  TCWTR=0  TCWNMI=0xFFFFFFFF  TCWOV=0x066FF380
So the SoC watchdog is OFF on stock and is definitively NOT the 3s killer.

CORRECTION: the micom is NOT I2C for the keepalive path. do_intMicomShareMemory
uses the shared-memory register block: COMMAND 0xB8060500, DATA 0xB8060504,
CTRL 0xB8060574 (phys 0x1806xxxx). It polls CTRL==0 for the 8051 to release.
It returns 0 (success) in our kernel => the 8051 is alive and answering us.
(LG_MICOM_ADDRESS 0x29 / channel 0 in rtk_emcu_export.h is a different path.)

ELIMINATED BY EXPERIMENT ON THE LIVE STOCK BOX: micomservice.service is NOT the
keepalive -- `systemctl stop micomservice` and stock survived 8s+ fine.

ALSO NOT THE KILLER (source-read): rtk_wdog_init_module (drivers/rtk_kdriver/
rtk_watchdog.c) only registers a chrdev + NMI IRQ, it arms nothing.
rtk_machine_restart is not on the kexec path (kexec -e goes through
machine_kexec, not machine_restart). No AVCPU/RPC watchdog found that resets.

STILL UNIDENTIFIED: what imposes the hard ~3.0s deadline after kexec. Best
remaining lead stays the secondary CPUs parked in stock code we overwrite
(see previous section).

### Crash-log channels (the "no console" workaround) -- checked, both empty
Stock cmdline carries `mmcoops=dump` and `wdtlog=dump@1M`, i.e. two crash
persistence paths onto an eMMC partition named "dump":
  - mmcoops (drivers/staging/webos/logger/mmcoops.c): a kmsg_dumper, fires on
    PANIC, magic KMSG_DUMP_MAGIC 0xBE1953D6, written at partition start.
  - wdt_log (drivers/staging/webos/logger/wdt_log.c): magic 0xFA14EB59 at
    partition+1M, triggered via the micom system-debug IRQ -> wakeup_wdt_thread.
Partition name resolution is Realtek's (lgemmc_get_partnum), NOT GPT: there is
no /dev/disk/by-partlabel and no PARTNAME in uevent on this box.
Scanned all 59 mmcblk0p* for both magics (offset 0 and offset 1M): NEITHER magic
found anywhere. So no panic dump and no wdt log was ever written by our kernel.
Caveat: absence is not proof -- mmcoops needs the low-level eMMC driver up in
OUR kernel, which is unverified.

Also confirmed: no `panic=` on the cmdline and /proc/sys/kernel/panic = 0, so a
panic HANGS rather than reboots.

=> Most consistent story now: our kernel HANGS (not panics) at ~3.0s; the micom
then notices the SCPU is dead and resets the board. Every watchdog chased this
session was downstream of that hang, which is why none of them moved the number.

### MAJOR CORRECTION: there is NO 3-second timer. Death is AT userspace handoff.
The "hard ~3.0s deadline" in the notes above was my misreading -- the micom pet
counter starts at device_initcall, so its ~3s was simply BOOT DURATION, not a
countdown. 7x500ms and 30x100ms both landed near 3s because both measured the
same boot, not a fuse.

Proved with an INDEPENDENT liveness kthread (late_initcall) stamping a counter
into the payload word 0x1806010C, with the micom thread's writes removed so the
register is uncontested:
    LIVENESS = 0xC0000002  (writes at n=0,1,2 => only ~200-300ms of life)
    reproduced bit-identically across runs
So the kernel dies ~200-300ms after late_initcall, i.e. immediately after crumb
0x92 (the exec of /sbin/init), NOT after 3 seconds.

panic() instrumented at its first line to stamp 0x0DEAD000 -> crumb stayed 0x92,
so panic() is NEVER REACHED. No panic, no oops, no mmcoops dump, no wdt log.
The kernel stops dead at the userspace handoff.

NEXT LEAD (untested, strongest): SB2, Realtek's illegal-bus-access monitor.
We set CONFIG_RTK_KDRV_SB2=n, which removes the DRIVER but does NOT disarm the
SB2 HARDWARE -- it is still armed from the stock kernel across the kexec. An
illegal access at the first userspace instruction would reset the board
instantly with no panic and no log, which matches every observation including
the total silence. The fire script's `echo "set$i clear" > /sys/realtek_boards/
sb2_dbg` loop (i=1..16) may be insufficient. Investigate the SB2 register block
and disarm it in our kernel's early boot, or fully before kexec -e.

### EXACT DEATH SITE FOUND (bisected, reproducible)
Chain of crumbs, each a separate fire, all bit-identical on repeat:
  0x92 reached      -> kernel_init, about to exec /sbin/init
  0xE1/0xE2 reached -> do_execveat_common, search_binary_handler
  0xE3 reached      -> load_elf_binary ENTERED
  0xE5 reached      -> load_elf_phdrs OK (program headers read from squashfs)
  0xEB reached      -> flush_old_exec RETURNED (exec_mmap survived! first user mm)
  0xEC reached      -> setup_new_exec OK
  0xF0 reached      -> about to call shift_arg_pages()
  0xF4 NEVER        -> first line INSIDE shift_arg_pages not reached
  0xE4/0xE7/0xED    -> never (no elf_map, never reached start_thread)
=> Dies in the shift_arg_pages() call itself or in its find_vma(), i.e. during
   user PAGE-TABLE work (move_page_tables / free_pgd_range / mmu_gather).
   NOTE: it never enters user mode at all -- start_thread is never reached, so
   "first userspace instruction" theories are dead.

TIMING (measured, not inferred): liveness kthread at 10ms resolution; the exec
point stamps the live tick count. AT_EXEC=0x92000010, FINAL=0xC0000010 -- same
tick. So death is WITHIN 10ms of reaching exec. ~160ms from late_initcall to
exec (16 ticks).

INSTRUMENTATION VALIDATED: with mem=256M the kernel panicked early and the panic
crumb DID fire (STAGE=0x0DEAD000). So panic() instrumentation genuinely works,
which confirms the earlier negatives: in the normal failure panic() is truly
NEVER called. Death is silent -- no panic, no BUG_ON trap, no -EFAULT return
path (all three would have surfaced a crumb). (mem=256M is too aggressive a
clamp -- DTB reserved-memory/CMA lives above 256M -- so it did not test the
memory hypothesis.)

### STRONGEST LEAD NOW: we deleted the bus-error handler
drivers/rtk_kdriver/hw_monitor/rtk_sb2.c defines
    int sb2_buserr(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
    int sb2_timeout_handler(unsigned long addr, unsigned int fsr, ...)
i.e. SB2 bus errors are delivered as ARM DATA ABORTS via an abort-handler hook
(hook_fault_code), and this driver is the handler. Setting
CONFIG_RTK_KDRV_SB2=n to stop rtk_sb2_driver_init from hanging ALSO removed the
bus-error handler, and left the SB2 HARDWARE armed from the stock kernel.
Registers (virt 0xB801xxxx = phys 0x1801xxxx): SB2_DBG3_CTRL_0 0xB801AB40,
SB2_DBG3_INT 0xB801AB70, SB2_DBG3_0_0 0xB801AB10, plus SB2_INV_INTSTAT,
SB2_DBG_INT, SB2_DEBUG_REG.
Next: either disarm SB2 in hardware early in our boot, or re-enable the driver
and work out why rtk_sb2_driver_init hangs (that hang may be a SYMPTOM of the
same underlying fault, not an independent bug).

## *** USERSPACE RUNS ON THE CUSTOM KERNEL (2026-08-18) ***
STAGE = 0xA6000EA8 = user-mode page-fault counter = 0xEA8 = 3752 USER page
faults. Counter is incremented in do_page_fault() only when user_mode(regs), so
this is proof that /sbin/init actually EXECUTED user code (mapping libraries,
running) -- not merely that exec was reached. Also reached crumb 0xA5 (after
start_thread) and 0xE4 (before start_thread).

### ROOT CAUSE OF EVERY "SILENT DEATH": our own instrumentation
PAGE_OFFSET = 0xA0000000. Our crumb writes used the PHYSICAL address 0x18060110,
which only worked via the identity section we installed in swapper_pg_dir's USER
half (debug_crumb_remap in arch/arm/mm/mmu.c). A freshly exec'd mm does NOT
inherit that: pgd_alloc copies only the KERNEL half. It survived a bit longer on
a stale global TLB entry, until shift_arg_pages' free_pgd_range + TLB flush.
Then the crumb write faulted -> abort handler -> panic() -> and panic()'s OWN
first line was another crumb write to the same dead address -> fault inside
fault -> totally silent hang. That is why panic() "was never reached" and why
every watchdog theory failed: THE INSTRUMENTATION WAS THE BUG.

FIX: late-running code must use the KERNEL virtual mapping, not the physical
address. From arch/arm/mach-rtd2875/include/mach/iomap.h:
    RBUS_BASE_PHYS 0x18000000, RBUS_BASE_SIZE 0x00200000
    RBUS_BASE_VIRT 0xFE000000     (0xB8000000 is RBUS_BASE_VIRT_OLD, legacy!)
    GIC_BASE_VIRT  0xFD000000
So phys 0x18060110 -> virt 0xFE060110. Driver code gets away with 0xB806xxxx
constants only because rtd_inl()/rtd_outl() translate them; RAW POINTER writes
must use 0xFE06xxxx. Converted: fs/exec.c, fs/binfmt_elf.c, kernel/panic.c,
arch/arm/mm/fault.c, intmicom.c, and the late helpers in init/main.c
(boot_crumb_live + liveness thread). Early code (head.S, setup.c, mmu.c,
mach-rtd2875) must KEEP physical 0x1806xxxx -- it runs before/around map_io.

THE ACTUAL KILLER was my own boot_wdog pet timer in init/main.c: it wrote the
PHYSICAL 0x18062208 from timer softirq every 250ms, so it faulted the moment a
user mm was installed. Caught by stamping regs->ARM_pc and addr in
__do_kernel_fault -> FAULT_ADDR=0x18062208. Deleting that timer moved the boot
from dying in shift_arg_pages all the way to running userspace.

### Technique that cracked it
- Stamp __builtin_return_address(0) in panic() -> resolve in System.map (gave
  "die+0x20c", proving a real oops rather than a hardware kill).
- Stamp regs->ARM_pc and addr in __do_kernel_fault, with a volatile
  boot_fault_stop flag so other threads stop writing and freeze the evidence.
  NOTE: panic()'s stamp overwrites the fault PC (same register) -- read the
  ADDRESS from the payload word, not the PC.
- Count user_mode(regs) page faults to prove userspace really executed.

## 2026-08-19: custom init + initramfs attempt
CONFIRMED EARLIER RESULT WAS INVALID: the `init=/bin/sh` test that "failed" was
run BEFORE the boot_wdog pet-timer bug was found (that timer wrote the PHYSICAL
0x18062208 from softirq and faulted once a user mm existed). After removing it,
firing with `init=/bin/sh` left the TV **alive, green screen, SSH dead, no reset
until power was pulled** => the kernel runs a custom init fine. webOS calling
reboot() was the only thing ending earlier boots.

### SoC watchdog kills long boots -- FIXED
Symptom: box died mid-boot, payload register held an address inside `my_panic`
(a0737724), which sits directly before `wdog_NMI_intr` in rtk_watchdog.c. So the
watchdog NMI fired during boot and panicked. Cause: a 9MB initramfs adds seconds
of decompression and nothing pets the watchdog until userspace.
FIX: disable it early in start_kernel (added just before boot_breadcrumb(0x30)
in init/main.c): `*(volatile unsigned *)0x18062204 = 0xA5;`
0xA5 = DISABLE, 0x00 = ENABLE (from LG bootcode macros). After this the TV no
longer resets itself mid-boot.

### Networking prerequisites all verified OK
Stock wifi module /lib/modules/4.4.84-723.kcl4tv.20/kernel/wlan_mt7663.ko has
`vermagic=4.4.84 SMP preempt mod_unload ARMv7` and `depends=` (empty!). NOTE the
vermagic is plain **4.4.84** -- NOT the -723.kcl4tv.20 suffix the modules dir is
named after. So build with **CONFIG_LOCALVERSION=""** (not the LG suffix, and
not -rexos). Confirmed our kernel then produces a byte-identical vermagic.
Also verified: MODULE_SIG not set, MODVERSIONS not set, CFG80211/MAC80211/USB
built in, DEVMEM=y (so busybox `devmem` gives userspace breadcrumbs too).

### OPEN BLOCKER: initramfs never executes
Built initramfs = busybox + /lib/{ld-linux.so.3,libc.so.6,libm.so.6,
libresolv.so.2} pulled off the TV + /init script (9MB gz). Image verified valid
(cpio lists `init` -rwxr-xr-x at root, busybox executable). Needed because the
squashfs root is read-only and /media/developer is NOT mounted when the kernel
execs init.
But /init never runs: no userspace crumbs, no log, and no self-reset (which was
unconditional after 60s). Kernel itself is fine (no panic once watchdog is off).
=> Suspect kexec is not passing the initrd. On ARM the initrd is handed over via
the DTB /chosen `linux,initrd-start`/`linux,initrd-end`, and we pass our own
`--dtb=/tmp/tv.dtb` (the live fdt), which may override kexec's patched copy.
NEXT: run `kexec -d -l /tmp/uImage-custom --dtb=... --initrd=... ` and check the
debug output for an initrd segment. If absent, drop `--dtb=` (let kexec generate
one) or inject the initrd properties into the DTB manually.

### Debug-loop improvements worth keeping
- A power pull CLEARS the standby crumb registers; only a WARM reset preserves
  them. Two runs' evidence was lost that way. The init now self-triggers a warm
  reset (`devmem 0x18062204 32 0x80000000` = enable + im_wdog_rst bit31) so each
  iteration returns to stock by itself with crumbs intact.
- The kernel's exec crumbs (0xE1/0xE2 in fs/exec.c) fire on EVERY exec and
  clobber userspace breadcrumbs -- removed.

### initrd handoff: what was ruled out (2026-08-19, late)
- kexec DOES stage the initrd correctly. `kexec -d -l ... --dtb=... --initrd=...`
  reports `nr_segments = 3` with segment[1].mem=0x8088000 and bufsz matching the
  initramfs byte-for-byte. So "kexec is not loading it" is WRONG.
- Dropping `--dtb=` (to let kexec generate a DT with initrd /chosen props) makes
  things WORSE: kernel then stalls at crumb 0x95 inside do_basic_setup with no
  panic (payload still the 0xEE seed) and the box resets. The live fdt from
  /sys/firmware/fdt is REQUIRED for drivers to init. Keep --dtb.
- Net: initrd reaches RAM, DTB is required, /init still never runs.

**RECOMMENDED NEXT APPROACH — skip the handoff entirely:**
build the initramfs INTO the kernel with `CONFIG_INITRAMFS_SOURCE=<dir>` (the
staged tree is at scratchpad/initramfs, or keep a copy in lgtv-toolkit). The cpio
is then linked into Image itself, unpacked unconditionally, and needs no
--initrd, no /chosen properties and no DTB cooperation. One rebuild, removes the
entire failure class.
Note the initramfs needs busybox + /lib/{ld-linux.so.3,libc.so.6,libm.so.6,
libresolv.so.2} copied off the TV (busybox is glibc-dynamic), /init executable at
the archive root, and CONFIG_DEVMEM=y lets /init breadcrumb via busybox devmem.

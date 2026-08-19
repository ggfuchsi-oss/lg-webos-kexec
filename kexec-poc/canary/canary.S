/* canary.S v2 - bare-metal armv7 payload kexec'd via the zImage path.
 *
 * Now INSTRUMENTED.  It writes staged progress markers to STB_WDOG_DATA10
 * (physical 0x18060124), a scratch register in the SoC's always-on standby
 * domain that SURVIVES a warm reset (baseline read = 0x00000000).  So even
 * though the screen is an unreliable channel across kexec, after the TV resets
 * and recovers we read 0x18060124 with devmem and learn exactly how far our
 * code got:
 *
 *   0x00000000  -> never wrote        => _start NEVER RAN. kexec control
 *                                         transfer (purgatory/jump) is the wall.
 *   0x50000000  -> reached entry, hung before/at the first framebuffer fill
 *   0x51000000  -> first 16MB fill completed (big DRAM write loop works),
 *                  hung later => our code runs fine; the DISPLAY is the only
 *                  broken channel.
 *   0x600000NN  -> ran NN full color-cycle iterations => fully alive.
 *
 * Framebuffer: PHYS 0x39000000, len 0x1030000, 32bpp [R,G,B,A].
 * Fully position-independent (PC-relative branches + absolute device consts).
 */
	.arch	armv7-a
	.text
	.global	_start
_start:
	@ marker r3 -> STB_WDOG_DATA10 (0x18060124)
	movw	r3, #0x0124
	movt	r3, #0x1806
	movw	r0, #0x0000		@ 0x50000000 = reached entry
	movt	r0, #0x5000
	str	r0, [r3]
	dsb

	movw	r4, #0x0000		@ fb base 0x39000000
	movt	r4, #0x3900
	movw	r5, #0x0000		@ fb end  0x3A030000
	movt	r5, #0x3A03

	movw	r2, #0xFF00		@ GREEN 0xFF00FF00
	movt	r2, #0xFF00
	bl	fill
	movw	r0, #0x0000		@ 0x51000000 = first fill done
	movt	r0, #0x5100
	str	r0, [r3]
	dsb
	bl	delay

	mov	r6, #0			@ loop iteration counter
loop_colors:
	movw	r2, #0x00FF		@ RED 0xFF0000FF
	movt	r2, #0xFF00
	bl	fill
	bl	delay
	movw	r2, #0x0000		@ BLUE 0xFFFF0000
	movt	r2, #0xFFFF
	bl	fill
	bl	delay
	movw	r2, #0xFF00		@ GREEN
	movt	r2, #0xFF00
	bl	fill
	bl	delay
	add	r6, r6, #1
	movw	r0, #0x0000		@ 0x600000NN = completed iteration NN
	movt	r0, #0x6000
	orr	r0, r0, r6
	str	r0, [r3]
	dsb
	b	loop_colors

fill:					@ fill [r4,r5) with color r2
	mov	r0, r4
1:	str	r2, [r0], #4
	cmp	r0, r5
	blo	1b
	dsb
	bx	lr

delay:					@ ~0.4s busy-wait
	movw	r0, #0x0000
	movt	r0, #0x0C00
1:	subs	r0, r0, #1
	bne	1b
	bx	lr

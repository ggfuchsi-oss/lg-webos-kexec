/* start.S - kexec entry stub for canary v3.
 * kexec jumps here (offset 0).  Caches may still be ON, which sends our MMIO
 * and framebuffer writes into the cache instead of DRAM (garbled screen + pets
 * that never reach the watchdog register).  So FIRST disable MMU + I/D caches so
 * every subsequent write goes straight to memory, then set up a stack and call
 * kmain(). */
	.arch	armv7-a
	.section .text.start,"ax"
	.global	_start
_start:
	cpsid	if			@ mask IRQ/FIQ - we own the CPU

	mrc	p15, 0, r0, c1, c0, 0	@ read SCTLR
	bic	r0, r0, #0x0005		@ clear M (bit0, MMU) and C (bit2, D-cache)
	bic	r0, r0, #0x1000		@ clear I (bit12, I-cache)
	mcr	p15, 0, r0, c1, c0, 0	@ write SCTLR
	dsb
	isb
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0	@ ICIALLU  - invalidate entire I-cache
	mcr	p15, 0, r0, c7, c5, 6	@ BPIALL   - invalidate branch predictor
	mcr	p15, 0, r0, c8, c7, 0	@ TLBIALL  - invalidate unified TLB
	dsb
	isb

	movw	sp, #0x0000		@ stack at 0x08000000 (System RAM, clear of us)
	movt	sp, #0x0800
	bl	kmain
0:	b	0b

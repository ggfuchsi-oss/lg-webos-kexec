/* Link at the ACTUAL load address 0x108000 (KEXEC_FIXED_BASE=0x100000 -> entry
 * base+0x8000 = 0x108000).  This makes absolute references (e.g. the FONT_CHARS
 * .rodata table) resolve to their real runtime address instead of a link-time
 * address near 0 - which is why text/.rodata was garbage before. */
ENTRY(_start)
SECTIONS
{
  . = 0x108000;
  .text : {
    *(.text.start)     /* _start MUST be at offset 0 */
    *(.text*)
  }
  .rodata : { *(.rodata*) }
  .data   : { *(.data*) }
  .bss    : { *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) *(.note*) }
}

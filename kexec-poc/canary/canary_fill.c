/* canary_fill.c - solid 32MB fill + hold, color set at compile time via FILLCOL.
 * For the "green then pink, no reboot" A/B test of the striping cause. */
#ifndef FILLCOL
#define FILLCOL 0xFF00FF00u
#endif
#define FB_BASE   0x39000000u
#define FILL_LEN  0x01030000u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u
static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF); poke(WDOG_TCWTR, 1);
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned n = 0; n < FILL_LEN/4; n++){
        fb[n] = (unsigned)FILLCOL;
        if ((n & 0x7FF) == 0) poke(WDOG_TCWTR, 1);
    }
    for (;;) poke(WDOG_TCWTR, 1);
}

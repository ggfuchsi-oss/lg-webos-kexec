/* solid_purple.c - just fills fb purple and loops */
#define FB_BASE 0x39000000u
#define WDOG_TCWTR 0x18062208u
static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
void kmain(void){
    poke(0x18062210, 0x7FFFFFFF);
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned n = 0; n < 0x01030000/4; n++) fb[n] = 0xFFFF00FFu;
    for (;;) poke(WDOG_TCWTR, 1);
}

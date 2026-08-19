/* block0.c - 8x8 block at absolute (0,0) with visible color */
#define FB_BASE   0x39000000u
#define FB_STRIDE 7680u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned int *fb = (volatile unsigned int*)FB_BASE;
    unsigned n = 0x01030000 / 4;
    for (unsigned i = 0; i < n; i++) fb[i] = 0xFF00FF00u;
    pet_wdt();
    /* 8x8 block at (0,0) - should be visible top-left if anything */
    for (unsigned dy = 0; dy < 8; dy++)
        for (unsigned dx = 0; dx < 8; dx++)
            fb[dy*7680 + dx] = 0x00FF0000u;
    for(;;) pet_wdt();
}

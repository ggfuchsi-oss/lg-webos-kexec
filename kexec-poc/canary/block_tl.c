/* block_tl.c - 64x64 red block at absolute top-left (0,0) */
#define FB_BASE   0x39000000u
#define FB_STRIDE 7680u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned n = 0; n < 0x01030000/4; n++) fb[n] = 0xFF00FF00u;
    pet_wdt();
    /* 64x64 red block at (0,0) - should be visible top-left */
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++)
            fb[y*7680 + x] = 0xFFFF0000u;
    for(;;) pet_wdt();
}

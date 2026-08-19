/* color_test.c - test different color values */
#define FB_BASE   0x39000000u
#define FB_STRIDE 7680u
#define WDOG_TCWTR 0x18062208u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

void kmain(void){
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    /* Top stripe: orange */
    for (unsigned x = 0; x < 1920; x++) fb[x] = 0xFFFF8000u;
    /* Middle stripe: yellow */
    for (unsigned x = 0; x < 1920; x++) fb[540*7680 + x] = 0xFFFFFF00u;
    /* Bottom stripe: white */
    for (unsigned x = 0; x < 1920; x++) fb[1079*7680 + x] = 0xFFFFFFFFu;
    for(;;) pet_wdt();
}

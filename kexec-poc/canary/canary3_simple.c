/* canary3_simple.c - just fill screen red and pet watchdog forever */
#define FB_BASE   0x39000000u
#define FB_STRIDE 7680u
#define FB_W      1920u
#define FB_H      2160u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u
#define FB_LEN     (FB_STRIDE * FB_H)

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    unsigned words = FB_LEN / 4;
    for (unsigned n = 0; n < words; n++){
        fb[n] = 0xFFFF0000u;
        if ((n & 0x7FF) == 0) pet_wdt();
    }
    for (;;) pet_wdt();
}

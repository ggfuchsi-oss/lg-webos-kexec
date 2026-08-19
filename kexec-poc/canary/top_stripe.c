/* top_stripe.c - fill top 100 lines bright red, immediate visual */
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
    for (unsigned y = 0; y < 100; y++){
        for (unsigned x = 0; x < 1920; x++)
            fb[y*7680 + x] = 0xFFFF0000u;
        pet_wdt();
    }
    for(;;) pet_wdt();
}

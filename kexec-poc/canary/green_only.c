/* green_only.c - solid green fill, unmistakable */
#define FB_BASE   0x39000000u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned n = 0; n < 0x01030000/4; n++) fb[n] = 0xFF00FF00u;
    for (;;) pet_wdt();
}

/* one_pixel.c - single pixel at (0,0) */
#define FB_BASE   0x39000000u
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
    fb[0] = 0xFFFF0000u;  /* ONE pixel at (0,0) */
    for(;;) pet_wdt();
}

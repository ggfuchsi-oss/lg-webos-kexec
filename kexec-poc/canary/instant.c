/* instant.c - draw immediately, nothing else */
#define FB_BASE 0x39000000u
#define WDOG_TCWTR 0x18062208u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }

void kmain(void){
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned x = 0; x < 100; x++) fb[x] = 0xFFFF0000u;  /* blue stripe */
    for (unsigned x = 100; x < 200; x++) fb[x] = 0xFF00FF00u; /* green stripe */
    for (unsigned x = 200; x < 300; x++) fb[x] = 0xFF0000FFu; /* red stripe */
    poke(0x18062208, 1);  /* pet watchdog once */
    for(;;){ poke(0x18062208, 1); }  /* keep petting */
}

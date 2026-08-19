/* simples.c - minimal diagnostic: horizontal B/W stripes, no watchdog
 * Just fills and halts. If you see stripes = base+stride are correct.
 */
#define FB_BASE 0x39000000u
#define WDOG_TCWTR 0x18062208u
static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
void kmain(void){
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned y = 0; y < 2160; y++){
        unsigned col = (y & 1) ? 0xFF000000u : 0xFFFFFFFFu;
        for (unsigned x = 0; x < 1920; x++){
            fb[y*7680 + x] = col;
        }
    }
    for (;;){}
}

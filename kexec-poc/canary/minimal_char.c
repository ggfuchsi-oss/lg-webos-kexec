/* minimal_char.c - draw ONE character using hardcoded block glyph */
#define FB_BASE   0x39000000u
#define FB_STRIDE 7680u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

/* Hardcoded 'X' glyph - all pixels on */
static void draw_x(unsigned px, unsigned py, unsigned int color, unsigned stride){
    for (unsigned row = 0; row < 64; row++){
        for (unsigned col = 0; col < 64; col++){
            poke(FB_BASE + (py+row)*stride + (px+col)*4, color);
        }
    }
}

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned n = 0; n < 0x01030000/4; n++) fb[n] = 0xFF00FF00u;
    pet_wdt();
    draw_x(400, 200, 0xFFFF0000u, FB_STRIDE);
    for(;;) pet_wdt();
}

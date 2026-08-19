/* one_char.c - draw single 'A' at center, red bg */
#include "font.h"
#define FB_BASE   0x39000000u
#define FB_STRIDE 7680u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

void draw_char(unsigned px, unsigned py, char ch, unsigned int color, unsigned stride){
    unsigned uc = (unsigned char)ch;
    if (uc < FONT_FIRST || uc >= FONT_FIRST+96) uc = ' ';
    const unsigned char *g = &FONT_CHARS[(uc - FONT_FIRST)*8];
    for (unsigned row = 0; row < 8; row++){
        unsigned char bits = g[row];
        for (unsigned col = 0; col < 8; col++){
            if (bits & (0x80 >> col)){
                for (unsigned dy = 0; dy < 20; dy++)
                    for (unsigned dx = 0; dx < 20; dx++)
                        poke(FB_BASE + (py+row*20+dy)*stride + (px+col*20+dx)*4, color);
            }
        }
    }
}

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    for (unsigned n = 0; n < 0x01030000/4; n++) fb[n] = 0xFFFF0000u;
    pet_wdt();
    draw_char(400, 200, 'A', 0xFF00FF00u, FB_STRIDE);
    for(;;) pet_wdt();
}

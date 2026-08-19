/* canary3_green.c - green bg with simple text */
#include "font.h"
#define FB_BASE   0x39000000u
#define FB_STRIDE 7680u
#define FB_W      1920u
#define FB_H      2160u
#define FB_LEN    (FB_STRIDE * FB_H)
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
                for (unsigned dy = 0; dy < 8; dy++)
                    for (unsigned dx = 0; dx < 8; dx++)
                        poke(FB_BASE + (py+row*8+dy)*stride + (px+col*8+dx)*4, color);
            }
        }
    }
}

void draw_str(unsigned px, unsigned py, const char *s, unsigned int color, unsigned stride){
    for (; *s; s++){ pet_wdt(); draw_char(px, py, *s, color, stride); px += 64; }
}

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    unsigned words = FB_LEN / 4;
    for (unsigned n = 0; n < words; n++){
        fb[n] = 0xFF00FF00u;  /* GREEN - known to work */
        if ((n & 0x7FF) == 0) pet_wdt();
    }
    pet_wdt();
    draw_str(600, 400, "HELLO", 0xFFFF0000u, FB_STRIDE);
    for(;;) pet_wdt();
}

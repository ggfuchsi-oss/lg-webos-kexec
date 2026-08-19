/* canary3.c - 16bpp RGB565 console.  Evidence: solid GREEN (0xFF00FF00, both
 * 16-bit halves = 0xFF00) rendered clean but solid RED (halves differ) showed a
 * grid => the plane is 16bpp RGB565, not 32bpp.  Real geometry: 3840x2160,
 * 2 bytes/px, stride 7680 (matches fbinfo line_length). */
#include "font.h"
#define FB_BASE   0x39000000u
#define STRIDE    7680u          /* bytes per line = 3840 px * 2 */
#define W         3840u
#define H         2160u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

/* RGB565 */
#define C_BLACK  0x0000u
#define C_WHITE  0xFFFFu
#define C_GREEN  0x07E0u
#define C_CYAN   0x07FFu
#define C_YELLOW 0xFFE0u
#define C_BG     0x0008u         /* very dark blue */

static inline void poke32(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke32(WDOG_TCWTR, 1); }

/* Plot a 16bpp pixel using a 32-BIT write (16-bit writes don't reach the plane).
 * Word-align to the pixel pair and set BOTH halves to the color (fine at our
 * scale). */
static inline void plot(unsigned x, unsigned y, unsigned short color){
    unsigned word = ((unsigned)color << 16) | color;
    poke32(FB_BASE + y*STRIDE + (x & ~1u)*2, word);
}

static void fill(unsigned short c){
    unsigned word = ((unsigned)c << 16) | c;      /* two 565 pixels per 32b write */
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    unsigned words = (STRIDE*H)/4;
    for (unsigned n = 0; n < words; n++){
        fb[n] = word;
        if ((n & 0x7FF) == 0) pet_wdt();
    }
}

/* 8x8 glyph, scale sc, at pixel (px,py), 16bpp */
static void draw_char(unsigned px, unsigned py, char ch, unsigned short color, unsigned sc){
    unsigned uc = (unsigned char)ch;
    if (uc < FONT_FIRST || uc >= FONT_FIRST+96) uc = ' ';
    const unsigned char *g = &FONT_CHARS[(uc - FONT_FIRST)*8];
    for (unsigned row = 0; row < 8; row++){
        unsigned char bits = g[row];
        for (unsigned col = 0; col < 8; col++){
            if (bits & (0x80 >> col)){
                for (unsigned dy = 0; dy < sc; dy++)
                    for (unsigned dx = 0; dx < sc; dx++){
                        unsigned x = px + col*sc + dx, y = py + row*sc + dy;
                        if (x < W && y < H) plot(x, y, color);
                        /* also draw into the OTHER page (visible page starts at
                         * row 1080 per fbinfo yoffset) so text shows regardless */
                        unsigned y2 = y + 1080;
                        if (x < W && y2 < H) plot(x, y2, color);
                    }
            }
        }
        pet_wdt();
    }
}

static void draw_str(unsigned px, unsigned py, const char *s, unsigned short color, unsigned sc){
    for (; *s; s++){ pet_wdt(); draw_char(px, py, *s, color, sc); px += 8*sc; }
}

void kmain(void){
    poke32(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    /* BUFFER MAP: 8 full-width horizontal bands over the whole buffer height, so
     * whatever the visible window is, it shows some of these colors in order.
     * Bands top->bottom: RED ORANGE YELLOW GREEN CYAN BLUE MAGENTA WHITE.
     * Report the colors you see & their order -> reveals the visible window. */
    {
        static const unsigned short pal[8] = {
            0xF800,0xFBE0,0xFFE0,0x07E0,0x07FF,0x001F,0xF81F,0xFFFF };
        unsigned bandh = H/8;
        for (unsigned y = 0; y < H; y++){
            unsigned short c = pal[(y/bandh) & 7];
            unsigned word = ((unsigned)c<<16)|c;
            unsigned rowbase = FB_BASE + y*STRIDE;
            for (unsigned x = 0; x < W/2; x++) poke32(rowbase + x*4, word);
            if ((y & 15)==0) pet_wdt();
        }
    }
    for(;;) pet_wdt();
    fill(C_BG);
    pet_wdt();
    /* DIAGNOSTIC: big solid white block in the visible page (rows 1080+).
     * Unmissable if poke16 drawing reaches the scanned buffer. */
    for (unsigned y = 1200; y < 1600; y++){
        for (unsigned x = 400; x < 1200; x++) plot(x, y, C_WHITE);
        pet_wdt();
    }
    unsigned sc = 8;             /* 64px text on a 4K panel */
    draw_str(100, 120, "CANARY V3 -- KEXEC OK", C_WHITE, sc);
    draw_str(100, 260, "BARE METAL ON SOC",     C_GREEN, sc);
    draw_str(100, 400, "16BPP RGB565 CONSOLE",  C_CYAN,  sc);
    draw_str(100, 540, "IF YOU CAN READ THIS",  C_YELLOW, sc);
    draw_str(100, 680, "WE WON",                C_WHITE, sc);
    for(;;) pet_wdt();
}

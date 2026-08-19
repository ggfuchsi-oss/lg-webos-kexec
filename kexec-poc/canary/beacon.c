/* posmap.c - find the real scanout base + stride, stride-invariantly.
 * Split the 33MB buffer into 8 CONTIGUOUS byte-chunks, each a distinct doubled
 * RGB565 color (equal 16-bit halves => renders clean regardless of format).
 * Whatever colors are visible + their arrangement reveal where the visible
 * window sits in the buffer and the stride. */
#define FB_BASE 0x39000000u
#define WORDS   ((7680u*2160u)/4u)
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u
static inline void poke32(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet(void){ poke32(WDOG_TCWTR, 1); }
void kmain(void){
    poke32(WDOG_TCWOV, 0x7FFFFFFF); pet();
    /* doubled RGB565: RED ORANGE YELLOW GREEN CYAN BLUE MAGENTA WHITE */
    static const unsigned pal[8] = {
        0xF800F800u, 0xFC00FC00u, 0xFFE0FFE0u, 0x07E007E0u,
        0x07FF07FFu, 0x001F001Fu, 0xF81FF81Fu, 0xFFFFFFFFu };
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    unsigned chunk = WORDS/8;
    for (unsigned i = 0; i < 8; i++){
        unsigned c = pal[i];
        for (unsigned n = i*chunk; n < (i+1)*chunk; n++){
            fb[n] = c;
            if ((n & 0x7FF)==0) pet();
        }
    }
    for(;;) pet();
}

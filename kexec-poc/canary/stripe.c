/* stripe.c - diagnostic: fill fb with horizontal color stripes
 * to reveal the actual framebuffer pitch/base visible to VCPU composite.
 */
#define FB_BASE   0x39000000u
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 0x01); }

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();
    volatile unsigned *fb = (volatile unsigned*)FB_BASE;
    unsigned colors[] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00,
                         0xFFFF00FF, 0xFF00FFFF, 0xFFFFFFFF, 0xFF000000};
    for (unsigned y = 0; y < 2160; y++){
        unsigned col = colors[(y / 120) % 8];
        for (unsigned x = 0; x < 1920; x++){
            fb[y*7680 + x] = col;
        }
        if ((y & 0x3F) == 0) pet_wdt();
    }
    for (;;) pet_wdt();
}

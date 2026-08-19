/* dot.c - single green pixel at top-left corner */
#define FB_BASE 0x39000000u
#define WDOG_TCWTR 0x18062208u

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
void kmain(void){
    poke(0x18062210, 0x7FFFFFFF);
    ((volatile unsigned*)FB_BASE)[0] = 0xFF00FF00u;
    for(;;){ poke(WDOG_TCWTR, 1); }
}

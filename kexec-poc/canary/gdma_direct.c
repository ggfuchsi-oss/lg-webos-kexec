/* gdma_direct.c - configure GDMA display controller directly
 * The venusfb driver does NOT configure GDMA on webOS.
 * We must program the hardware registers ourselves to make the
 * framebuffer visible.
 */
#define FB_BASE   0x39000000u
#define GDMA_WIN_PHYS 0x08000000u   /* our GDMA_WIN structure in RAM */
#define WDOG_TCWTR 0x18062208u
#define WDOG_TCWOV 0x18062210u

/* GDMA register addresses (physical) */
#define GDMA_OSD1_reg      0xB802F204
#define GDMA_OSD1_CTRL_reg 0xB802F200
#define GDMA_OSD1_WI_reg   0xB802F210
#define GDMA_OSD1_SIZE_reg 0xB802F218
#define GDMA_CTRL_reg      0xB802F004
#define MIXER_CTRL2        0xb802b000

static inline void poke(unsigned a, unsigned v){ *(volatile unsigned*)a = v; }
static inline void pet_wdt(void){ poke(WDOG_TCWTR, 1); }

void kmain(void){
    poke(WDOG_TCWOV, 0x7FFFFFFF);
    pet_wdt();

    /* Build GDMA_WIN structure at GDMA_WIN_PHYS */
    volatile unsigned int *win = (volatile unsigned int*)GDMA_WIN_PHYS;

    /* nxtAddr: last=1, addr=0 */
    win[0] = 0x00000001;
    /* winXY: x=0, y=0 */
    win[1] = 0x00000000;
    /* winWH: width=1920, height=1080 */
    win[2] = (1920 << 16) | 1080;
    /* attr: type=7(ARGB8888), littleEndian=1, alpha=0xFF, rgbOrder=0 */
    win[3] = 0x00FF0207;
    /* CLUT_addr */
    win[4] = 0x00000000;
    /* colorKey: keyEn=0, key=0xFFFFFF */
    win[5] = 0x00FFFFFF;
    /* top_addr = framebuffer physical address */
    win[6] = FB_BASE;
    /* bot_addr */
    win[7] = 0x00000000;
    /* pitch = 7680 */
    win[8] = 0x00001E00;
    /* objOffset: objXoffset=0, objYoffset=0 */
    win[9] = 0x00000000;

    /* Fill framebuffer bright red */
    volatile unsigned int *fb = (volatile unsigned int*)FB_BASE;
    for (unsigned n = 0; n < 0x01030000/4; n++) fb[n] = 0xFFFF0000u;
    pet_wdt();

    /* Enable OSD1 on GDMA */
    poke(GDMA_OSD1_reg, 0x00010001);
    /* Enable OSD1 on mixer */
    poke(MIXER_CTRL2, 0x00001111);

    /* Reset OSD1 */
    poke(GDMA_OSD1_CTRL_reg, ~1);
    /* Enable OSD1 */
    poke(GDMA_OSD1_CTRL_reg, 0x00000003);

    /* Point GDMA to our window descriptor (physical address) */
    poke(GDMA_OSD1_WI_reg, GDMA_WIN_PHYS);

    /* Set window size */
    poke(GDMA_OSD1_SIZE_reg, (1920 << 16) | 1080);

    /* Trigger update */
    poke(GDMA_CTRL_reg, 0x00000041);

    for(;;) pet_wdt();
}

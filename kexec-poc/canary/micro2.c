void kmain(void){
    *(volatile unsigned*)0x18060124 = 0x50000000u;
    for(;;){ *(volatile unsigned*)0x18062208 = 1; }
}

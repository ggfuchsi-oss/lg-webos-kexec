/* marker.c - write unique marker to persistent register, then loop */
void kmain(void){
    *(volatile unsigned*)0x18060124 = 0xABCD1234u;
    for(;;){ *(volatile unsigned*)0x18062208 = 1; }
}

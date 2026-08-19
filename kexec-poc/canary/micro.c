/* micro.c - 2-instruction payload: write marker then halt.
 * If the TV reboots and devmem 0x18060124 shows 0x50000000,
 * kexec control transfer WORKS. If it stays 0x00000000, the
 * purgatory/jump itself is failing.
 */
void kmain(void){
    *(volatile unsigned*)0x18060124 = 0x50000000u;
    for(;;){}
}

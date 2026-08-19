/*
 * micom-pet.c — keep an LG webOS TV (RTD2875 / K7LP) alive after kexec.
 *
 * Reverse-engineered from the stock LG kernel driver
 * (drivers/rtk_kdriver/platform/tv006/intmicom.c). There are TWO watchdogs:
 *
 *  1) SoC watchdog (the killer that resets our kexec'd kernel ~10-21s in).
 *     The stock kernel's micom_wdt_thread() holds it off with three raw
 *     register pokes to the TC (timer/counter) block:
 *         TCWCR @ 0xFE062204 <- 0xA5         (unlock)
 *         TCWOV @ 0xFE062210 <- 0x0FFFFFFF   (max overflow => huge timeout)
 *         TCWTR @ 0xFE062208 <- 0x01         (kick / trigger)
 *     Under kexec the driver's `*(volatile unsigned*)0xFE062208 = 0x01`
 *     lands on an UNMAPPED virtual address and is a silent no-op, so the SoC
 *     watchdog fires. We do the same pokes from userspace via /dev/mem
 *     (CONFIG_DEVMEM=y, CONFIG_STRICT_DEVMEM off), which maps correctly.
 *
 *  2) External Micom MCU watchdog. Fed by the kernel's own micom_wdt_thread
 *     via do_intMicomShareMemory(0xB1) — that path uses rtd_outl, which DOES
 *     work under kexec, so the micom is already covered. We additionally send
 *     the same 0xB1 keepalive through /dev/sys-intmicom (ioctl 's',16 =
 *     INTMICOM_IPC_WRITE) as defense-in-depth.
 *
 * Build: arm-linux-musleabihf-gcc -static -Os -o micom-pet micom-pet.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

/* ---- SoC watchdog (TC block), physical addresses ---- */
#define TC_BASE   0xFE062000u
#define TCWCR_OFF 0x204u   /* unlock register  */
#define TCWOV_OFF 0x210u   /* overflow (timeout) */
#define TCWTR_OFF 0x208u   /* trigger / kick   */

/* ---- breadcrumb scratch (same SoC block the kernel uses for 0xFE06010C) ---- */
#define CRUMB_BASE 0xFE060000u
#define CRUMB_OFF  0x110u

/* ---- micom IPC ioctl (drivers/rtk_kdriver/include/rtk_kdriver/intmicom.h) ---- */
#define INTMICOM_IOC_MAGIC 's'
#define INTMICOM_IPC_WRITE _IOW(INTMICOM_IOC_MAGIC, 16, unsigned int)

typedef struct {
    uint8_t  CmdSize;
    uint32_t pCmdBuf;
    uint8_t  DataSize;
    uint32_t pDataBuf;
    uint8_t  retryCnt;
} IPC_ARG_T;

static volatile uint32_t *map_phys(int fd, uint32_t phys, size_t len) {
    void *m = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys);
    if (m == MAP_FAILED) { perror("mmap"); return NULL; }
    return (volatile uint32_t *)m;
}

/* Pet the SoC watchdog. This is the critical one. */
static void pet_soc_wdt(volatile uint32_t *tc) {
    tc[TCWCR_OFF / 4] = 0xA5;          /* TCWCR unlock */
    tc[TCWOV_OFF / 4] = 0x0FFFFFFF;    /* TCWOV max   */
    tc[TCWTR_OFF / 4] = 0x01;          /* TCWTR kick  */
}

/* Send the 0xB1 (CP_READ_MICOM_STATUS) keepalive to the micom via the kernel
 * driver's IPC_WRITE ioctl. The handler requires BOTH Cmd and Data to be set,
 * so we supply a 1-byte command and a 1-byte (zero) data payload. */
static void pet_micom(int fd) {
    uint8_t cmd = 0xB1;     /* CP_READ_MICOM_STATUS — kernel's own keepalive */
    uint8_t data = 0x00;
    IPC_ARG_T a;
    memset(&a, 0, sizeof a);
    a.CmdSize  = 1;
    a.pCmdBuf  = (uint32_t)(uintptr_t)&cmd;
    a.DataSize = 1;
    a.pDataBuf = (uint32_t)(uintptr_t)&data;
    a.retryCnt = 3;
    ioctl(fd, INTMICOM_IPC_WRITE, &a);   /* ignore errors: defense-in-depth */
}

int main(void) {
    int mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem < 0) { perror("/dev/mem"); return 1; }

    volatile uint32_t *tc = map_phys(mem, TC_BASE, 0x1000);
    if (!tc) return 1;
    /* breadcrumb page is optional; only write it if the mmap succeeds */
    volatile uint32_t *crumb = map_phys(mem, CRUMB_BASE, 0x1000);

    int micom = open("/dev/sys-intmicom", O_RDWR);
    if (micom < 0)
        fprintf(stderr, "micom-pet: /dev/sys-intmicom: %s (SoC pet only)\n",
                strerror(errno));

    unsigned long beat = 0;
    for (;;) {
        pet_soc_wdt(tc);                       /* THE fix */
        if (micom >= 0) pet_micom(micom);     /* defense-in-depth */
        if (crumb) crumb[CRUMB_OFF / 4] = 0x52455800u | (beat & 0xff); /* "REX"+beat */
        beat++;
        usleep(100000);   /* 100 ms — matches the stock micom_wdt_thread cadence */
    }
    return 0;
}

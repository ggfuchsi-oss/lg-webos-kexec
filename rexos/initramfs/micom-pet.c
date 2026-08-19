/*
 * micom-pet.c — keep the LG TV's external Micom MCU fed so it doesn't reset
 * the board. Runs as a userspace daemon inside RexOS-TV (RAM only).
 *
 * The kernel's intmicom thread (do_intMicomShareMemory) is the AUTHORITATIVE
 * petter and is proven to return success on this hardware. This program is
 * defense-in-depth AND a visible heartbeat: it writes a breadcrumb counter into
 * the standby-domain register 0x18060110 (survives a warm reset) so we can
 * confirm, from the stock side afterwards, that RexOS really ran.
 *
 * Register block (phys):  COMMAND 0x18060500, DATA 0x18060504, CTRL 0x18060574.
 * Protocol (from the kernel driver / head.S pet): post command+data, then poll
 * CTRL until the 8051 releases it (==0). 0x01000605 / 0xB1 = read-micom-status.
 *
 * Build: arm-linux-musleabihf-gcc -static -Os -o micom-pet micom-pet.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#define RBUS_PHYS   0x18000000u
#define RBUS_SIZE   0x00200000u   /* RBUS_BASE_SIZE from iomap.h */
#define MICOM_CMD   0x500u       /* +RBUS_PHYS = 0x18060500 */
#define MICOM_DATA  0x504u       /* 0x18060504 */
#define MICOM_CTRL  0x574u       /* 0x18060574 */
#define CRUMB_REG   0x110u       /* 0x18060110 — stage/breadcrumb (warm-reset safe) */

#define CMD_VALUE   0x01000605u
#define DATA_VALUE  0xB1u

static volatile uint32_t *rbus;

static inline uint32_t rd(unsigned off) { return rbus[off / 4]; }
static inline void      wr(unsigned off, uint32_t v) { rbus[off / 4] = v; }

static void pet_once(void) {
    wr(MICOM_CMD, CMD_VALUE);
    wr(MICOM_DATA, DATA_VALUE);
    /* wait (bounded) for the 8051 to release the shared-memory block */
    for (int i = 0; i < 1000; i++) {
        if (rd(MICOM_CTRL) == 0) return;
        usleep(10);
    }
    /* 8051 didn't release in time — best effort, don't spin forever */
}

int main(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("/dev/mem"); return 1; }

    void *m = mmap(NULL, RBUS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, RBUS_PHYS);
    if (m == MAP_FAILED) { perror("mmap"); return 1; }
    rbus = (volatile uint32_t *)m;

    uint32_t beat = 0;
    for (;;) {
        pet_once();
        wr(CRUMB_REG, 0x52455800 | (beat & 0xff));  /* "REX" + beat */
        beat++;
        usleep(200000);   /* ~5 Hz; matches the ~500ms stock cadence, safe margin */
    }
    return 0;
}

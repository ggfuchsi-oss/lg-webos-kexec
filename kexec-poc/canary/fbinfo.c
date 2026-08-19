/* fbinfo - read-only probe of /dev/fb0 geometry + physical address.
 * No writes to the framebuffer, so no visible screen change. */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

int main(void)
{
    int fd = open("/dev/fb0", O_RDONLY);
    if (fd < 0) { perror("open /dev/fb0"); return 1; }

    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) { perror("FSCREENINFO"); return 1; }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) { perror("VSCREENINFO"); return 1; }

    printf("smem_start (phys) = 0x%08lx\n", (unsigned long)fix.smem_start);
    printf("smem_len          = 0x%08x (%u bytes)\n", fix.smem_len, fix.smem_len);
    printf("line_length       = %u\n", fix.line_length);
    printf("type/visual       = %u/%u\n", fix.type, fix.visual);
    printf("xres,yres         = %u,%u   virtual %u,%u\n",
           var.xres, var.yres, var.xres_virtual, var.yres_virtual);
    printf("bpp               = %u\n", var.bits_per_pixel);
    printf("R off/len         = %u/%u\n", var.red.offset, var.red.length);
    printf("G off/len         = %u/%u\n", var.green.offset, var.green.length);
    printf("B off/len         = %u/%u\n", var.blue.offset, var.blue.length);
    printf("A off/len         = %u/%u\n", var.transp.offset, var.transp.length);
    printf("xoffset,yoffset   = %u,%u\n", var.xoffset, var.yoffset);
    close(fd);
    return 0;
}

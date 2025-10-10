/**
 * @file fbdev.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "fbdev.h"
#include "gui.h"
#if USE_FBDEV || USE_BSD_FBDEV

#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#if USE_BSD_FBDEV
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/consio.h>
#include <sys/fbio.h>
#else /* USE_BSD_FBDEV */
#include <linux/fb.h>
#endif /* USE_BSD_FBDEV */

/*********************
 *      DEFINES
 *********************/
#ifndef FBDEV_PATH
#define FBDEV_PATH "/dev/fb0"
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *      STRUCTURES
 **********************/

struct bsd_fb_var_info
{
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t xres;
    uint32_t yres;
    int bits_per_pixel;
};

struct bsd_fb_fix_info
{
    long int line_length;
    long int smem_len;
};

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
#if USE_BSD_FBDEV
static struct bsd_fb_var_info vinfo;
static struct bsd_fb_fix_info finfo;
#else
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
#endif /* USE_BSD_FBDEV */
static char *fbp = 0;
uint32_t *fbp32 = 0;
static long int screensize = 0;
static int fbfd = 0;
static bool pan = false;

/**********************
 *      MACROS
 **********************/

#if USE_BSD_FBDEV
#define FBIOBLANK FBIO_BLANK
#endif /* USE_BSD_FBDEV */

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int fb_rotate_mode;

void fbdev_init(void *raw, uint32_t width, uint32_t height, int rotate_mode)
{
    fb_rotate_mode = rotate_mode;
    // Open the file for reading and writing
    fbfd = open(FBDEV_PATH, O_RDWR);
    if (fbfd == -1)
    {
        perror("Error: cannot open framebuffer device");
        return;
    }
    LV_LOG_INFO("The framebuffer device was opened successfully");

    // Make sure that the display is on.
    if (ioctl(fbfd, FBIOBLANK, FB_BLANK_UNBLANK) != 0)
    {
        perror("ioctl(FBIOBLANK)");
        return;
    }
#if USE_BSD_FBDEV
    struct fbtype fb;
    unsigned line_length;

    // Get fb type
    if (ioctl(fbfd, FBIOGTYPE, &fb) != 0)
    {
        perror("ioctl(FBIOGTYPE)");
        return;
    }

    // Get screen width
    if (ioctl(fbfd, FBIO_GETLINEWIDTH, &line_length) != 0)
    {
        perror("ioctl(FBIO_GETLINEWIDTH)");
        return;
    }

    vinfo.xres = (unsigned)fb.fb_width;
    vinfo.yres = (unsigned)fb.fb_height;
    vinfo.bits_per_pixel = fb.fb_depth;
    vinfo.xoffset = 0;
    vinfo.yoffset = 0;
    finfo.line_length = line_length;
    finfo.smem_len = finfo.line_length * vinfo.yres;
#else  /* USE_BSD_FBDEV */
    memset(&finfo, 0, sizeof(struct fb_fix_screeninfo));
    memset(&vinfo, 0, sizeof(struct fb_var_screeninfo));
    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        perror("Error reading fixed information");
        return;
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        perror("Error reading variable information");
        return;
    }
#endif /* USE_BSD_FBDEV */

    LV_LOG_INFO("%dx%d, %dbpp", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // Figure out the size of the screen in bytes
    screensize = finfo.smem_len; // finfo.line_length * vinfo.yres;

    // Map the device to memory
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((intptr_t)fbp == -1)
    {
        perror("Error: failed to map framebuffer device to memory");
        return;
    }

    //防止花屏
    fbp32 = (uint32_t *)fbp;
    if (raw)
    {
        usleep(10000);
        lv_area_t area;
        area.x1 = 0;
        area.y1 = 0;
        area.x2 = width - 1;
        area.y2 = height - 1;
        fbdev_flush2(fbp32, fbp32 + vinfo.xres * vinfo.yres, &area, raw);
    }
    else
    {
        for (int i = 0; i < screensize / sizeof(uint32_t); i++)
            fbp32[i] = 0xff000000;
    }
    pan = false;
    vinfo.yoffset = 0;
    ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);

    // Don't initialise the memory to retain what's currently displayed / avoid clearing the screen.
    // This is important for applications that only draw to a subsection of the full framebuffer.

    LV_LOG_INFO("The framebuffer device was mapped to memory successfully");
}

void fbdev_exit(void)
{
    close(fbfd);
}

/**
 * Flush a buffer to the marked area
 * @param drv pointer to driver where this function belongs
 * @param area an area where to copy `color_p`
 * @param color_p an array of pixels to copy to the `area` part of the screen
 */
void fbdev_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (pan)
    {
        vinfo.yoffset = 0;
        fbp32 = (uint32_t *)fbp;
    }
    else
    {
        vinfo.yoffset = vinfo.yres;
        fbp32 = (uint32_t *)fbp + vinfo.xres * vinfo.yres;
    }
    pan = !pan;
    fbdev_flush2(fbp32, NULL, area, (uint32_t *)color_p);
    ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
    if (drv)
        lv_disp_flush_ready(drv);
}

void fbdev_get_sizes(uint32_t *width, uint32_t *height)
{
    if (width)
        *width = vinfo.xres;

    if (height)
        *height = vinfo.yres;
}

void fbdev_set_offset(uint32_t xoffset, uint32_t yoffset)
{
    vinfo.xoffset = xoffset;
    vinfo.yoffset = yoffset;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

void fbdev_flush2(uint32_t *fb1, uint32_t *fb2, const lv_area_t *area, uint32_t *data)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    uint32_t *pdata;
    uint32_t *pfb1 = fb1 ? fb1 + area->y1 * w + area->x1 : NULL;
    uint32_t *pfb2 = fb2 ? fb2 + area->y1 * w + area->x1 : NULL;

    if (fb_rotate_mode == 0)
    {
        pdata = data;
        for (int y = 0; y < h; y++)
        {
            if (pfb1)
            {
                memcpy(pfb1, pdata, w * sizeof(uint32_t));
                pfb1 += vinfo.xres;
            }
            if (pfb2)
            {
                memcpy(pfb2, pdata, w * sizeof(uint32_t));
                pfb2 += vinfo.xres;
            }
            pdata += w;
        }
    }
    else if (fb_rotate_mode == 1)
    {
        for (int y = 0; y < w; y++)
        {
            pdata = data + w - y - 1;
            uint32_t *pfb1_iter = pfb1;
            uint32_t *pfb2_iter = pfb2;
            for (int x = 0; x < h; x++)
            {
                if (pfb1)
                    memcpy(pfb1_iter, pdata, sizeof(uint32_t));
                if (pfb2)
                    memcpy(pfb2_iter, pdata, sizeof(uint32_t));
                pdata += w;
                pfb1_iter++;
                pfb2_iter++;
            }
            if (pfb1)
                pfb1 += vinfo.xres;
            if (pfb2)
                pfb2 += vinfo.xres;
        }
    }
    else if (fb_rotate_mode == 2)
    {
        pdata = data + h * w - 1;
        for (int y = 0; y < h; y++)
        {
            uint32_t *pfb1_iter = pfb1;
            uint32_t *pfb2_iter = pfb2;
            for (int x = 0; x < w; x++)
            {
                if (pfb1)
                    memcpy(pfb1_iter, pdata, sizeof(uint32_t));
                if (pfb2)
                    memcpy(pfb2_iter, pdata, sizeof(uint32_t));
                pfb1_iter++;
                pfb2_iter++;
                pdata--;
            }
            if (pfb1)
                pfb1 += vinfo.xres;
            if (pfb2)
                pfb2 += vinfo.xres;
        }
    }
    else if (fb_rotate_mode == 3)
    {
        pdata = data + (h - 1) * w - 1;
        for (int y = 0; y < w; y++)
        {
            uint32_t *pdata_iter = pdata + y;
            uint32_t *pfb1_iter = pfb1;
            uint32_t *pfb2_iter = pfb2;
            for (int x = 0; x < h; x++)
            {
                if (pfb1)
                    memcpy(pfb1_iter, pdata_iter, sizeof(uint32_t));
                if (pfb2)
                    memcpy(pfb2_iter, pdata_iter, sizeof(uint32_t));
                pdata_iter -= w;
                pfb1_iter++;
                pfb2_iter++;
            }
            if (pfb1)
                pfb1 += vinfo.xres;
            if (pfb2)
                pfb2 += vinfo.xres;
        }
    }
}

#endif
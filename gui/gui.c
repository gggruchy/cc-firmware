#include "gui.h"
#include "utils.h"
#include "fbdev.h"
#include "evdev.h"
#include "ui.h"
#include "gui/lvgl/src/extra/libs/png/lodepng.h"
#include "hl_common.h"
#include <time.h>
#include "config.h"

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static lv_disp_t *disp;
static lv_indev_t *indev;
lv_color_t *framebuffer = 0;
void *lv_buffer = 0;

extern sem_t sem;

#include <linux/fb.h>
extern struct fb_var_screeninfo vinfo;
extern struct fb_fix_screeninfo finfo;

enum
{
    ROTATE_0 = 0,
    ROTATE_90 = 1,
    ROTATE_180 = 2,
    ROTATE_270 = 3,
};

int gui_init(void)
{
    uint32_t src_w = 0, src_h = 0;
    uint32_t w = 0, h = 0;
    unsigned char *cache_png_data[100];
    size_t cache_png_size[100];
    uint32_t png_width;
    uint32_t png_height;
    uint8_t *img_data = NULL;
    uint32_t png_number = 0;

    // 首屏衔接Bootlogo
#if CONFIG_UI == PROJECT_UI_E100
    lodepng_load_file(&cache_png_data[0], &cache_png_size[0], ui_get_image_src(1001 + 0));
    lodepng_decode32(&img_data, &png_width, &png_height, cache_png_data[0], cache_png_size[0]);
    convert_color_depth(img_data, png_width * png_height);
    fbdev_init(img_data, png_width, png_height, ROTATE_0);
    fbdev_get_sizes(&src_w, &src_h);
    free(img_data);

    w = src_w;
    h = src_h;

    for (int i = 0; i < 100; i++)
    {
        lodepng_load_file(&cache_png_data[i], &cache_png_size[i], ui_get_image_src(1001 + i));
    }

    lv_area_t area;
    area.x1 = 0;
    area.y1 = 0;
    area.x2 = png_width - 1;
    area.y2 = png_height - 1;
    for (int i = 0; i < 27; i++)
    {
        lodepng_decode32(&img_data, &png_width, &png_height, cache_png_data[i], cache_png_size[i]);
        convert_color_depth(img_data, png_width * png_height);
        fbdev_flush(NULL, &area, (lv_color_t *)img_data);
        free(img_data);
        usleep(50000);
    }

    int value = 0;
    usleep(2000000);
    for (int i = 27; i < 100; i++)
    {
        lodepng_decode32(&img_data, &png_width, &png_height, cache_png_data[i], cache_png_size[i]);
        convert_color_depth(img_data, png_width * png_height);
        fbdev_flush(NULL, &area, (lv_color_t *)img_data);
        free(img_data);
        usleep(200000);
    }
    sem_getvalue(&sem,&value);
    while (value == 0)
    {
        //等待开机流程完成
        sem_getvalue(&sem,&value);
    }
    

    for (int i = 0; i < 100; i++)
        free(cache_png_data[i]);
#else
    fbdev_init(NULL, png_width, png_height, ROTATE_0);
    fbdev_get_sizes(&src_w, &src_h);
    w = src_w;
    h = src_h;
#endif
    evdev_init(src_w, src_h, ROTATE_0);

    if (w > 0 && h > 0)
    {
        lv_buffer = (void *)malloc(LV_MEM_SIZE);
        if (lv_buffer == NULL)
        {
            printf("can't allocate memory for lv_buffer\n");
            return -1;
        }

        framebuffer = (lv_color_t *)malloc(w * h * sizeof(lv_color_t));
        if (framebuffer == NULL)
        {
            printf("can't allocate memory for framebuffer\n");
            free(lv_buffer);
            return -1;
        }
    }

    // 初始化LVGL
    lv_init();

    // 注册显示设备
    lv_disp_draw_buf_init(&draw_buf, framebuffer, 0, w * h);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = w;
    disp_drv.ver_res = h;
    disp_drv.full_refresh = 1;
    disp = lv_disp_drv_register(&disp_drv);

    // 注册输入设备
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;
    indev = lv_indev_drv_register(&indev_drv);

    printf("width %d, height %d\n", w, h);
    return 0;
}

uint32_t gui_tick(void)
{
    static uint64_t start_ms = 0;
    uint64_t now_ms = 0;
    if (start_ms == 0)
        start_ms = hl_get_tick_ms();
    now_ms = hl_get_tick_ms();
    return (uint32_t)(now_ms - start_ms);
}

lv_indev_t *gui_get_lndev_device(void)
{
    return indev;
}

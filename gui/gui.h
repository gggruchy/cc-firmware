#ifndef GUI_H
#define GUI_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include "lvgl.h"
    int gui_init(void);
    uint32_t gui_tick(void);
    lv_indev_t *gui_get_lndev_device(void);

#ifdef __cplusplus
}
#endif

#endif

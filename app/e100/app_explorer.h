#ifndef __APP_EXPLORER_H__
#define __APP_EXPLORER_H__
#include "app.h"

void app_explorer_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_file_info_callback(widget_t **widget_list, widget_t *widget, void *e);
void load_thumbnail(lv_obj_t *target, char *file_name, double thumbnail_x, double thumbnail_y, const char *origin_src);
#endif
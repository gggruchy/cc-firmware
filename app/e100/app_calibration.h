#ifndef __APP_CALIBRATION_H__
#define __APP_CALIBRATION_H__
#include "app.h"

void app_calibration_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_auto_level_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_one_click_detection_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_pid_vibration_callback(widget_t **widget_list, widget_t *widget, void *e);

#endif
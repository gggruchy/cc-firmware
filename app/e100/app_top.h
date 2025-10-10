#ifndef __APP_TOP_H__
#define __APP_TOP_H__

#include "app.h"
#include "hl_queue.h"

void app_top_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_top_update_style(widget_t **widget_list);
void update_screen_off_time();
int get_screen_off_time();
void break_resume_msgbox_callback(bool resume_print_switch);
void app_top_back_dousing_state(void);
bool app_top_get_dousing_screen_state(void);
bool app_top_get_autoleveling_busy(void);
#endif
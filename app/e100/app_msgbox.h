#ifndef APP_MSGBOX_H
#define APP_MSGBOX_H
#include "app.h"

typedef bool (*msgbox_callback)(window_t *win, widget_t *widget, void *user_data, void *e);

void app_msgbox_init(void);
void app_msgbox_deinit(void);
void app_msgbox_run(void);
void app_msgbox_push(int window_index, bool modal, msgbox_callback callback, void *user_data);
bool app_msgbox_is_active(); // 返回当前是否有在活跃的msgbox
void app_msgbox_close_avtive_msgbox();
void app_msgbox_close_all_avtive_msgbox(void);
void app_msgbox_reset_msgbox_queue(void);
bool get_app_msgbox_is_busy(void);
#endif
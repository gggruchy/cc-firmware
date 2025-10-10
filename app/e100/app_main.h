#ifndef __APP_MAIN_H__
#define __APP_MAIN_H__
#include "app.h"

enum
{
    MSGBOX_TIP_AIC_NEW_VERSION = 1,
    MSGBOX_TIP_AIC_VERSION_ALREADY_TIP,
    MSGBOX_TIP_AIC_VERSION_CANCEL,
    MSGBOX_TIP_AIC_VERSION_DOWNLOADING,
    MSGBOX_TIP_AIC_VERSION_DOWNLOAD_FAIL,
    MSGBOX_TIP_AIC_VERSION_VERIFYING,
    MSGBOX_TIP_AIC_VERSION_VERIFY_FAIL,
    MSGBOX_TIP_AIC_VERSION_UPDATING,
    MSGBOX_TIP_AIC_VERSION_UPDATE_FINISH,
    MSGBOX_TIP_AIC_VERSION_UPDATE_FAIL,
};

void app_main_callback(widget_t **widget_list, widget_t *widget, void *e);
// bool app_aic_version_update_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
bool app_aic_function_light_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
bool app_aic_update_fail_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
#endif
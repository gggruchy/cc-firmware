#ifndef __APP_SETTING_H__
#define __APP_SETTING_H__
#include "app.h"

enum
{
    MSGBOX_TIP_CURRENT_NEW_VERSION = 0, // 已是最新版本
    MSGBOX_TIP_VERSION_LOCAL_UPDATE,
    MSGBOX_TIP_VERSION_OTA_UPDATE,
    MSGBOX_TIP_EXPORT_LOG,
    MSGBOX_TIP_EXPORT_LOG_COMPLETE,
    MSGBOX_TIP_NO_USB,
    MSGBOX_TIP_SILENT_MODE_ON,
    MSGBOX_TIP_RESET_ENTRANCE,
    MSGBOX_TIP_OTA_NETWORK_DISCONNECT,
    MSGBOX_TIP_OTA_NETWORK_TIMEOUT,
    MSGBOX_TIP_NO_PRINT_Z_OFFSET,
    MSGBOX_TIP_Z_OFFSET_MAX_VALUE,
    MSGBOX_TIP_Z_OFFSET_MIN_VALUE,
    MSGBOX_TIP_OTA_FAIL_RETRY,
    MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION,
    MSGBOX_TIP_EXECUTING_OTHER_TASK,
    MSGBOX_TIP_LOCAL_STORAGE_LACK,
    MSGBOX_TIP_Z_OFFSET_SAVE,
};

void app_setting_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_lamplight_language_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_info_callback(widget_t **widget_list, widget_t *widget, void *e);
bool app_version_update_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e); 
bool has_newer_FW(void);
bool has_newer_localFW(void);
bool has_newer_cloudFW(void);
void app_cutter_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_reset_factory_callback(widget_t **widget_list, widget_t *widget, void *e);
void app_detect_update_callback(widget_t **widget_list, widget_t *widget, void *e);
void ota_firmware_file_remove(void);
#endif

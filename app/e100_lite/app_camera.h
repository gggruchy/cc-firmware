#ifndef __APP_CAMERA_H__
#define __APP_CAMERA_H__
#include "app.h"

enum
{
    MSGBOX_TIP_PRINTING_FORBIDDEN = 0,    //打印中暂不允许操作
    MSGBOX_TIP_UDISK_STORAGE_LACK,    //U盘存储空间不足
    MSGBOX_TIP_UDISK_ABNORMAL,        //U盘读取异常
    MSGBOX_TIP_EXPORT_COMPLETE,       //导出成功
    MSGBOX_TIP_EXPORTING,             //导出中
    MSGBOX_TIP_NOT_CAMERA,            //摄像头连接异常
    MSGBOX_TIP_EXECUTING_OTHER,
};

void app_camera_callback(widget_t **widget_list, widget_t *widget, void *e);
bool app_camera_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
#endif
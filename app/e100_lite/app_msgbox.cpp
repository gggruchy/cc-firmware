#include "app_msgbox.h"
#include "hl_queue.h"
#include "app_top.h"

#define LOG_TAG "app_msgbox"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_ERROR
#include "log.h"

#define MAX_MSGBOX_SIZE 128
hl_queue_t msgbox_queue = NULL;
static bool msgbox_is_busy = false;
static window_t *current_window = NULL;
typedef struct
{
    bool modal;
    uint16_t window_index;
    msgbox_callback callback;
    void *user_data;
} msgbox_info_t;

bool get_app_msgbox_is_busy(void)
{
    return msgbox_is_busy;
}

static void app_msgbox_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    bool exit = false;
    if (widget_list && widget_list[0]->win && widget_list[0]->win->user_data)
    {
        msgbox_info_t *info = (msgbox_info_t *)widget_list[0]->win->user_data;
        if (((lv_event_t *)e)->code == (lv_event_code_t)LV_EVENT_EXIT)
        {
            window_copy_destory(widget_list[0]->win);
            free(info);
            hl_queue_dequeue(msgbox_queue, NULL, 1);
            msgbox_is_busy = false;
            current_window = NULL;
        }
        else
        {
            if (info->callback)
                exit = info->callback(widget_list[0]->win, widget, info->user_data, e);
            if (exit)
            {
                window_copy_destory(widget_list[0]->win);
                free(info);
                hl_queue_dequeue(msgbox_queue, NULL, 1);
                msgbox_is_busy = false;
                current_window = NULL;
                app_msgbox_run();
                ui_update_window_immediately();
            }
        }
    }
}

void app_msgbox_close_avtive_msgbox()
{
    if (app_msgbox_is_active())
    {
        if (current_window != NULL)
        {
            lv_event_t e;
            // msgbox_info_t *msgbox_info;
            e.code = (lv_event_code_t)LV_EVENT_EXIT;
            app_msgbox_callback(current_window->widget_list, NULL, (void *)&e);
        }
    }
}

void app_msgbox_close_all_avtive_msgbox(void)
{
    if (app_msgbox_is_active())
    {
        if (current_window != NULL)
        {
            lv_event_t e;
            // msgbox_info_t *msgbox_info;
            e.code = (lv_event_code_t)LV_EVENT_EXIT;
            app_msgbox_callback(current_window->widget_list, NULL, (void *)&e);
        }
        app_msgbox_reset_msgbox_queue();
    }
}

void app_msgbox_reset_msgbox_queue(void)
{
    hl_queue_reset(msgbox_queue);
}

void app_msgbox_init(void)
{
    // 对话框生命周期需要交互，队列存放的是对话框资源的指针
    hl_queue_create(&msgbox_queue, sizeof(msgbox_info_t *), MAX_MSGBOX_SIZE);
    msgbox_is_busy = false;
    if (msgbox_queue == NULL)
    {
        LOG_E("msgbox queue allocation failed\n");
    }
}

void app_msgbox_deinit(void)
{
    hl_queue_destory(&msgbox_queue);
}

void app_msgbox_run(void)
{
    if (!msgbox_is_busy && !hl_queue_is_empty(msgbox_queue))
    {
        msgbox_info_t *msgbox_info;
        hl_queue_peek(msgbox_queue, &msgbox_info, 1);
        current_window = window_copy(msgbox_info->window_index, app_msgbox_callback, lv_scr_act(), msgbox_info);
        msgbox_is_busy = true;

        app_top_back_dousing_state();
    }
    else if (msgbox_is_busy)
    {
        lv_event_t e;
        msgbox_info_t *msgbox_info;
        e.code = (lv_event_code_t)LV_EVENT_UPDATE;
        if(app_top_get_dousing_screen_state() == false)// 不息屏状态才置顶弹窗
        {
            if(current_window->widget_list[0]->obj_numbers > 0 && current_window->widget_list[0]->obj_container[0] != NULL){
                lv_obj_move_foreground(current_window->widget_list[0]->obj_container[0]);
            }
        }
        app_msgbox_callback(current_window->widget_list, NULL, (void *)&e);
        
    }
}

void app_msgbox_push(int window_index, bool modal, msgbox_callback callback, void *user_data)
{
    msgbox_info_t *info = (msgbox_info_t *)malloc(sizeof(msgbox_info_t));
    memset(info, 0, sizeof(msgbox_info_t));
    info->window_index = window_index;
    info->modal = modal;
    info->user_data = user_data;
    info->callback = callback;
    hl_queue_enqueue(msgbox_queue, &info, 1);
}

bool app_msgbox_is_active()
{
    return !hl_queue_is_empty(msgbox_queue);
}
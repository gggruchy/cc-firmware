#include "app_timezone.h"
#include "hl_common.h"
#include "configfile.h"
#include "Define_config_path.h"
#define LOG_TAG "app_timezone"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
static app_listitem_model_t *timezone_model = NULL;
static int timezone_index = 0;

static void timezone_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void timezone_listitem_update(void);
extern ConfigParser *get_sysconf();
const char *timezone_path[] = {
    "GMT+11",
    "GMT+11",
    "GMT+10",
    "Pacific/Marquesas",
    "GMT+9", //
    "GMT+8", //
    "GMT+7", //
    "GMT+6", //
    "GMT+5", //
    "GMT+4", //
    "Canada/Newfoundland",
    "GMT+3", // 巴西利亚
    "GMT+2", // 太平洋中部
    "GMT+1", // 佛得角标准时间
    "UTC",   // UTC
    "GMT-1", //
    "GMT-2", //
    "Moscow",
    "Asia/Tehran", //错误
    "Asia/Muscat",
    "Asia/Kabul",
    "Asia/Karachi",
    "Asia/Kolkata",
    "Asia/Kathmandu",
    "Asia/Dhaka",
    "Asia/Yangon",
    "Asia/Bangkok",
    "Asia/Shanghai",       // 中国
    "Asia/Tokyo",          // 小日本
    "Asia/Adelaide",       //错误
    "Sydney",              //
    "Australia/Lord_Howe", //错误
    "Asia/Magadan",
    "Pacific/Fiji",
    "Pacific/Chatham",
    "GMT-13",  //
    "GMT-14"}; //
void app_timezone_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        if (timezone_model == NULL)
        {
            timezone_model = app_listitem_model_create(WINDOW_ID_TIMEZONE_LIST_TEMPLATE, widget_list[WIDGET_ID_TIMEZONE_CONTAINER_LIST]->obj_container[0], timezone_listitem_callback, NULL);
            for (int i = 0; i < 37; i++)
                app_listitem_model_push_back(timezone_model);
        }
        timezone_index = get_sysconf()->GetInt("system", "timezone", 27);
        timezone_listitem_update();
        break;
    case LV_EVENT_DESTROYED:
        app_listitem_model_destory(timezone_model);
        timezone_model = NULL;
        break;
    case LV_EVENT_CHILD_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_TIMEZONE_CONTAINER_LIST:
        {
            widget_t *list_widget = (widget_t *)lv_event_get_param((lv_event_t *)e);
            window_t *win = list_widget->win;
            int model_size = app_listitem_model_count(timezone_model);
            app_listitem_t *item;
            int item_index;
            for (item_index = 0; item_index < model_size; item_index++)
            {
                item = app_listitem_model_get_item(timezone_model, item_index);
                if (item->win == win)
                    break;
            }
            timezone_index = item_index;
            timezone_listitem_update();
        }
        break;
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_TIMEZONE_BTN_CONFIRM:
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(window_get_top_widget_list());
            if (timezone_index <= 36)
            {
                LOG_D("timezone_index:%d -> %s\n", timezone_index, timezone_path[timezone_index]);
                hl_system("ln -s /usr/share/zoneinfo/%s /etc/localtime -f", timezone_path[timezone_index]);
                get_sysconf()->SetInt("system", "timezone", timezone_index);
                get_sysconf()->WriteIni(SYSCONF_PATH);
            }
            else
                LOG_E("timezone_index ERROR:%d", timezone_index);
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static void timezone_listitem_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_LONG_PRESSED:
        lv_event_send(app_listitem_model_get_parent(timezone_model), (lv_event_code_t)LV_EVENT_CHILD_LONG_PRESSED, widget);
        break;
    case LV_EVENT_CLICKED:
        lv_event_send(app_listitem_model_get_parent(timezone_model), (lv_event_code_t)LV_EVENT_CHILD_CLICKED, widget);
        break;
    }
}

static void timezone_listitem_update(void)
{
    for (int i = 0; i < 37; i++)
    {
        app_listitem_t *item = app_listitem_model_get_item(timezone_model, i);
        window_t *win = item->win;
        widget_t **widget_list = win->widget_list;

        lv_obj_set_pos(widget_list[WIDGET_ID_TIMEZONE_LIST_TEMPLATE_BTN_LIST_ITEM]->obj_container[0], 0, 30 * i);
        lv_label_set_text(widget_list[WIDGET_ID_TIMEZONE_LIST_TEMPLATE_BTN_LIST_ITEM]->obj_container[2], tr(141 + i));
        lv_label_set_long_mode(widget_list[WIDGET_ID_TIMEZONE_LIST_TEMPLATE_BTN_LIST_ITEM]->obj_container[2], LV_LABEL_LONG_DOT);
        if (timezone_index == i)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TIMEZONE_LIST_TEMPLATE_BTN_LIST_ITEM]->obj_container[1], ui_get_image_src(188));
            lv_obj_clear_flag(widget_list[WIDGET_ID_TIMEZONE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_img_set_src(widget_list[WIDGET_ID_TIMEZONE_LIST_TEMPLATE_BTN_LIST_ITEM]->obj_container[1], ui_get_image_src(187));
            lv_obj_add_flag(widget_list[WIDGET_ID_TIMEZONE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

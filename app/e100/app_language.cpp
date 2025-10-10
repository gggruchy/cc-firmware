#include "app_language.h"
#include "configfile.h"

static int language_index = 0;
static app_listitem_model_t *language_model = NULL;
extern ConfigParser *get_sysconf();

static void language_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void language_listitem_update(void);

void app_language_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        language_index = get_sysconf()->GetInt("system", "language", 0);
        language_model = app_listitem_model_create(WINDOW_ID_LANGUAGE_LIST_TEMPLATE, widget_list[WIDGET_ID_LANGUAGE_CONTAINER_LIST]->obj_container[0], language_listitem_callback, NULL);
        for (int i = 0; i < 11; i++)
            app_listitem_model_push_back(language_model);
        language_listitem_update();
        break;
    case LV_EVENT_DESTROYED:
        app_listitem_model_destory(language_model);
        language_model = NULL;
        break;
    case LV_EVENT_CHILD_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_LANGUAGE_CONTAINER_LIST:
        {
            widget_t *list_widget = (widget_t *)lv_event_get_param((lv_event_t *)e);
            window_t *win = list_widget->win;
            int model_size = app_listitem_model_count(language_model);
            app_listitem_t *item;
            int item_index;
            for (item_index = 0; item_index < model_size; item_index++)
            {
                item = app_listitem_model_get_item(language_model, item_index);
                if (item->win == win)
                    break;
            }
            language_index = item_index;
            language_listitem_update();
            tr_set_language(language_index);
            get_sysconf()->SetInt("system", "language", language_index);
            get_sysconf()->WriteIni(SYSCONF_PATH);

            lv_label_set_text(widget_list[WIDGET_ID_LANGUAGE_BTN_CONFIRM]->obj_container[2], tr(49));
            lv_label_set_text(widget_list[WIDGET_ID_LANGUAGE_CONTAINER_MASK]->obj_container[2], tr(67));
        }
        break;
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_LANGUAGE_BTN_CONFIRM:
            ui_set_window_index(WINDOW_ID_DEVICE_INSPECTION, NULL);
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static void language_listitem_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_LONG_PRESSED:
        lv_event_send(app_listitem_model_get_parent(language_model), (lv_event_code_t)LV_EVENT_CHILD_LONG_PRESSED, widget);
        break;
    case LV_EVENT_CLICKED:
        lv_event_send(app_listitem_model_get_parent(language_model), (lv_event_code_t)LV_EVENT_CHILD_CLICKED, widget);
        break;
    }
}

static void language_listitem_update(void)
{
    for (int i = 0; i < 11; i++)
    {
        app_listitem_t *item = app_listitem_model_get_item(language_model, i);
        window_t *win = item->win;
        widget_t **widget_list = win->widget_list;

        int row = i / 3;
        int col = i % 3;

        if (row == 0)
        lv_obj_set_pos(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], col * 146, 0);
        else
        lv_obj_set_pos(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], col * 146, row * 47);

        lv_obj_set_height(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], 46);
        lv_obj_set_width(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], 145);
        lv_obj_set_height(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], 37);
        lv_obj_set_width(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], 136);
        lv_obj_set_pos(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], 127, 0);

        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
        // lv_img_set_src(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[1], ui_get_image_src(163 + i));
        // lv_obj_align(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[1], LV_ALIGN_TOP_MID, 0, 16);
        // lv_obj_align(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[2], LV_ALIGN_TOP_MID, 0, 58);
        lv_label_set_text(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[2], language_infomation[i]);

        if (i == language_index)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0]);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
        }
    }
}

#include "app_print_platform.h"
#include "configfile.h"
#include "Define_config_path.h"
#include "klippy.h"

#define LOG_TAG "app_print_platform"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

void app_print_platform_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    extern ConfigParser *get_sysconf();
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 0) // A面
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            manual_control_sq.push("BED_MESH_SET_INDEX TYPE=standard INDEX=0");
            Printer::GetInstance()->manual_control_signal();
        }
        else
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            manual_control_sq.push("BED_MESH_SET_INDEX TYPE=enhancement INDEX=0");
            Printer::GetInstance()->manual_control_signal();
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_PRINT_PLATFORM_BTN_BACK:
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE:
            get_sysconf()->SetInt("system", "print_platform_type", 0);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            manual_control_sq.push("BED_MESH_SET_INDEX TYPE=standard INDEX=0");
            Printer::GetInstance()->manual_control_signal();
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            break;
        case WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE:
            get_sysconf()->SetInt("system", "print_platform_type", 1);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            manual_control_sq.push("BED_MESH_SET_INDEX TYPE=enhancement INDEX=0");
            Printer::GetInstance()->manual_control_signal();
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_A_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_PLATFORM_BTN_B_FACE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

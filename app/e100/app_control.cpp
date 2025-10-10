#include "app_control.h"
#include "simplebus.h"
#include "service.h"
#include "ui_api.h"
#include "gpio.h"
#include "klippy.h"


#define LOG_TAG "app_control"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"


#define FEED_TEMPERATURE_DEFICIENCY_VALUE 170
enum
{
    CURRENT_SCALE_01MM = 0,
    CURRENT_SCALE_1MM,
    CURRENT_SCALE_10MM,
    CURRENT_SCALE_30MM,
};

enum
{
    AXIS_HOME_FAIL_X = 0, // x轴归零失败
    AXIS_HOME_FAIL_Y,
    AXIS_HOME_FAIL_Z,
};

enum
{
    MSGBOX_TIP_MOVE_FAILED = 0,
    MSGBOX_TIP_HOMING,
    MSGBOX_TIP_HOME_SUCCESS,
    MSGBOX_TIP_EXTRUDER_MOVE_FAILED,

    MSGBOX_TIP_MANUAL_OUT_FEED, // 手动切料提示
    MSGBOX_TIP_EXTRUSION_FEED,  // 耗材挤出提示
    MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION,    //打印中暂不允许操作
    MSGBOX_TIP_NON_PRINTING_FORBIDDEN_OPERATION,    //非打印中暂不允许操作

    MSGBOX_TIP_FEED_TEMPERATURE_DEFICIENCY, 
    MSGBOX_TIP_EXECUTING_OTHER_TASK,    
    MSGBOX_TIP_ENGINEERING_MODE_PRINTING_FAILED,    
};

enum
{
    FEED_STATE_IDLE = 0, // 空闲
    FEED_STATE_PROCEDURE_1,
    FEED_STATE_PROCEDURE_2,
    FEED_STATE_PROCEDURE_3,
    FEED_STATE_PROCEDURE_4,

};

typedef enum
{
    UI_EVENT_CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE,
    UI_EVENT_CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT,
    UI_EVENT_CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT,
} ui_event_change_filament_t;
static hl_queue_t ui_change_filament_queue = NULL;

typedef enum
{
    UI_EVENT_FEED_TYPE_IN_FEED,
    UI_EVENT_FEED_TYPE_OUT_FEED,
} ui_event_feed_type_t;
static hl_queue_t ui_feed_type_queue = NULL;

int feed_current_state = 0;
static int feed_type_inedx = 0;

static int scale_index = 0;
static int print_speed_index = 0;
int printing_speed_value = 100;    // 打印速度值
static double real_printing_speed = 100.; // 真实打印速度值

static int model_helper_fan_switch = 0;
static int model_fan_switch = 0;
static int box_fan_switch = 0;
static int model_helper_fan_value = 0;
static int model_fan_value = 0;

static int extruder_temperature = 0;
static int hot_bed_temperature = 0;
static int extruder_max_temperature = 320;
static int printing_extruder_min_temperature = 170;
static int hot_bed_max_temperature = 110;
static int extruder_last_temperature = 0;
static int hot_bed_last_temperature = 0;
static bool homing_fail_msgbox_flag = false;

static bool print_speed_set_state = false;
static keyboard_t *digital_keyboard = NULL;
static bool homeing_state = false;
static bool extrusion_temp_deficiency_action = false;
static bool engineering_mode = false;  // 添加工程模式标志位

static void app_control_update_style(widget_t **widget_list);
static void app_control_update_temp(widget_t **widget_list);
static void app_feed_update(widget_t **widget_list);
static void app_fan_update_style(widget_t **widget_list);
static void app_fan_init(widget_t **widget_list);
static bool app_control_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_control_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_control_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static void app_feed_type_update(widget_t **widget_list);
static explorer_item_t file_item = {0};
extern ConfigParser *get_sysconf();

#define FAN_RANGE_MINI_HELPER 50    //辅助风扇调整区间改为50-100
#define FAN_RANGE_MIN 10
#define FAN_RANGE_MAX 100
#define FAN_RANGE_STEP 10
#define BOX_FAN_DEFAULT_VALUE 100

#define FAN_SLINET_VALUE 50
#define FAN_NORMAL_VALUE 80
#define FAN_CRAZY_VALUE 100

static void app_fan_ctrl(int fan_id, int sw, int value)
{
    srv_control_req_fan_t req;
    if (sw == 0)
        value = 0;
    req.fan_id = fan_id;
    req.value = value / 100.;
    simple_bus_request("srv_control", SRV_CONTROL_FAN_SPEED, &req, NULL);
}

void ui_change_filament_callback(int state)
{
    ui_event_change_filament_t ui_event;
    switch (state)
    {
    case CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE:
        ui_event = UI_EVENT_CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE;
        hl_queue_enqueue(ui_change_filament_queue, &ui_event, 1);
        break;
    case CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT:
        ui_event = UI_EVENT_CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT;
        hl_queue_enqueue(ui_change_filament_queue, &ui_event, 1);
        break;
    case CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT:
        ui_event = UI_EVENT_CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT;
        hl_queue_enqueue(ui_change_filament_queue, &ui_event, 1);
        break;
    }
}

void ui_feed_type_callback(int state)
{
    ui_event_feed_type_t ui_event;
    switch (state)
    {
    case FEED_TYPE_IN_FEED:
        ui_event = UI_EVENT_FEED_TYPE_IN_FEED;
        hl_queue_enqueue(ui_feed_type_queue, &ui_event, 1);
        break;
    case FEED_TYPE_OUT_FEED:
        ui_event = UI_EVENT_FEED_TYPE_OUT_FEED;
        hl_queue_enqueue(ui_feed_type_queue, &ui_event, 1);
        break;
    }
}

static double get_move_distance(int index)
{
    if (index == CURRENT_SCALE_01MM)
        return 0.1;
    else if (index == CURRENT_SCALE_1MM)
        return 1;
    else if (index == CURRENT_SCALE_10MM)
        return 10;
    else if (index == CURRENT_SCALE_30MM)
        return 30;
}

void app_control_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static int keyboard_edit_index = 0;
    static int keyboard_exit_via_ok = 0;
    static int other_win_entrance_index = 0;
    static bool print_speed_set_state = false;
    static bool extrusion_tip_state = false;
    static uint16_t extruder_idx = 0;

    srv_state_t *ss = app_get_srv_state();

    srv_control_req_move_t move_request = {};
    srv_control_res_move_t move_response = {};
    srv_control_req_home_t home_request = {};
    srv_control_res_home_t home_response = {};

    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:

#define MANUAL_MOVE_SPEED 3600
#define MANUAL_MOVE_Z_SPEED 900
#define EXTRUDER_MOVE_DISTANCE 120
#define EXTRUDER_OUT_MOVE_DISTANCE 60
#define EXTRUDER_MOVE_SPEED 240

        extrusion_tip_state = true;
        extrusion_temp_deficiency_action = false;
        scale_index = CURRENT_SCALE_1MM;
        ui_cb[get_print_speed_cb](&real_printing_speed);
        switch ((int)(real_printing_speed * 100.))
        {
        case PRINTING_EQUILIBRIUM_SPEED:
            print_speed_index = EQUILIBRIUM_PRINT_SPEED;
            break;
        case PRINTING_EXERCISE_SPEED:
            print_speed_index = EXERCISE_PRINT_SPEED;
            break;
        case PRINTING_RAGE_SPEED:
            print_speed_index = RAGE_PRINT_SPEED;
            break;
        case PRINTING_SILENCE_SPEED:
            print_speed_index = SILENCE_PRINT_SPEED;
            break;
        default:
            print_speed_index = EQUILIBRIUM_PRINT_SPEED;
            printing_speed_value = PRINTING_EQUILIBRIUM_SPEED;
            ui_cb[set_print_speed_cb](&printing_speed_value);
            break;
        }
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_BOX_TEMPERATURE]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[0], 64, 74);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[1], 5, 15);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[2], 26, 20);
        lv_obj_set_size(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], 64, 74);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], 0, 74);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[1], 7, 17);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[2], 30, 21);
        lv_obj_set_size(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[0], 64, 74);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[0], 0, 147);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[1], 0, 18);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[2], 0, 45);

        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_LABEL_SHOWER_TEMPERATURE_REAL]->obj_container[0], 0, 43);
        lv_obj_set_pos(widget_list[WIDGET_ID_CONTROL_LABEL_HOT_BED_REAL]->obj_container[0], 0, 42);
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_BOX_TEMPERATURE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
#endif
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
        lv_img_set_src(widget_list[WIDGET_ID_CONTROL_IMAGE_XY_PLUS_MINUS]->obj_container[0], ui_get_image_src(30));
        app_control_update_style(widget_list);

        // 若处于回零状态弹窗
        if (ss->home_state[0] == SRV_STATE_HOME_HOMING ||
            ss->home_state[1] == SRV_STATE_HOME_HOMING ||
            ss->home_state[2] == SRV_STATE_HOME_HOMING)
        {
            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_HOMING);
        }

        // 快捷操作
        other_win_entrance_index = (int)lv_event_get_param((lv_event_t *)e);
        if (other_win_entrance_index == WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE ||
            other_win_entrance_index == WIDGET_ID_MAIN_BTN_SHOWER_TEMPERATURE)
        {
            // event_send前设置update_layout便于event_send的ui改动生效
            lv_obj_update_layout(widget_list[0]->obj_container[0]);
            lv_event_send(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[0], LV_EVENT_CLICKED, NULL);
        }
        else if (other_win_entrance_index == WIDGET_ID_PRINT_BTN_HOT_BED ||
                 other_win_entrance_index == WIDGET_ID_MAIN_BTN_HOT_BED)
        {
            lv_obj_update_layout(widget_list[0]->obj_container[0]);
            lv_event_send(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], LV_EVENT_CLICKED, NULL);
        }

        // if (app_print_get_print_state())
        // {
        //     // 打印时禁止点击打印界面按键
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_X_AXIS_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_X_AXIS_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_Y_AXIS_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_Y_AXIS_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_Z_AXIS_UP]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_Z_AXIS_DOWN]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_MOTOR_OFF]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_X_HOME]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_Y_HOME]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_Z_HOME]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_ALL_HOME]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_IN_FEED]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_OUT_FEED]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

        //     lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CONTROL_BTN_IN_FEED]->obj_container[2], 127, LV_PART_MAIN);
        //     lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CONTROL_BTN_OUT_FEED]->obj_container[2], 127, LV_PART_MAIN);
        // }
        // else
        // {
        //     lv_obj_add_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        // }

        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_FAN_PAGA]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_IN_FEED]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_OUT_FEED]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    case LV_EVENT_DESTROYED:
        if (digital_keyboard)
        {
            keyboard_exit_via_ok = 0;
            window_copy_destory((window_t *)(digital_keyboard->user_data));
            keyboard_destroy(digital_keyboard);
            digital_keyboard = NULL;
        }

        lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
        lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
        lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_FUNCTION_SETTING]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
        break;
    case LV_EVENT_PRESSING:
        switch (widget->header.index)
        {
        case WIDGET_ID_CONTROL_BTN_X_AXIS_MINUS:
            lv_img_set_src(widget_list[WIDGET_ID_CONTROL_IMAGE_XY_PLUS_MINUS]->obj_container[0], ui_get_image_src(31));
            break;
        case WIDGET_ID_CONTROL_BTN_X_AXIS_PLUS:
            lv_img_set_src(widget_list[WIDGET_ID_CONTROL_IMAGE_XY_PLUS_MINUS]->obj_container[0], ui_get_image_src(32));
            break;
        case WIDGET_ID_CONTROL_BTN_Y_AXIS_PLUS:
            lv_img_set_src(widget_list[WIDGET_ID_CONTROL_IMAGE_XY_PLUS_MINUS]->obj_container[0], ui_get_image_src(33));
            break;
        case WIDGET_ID_CONTROL_BTN_Y_AXIS_MINUS:
            lv_img_set_src(widget_list[WIDGET_ID_CONTROL_IMAGE_XY_PLUS_MINUS]->obj_container[0], ui_get_image_src(34));
            break;
        }
        break;
    case LV_EVENT_RELEASED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CONTROL_BTN_Y_AXIS_PLUS:
        case WIDGET_ID_CONTROL_BTN_Y_AXIS_MINUS:
        case WIDGET_ID_CONTROL_BTN_X_AXIS_MINUS:
        case WIDGET_ID_CONTROL_BTN_X_AXIS_PLUS:
            lv_img_set_src(widget_list[WIDGET_ID_CONTROL_IMAGE_XY_PLUS_MINUS]->obj_container[0], ui_get_image_src(30));
            break;
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CONTROL_BTN_PRINT_SPEED:
            if (digital_keyboard)
            {
                keyboard_exit_via_ok = 0;
                window_copy_destory((window_t *)(digital_keyboard->user_data));
                keyboard_destroy(digital_keyboard);
                digital_keyboard = NULL;
                lv_event_send(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], (lv_event_code_t)LV_EVENT_CHILD_DESTROYED, NULL);
                lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_ADJUST_POS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            }
            print_speed_set_state = true;
            lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0]);
            lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_FUNCTION_SETTING]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0]);
            lv_obj_move_foreground(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0]);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[0], lv_color_hex(0xFF3C3E3B), LV_PART_MAIN);
            break;
        case WIDGET_ID_CONTROL_CONTAINER_MASK:
            if (print_speed_set_state)
            {
                print_speed_set_state = false;
                lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
                lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_FUNCTION_SETTING]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
            }
            if (digital_keyboard)
            {
                keyboard_exit_via_ok = 0;
                window_copy_destory((window_t *)(digital_keyboard->user_data));
                keyboard_destroy(digital_keyboard);
                digital_keyboard = NULL;
                lv_event_send(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], (lv_event_code_t)LV_EVENT_CHILD_DESTROYED, NULL);
                lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_ADJUST_POS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            }
            if (other_win_entrance_index != 0)
            {
                other_win_entrance_index = 0;
            }
            break;
        case WIDGET_ID_CONTROL_BTN_SCALE_01MM:
            scale_index = CURRENT_SCALE_01MM;
            app_control_update_style(widget_list);
            break;
        case WIDGET_ID_CONTROL_BTN_SCALE_1MM:
            scale_index = CURRENT_SCALE_1MM;
            app_control_update_style(widget_list);
            break;
        case WIDGET_ID_CONTROL_BTN_SCALE_10MM:
            scale_index = CURRENT_SCALE_10MM;
            app_control_update_style(widget_list);
            break;
        case WIDGET_ID_CONTROL_BTN_SCALE_30MM:
            scale_index = CURRENT_SCALE_30MM;
            app_control_update_style(widget_list);
            break;
        case WIDGET_ID_CONTROL_BTN_RAGE_SPEED:
            if(app_print_get_print_state() != true)       //非打印中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_NON_PRINTING_FORBIDDEN_OPERATION);
            }
            else
            {
                print_speed_index = RAGE_PRINT_SPEED;
                printing_speed_value = PRINTING_RAGE_SPEED;
                ui_cb[set_print_speed_cb](&printing_speed_value);
                app_control_update_style(widget_list);
            }
            break;
        case WIDGET_ID_CONTROL_BTN_EXERCISE_SPEED:
            if(app_print_get_print_state() != true)       //非打印中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_NON_PRINTING_FORBIDDEN_OPERATION);
            }
            else
            {
                print_speed_index = EXERCISE_PRINT_SPEED;
                printing_speed_value = PRINTING_EXERCISE_SPEED;
                ui_cb[set_print_speed_cb](&printing_speed_value);
                app_control_update_style(widget_list);
            }
            break;
        case WIDGET_ID_CONTROL_BTN_EQUILIBRIUM_SPEED:
            if(app_print_get_print_state() != true)       //非打印中
            {
                break;
            }
            else
            {
                print_speed_index = EQUILIBRIUM_PRINT_SPEED;
                printing_speed_value = PRINTING_EQUILIBRIUM_SPEED;
                ui_cb[set_print_speed_cb](&printing_speed_value);
                app_control_update_style(widget_list);
            }
            break;
        case WIDGET_ID_CONTROL_BTN_SILENCE_SPEED:
            if(app_print_get_print_state() != true)       //非打印中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_NON_PRINTING_FORBIDDEN_OPERATION);
            }
            else
            {
                print_speed_index = SILENCE_PRINT_SPEED;
                if (model_helper_fan_value > Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_SILENCE_MODE))
                {
                    // model_helper_fan_value = model_helper_fan_value < FAN_RANGE_MIN ? FAN_RANGE_MIN : model_helper_fan_value;
                    // model_helper_fan_value = model_helper_fan_value > FAN_RANGE_MAX ? FAN_RANGE_MAX : model_helper_fan_value;
                    model_helper_fan_value = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_SILENCE_MODE);
                    app_fan_ctrl(FAN_ID_MODEL_HELPER, model_helper_fan_switch, model_helper_fan_value);
                }
                printing_speed_value = PRINTING_SILENCE_SPEED;
                ui_cb[set_print_speed_cb](&printing_speed_value);
                app_control_update_style(widget_list);
            }
            break;
        case WIDGET_ID_CONTROL_BTN_X_AXIS_PLUS:
        case WIDGET_ID_CONTROL_BTN_X_AXIS_MINUS:
        case WIDGET_ID_CONTROL_BTN_Y_AXIS_PLUS:
        case WIDGET_ID_CONTROL_BTN_Y_AXIS_MINUS:
        case WIDGET_ID_CONTROL_BTN_Z_AXIS_UP:
        case WIDGET_ID_CONTROL_BTN_Z_AXIS_DOWN:
        {
            if(app_print_get_print_state() == true)       //打印中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                uint16_t idx = widget->header.index;
                if (idx == WIDGET_ID_CONTROL_BTN_X_AXIS_PLUS)
                    move_request.x = get_move_distance(scale_index);
                else if (idx == WIDGET_ID_CONTROL_BTN_X_AXIS_MINUS)
                    move_request.x = -get_move_distance(scale_index);
                else if (idx == WIDGET_ID_CONTROL_BTN_Y_AXIS_PLUS)
                    move_request.y = get_move_distance(scale_index);
                else if (idx == WIDGET_ID_CONTROL_BTN_Y_AXIS_MINUS)
                    move_request.y = -get_move_distance(scale_index);
                else if (idx == WIDGET_ID_CONTROL_BTN_Z_AXIS_UP)
                    move_request.z = -get_move_distance(scale_index);
                else if (idx == WIDGET_ID_CONTROL_BTN_Z_AXIS_DOWN)
                    move_request.z = get_move_distance(scale_index);
                if (idx == WIDGET_ID_CONTROL_BTN_Z_AXIS_UP || idx == WIDGET_ID_CONTROL_BTN_Z_AXIS_DOWN)
                    move_request.f = MANUAL_MOVE_Z_SPEED;
                else
                    move_request.f = MANUAL_MOVE_SPEED;
                simple_bus_request("srv_control", SRV_CONTROL_STEPPER_MOVE, &move_request, &move_response);
                if (move_response.ret != SRV_CONTROL_RET_OK)
                {
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_MOVE_FAILED);
                }
            }
        }
        break;
        // 进出料
        case WIDGET_ID_CONTROL_BTN_IN_FEED:
        case WIDGET_ID_CONTROL_BTN_OUT_FEED:
        {
            if(app_print_get_print_state() == true)       //打印中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                if (print_speed_set_state || digital_keyboard)
                    break;
                uint16_t idx = widget->header.index;
                if (idx == WIDGET_ID_CONTROL_BTN_IN_FEED)
                    move_request.e = EXTRUDER_MOVE_DISTANCE;
                else if (idx == WIDGET_ID_CONTROL_BTN_OUT_FEED)
                    move_request.e = -EXTRUDER_OUT_MOVE_DISTANCE;
                move_request.f = EXTRUDER_MOVE_SPEED;
                simple_bus_request("srv_control", SRV_CONTROL_STEPPER_MOVE, &move_request, &move_response);
                if (move_response.ret != SRV_CONTROL_RET_OK)
                {
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXTRUDER_MOVE_FAILED);
                }
            }
        }
        break;

        case WIDGET_ID_CONTROL_BTN_X_HOME:
        case WIDGET_ID_CONTROL_BTN_Y_HOME:
        case WIDGET_ID_CONTROL_BTN_Z_HOME:
        case WIDGET_ID_CONTROL_BTN_ALL_HOME:
        {
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                uint16_t idx = widget->header.index;
                if (idx == WIDGET_ID_CONTROL_BTN_X_HOME)
                    home_request.x = 1;
                else if (idx == WIDGET_ID_CONTROL_BTN_Y_HOME)
                    home_request.y = 1;
                else if (idx == WIDGET_ID_CONTROL_BTN_Z_HOME)
                    home_request.z = 1;
                else if (idx == WIDGET_ID_CONTROL_BTN_ALL_HOME)
                    home_request.x = home_request.y = home_request.z = 1;
                simple_bus_request("srv_control", SRV_CONTROL_STEPPER_HOME, &home_request, &home_response);
                app_update_srv_state();
                if (home_response.ret == SRV_CONTROL_RET_OK)
                {
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_HOMING);

                }
            }
        }
        break;
        case WIDGET_ID_CONTROL_BTN_MOTOR_OFF:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                simple_bus_request("srv_control", SRV_CONTROL_STEPPER_DISABLE, NULL, NULL);
            }
            break;
        case WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE:
        case WIDGET_ID_CONTROL_BTN_HOT_BED:
            if (print_speed_set_state)
            {
                print_speed_set_state = false;
                lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
                lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_FUNCTION_SETTING]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
            }
            if (widget->header.index == WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE)
            {
                keyboard_edit_index = 0;
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[0], lv_color_hex(0xFF3C3E3B), LV_PART_MAIN);
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
            }
            else if (widget->header.index == WIDGET_ID_CONTROL_BTN_HOT_BED)
            {
                keyboard_edit_index = 1;
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], lv_color_hex(0xFF3C3E3B), LV_PART_MAIN);
            }

            if (digital_keyboard == NULL)
            {
                lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_ADJUST_POS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

                lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0]);
                lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_FUNCTION_SETTING]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0]);
                lv_obj_move_foreground(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0]);

                keyboard_exit_via_ok = 1;
                if ((digital_keyboard = app_digital_keyboard_create(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD], 0)) == NULL)
                    LOG_E("keyboard null\n");
                keyboard_set_text(digital_keyboard, "");
            }
            else
                keyboard_set_text(digital_keyboard, "");
            break;
        case WIDGET_ID_CONTROL_BTN_EXTRUDER_UP:
        case WIDGET_ID_CONTROL_BTN_EXTRUDER_DOWN:
        {
            extruder_idx = widget->header.index;
            if(app_print_get_print_state() == true)       //打印中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
                break;
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
                break;
            }
            else if (app_get_srv_state()->heater_state[HEATER_ID_EXTRUDER].current_temperature < FEED_TEMPERATURE_DEFICIENCY_VALUE && extrusion_tip_state && extruder_idx == WIDGET_ID_CONTROL_BTN_EXTRUDER_DOWN)
            {
                extrusion_tip_state = false;
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_control_routine_msgbox_callback, (void *)MSGBOX_TIP_FEED_TEMPERATURE_DEFICIENCY);
                break;
            }

            if (print_speed_set_state || digital_keyboard)
                break;
            
            if (extruder_idx == WIDGET_ID_CONTROL_BTN_EXTRUDER_DOWN)
            {
                move_request.e = Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "extrude_length", 10);
                move_request.f = Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "extrude_speed", 150);
            }
            else if (extruder_idx == WIDGET_ID_CONTROL_BTN_EXTRUDER_UP)
            {
                move_request.e = -Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "retract_length", 10);
                move_request.f = Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "retract_speed", 150);
            }
            simple_bus_request("srv_control", SRV_CONTROL_STEPPER_MOVE, &move_request, &move_response);
            if (move_response.ret != SRV_CONTROL_RET_OK)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXTRUDER_MOVE_FAILED);
            }
        }
            break;
        }
        break;
    case LV_EVENT_CHILD_VALUE_CHANGE:
        switch (widget->header.index)
        {
        case WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD:
            const char *str = keyboard_get_text(digital_keyboard);
            if (str)
            {
                if (keyboard_edit_index == 0)
                {
                    extruder_temperature = atoi(str);
                    if (extruder_temperature > extruder_max_temperature)
                    {
                        extruder_temperature = extruder_max_temperature;
                        keyboard_set_text(digital_keyboard, "%d", extruder_max_temperature);
                    }

                    lv_label_set_text_fmt(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[2], "%d℃",  extruder_temperature);
                }
                else if (keyboard_edit_index == 1)
                {
                    hot_bed_temperature = atoi(str);
                    if (hot_bed_temperature > hot_bed_max_temperature)
                    {
                        hot_bed_temperature = hot_bed_max_temperature;
                        keyboard_set_text(digital_keyboard, "%d", hot_bed_max_temperature);
                    }

                    lv_label_set_text_fmt(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[2], "%d℃", hot_bed_temperature);
                }
                app_control_update_style(widget_list);
            }
            break;
        }
        break;
    case LV_EVENT_CHILD_DESTROYED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD:
            digital_keyboard = NULL;
            lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_ADJUST_POS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_CONTAINER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
            lv_obj_set_parent(widget_list[WIDGET_ID_CONTROL_CONTAINER_FUNCTION_SETTING]->obj_container[0], widget_list[WIDGET_ID_CONTROL_CONTAINER_CONTROL_PAGA]->obj_container[0]);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[0], lv_color_hex(0xFF1E1E20), LV_PART_MAIN);

            if (other_win_entrance_index != 0 && keyboard_exit_via_ok)
            {
                other_win_entrance_index = 0;
                ui_set_window_index(ui_get_window_last_index(), NULL);
                app_top_update_style(window_get_top_widget_list());
            }

            // 修改目标温度
            if (keyboard_exit_via_ok)
            {
                srv_control_req_heater_t heater_request = {};

                if (Printer::GetInstance()->m_virtual_sdcard->is_active() && extruder_temperature < printing_extruder_min_temperature)
                {
                    extruder_temperature = printing_extruder_min_temperature;
                }
                if (keyboard_edit_index == 0)
                {
                    heater_request.heater_id = HEATER_ID_EXTRUDER;
                    heater_request.temperature = extruder_temperature;
                }
                else if (keyboard_edit_index == 1)
                {
                    heater_request.heater_id = HEATER_ID_BED;
                    heater_request.temperature = hot_bed_temperature;
                }
                simple_bus_request("srv_control", SRV_CONTROL_HEATER_TEMPERATURE, &heater_request, NULL);
                app_update_srv_state();
            }
            else
            {
                extruder_temperature = ss->heater_state[HEATER_ID_EXTRUDER].target_temperature;
                hot_bed_temperature = ss->heater_state[HEATER_ID_BED].target_temperature;
            }
            app_control_update_style(widget_list);
            app_control_update_temp(widget_list);

            break;
        }
        break;
    case LV_EVENT_UPDATE:
        if (homing_fail_msgbox_flag)
        {
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_control_routine_msgbox_callback, (void *)AXIS_HOME_FAIL_X);
            homing_fail_msgbox_flag = false;
        }
        app_control_update_temp(widget_list);


        // 若处于回零状态弹窗
        if ((ss->home_state[0] == SRV_STATE_HOME_HOMING ||
             ss->home_state[1] == SRV_STATE_HOME_HOMING ||
             ss->home_state[2] == SRV_STATE_HOME_HOMING) &&
            homeing_state == false)
        {
            app_msgbox_close_all_avtive_msgbox();
            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_HOMING);
        }

        // 弹窗后继续挤出
        if(extrusion_temp_deficiency_action)
        {
            extrusion_temp_deficiency_action = false;
            if (extruder_idx == WIDGET_ID_CONTROL_BTN_EXTRUDER_DOWN)
            {
                move_request.e = Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "extrude_length", 10);
                move_request.f = Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "extrude_speed", 150);
            }
            else if (extruder_idx == WIDGET_ID_CONTROL_BTN_EXTRUDER_UP)
            {
                move_request.e = -Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "retract_length", 10);
                move_request.f = Printer::GetInstance()->m_pconfig->GetDouble("change_filament", "retract_speed", 150);
            }
            simple_bus_request("srv_control", SRV_CONTROL_STEPPER_MOVE, &move_request, &move_response);
            if (move_response.ret != SRV_CONTROL_RET_OK)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_EXTRUDER_MOVE_FAILED);
            }
        }

        break;
    }
}

static bool app_control_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    srv_state_t *ss = app_get_srv_state();

    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        // 移动前未归零
        if (tip_index == MSGBOX_TIP_MOVE_FAILED)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(41));
        // 加热喷头后移动
        else if (tip_index == MSGBOX_TIP_EXTRUDER_MOVE_FAILED)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(226));
        // 归零中
        else if (tip_index == MSGBOX_TIP_HOMING)
        {
            homeing_state = true;
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(34));
        }
        // 归零完成
        else if (tip_index == MSGBOX_TIP_HOME_SUCCESS)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(45));
        // 打印中不允许操作
        else if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(290));
        else if(tip_index == MSGBOX_TIP_NON_PRINTING_FORBIDDEN_OPERATION)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(320));
        else if (tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(313));
        else if (tip_index == MSGBOX_TIP_ENGINEERING_MODE_PRINTING_FAILED)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(198));
        start_tick = utils_get_current_tick();
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
        case WIDGET_ID_SINGLE_MSGBOX_BTN_CONTAINER:
            if (tip_index == MSGBOX_TIP_ENGINEERING_MODE_PRINTING_FAILED)
            {
                ui_cb[manual_control_cb]((char *)"M140 S0");
            }
            if (tip_index != MSGBOX_TIP_HOMING)
                return true;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_HOMING)
        {
            if ((ss->home_state[0] == SRV_STATE_HOME_END_SUCCESS || ss->home_state[0] == SRV_STATE_HOME_IDLE) &&
                (ss->home_state[1] == SRV_STATE_HOME_END_SUCCESS || ss->home_state[1] == SRV_STATE_HOME_IDLE) &&
                (ss->home_state[2] == SRV_STATE_HOME_END_SUCCESS || ss->home_state[2] == SRV_STATE_HOME_IDLE))
            {
                // app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_HOME_SUCCESS);
                homeing_state = false;
                return true;
            }

            if (ss->home_state[0] == SRV_STATE_HOME_END_FAILED)
            {
                app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_control_over_msgbox_callback, (void *)AXIS_HOME_FAIL_X);
                return true;
            }
            else if (ss->home_state[1] == SRV_STATE_HOME_END_FAILED)
            {
                app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_control_over_msgbox_callback, (void *)AXIS_HOME_FAIL_Y);
                return true;
            }
            else if (ss->home_state[2] == SRV_STATE_HOME_END_FAILED)
            {
                // app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_control_over_msgbox_callback, (void *)AXIS_HOME_FAIL_Z);
                return true;
            }
        }
        else if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION  || tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK ||
                tip_index == MSGBOX_TIP_NON_PRINTING_FORBIDDEN_OPERATION)
        {
            if (utils_get_current_tick() - start_tick > 2 * 1000)
            {
                return true;
            }
        }
        break;
    }
    return false;
}

static bool app_control_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    bool ret = false;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], tr(37));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_BTN_PARTICULARS]->obj_container[2], tr(43));
        if (tip_index == AXIS_HOME_FAIL_X)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：302,%s", tr(202));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == AXIS_HOME_FAIL_Y)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：303,%s", tr(204));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == AXIS_HOME_FAIL_Z)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：304,%s", tr(206));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_OVER_MSGBOX_BTN_PARTICULARS:
            homing_fail_msgbox_flag = true;
            ret = true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return ret;
}

static void app_control_update_temp(widget_t **widget_list)
{
    srv_state_t *ss = app_get_srv_state();
    // printf("extruder_temperature %lf, hot_bed_temperature %lf box %lf\n",
    //        ss->heater_state[HEATER_ID_EXTRUDER].current_temperature,
    //        ss->heater_state[HEATER_ID_BED].current_temperature,
    //        ss->heater_state[HEATER_ID_BOX].current_temperature);
    if (digital_keyboard == NULL)
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_CONTROL_LABEL_SHOWER_TEMPERATURE_REAL]->obj_container[0], "%d℃", (int)ss->heater_state[HEATER_ID_EXTRUDER].current_temperature);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_CONTROL_LABEL_HOT_BED_REAL]->obj_container[0], "%d℃", (int)ss->heater_state[HEATER_ID_BED].current_temperature);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_CONTROL_BTN_SHOWER_TEMPERATURE]->obj_container[2], "%d℃",  (int)ss->heater_state[HEATER_ID_EXTRUDER].target_temperature);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_CONTROL_BTN_HOT_BED]->obj_container[2], "%d℃",  (int)ss->heater_state[HEATER_ID_BED].target_temperature);
        
        extruder_last_temperature = (int)ss->heater_state[HEATER_ID_EXTRUDER].current_temperature;
        hot_bed_last_temperature = (int)ss->heater_state[HEATER_ID_BED].current_temperature;

        // 网页管理改变目标温度后更新到UI
        if (extruder_temperature != (int)ss->heater_state[HEATER_ID_EXTRUDER].target_temperature)
        {
            extruder_temperature = (int)ss->heater_state[HEATER_ID_EXTRUDER].target_temperature;
        }
        if (hot_bed_temperature != (int)ss->heater_state[HEATER_ID_BED].target_temperature)
        {
            hot_bed_temperature = (int)ss->heater_state[HEATER_ID_BED].target_temperature;
        }
    }
    lv_label_set_text_fmt(widget_list[WIDGET_ID_CONTROL_BTN_BOX_TEMPERATURE]->obj_container[2], "%d℃", (int)ss->heater_state[HEATER_ID_BOX].current_temperature);
}

static void app_control_update_style(widget_t **widget_list)
{
    lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_01MM]->obj_container[2], "0.1mm");
    lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_1MM]->obj_container[2], "1mm");
    lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_10MM]->obj_container[2], "10mm");
    lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_30MM]->obj_container[2], "30mm");
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_01MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_1MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_10MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_30MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_RAGE_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_EXERCISE_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_EQUILIBRIUM_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CONTROL_BTN_SILENCE_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    if (scale_index == CURRENT_SCALE_01MM)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_01MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    }
    else if (scale_index == CURRENT_SCALE_1MM)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_1MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    }
    else if (scale_index == CURRENT_SCALE_10MM)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_10MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    }
    else if (scale_index == CURRENT_SCALE_30MM)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_SCALE_30MM]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    }

    if (print_speed_index == RAGE_PRINT_SPEED)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_RAGE_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[2], tr(4));
    }
    if (print_speed_index == EXERCISE_PRINT_SPEED)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_EXERCISE_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[2], tr(5));
    }
    if (print_speed_index == EQUILIBRIUM_PRINT_SPEED)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_EQUILIBRIUM_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[2], tr(6));
    }
    if (print_speed_index == SILENCE_PRINT_SPEED)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CONTROL_BTN_SILENCE_SPEED]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_CONTROL_BTN_PRINT_SPEED]->obj_container[2], tr(12));
    }
    app_control_update_temp(widget_list);
}

void app_fan_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    srv_state_t *ss = app_get_srv_state();
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        app_fan_init(widget_list);
        app_fan_update_style(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_FAN_BTN_ASSIST_FAN_SWITCH:
            model_helper_fan_switch = !model_helper_fan_switch;
            if (model_helper_fan_switch)
            {
                // model_helper_fan_value = model_helper_fan_value < FAN_RANGE_MIN ? FAN_RANGE_MIN : model_helper_fan_value;
                // model_helper_fan_value = model_helper_fan_value > FAN_RANGE_MAX ? FAN_RANGE_MAX : model_helper_fan_value;
                if(print_speed_index == SILENCE_PRINT_SPEED)
                    model_helper_fan_value = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_SILENCE_MODE);
                else
                    model_helper_fan_value = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_NORMAL_MODE);
            }
            app_fan_ctrl(FAN_ID_MODEL_HELPER, model_helper_fan_switch, model_helper_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_MODEL_FAN_SWITCH:
            model_fan_switch = !model_fan_switch;
            if (model_fan_switch)
            {
                // model_fan_value = model_fan_value < FAN_RANGE_MIN ? FAN_RANGE_MIN : model_fan_value;
                // model_fan_value = model_fan_value > FAN_RANGE_MAX ? FAN_RANGE_MAX : model_fan_value;
                model_fan_value = FAN_RANGE_MAX;
            }
            app_fan_ctrl(FAN_ID_MODEL, model_fan_switch, model_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_SWITCH:
            model_fan_switch = !model_fan_switch;
            if (model_fan_switch)
            {
                // model_fan_value = model_fan_value < FAN_RANGE_MIN ? FAN_RANGE_MIN : model_fan_value;
                // model_fan_value = model_fan_value > FAN_RANGE_MAX ? FAN_RANGE_MAX : model_fan_value;
                model_fan_value = FAN_RANGE_MAX;
            }
            app_fan_ctrl(FAN_ID_MODEL, model_fan_switch, model_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_CRATE_FAN_SWITCH:
            box_fan_switch = !box_fan_switch;
            app_fan_ctrl(FAN_ID_BOX, box_fan_switch, BOX_FAN_DEFAULT_VALUE);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_ASSIST_FAN_MINUS:
            model_helper_fan_value -= FAN_RANGE_STEP;
            model_helper_fan_value = model_helper_fan_value < FAN_RANGE_MINI_HELPER ? FAN_RANGE_MINI_HELPER : model_helper_fan_value;
            app_fan_ctrl(FAN_ID_MODEL_HELPER, model_helper_fan_switch, model_helper_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_ASSIST_FAN_PLUS:
            model_helper_fan_value += FAN_RANGE_STEP;
            model_helper_fan_value = model_helper_fan_value > FAN_RANGE_MAX ? FAN_RANGE_MAX : model_helper_fan_value;
            app_fan_ctrl(FAN_ID_MODEL_HELPER, model_helper_fan_switch, model_helper_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_MODEL_FAN_MINUS:
            model_fan_value -= FAN_RANGE_STEP;
            model_fan_value = model_fan_value < FAN_RANGE_MIN ? FAN_RANGE_MIN : model_fan_value;
            app_fan_ctrl(FAN_ID_MODEL, model_fan_switch, model_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_MINUS:
            model_fan_value -= FAN_RANGE_STEP;
            model_fan_value = model_fan_value < FAN_RANGE_MIN ? FAN_RANGE_MIN : model_fan_value;
            app_fan_ctrl(FAN_ID_MODEL, model_fan_switch, model_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_MODEL_FAN_PLUS:
            model_fan_value += FAN_RANGE_STEP;
            model_fan_value = model_fan_value > FAN_RANGE_MAX ? FAN_RANGE_MAX : model_fan_value;
            app_fan_ctrl(FAN_ID_MODEL, model_fan_switch, model_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_PLUS:
            model_fan_value += FAN_RANGE_STEP;
            model_fan_value = model_fan_value > FAN_RANGE_MAX ? FAN_RANGE_MAX : model_fan_value;
            app_fan_ctrl(FAN_ID_MODEL, model_fan_switch, model_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_ASSIST_FAN_MUTE:
            model_helper_fan_value = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_SILENCE_MODE);
            app_fan_ctrl(FAN_ID_MODEL_HELPER, model_helper_fan_switch, model_helper_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_ASSIST_FAN_NORMAL:
            model_helper_fan_value = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_NORMAL_MODE);
            app_fan_ctrl(FAN_ID_MODEL_HELPER, model_helper_fan_switch, model_helper_fan_value);
            app_fan_update_style(widget_list);
            break;
        case WIDGET_ID_FAN_BTN_ASSIST_FAN_RAGE:
            model_helper_fan_value = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_CRAZY_MODE);
            app_fan_ctrl(FAN_ID_MODEL_HELPER, model_helper_fan_switch, model_helper_fan_value);
            app_fan_update_style(widget_list);
            break;
        }
        break;
    case LV_EVENT_UPDATE:
    {
        static uint64_t start_tick = 0;
        if (utils_get_current_tick() - start_tick > 2 * 1000)
        {
            start_tick = utils_get_current_tick();
            int __model_fan_value = (int)(ss->fan_state[FAN_ID_MODEL].value * 100);
            int __model_helper_fan_value = (int)(ss->fan_state[FAN_ID_MODEL_HELPER].value * 100);
            int __box_fan_switch = (int)(ss->fan_state[FAN_ID_BOX].value * 100) > 0 ? 1 : 0;
            int update = 0;

            if (__model_fan_value != model_fan_value)
            {
                model_fan_value = __model_fan_value;
                model_fan_switch = model_fan_value > 0 ? 1 : 0;
                update = 1;
            }

            if (__model_helper_fan_value != model_helper_fan_value)
            {
                model_helper_fan_value = __model_helper_fan_value;
                model_helper_fan_switch = model_helper_fan_value > 0 ? 1 : 0;
                update = 1;
            }

            if (__box_fan_switch != box_fan_switch)
            {
                box_fan_switch = __box_fan_switch;
                update = 1;
            }

            if (update)
                app_fan_update_style(widget_list);
        }
    }

    break;
    }
}

/**
 * @brief 隐藏高配版风扇控件
 * 
 * @param widget_list 
 */
static void app_board_plus_fan_hidden(widget_t **widget_list)
{
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_CONTAINER_ASSIST_FAN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_SWITCH]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_LABEL_ASSIST_FAN_PERCENTAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MUTE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_NORMAL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_RAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_LABEL_ASSIST_FAN_MUTE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_CONTAINER_MODEL_FAN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_SWITCH]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_LABEL_MODEL_FAN_PERCENTAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);


    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_CONTAINER_CRATE_FAN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_CRATE_FAN_SWITCH]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 隐藏低配版风扇控件
 * 
 * @param widget_list 
 */
static void app_board_fan_hidden(widget_t **widget_list)
{
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_CONTAINER_ASSIST_FAN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_CONTAINER_MODEL_FAN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_SWITCH]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_LABEL_MODEL_FAN_PERCENTAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

}

static void app_fan_init(widget_t **widget_list)
{
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
    app_board_plus_fan_hidden(widget_list);
#elif CONFIG_BOARD_E100 == BOARD_E100
    app_board_fan_hidden(widget_list);
#endif
}

#if CONFIG_BOARD_E100 == BOARD_E100
static void app_fan_update_style(widget_t **widget_list)
{
    if (model_helper_fan_switch)
    {
        lv_img_set_src(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_SWITCH]->obj_container[1], ui_get_image_src(61));
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MUTE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_NORMAL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_RAGE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        if (model_helper_fan_value == Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_SILENCE_MODE))
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MUTE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        }
        else if (model_helper_fan_value == Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_NORMAL_MODE))
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_NORMAL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        else if (model_helper_fan_value == Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_CRAZY_MODE))
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_RAGE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        }
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_SWITCH]->obj_container[1], ui_get_image_src(60));
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MUTE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_NORMAL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_RAGE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    }

    if (model_fan_switch)
    {
        lv_img_set_src(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_SWITCH]->obj_container[1], ui_get_image_src(61));
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        if (model_fan_value == 10)
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        else if (model_fan_value == 100)
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_SWITCH]->obj_container[1], ui_get_image_src(60));
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    }

    if (box_fan_switch)
        lv_img_set_src(widget_list[WIDGET_ID_FAN_BTN_CRATE_FAN_SWITCH]->obj_container[1], ui_get_image_src(61));
    else
        lv_img_set_src(widget_list[WIDGET_ID_FAN_BTN_CRATE_FAN_SWITCH]->obj_container[1], ui_get_image_src(60));

    // 辅助风扇模式
    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MUTE]->obj_container[0], lv_color_hex(0xFF3C3E3B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_NORMAL]->obj_container[0], lv_color_hex(0xFF3C3E3B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_RAGE]->obj_container[0], lv_color_hex(0xFF3C3E3B), LV_PART_MAIN);
    if (model_helper_fan_value == Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_SILENCE_MODE)) // 静音
    {
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_MUTE]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
    }
    else if (model_helper_fan_value == Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_NORMAL_MODE)) // 正常
    {
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_NORMAL]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
    }
    else if (model_helper_fan_value == Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_fan_speed_mode(FAN_CRAZY_MODE)) // 狂暴
    {
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FAN_BTN_ASSIST_FAN_RAGE]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
    }

    lv_label_set_text_fmt(widget_list[WIDGET_ID_FAN_LABEL_ASSIST_FAN_PERCENTAGE]->obj_container[0], "%d%%", model_helper_fan_value);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_FAN_LABEL_MODEL_FAN_PERCENTAGE]->obj_container[0], "%d%%", model_fan_value);

    // 风扇状态变化,上报状态
}
#elif CONFIG_BOARD_E100 == BOARD_E100_LITE
static void app_fan_update_style(widget_t **widget_list)
{
    if (model_fan_switch)
    {
        lv_img_set_src(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_SWITCH]->obj_container[1], ui_get_image_src(61));
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        if (model_fan_value == 10)
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        else if (model_fan_value == 100)
            lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_SWITCH]->obj_container[1], ui_get_image_src(60));
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_MINUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FAN_SECOND_EDITION_BTN_MODEL_FAN_PLUS]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    }

    lv_label_set_text_fmt(widget_list[WIDGET_ID_FAN_SECOND_EDITION_LABEL_MODEL_FAN_PERCENTAGE]->obj_container[0], "%d%%", model_fan_value);

        // 风扇状态变化,上报状态
}
#endif


void extrude_filament(int feed_type)
{
    char control_command[MANUAL_COMMAND_MAX_LENGTH];

    if(Printer::GetInstance()->m_change_filament->is_feed_busy() == false)
    {
        sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", true);
        ui_cb[manual_control_cb](control_command);
    }

    if (feed_type == FEED_TYPE_IN_FEED)
    {
        sprintf(control_command, "EXTRUDE_FILAMENT E=%d F=%d FAN_ON=1", EXTRUDER_MOVE_DISTANCE, EXTRUDER_MOVE_SPEED);
    }
    else
    {
        sprintf(control_command, "EXTRUDE_FILAMENT E=-%d F=%d FAN_ON=0", EXTRUDER_OUT_MOVE_DISTANCE, EXTRUDER_MOVE_SPEED);
    }

    if (app_print_get_print_state())
    {
        manual_control_sq.push(control_command);
    }
    else
    {
        manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=0");
        manual_control_sq.push(control_command);
        manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=1");
    }

    Printer::GetInstance()->manual_control_signal();
}


static int feed_extruder_temperature = 250;
static int feed_min_temperature = 170;
static int feed_max_temperature = 280;
static uint64_t start_tick = 0;
static uint64_t done_tick = 0;
static bool msgbox_push = false;
static bool command_push = false;
static bool filament_out_in_printing = false;   // 标记 是否为打印过程中触发料材耗尽

//打印过程中用尽耗材设置为true,此时点击进退料可点击；否则打印过程中不允许点击进退料
void set_filament_out_in_printing_flag(bool flag)
{
    filament_out_in_printing = flag;
}

int get_feed_type(void)
{
    return feed_type_inedx;
}

void app_feed_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    srv_state_t *ss = app_get_srv_state();
    srv_control_req_heater_t heater_request = {};
    srv_control_req_home_t home_request = {};
    srv_control_res_home_t home_response = {};
    char control_command[MANUAL_COMMAND_MAX_LENGTH];
    bool done = false;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        // 检查工程模式是否触发
        engineering_mode = get_sysconf()->GetInt("system", "engineering_mode", 0);
        if(feed_current_state == FEED_STATE_IDLE)
        {
            start_tick = 0;
            done_tick = 0;
            msgbox_push = false;
            command_push = false;
        } 
        std::cout << "current state : " << feed_current_state << " " << __LINE__ << std::endl;
        if(ui_change_filament_queue == NULL)
        {
            hl_queue_create(&ui_change_filament_queue, sizeof(ui_event_change_filament_t), 8);
            change_filament_register_state_callback(ui_change_filament_callback);
        }
        if(ui_feed_type_queue == NULL)
        {
            hl_queue_create(&ui_feed_type_queue, sizeof(ui_event_feed_type_t), 8);
            feed_type_register_state_callback(ui_feed_type_callback);
            LOG_I("ui_feed_type_queue create\n");
        }
        
        app_feed_update(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        // hl_queue_destory(&ui_change_filament_queue);
        // change_filament_unregister_state_callback(ui_change_filament_callback);
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_FEED_BTN_TEMPERATURE_SET_MINUS:
            feed_extruder_temperature -= 10;
            feed_extruder_temperature = std::max(feed_extruder_temperature, feed_min_temperature);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_SET_VALUE]->obj_container[2], "%d℃", feed_extruder_temperature);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_LABEL_TEMPERATURE_RUN_SET_VALUE]->obj_container[0], "%d℃", feed_extruder_temperature);
            break;
        case WIDGET_ID_FEED_BTN_TEMPERATURE_SET_PLUS:
            feed_extruder_temperature += 10;
            feed_extruder_temperature = std::min(feed_extruder_temperature, extruder_max_temperature);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_SET_VALUE]->obj_container[2], "%d℃", feed_extruder_temperature);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_LABEL_TEMPERATURE_RUN_SET_VALUE]->obj_container[0], "%d℃", feed_extruder_temperature);
            break;
        case WIDGET_ID_FEED_BTN_IN_FEED:
            if((Printer::GetInstance()->m_virtual_sdcard->is_active() && filament_out_in_printing == false) || app_print_get_print_busy())
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else
            {
                feed_type_inedx = FEED_TYPE_IN_FEED;
                feed_current_state = FEED_STATE_PROCEDURE_1;

                sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", true);
                ui_cb[manual_control_cb](control_command);

                std::cout << "current state : " << feed_current_state << " " << __LINE__ << std::endl;
                app_feed_update(widget_list);
                // home_request.x = home_request.y = 1;
                // simple_bus_request("srv_control", SRV_CONTROL_STEPPER_HOME, &home_request, &home_response);
                // // ui_cb[manual_control_cb]((char *)"G28 X Y");
                // heater_request.heater_id = HEATER_ID_EXTRUDER;
                // heater_request.temperature = feed_extruder_temperature;
                // simple_bus_request("srv_control", SRV_CONTROL_HEATER_TEMPERATURE, &heater_request, NULL);
                // app_update_srv_state();
                // ui_cb[reset_extrude_filament_done_cb](NULL);
                // ui_cb[extrude_filament_done_cb](&done);
                // std::cout << "done : " << done << " " << __LINE__ << std::endl;
                if (app_print_get_print_state())
                {
                    sprintf(control_command, "MOVE_TO_EXTRUDE TARGET_TEMP=%d MOVE=1 ZERO_Z=1", feed_extruder_temperature);
                    manual_control_sq.push(control_command);
                }
                else
                {
                    manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=0");
                    sprintf(control_command, "MOVE_TO_EXTRUDE TARGET_TEMP=%d MOVE=1 ZERO_Z=0", feed_extruder_temperature);
                    manual_control_sq.push(control_command);
                    manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=1");
                }
                Printer::GetInstance()->manual_control_signal();

                if(filament_out_in_printing == true)
                {
                    filament_out_in_printing = false;
                }
            }
            break;
        case WIDGET_ID_FEED_BTN_OUT_FEED:
            if((Printer::GetInstance()->m_virtual_sdcard->is_active() && filament_out_in_printing == false) || app_print_get_print_busy())
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else
            {
                feed_type_inedx = FEED_TYPE_OUT_FEED;
                feed_current_state = FEED_STATE_PROCEDURE_1;

                sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", true);
                ui_cb[manual_control_cb](control_command);

                std::cout << "current state : " << feed_current_state << " " << __LINE__ << std::endl;
                app_feed_update(widget_list);
                // sprintf(control_command, "MOVE_TO_EXTRUDE TARGET_TEMP=%d MOVE=1", feed_extruder_temperature);
                // ui_cb[manual_control_cb](control_command);

                if (get_sysconf()->GetBool("system", "cutting_mode", 0) && !msgbox_push) // 自动切料
                {
                    char control_command[MANUAL_COMMAND_MAX_LENGTH];
                    if (app_print_get_print_state())
                    {
                        sprintf(control_command, "CUT_OFF_FILAMENT ZERO_Z=1");
                        manual_control_sq.push(control_command);
                    }
                    else
                    {
                        manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=0");
                        sprintf(control_command, "CUT_OFF_FILAMENT ZERO_Z=0");
                        manual_control_sq.push(control_command);
                        manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=1");
                    }
                    Printer::GetInstance()->manual_control_signal();
                }



                if(filament_out_in_printing == true)
                {
                    filament_out_in_printing = false;
                }
            }
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        app_feed_type_update(widget_list);
        app_feed_update(widget_list);
        break;
    }
}

static void app_feed_type_update(widget_t **widget_list)
{
    if (engineering_mode) //工程模式一直触发空闲更新，避免自动降温
    {
        app_update_idle_timer_tick();
    }
    ui_event_feed_type_t ui_feed_type_stats;
    char control_command[MANUAL_COMMAND_MAX_LENGTH];
    if (hl_queue_dequeue(ui_feed_type_queue, &ui_feed_type_stats, 1))
    {
        LOG_D("ui_feed_type_stats: %d\n",ui_feed_type_stats);
        switch (ui_feed_type_stats)
        {
        case UI_EVENT_FEED_TYPE_IN_FEED:
            feed_type_inedx = FEED_TYPE_IN_FEED;
            feed_current_state = FEED_STATE_PROCEDURE_1;

            sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", true);
            ui_cb[manual_control_cb](control_command);

            std::cout << "current state : " << feed_current_state << " " << __LINE__ << std::endl;
            app_feed_update(widget_list);
            // home_request.x = home_request.y = 1;
            // simple_bus_request("srv_control", SRV_CONTROL_STEPPER_HOME, &home_request, &home_response);
            // // ui_cb[manual_control_cb]((char *)"G28 X Y");
            // heater_request.heater_id = HEATER_ID_EXTRUDER;
            // heater_request.temperature = feed_extruder_temperature;
            // simple_bus_request("srv_control", SRV_CONTROL_HEATER_TEMPERATURE, &heater_request, NULL);
            // app_update_srv_state();
            // ui_cb[reset_extrude_filament_done_cb](NULL);
            // ui_cb[extrude_filament_done_cb](&done);
            // std::cout << "done : " << done << " " << __LINE__ << std::endl;
                if (app_print_get_print_state())
                {
                    sprintf(control_command, "MOVE_TO_EXTRUDE TARGET_TEMP=%d MOVE=1 ZERO_Z=1", feed_extruder_temperature);
                    manual_control_sq.push(control_command);
                }
                else
                {
                    manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=0");
                    sprintf(control_command, "MOVE_TO_EXTRUDE TARGET_TEMP=%d MOVE=1 ZERO_Z=0", feed_extruder_temperature);
                    manual_control_sq.push(control_command);
                    manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=1");
                }
                Printer::GetInstance()->manual_control_signal();

            if(filament_out_in_printing == true)
            {
                filament_out_in_printing = false;
            }
            break;
        case UI_EVENT_FEED_TYPE_OUT_FEED:
            feed_type_inedx = FEED_TYPE_OUT_FEED;
            feed_current_state = FEED_STATE_PROCEDURE_1;

            sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", true);
            ui_cb[manual_control_cb](control_command);

            std::cout << "current state : " << feed_current_state << " " << __LINE__ << std::endl;
            app_feed_update(widget_list);
            // sprintf(control_command, "MOVE_TO_EXTRUDE TARGET_TEMP=%d MOVE=1", feed_extruder_temperature);
            // ui_cb[manual_control_cb](control_command);

            if (get_sysconf()->GetBool("system", "cutting_mode", 0) && !msgbox_push) // 自动切料
            {
                char control_command[MANUAL_COMMAND_MAX_LENGTH];
                if (app_print_get_print_state())
                {
                    sprintf(control_command, "CUT_OFF_FILAMENT ZERO_Z=1");
                    manual_control_sq.push(control_command);
                }
                else
                {
                    manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=0");
                    sprintf(control_command, "CUT_OFF_FILAMENT ZERO_Z=0");
                    manual_control_sq.push(control_command);
                    manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=1");
                }
                Printer::GetInstance()->manual_control_signal();
            }

            if(filament_out_in_printing == true)
            {
                filament_out_in_printing = false;
            }
            break;
        
        default:
            break;
        }
    }
}

static void app_feed_update(widget_t **widget_list)
{
    double extruder_cur_temp = 0;
    char control_command[MANUAL_COMMAND_MAX_LENGTH];
    srv_state_t *ss = app_get_srv_state();
    switch (feed_current_state)
    {
    case FEED_STATE_IDLE:
        lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_LABEL_LEFT_TIP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_SET_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_RUN_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_NORMAL_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_IN_FEED]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_OUT_FEED]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_IN_FEED]->obj_container[2], 255, LV_PART_MAIN);
        lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_OUT_FEED]->obj_container[2], 255, LV_PART_MAIN);
        lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_LEFT_CONTAINER]->obj_container[2], tr(53));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_SET_VALUE]->obj_container[2], "%d℃", feed_extruder_temperature);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_LABEL_TEMPERATURE_RUN_SET_VALUE]->obj_container[0], "%d℃", feed_extruder_temperature);
        break;
    case FEED_STATE_PROCEDURE_1:
    case FEED_STATE_PROCEDURE_2:
    case FEED_STATE_PROCEDURE_3:
    case FEED_STATE_PROCEDURE_4:
        lv_obj_add_flag(widget_list[WIDGET_ID_FEED_LABEL_LEFT_TIP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_SET_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_IN_FEED]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_OUT_FEED]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_IN_FEED]->obj_container[2], 127, LV_PART_MAIN);
        lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_OUT_FEED]->obj_container[2], 127, LV_PART_MAIN);

        lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_LABEL_TEMPERATURE_RUN_SET_VALUE]->obj_container[0], "%d℃", feed_extruder_temperature);

        if (feed_current_state == FEED_STATE_PROCEDURE_1)
        {
            if (feed_type_inedx == FEED_TYPE_OUT_FEED && get_sysconf()->GetBool("system", "cutting_mode", 0))
            {
                lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_RUN_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_NORMAL_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);   
            }
            else
            {
                lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_RUN_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_NORMAL_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            }

            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[2], 127, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[2], 127, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[2], 127, LV_PART_MAIN);
        }
        else if (feed_current_state == FEED_STATE_PROCEDURE_2)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_RUN_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_NORMAL_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[2], 127, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[2], 127, LV_PART_MAIN);

            if (feed_type_inedx == FEED_TYPE_IN_FEED)
            {
                if (material_detection_switch)
                {
                    if (gpio_is_init(MATERIAL_BREAK_DETECTION_GPIO) < 0)
                    {
                        gpio_init(MATERIAL_BREAK_DETECTION_GPIO); // 断料检测IO初始化
                        gpio_set_direction(MATERIAL_BREAK_DETECTION_GPIO, GPIO_INPUT);
                        LOG_E("material detection gpio error!!!!!\n");
                    }
                    if ((gpio_is_init(MATERIAL_BREAK_DETECTION_GPIO) == 0) &&
                        (gpio_get_value(MATERIAL_BREAK_DETECTION_GPIO) == MATERIAL_BREAK_DETECTION_TRIGGER_LEVEL))
                    {
                        // 断料
                        if (utils_get_current_tick() - start_tick > 1000 * 600 && !engineering_mode) //工程模式则一直卡在进料界面
                        {
                            // 结束加热 回到主界面
                            feed_current_state = FEED_STATE_IDLE;
                            
                            sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", false);
                            ui_cb[manual_control_cb](control_command);
                            
                            done_tick = 0;
                            command_push = false;
                            msgbox_push = false;
                            if (app_print_get_print_state() != true)
                            {
                                ui_cb[manual_control_cb]((char *)"M104 S0");
                                ui_cb[manual_control_cb]((char *)"M140 S0");
                            }

                            if(app_print_get_print_state() == true)       //打印中
                            {
                                ui_set_window_index(WINDOW_ID_PRINT, NULL);
                                app_top_update_style(window_get_top_widget_list());
                            }
                        }
                    }
                    else
                    {
                        static uint8_t handle = 0;      
                        if(handle == 0)     
                        {
                            extrude_filament(feed_type_inedx);  //防止在插入料材后立即退出该页面，不执行该语句导致后续收不到回调
                            handle = 1;
                        }
                        
                        // 正常
                        if (utils_get_current_tick() - start_tick > 1000 * 3)
                        {
                            feed_current_state = FEED_STATE_PROCEDURE_3;
                            handle = 0;
                        }
                    }
                }
                else
                {
                    feed_current_state = FEED_STATE_PROCEDURE_3;
                    extrude_filament(feed_type_inedx);
                }
            }
            // else if (feed_type_inedx == FEED_TYPE_OUT_FEED)
            // {
            //     if (!get_sysconf()->GetBool("system", "cutting_mode", 0) && !msgbox_push) // 无
            //     {
            //         app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_control_routine_msgbox_callback, (void *)MSGBOX_TIP_MANUAL_OUT_FEED);
            //         msgbox_push = true;
            //     }
            //     else if (get_sysconf()->GetBool("system", "cutting_mode", 0) && !msgbox_push) // 有
            //     {
            //         char control_command[MANUAL_COMMAND_MAX_LENGTH];
            //         sprintf(control_command, "CUT_OFF_FILAMENT");
            //         ui_cb[manual_control_cb](control_command);
            //     }
            // }
        }
        else if (feed_current_state == FEED_STATE_PROCEDURE_3)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_RUN_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_NORMAL_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[2], 127, LV_PART_MAIN);
        }
        else if (feed_current_state == FEED_STATE_PROCEDURE_4)
        {
            if (fabs(done_tick) < 1e-15)
            {
                done_tick = utils_get_current_tick();
                command_push = false;
                msgbox_push = false;
                if (app_print_get_print_state() != true)
                {
                    ui_cb[manual_control_cb]((char *)"M104 S0");
                    ui_cb[manual_control_cb]((char *)"M140 S0");
                }
            }
            lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_RUN_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_NORMAL_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[2], 255, LV_PART_MAIN);

            if (utils_get_current_tick() - done_tick > 2 * 1000)
            {
                feed_current_state = FEED_STATE_IDLE;
                done_tick = 0;

                if (Printer::GetInstance()->m_pause_resume->get_status().is_paused)
                {
                    print_stats_set_state(PRINT_STATS_STATE_PAUSED);
                    LOG_I(">>>>>> m_print_stats.state : %d (%s)\n", Printer::GetInstance()->m_print_stats->m_print_stats.state, __func__);
                    print_stats_state_callback_call(PRINT_STATS_STATE_PAUSED);
                }
            }
        }

        if (feed_type_inedx == FEED_TYPE_IN_FEED)
        {
            // ui显示调整
            lv_obj_set_pos(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_CONTAINER]->obj_container[1],11,15);
            lv_obj_set_y(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[0],4);
            lv_obj_set_y(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[0],47);
            lv_obj_set_y(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[0],92);
            lv_obj_set_y(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[0],136);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
            lv_img_set_src(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_CONTAINER]->obj_container[1],ui_get_image_src(228));

            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_LEFT_CONTAINER]->obj_container[2], tr(7));
            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[2], tr(124));
            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[2], tr(231));
            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[2], tr(232));
            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[2], tr(233));
        }
        else if (feed_type_inedx == FEED_TYPE_OUT_FEED)
        {
            // ui显示调整
            lv_obj_set_pos(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_CONTAINER]->obj_container[1],11,21);
            lv_obj_set_y(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[0],10);
            lv_obj_set_y(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[0],73);
            lv_obj_set_y(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[0],136);
            lv_obj_add_flag(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_4]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
            lv_img_set_src(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_CONTAINER]->obj_container[1],ui_get_image_src(263));

            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_LEFT_CONTAINER]->obj_container[2], tr(8));
            // 判断有无撞击块
            if (!get_sysconf()->GetBool("system", "cutting_mode", 0)) // 无
            {
                lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[2], tr(235));
            }
            else // 有
            {
                lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_1]->obj_container[2], tr(234));
            }
            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_2]->obj_container[2], tr(236));
            lv_label_set_text(widget_list[WIDGET_ID_FEED_BTN_PROCEDURE_3]->obj_container[2], tr(237));
        }

        ui_cb[get_extruder1_curent_temp_cb](&extruder_cur_temp);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_LABEL_TEMPERATURE_RUN_CURRENT_VALUE]->obj_container[0], "%d℃", (int)extruder_cur_temp);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_FEED_BTN_TEMPERATURE_NORMAL_CONTAINER]->obj_container[2], "%d℃", (int)extruder_cur_temp);
        break;
    }

    ui_event_change_filament_t ui_change_filament_stats;
    while (hl_queue_dequeue(ui_change_filament_queue, &ui_change_filament_stats, 1))
    {
        LOG_D("ui_change_filament_stats: %d\n",ui_change_filament_stats);
        switch (ui_change_filament_stats)
        {
        case UI_EVENT_CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE:
            if (feed_current_state == FEED_STATE_PROCEDURE_1)
            {
                feed_current_state = FEED_STATE_PROCEDURE_2;
                start_tick = utils_get_current_tick();
            }
            break;
        case UI_EVENT_CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT:
            if (feed_type_inedx == FEED_TYPE_OUT_FEED && feed_current_state == FEED_STATE_PROCEDURE_1)
            {
                if (get_sysconf()->GetBool("system", "cutting_mode", 0))
                {
                    feed_current_state = FEED_STATE_PROCEDURE_2;
                }
            }
            break;
        case UI_EVENT_CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT:
            if (feed_current_state == FEED_STATE_PROCEDURE_2)
            {
                if (feed_type_inedx == FEED_TYPE_OUT_FEED)
                {
                    feed_current_state = FEED_STATE_PROCEDURE_4;
                    char control_command[MANUAL_COMMAND_MAX_LENGTH] = {0};
                    sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", false);
                    ui_cb[manual_control_cb](control_command);
                }
            }
            else if (feed_current_state == FEED_STATE_PROCEDURE_3)
            {
                if (feed_type_inedx == FEED_TYPE_IN_FEED)
                {
                    // 耗材挤出提示
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_control_routine_msgbox_callback, (void *)MSGBOX_TIP_EXTRUSION_FEED);
                }
            }
            break;
        }
    }
}

static bool app_control_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_MANUAL_OUT_FEED)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(247));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_EXTRUSION_FEED)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(248));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(245));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(89));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            if(tr_get_language() == 7)  //日语
                lv_obj_set_width(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2],230);
        }
        else if (tip_index == AXIS_HOME_FAIL_X)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(251));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == MSGBOX_TIP_FEED_TEMPERATURE_DEFICIENCY)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(309), FEED_TEMPERATURE_DEFICIENCY_VALUE);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(49));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(327));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT:
            if (tip_index == MSGBOX_TIP_EXTRUSION_FEED) // 重试
            {
                extrude_filament(feed_type_inedx);
            }
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT:
            if (tip_index == MSGBOX_TIP_EXTRUSION_FEED) // 完成
            {
                feed_current_state = FEED_STATE_PROCEDURE_4;
                std::cout << "current state : " << feed_current_state << " " << __LINE__ << std::endl;
                char control_command[MANUAL_COMMAND_MAX_LENGTH] = {0};
                sprintf(control_command, "CHANGE_FILAMENT_SET_BUSY BUSY=%d", false);
                ui_cb[manual_control_cb](control_command);
                if(ui_get_window_index() == WINDOW_ID_FEED)
                {
                    app_feed_update(window_get_widget_list());
                    if (engineering_mode) //工程模式下点击完成后自动打印FirstLayer.gcode
                    {
                        get_sysconf()->SetInt("system", "engineering_mode", 0);
                        get_sysconf()->WriteIni(SYSCONF_PATH);
                        char command[256];
                        int result = -1;
                        snprintf(command, sizeof(command), "%s/%s", USB_DISK_PATH, "FirstLayer.gcode");
                        if (access(command, F_OK) == 0)
                        {
                            snprintf(command, sizeof(command), "cp %s/%s %s", USB_DISK_PATH, "FirstLayer.gcode", USER_RESOURCE_PATH);
                            result = system(command);
                        } 
                        sprintf(file_item.path, "%s/%s", USER_RESOURCE_PATH, "FirstLayer.gcode");
                        sprintf(file_item.name, "%s", " FirstLayer.gcode");
                        if (access(file_item.path, F_OK) == 0 && result == 0)
                        {
                            ui_set_window_index(WINDOW_ID_PRINT, &file_item);
                            app_top_update_style(window_get_top_widget_list());
                        }
                        else
                        {
                            ui_cb[manual_control_cb]((char *)"M140 S60");
                            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_control_single_msgbox_callback, (void *)MSGBOX_TIP_ENGINEERING_MODE_PRINTING_FAILED);
                        }
                    }
                }
                else
                {
                    return true;
                }
            }
            else if (tip_index == MSGBOX_TIP_FEED_TEMPERATURE_DEFICIENCY)
            {
                extrusion_temp_deficiency_action = true;
            }
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE:
            if (tip_index == MSGBOX_TIP_MANUAL_OUT_FEED)
            {
                feed_current_state = FEED_STATE_PROCEDURE_3;
                std::cout << "current state : " << feed_current_state << " " << __LINE__ << std::endl;
                extrude_filament(feed_type_inedx);
                if(ui_get_window_index() == WINDOW_ID_FEED)     //非进退料界面调用app_feed_update会卡死
                    app_feed_update(window_get_widget_list());
                else
                    return true;
            }
            else if (tip_index == AXIS_HOME_FAIL_X)
            {
                // system("reboot");
                system_reboot(0, false);
            }
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

#define FEED_TYPE_STATE_CALLBACK_SIZE 5
static feed_type_state_callback_t feed_type_state_callback[FEED_TYPE_STATE_CALLBACK_SIZE];

int feed_type_register_state_callback(feed_type_state_callback_t state_callback)
{
    for (int i = 0; i < FEED_TYPE_STATE_CALLBACK_SIZE; i++)
    {
        if (feed_type_state_callback[i] == NULL)
        {
            feed_type_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}

int feed_type_unregister_state_callback(feed_type_state_callback_t state_callback)
{
    for (int i = 0; i < FEED_TYPE_STATE_CALLBACK_SIZE; i++)
    {
        if (feed_type_state_callback[i] == state_callback)
        {
            feed_type_state_callback[i] = NULL;
            return 0;
        }
    }
    return -1;
}

int feed_type_state_callback_call(int state)
{
    for (int i = 0; i < FEED_TYPE_STATE_CALLBACK_SIZE; i++)
    {
        if (feed_type_state_callback[i] != NULL)
        {
            feed_type_state_callback[i](state);
        }
    }
    return 0;
}
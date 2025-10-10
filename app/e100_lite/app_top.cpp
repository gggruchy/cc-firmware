#include "app_top.h"
#include "configfile.h"
#include "Define_config_path.h"
#include "ui_api.h"
#include "verify_heater.h"
#include "fan.h"
#include "ui_api.h"
#include "klippy.h"
#include "print_history.h"
#include "params.h"
#include "app_control.h"
#include "hl_camera.h"
#include "aic_tlp.h"
#define LOG_TAG "app_top"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
enum
{
    CHECK_IDLE = 0,
    CHECK_HOTBED,
    CHECK_NOZZLE,
    CHECK_HOTBED_NTC,
    CHECK_NOZZLE_NTC,
    CHECK_MODEL_FAN,
    CHECK_BOARD_COOLING_FAN,
    CHECK_HOTEND_COOLING_FAN,
    CHECK_BREAK_SAVE,
    CHECK_MCU,
    CHECK_STM32,
    CHECK_STRAIN_GAUGE_MCU,
    CHECK_BOX_TEMP,
    CHECK_BOOT_FAIL,
    CHECK_BED_MESH_FALT,
    CHECK_AXIS_HOME_FAIL_Z,
    CHECK_SENSOR_DATA_COLLECTION,
};

typedef enum
{
    UI_EVENT_ID_HOT_BED_ERROR,
    UI_EVENT_ID_EXTRUDER_ERROR,
    UI_EVENT_ID_NTC_HOT_BED_ERROR,
    UI_EVENT_ID_NTC_EXTRUDER_ERROR,
    UI_EVENT_ID_HOT_BED_WITHIN_TARGET,
    UI_EVENT_ID_EXTRUDER_WITHIN_TARGET,
} ui_temp_event_id_t;

typedef enum
{
    UI_EVENT_ID_MODEL_ERROR,
    UI_EVENT_ID_BOARD_COOLING_FAN_ERROR,
    UI_EVENT_ID_HOTEND_COOLING_FAN_ERROR,
    UI_EVENT_ID_NUM_FAN_ERROR,
} ui_fan_event_id_t;

typedef enum
{
    UI_EVENT_ID_MCU_ERROR,
    UI_EVENT_ID_STM32_ERROR,
    UI_EVENT_ID_STRAIN_GAUGE_MCU_ERROR,
    UI_EVENT_ID_NUM_MCU_ERROR,
} ui_mcu_event_id_t;

typedef enum
{
    UI_EVENT_ID_PROBE_FALT,
    UI_EVENT_ID_BED_MESH_FALT,
    UI_EVENT_ID_PROBEING,
    UI_EVENT_ID_NUM
} ui_event_id_t;

typedef enum
{
    UI_EVENT_CHANGE_FILAMENT_CHECK_MOVE,
    UI_EVENT_CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE,
    UI_EVENT_CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT,
    UI_EVENT_CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT,
} ui_event_change_filament_t;
typedef enum
{
    UI_EVENT_ID_AUTO_LEVELING_STATE_START = UI_EVENT_ID_NUM,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_PREHEAT,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_EXTURDE,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_PROBE,
    UI_EVENT_ID_AUTO_LEVELING_STATE_FINISH,
    UI_EVENT_ID_AUTO_LEVELING_STATE_ERROR,
} ui_event_id_auto_leveling_t;

static int upper_bar_operation_index = 0;
static bool dousing_screen_state = false;
static int dousing_screen_time = 0;

static int set_heat_value_nozzle = 0;
static int set_heat_value_hotbed = 0;
static uint8_t temp_msgbox_step = 0;   //弹窗步骤
static uint8_t fan_msgbox_step = 0;   //弹窗步骤
static bool msgbox_push = false;  //弹窗是否弹出 false：未弹窗 true：弹窗
uint8_t temp_status = 0;
uint8_t fan_status = 0;
uint8_t mcu_status = 0;
static explorer_item_t break_save_item;
bool resume_print_aic_state = false; // AI摄像头专用(断电续打)
static bool autoleveling_busy = false;

static void app_top_upper_bar_clicked_handle(int upper_bar_operation_index);
static void app_top_upper_bar_update_style(widget_t **widget_list);
static void app_top_dousing_screen_update(widget_t **widget_list);
static bool app_top_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_top_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
hl_queue_t ui_verify_heater_event_queue = NULL;
hl_queue_t ui_fan_event_queue = NULL;
static hl_queue_t ui_auto_level_event_queue = NULL;
static hl_queue_t ui_change_filament_queue = NULL;
static bool check_boot_fail = false;
extern ConfigParser *get_sysconf();


static void ui_verify_heater_state_callback(int state)
{
    ui_temp_event_id_t ui_event;
    switch (state)
    {
    case VERIFY_HEATER_STATE_HOT_BED_ERROR:
        ui_event = UI_EVENT_ID_HOT_BED_ERROR;
        hl_queue_enqueue(ui_verify_heater_event_queue, &ui_event, 1);
        break;
    case VERIFY_HEATER_STATE_EXTRUDER_ERROR:
        ui_event = UI_EVENT_ID_EXTRUDER_ERROR;
        hl_queue_enqueue(ui_verify_heater_event_queue, &ui_event, 1);
        break;
    case VERIFY_HEATER_STATE_NTC_HOT_BED_ERROR:
        ui_event = UI_EVENT_ID_NTC_HOT_BED_ERROR;
        hl_queue_enqueue(ui_verify_heater_event_queue, &ui_event, 1);
        break;
    case VERIFY_HEATER_STATE_NTC_EXTRUDER_ERROR:
        ui_event = UI_EVENT_ID_NTC_EXTRUDER_ERROR;
        hl_queue_enqueue(ui_verify_heater_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

static void ui_fan_state_callback(int state)
{
    ui_fan_event_id_t ui_event;
    switch (state)
    {
    case FAN_STATE_MODEL_ERROR:
        ui_event = UI_EVENT_ID_MODEL_ERROR;
        printf("Fan model is not working\n");
        hl_queue_enqueue(ui_fan_event_queue, &ui_event, 1);
        break;
    case FAN_STATE_BOARD_COOLING_FAN_ERROR:
        ui_event = UI_EVENT_ID_BOARD_COOLING_FAN_ERROR;
        printf("Fan board_cooling_fan is not working\n");
        hl_queue_enqueue(ui_fan_event_queue, &ui_event, 1);
        break;
    case FAN_STATE_HOTEND_COOLING_FAN_ERROR:
        ui_event = UI_EVENT_ID_HOTEND_COOLING_FAN_ERROR;
        printf("Fan hotend_cooling_fan is not working\n");
        hl_queue_enqueue(ui_fan_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

void app_boot_detection(void)
{
    int boot_fail_count = get_sysconf()->GetInt("system", "boot_fail_count", 0);
    if(boot_fail_count >= 1)
    {
        get_sysconf()->SetInt("system", "boot", 0);
        get_sysconf()->SetInt("system", "boot_fail_count", 0);
        get_sysconf()->WriteIni(SYSCONF_PATH);
        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)CHECK_BOOT_FAIL);
        check_boot_fail = true;
    }

}   

bool app_top_get_autoleveling_busy(void)
{
    return autoleveling_busy;
}

void app_home_fail_z_detection(void)
{
    srv_state_t *ss = app_get_srv_state();
    static bool is_home_fail_z = false;

    if (ss->home_state[2] == SRV_STATE_HOME_END_FAILED && !is_home_fail_z)
    {
        app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_top_over_msgbox_callback, (void *)CHECK_AXIS_HOME_FAIL_Z);
        is_home_fail_z = true;
    }
}   

void app_exceptional_temp_detection(void){
    static uint64_t tick = 0;                         // 时间戳
    if(utils_get_current_tick() - tick > 100){

        tick = utils_get_current_tick();

        ui_temp_event_id_t ui_event;
        if (hl_queue_dequeue(ui_verify_heater_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_HOT_BED_ERROR:
                temp_status = (1 << CHECK_HOTBED);
                Printer::GetInstance()->set_exceptional_temp_status(CHECK_HOTBED);
                break;
            case UI_EVENT_ID_EXTRUDER_ERROR:
                temp_status = (1 << CHECK_NOZZLE);
                Printer::GetInstance()->set_exceptional_temp_status(CHECK_NOZZLE);
                break;
            case UI_EVENT_ID_NTC_HOT_BED_ERROR:
                temp_status = (1 << CHECK_HOTBED_NTC);
                Printer::GetInstance()->set_exceptional_temp_status(CHECK_HOTBED_NTC);
                break;
            case UI_EVENT_ID_NTC_EXTRUDER_ERROR:
                temp_status =  (1 << CHECK_NOZZLE_NTC);
                Printer::GetInstance()->set_exceptional_temp_status(CHECK_NOZZLE_NTC);
                break;
            }
        }
    }
}

void app_exceptional_temp_windows(void){
    static uint64_t tick = 0;                         // 时间戳
    static int error_type = 0;
    if(utils_get_current_tick() - tick > 100){
        // printf("top_exceptional_temp_status = %d\n", Printer::GetInstance()->get_exceptional_temp_status());

        // printf("app_top is running!!!!!!!!!!!\n");
        // 检查到异常温度，关闭加热
        if (Printer::GetInstance()->get_exceptional_temp_status())
        {
            printf("exceptional_temp_status = %d\n", Printer::GetInstance()->get_exceptional_temp_status());
            set_heat_value_nozzle = 0;
            ui_cb[extruder1_heat_cb](&set_heat_value_nozzle);

            set_heat_value_hotbed = 0;
            ui_cb[bed_heat_cb](&set_heat_value_hotbed);
        }

        if(temp_status){
            if((!temp_msgbox_step)){
                for (uint8_t i = CHECK_HOTBED; i < CHECK_MODEL_FAN; i++)
                {
                    if (temp_status & (1 << i))
                    {
                        temp_status &= ~(1 << i);
                        error_type = i;
                        temp_msgbox_step = 1;
                        break;
                    }
                }
            }
        }

        switch (temp_msgbox_step)
        {
            case 1:
                if(!msgbox_push){
                    msgbox_push = true;
                    app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_top_over_msgbox_callback, (void *)error_type);
                }
                break;
            case 2:
                if(!msgbox_push){
                    msgbox_push = true;
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)error_type);
                }
                break;
            default:
                break;
        }

        tick = utils_get_current_tick();
    }
}

void app_exceptional_fan_detection(void)
{
    static uint64_t tick = 0;                         // 时间戳
    if(utils_get_current_tick() - tick > 100){

        tick = utils_get_current_tick();

        ui_fan_event_id_t ui_event;
        if (hl_queue_dequeue(ui_fan_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_MODEL_ERROR:
                fan_status |= (1 << UI_EVENT_ID_MODEL_ERROR);
                break;
            case UI_EVENT_ID_BOARD_COOLING_FAN_ERROR:
                fan_status |= (1 << UI_EVENT_ID_BOARD_COOLING_FAN_ERROR);
                break;
            case UI_EVENT_ID_HOTEND_COOLING_FAN_ERROR:
                fan_status |= (1 << UI_EVENT_ID_HOTEND_COOLING_FAN_ERROR);
                break;
            }
            printf("fan_status = %d\n", fan_status);
        }
    }
}

void app_top_handle_change_filament(void)
{
    ui_event_change_filament_t ui_change_filament_stats;
    while (hl_queue_dequeue(ui_change_filament_queue, &ui_change_filament_stats, 1))
    {
        switch (ui_change_filament_stats)
        {
        case UI_EVENT_CHANGE_FILAMENT_CHECK_MOVE:
            if(ui_get_window_index() != WINDOW_ID_PRINT)
            {
                ui_set_window_index(WINDOW_ID_PRINT,NULL);
                app_top_update_style(window_get_top_widget_list());
            }
            break;
            
        case UI_EVENT_CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE:
        case UI_EVENT_CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT:
            if(ui_get_window_index() != WINDOW_ID_FEED)
            {
                ui_set_window_index(WINDOW_ID_FEED,NULL);
                app_top_update_style(window_get_top_widget_list());
            }
            break;

        case UI_EVENT_CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT:
            if(get_feed_type() == FEED_TYPE_OUT_FEED //退料模式
                && get_sysconf()->GetBool("system", "cutting_mode", 0)) //自动切刀
            {
                extrude_filament(FEED_TYPE_OUT_FEED);
            }
            break;
        }
    }
}

void app_exceptional_fan_windows(void){
    static uint64_t tick = 0;                         // 时间戳
    static int error_type = 0;
    
    if(utils_get_current_tick() - tick > 100){

        if(fan_status){
            if( (!fan_msgbox_step)){
                for (uint8_t i = UI_EVENT_ID_MODEL_ERROR; i < UI_EVENT_ID_NUM_FAN_ERROR; i++)
                {
                    if (fan_status & (1 << i))
                    {
                        fan_status &= ~(1 << i);
                        error_type = i + CHECK_MODEL_FAN;
                        fan_msgbox_step = 1;
                        break;
                    }
                }
            }
        }

        switch (fan_msgbox_step)
        {
            case 1:
                if(!msgbox_push){
                    msgbox_push = true;
                    app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_top_over_msgbox_callback, (void *)error_type);
                }
                break;
            case 2:
                if(!msgbox_push){
                    msgbox_push = true;
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)error_type);
                }
                break;
            default:
                break;
        }

        tick = utils_get_current_tick();
    }
}

void app_camera_detection(void)
{
    extern bool photography_switch;

    if (utils_get_current_tick() < 500000)
    {
        return;
    }

    if (hl_camera_get_exist_state() == false && photography_switch)
    {
        LOG_I("Camera offline\n");
        photography_switch = false;
        get_sysconf()->SetBool("system", "tlp_switch", photography_switch);
        get_sysconf()->WriteIni(SYSCONF_PATH);
    }
}

void app_exceptional_serial_detection(void){
    // static uint64_t tick = 0;                         // 时间戳
    std::vector<std::string> error_type;
    static bool serial_error = false;
    
    if(!serial_error){

        error_type = Printer::GetInstance()->get_serial_data_error_state();
        // tick = utils_get_current_tick();

        for (const auto& str : error_type) 
        {
            if(str == "stm32" || str == "mcu")
            {
                mcu_status = UI_EVENT_ID_STM32_ERROR;
            }
            if(str == "strain_gauge_mcu")
            {
                mcu_status = UI_EVENT_ID_STRAIN_GAUGE_MCU_ERROR;
            }
            mcu_status += CHECK_MCU;
            LOG_I("serial data error popup = %s\n", str.c_str());
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)mcu_status);
            serial_error = true;
            break;
        }
    }
}

static void probe_state_callback(probe_data_t state)
{
    ui_event_id_t ui_event;
    switch (state.state)
    {
    case CMD_PROBE_FALT:
        ui_event = UI_EVENT_ID_PROBE_FALT;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case CMD_BEDMESH_PROBE_FALT:
        ui_event = UI_EVENT_ID_BED_MESH_FALT;
        if (ui_get_window_index() != WINDOW_ID_DEVICE_INSPECTION)
            app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_top_over_msgbox_callback, (void *)CHECK_BED_MESH_FALT);
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case CMD_BEDMESH_PROBE_EXCEPTION:
        ui_event = UI_EVENT_ID_BED_MESH_FALT;
        if (ui_get_window_index() != WINDOW_ID_DEVICE_INSPECTION)
            app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_top_over_msgbox_callback, (void *)CHECK_SENSOR_DATA_COLLECTION);
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_PROBEING:
        ui_event = UI_EVENT_ID_PROBEING;
        // auto_level_vals.push_back(state.value);
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

static void ui_auto_leveling_state_callback(int state)
{
    ui_event_id_auto_leveling_t ui_event;
    switch (state)
    {
    case AUTO_LEVELING_STATE_START:
        ui_event = UI_EVENT_ID_AUTO_LEVELING_STATE_START;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_STATE_START_PREHEAT:
        ui_event = UI_EVENT_ID_AUTO_LEVELING_STATE_START_PREHEAT;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_STATE_START_EXTURDE:
        ui_event = UI_EVENT_ID_AUTO_LEVELING_STATE_START_EXTURDE;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_STATE_START_PROBE:
        ui_event = UI_EVENT_ID_AUTO_LEVELING_STATE_START_PROBE;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_STATE_FINISH:
        ui_event = UI_EVENT_ID_AUTO_LEVELING_STATE_FINISH;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_STATE_ERROR:
        ui_event = UI_EVENT_ID_AUTO_LEVELING_STATE_ERROR;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

// brief : 当打印结束时不在打印界面跳转到打印界面
static void top_printint_stats_callback(int state)
{
    switch (state)
    {
    case PRINT_STATS_STATE_COMPLETED:
        if(ui_get_window_index() != WINDOW_ID_PRINT)
        {
            ui_set_window_index(WINDOW_ID_PRINT,NULL);
            app_top_update_style(window_get_top_widget_list());
        }
        break;
    case PRINT_STATS_STATE_ERROR:
        if(ui_get_window_index() != WINDOW_ID_PRINT)
        {
            ui_set_window_index(WINDOW_ID_PRINT,NULL);
            app_top_update_style(window_get_top_widget_list());
        }
        break;
    case PRINT_STATS_STATE_CHANGE_FILAMENT_START:
        if(ui_get_window_index() != WINDOW_ID_PRINT)
        {
            ui_set_window_index(WINDOW_ID_PRINT,NULL);
            app_top_update_style(window_get_top_widget_list());
        }
        break;
    case PRINT_STATS_STATE_AUTOLEVELING:
        autoleveling_busy = true;
        break;
    case PRINT_STATS_STATE_AUTOLEVELING_COMPLETED:
        autoleveling_busy = false;
        break;
    }
}

// 进退料时跳转到进退料界面
static void ui_change_filament_callback(int state)
{
    ui_event_change_filament_t ui_event;
    switch (state)
    {
    case CHANGE_FILAMENT_STATE_CHECK_MOVE:
        ui_event = UI_EVENT_CHANGE_FILAMENT_CHECK_MOVE;
        hl_queue_enqueue(ui_change_filament_queue, &ui_event, 1);
        break;
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

static void app_top_callback_update(lv_timer_t *timer);
static lv_timer_t *app_top_callback_timer = NULL;
void app_top_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static bool top_init = false;
    if (!top_init)
    {
        top_init = true;
        dousing_screen_state = false;
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_DOUSING_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        app_top_update_style(widget_list);
        if (ui_verify_heater_event_queue == NULL)
            check_heater_init(&ui_verify_heater_event_queue, ui_verify_heater_state_callback);
        print_stats_register_state_callback(top_printint_stats_callback);
        if (ui_change_filament_queue == NULL)
            hl_queue_create(&ui_change_filament_queue, sizeof(ui_event_change_filament_t), 8);
        change_filament_register_state_callback(ui_change_filament_callback);
        if (ui_fan_event_queue == NULL)
            check_fan_init(&ui_fan_event_queue, ui_fan_state_callback);
        if (ui_auto_level_event_queue == NULL)
            hl_queue_create(&ui_auto_level_event_queue, sizeof(ui_event_id_t), 8);
        probe_register_state_callback(probe_state_callback);
        auto_leveling_register_state_callback(ui_auto_leveling_state_callback);
        if (app_top_callback_timer == NULL)
        {
            app_top_callback_timer = lv_timer_create(app_top_callback_update, 200, (void *)widget_list);
            lv_timer_ready(app_top_callback_timer);
        }
    }
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
    lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
#endif

    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_TOP_BTN_LEFT_BAR_MAIN:
            if(app_msgbox_is_active())    //存在弹窗不切换界面
                break;
            if (app_print_get_print_state())
                ui_set_window_index(WINDOW_ID_PRINT, NULL);
            else
                ui_set_window_index(WINDOW_ID_MAIN, NULL);
            app_top_update_style(widget_list);
            break;
        case WIDGET_ID_TOP_BTN_LEFT_BAR_FILE:
            if(app_msgbox_is_active())    //存在弹窗不切换界面
                break;
            ui_set_window_index(WINDOW_ID_EXPLORER, NULL);
            app_top_update_style(widget_list);
            break;
        case WIDGET_ID_TOP_BTN_LEFT_BAR_CONTROL:
            if(app_msgbox_is_active())    //存在弹窗不切换界面
                break;
            if(ui_get_window_last_index() == WINDOW_ID_FEED)
                ui_set_window_index(WINDOW_ID_FEED,NULL);
            else if(ui_get_window_last_index() == WINDOW_ID_FAN)
                ui_set_window_index(WINDOW_ID_FAN,NULL);
            else
                ui_set_window_index(WINDOW_ID_CONTROL, NULL);
                
            app_top_update_style(widget_list);
            break;
        case WIDGET_ID_TOP_BTN_LEFT_BAR_SETTING:
            if(app_msgbox_is_active())    //存在弹窗不切换界面
                break;
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(widget_list);
            break;
        case WIDGET_ID_TOP_BTN_LEFT_BAR_CALIBRATION:
            if(app_msgbox_is_active())    //存在弹窗不切换界面
                break;
            ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
            app_top_update_style(widget_list);
            break;
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1:
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2:
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3:
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4:
            app_top_upper_bar_clicked_handle(widget->header.index);
            break;
        case WIDGET_ID_TOP_BTN_DOUSING_CONTAINER:
            dousing_screen_state = false;
            lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_DOUSING_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            // 亮屏
            utils_vfork_system("echo setbl > /sys/kernel/debug/dispdbg/command");
            utils_vfork_system("echo lcd0 > /sys/kernel/debug/dispdbg/name");
            utils_vfork_system("echo 255 > /sys/kernel/debug/dispdbg/param");
            utils_vfork_system("echo 1 > /sys/kernel/debug/dispdbg/start");
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static void app_top_callback_update(lv_timer_t *timer)
{
    widget_t **widget_list = (widget_t **)timer->user_data;

    app_top_upper_bar_update_style(widget_list);
    app_top_dousing_screen_update(widget_list);
    app_exceptional_temp_detection();
    app_exceptional_fan_detection();
    app_top_handle_change_filament();
    if(get_sysconf()->GetInt("system", "boot", 0) || (ui_get_window_index() != WINDOW_ID_DEVICE_INSPECTION 
        && ui_get_window_index() != WINDOW_ID_LANGUAGE))
    {
        app_camera_detection();
        app_boot_detection();
        if(!check_boot_fail)
        {
            app_home_fail_z_detection();
            app_exceptional_temp_windows();
            app_exceptional_fan_windows();
        }
    }
    if (!app_print_get_print_state())
    {
        app_exceptional_serial_detection();
    }
#if CONFIG_SUPPORT_AIC
    app_print_foreign_capture_detection();
#endif
}

void app_top_update_style(widget_t **widget_list)
{
    static int last_window_index = 0;
    if (last_window_index != ui_get_window_next_index())
    {
        last_window_index = ui_get_window_next_index();
        lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_MAIN]->obj_container[1], ui_get_image_src(2));
        lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_FILE]->obj_container[1], ui_get_image_src(3));
        lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CONTROL]->obj_container[1], ui_get_image_src(4));
        lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_SETTING]->obj_container[1], ui_get_image_src(5));
        lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CALIBRATION]->obj_container[1], ui_get_image_src(6));
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_MAIN]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_FILE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CONTROL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_SETTING]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CALIBRATION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_MAIN]->obj_container[0], lv_color_hex(0xFF171718), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_FILE]->obj_container[0], lv_color_hex(0xFF171718), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CONTROL]->obj_container[0], lv_color_hex(0xFF171718), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_SETTING]->obj_container[0], lv_color_hex(0xFF171718), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CALIBRATION]->obj_container[0], lv_color_hex(0xFF171718), LV_PART_MAIN);

        if (ui_get_window_next_index() == WINDOW_ID_MAIN || ui_get_window_next_index() == WINDOW_ID_PRINT)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_MAIN]->obj_container[1], ui_get_image_src(7));
            lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_MAIN]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_MAIN]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
        }
        if (ui_get_window_next_index() == WINDOW_ID_EXPLORER)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_FILE]->obj_container[1], ui_get_image_src(8));
            lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_FILE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_FILE]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
        }
        if (ui_get_window_next_index() == WINDOW_ID_CONTROL || ui_get_window_next_index() == WINDOW_ID_FAN || ui_get_window_next_index() == WINDOW_ID_FEED)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CONTROL]->obj_container[1], ui_get_image_src(9));
            lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CONTROL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CONTROL]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
        }
        if (ui_get_window_next_index() == WINDOW_ID_SETTING || ui_get_window_next_index() == WINDOW_ID_CAMERA)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_SETTING]->obj_container[1], ui_get_image_src(10));
            lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_SETTING]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_SETTING]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
        }
        if (ui_get_window_next_index() == WINDOW_ID_CALIBRATION)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CALIBRATION]->obj_container[1], ui_get_image_src(11));
            lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CALIBRATION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_TOP_BTN_LEFT_BAR_CALIBRATION]->obj_container[0], lv_color_hex(0xFF2A2A2A), LV_PART_MAIN);
        }
    }
}

static void app_top_upper_bar_clicked_handle(int upper_bar_operation_index)
{
    switch (ui_get_window_index())
    {
    case WINDOW_ID_SETTING:
    case WINDOW_ID_LAMPLIGHT_LANGUAGE:
    case WINDOW_ID_NETWORK:
    case WINDOW_ID_INFO:
    case WINDOW_ID_CAMERA:
    case WINDOW_ID_CUTTER:
    case WINDOW_ID_DETECT_UPDATE:
    case WINDOW_ID_RESET_FACTORY:
    case WINDOW_ID_PRINT_PLATFORM:
        switch (upper_bar_operation_index)
        {
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1:
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2:
#if CONFIG_SUPPORT_NETWORK
            ui_set_window_index(WINDOW_ID_NETWORK, NULL);
#endif
            break;
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3:
            ui_set_window_index(WINDOW_ID_INFO, NULL);
            break;
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4:
            ui_set_window_index(WINDOW_ID_CAMERA, NULL);
            break;
        }
        break;
    case WINDOW_ID_CONTROL:
    case WINDOW_ID_FAN:
    case WINDOW_ID_FEED:
        switch (upper_bar_operation_index)
        {
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1:
            ui_set_window_index(WINDOW_ID_CONTROL, NULL);
            break;
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2:
            ui_set_window_index(WINDOW_ID_FAN, NULL);
            break;
        case WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3:
            ui_set_window_index(WINDOW_ID_FEED, NULL);
            break;
        }
        break;
    }
}

static void app_top_upper_bar_update_style(widget_t **widget_list)
{
    static int last_window_index = -1;
    if (last_window_index == ui_get_window_index())
        return;
    else
        last_window_index = ui_get_window_index();

    if(ui_get_window_index() != WINDOW_ID_CONTROL)      //控制界面UPPER_BAR置顶会导致蒙板出现时依然可点击UPPER_BAR，故屏蔽控制界面
        lv_obj_move_foreground(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0]);
    
    lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_align(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[2], LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[2], LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[2], LV_ALIGN_CENTER, 0, 0);

    switch (ui_get_window_index())
    {
    case WINDOW_ID_SETTING:
    case WINDOW_ID_LAMPLIGHT_LANGUAGE:
    case WINDOW_ID_NETWORK:
    case WINDOW_ID_INFO:
    case WINDOW_ID_CAMERA:
    case WINDOW_ID_CUTTER:
    case WINDOW_ID_DETECT_UPDATE:
    case WINDOW_ID_RESET_FACTORY:
    case WINDOW_ID_PRINT_PLATFORM:
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], tr(18));
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[2], tr(19));
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[2], tr(20));
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[2], tr(21));
        lv_obj_set_pos(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[0],11,5);
        lv_obj_set_pos(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[0],114,5);
        lv_obj_set_pos(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[0],215,5);
        lv_obj_set_pos(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[0],317,5);
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[0],96,27);
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[0],96,27);
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[0],96,27);
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[0],96,27);

        if (ui_get_window_index() == WINDOW_ID_SETTING || ui_get_window_index() == WINDOW_ID_LAMPLIGHT_LANGUAGE
         || ui_get_window_index() == WINDOW_ID_CUTTER || ui_get_window_index() == WINDOW_ID_RESET_FACTORY
         || ui_get_window_index() == WINDOW_ID_DETECT_UPDATE
         || ui_get_window_index() == WINDOW_ID_PRINT_PLATFORM)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(56));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (ui_get_window_index() == WINDOW_ID_NETWORK)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(57));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (ui_get_window_index() == WINDOW_ID_INFO)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(75));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (ui_get_window_index() == WINDOW_ID_CAMERA)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(76));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        break;
    case WINDOW_ID_CALIBRATION:
    case WINDOW_ID_ONE_CLICK_DETECTION:
    case WINDOW_ID_PID_VIBRATION:
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(255));
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], tr(22));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN); 
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[0],106,27);
        break;
    case WINDOW_ID_CONTROL:
    case WINDOW_ID_FAN:
    case WINDOW_ID_FEED:
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], tr(1));
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[2], tr(2));
        lv_label_set_text(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[2], tr(229));

        lv_obj_set_pos(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[0],11,5);
        lv_obj_set_pos(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[0],127,5);
        lv_obj_set_pos(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[0],242,5);
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[0],106,27);
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[0],106,27);
        lv_obj_set_size(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[0],106,27);

        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        if (ui_get_window_index() == WINDOW_ID_CONTROL)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(255));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (ui_get_window_index() == WINDOW_ID_FAN)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(256));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (ui_get_window_index() == WINDOW_ID_FEED)
        {
            lv_img_set_src(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[1], ui_get_image_src(257));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        break;
    default:
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    }

    switch (ui_get_window_index())
    {
    case WINDOW_ID_DEVICE_INSPECTION:
    case WINDOW_ID_AUTO_LEVEL:
        lv_obj_add_flag(widget_list[WIDGET_ID_TOP_CONTAINER_LEFT_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_CONTAINER_LEFT_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

static bool app_top_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    bool ret = false;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        printf("over_msgbox_tip_index = %d\n", tip_index);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], tr(37));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_BTN_PARTICULARS]->obj_container[2], tr(43));
        if (tip_index == CHECK_HOTBED)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：101,%s", tr(39));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_HOTBED_NTC)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：102,%s", tr(190));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_NOZZLE)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：103,%s", tr(192));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_NOZZLE_NTC)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：104,%s", tr(194));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_MODEL_FAN)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(114));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：703,%s", tr(223));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_BOARD_COOLING_FAN)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(114));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：701,%s", tr(219));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_HOTEND_COOLING_FAN)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(114));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：702,%s", tr(221));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_BED_MESH_FALT)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：502,%s", tr(213));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_SENSOR_DATA_COLLECTION)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：502,%s", tr(337));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_AXIS_HOME_FAIL_Z)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
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
            if(tip_index == CHECK_HOTBED || tip_index == CHECK_HOTBED_NTC ||
                tip_index == CHECK_NOZZLE || tip_index == CHECK_NOZZLE_NTC){
                temp_msgbox_step = 2;
            }
            else if(tip_index == CHECK_MODEL_FAN || tip_index == CHECK_BOARD_COOLING_FAN ||
                        tip_index == CHECK_HOTEND_COOLING_FAN){
                fan_msgbox_step = 2;
            }
            msgbox_push = false;
            ret = true;
            if (tip_index == CHECK_BED_MESH_FALT)
            {
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)CHECK_BED_MESH_FALT);
            }
            else if (tip_index == CHECK_SENSOR_DATA_COLLECTION)
            {
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)CHECK_SENSOR_DATA_COLLECTION);
            }
            else if(tip_index == CHECK_AXIS_HOME_FAIL_Z)
            {
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)CHECK_AXIS_HOME_FAIL_Z);
            }
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return ret;
}

static bool app_top_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static int calibration_switch = 0;
    bool ret = false;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        printf("routine_msgbox_tip_index = %d\n", tip_index);
        if (tip_index == CHECK_HOTBED)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(40));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_HOTBED_NTC)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(191));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_NOZZLE)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(193));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_NOZZLE_NTC)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(195));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_MODEL_FAN)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(224));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(46));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(114));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_BOARD_COOLING_FAN)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(220));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(46));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(114));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_HOTEND_COOLING_FAN)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(222));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(46));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFDE200), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(114));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_BREAK_SAVE)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(255));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(256));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_STM32)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(300));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_STRAIN_GAUGE_MCU)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(301));
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[0], LV_OBJ_FLAG_SCROLLABLE);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_BOOT_FAIL)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(321));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == CHECK_BED_MESH_FALT)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(325));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_SENSOR_DATA_COLLECTION)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(338));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == CHECK_AXIS_HOME_FAIL_Z)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(208));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT:
            if (tip_index == CHECK_BREAK_SAVE)
            {
                if (Printer::GetInstance()->m_break_save != nullptr)
                {
                    Printer::GetInstance()->m_break_save->load_variables();
                    uint64_t cumulative_time = 0;
                    cumulative_time = get_sysconf()->GetInt("system", "cumulative_time", 0);
                    cumulative_time += (uint64_t)Printer::GetInstance()->m_break_save->s_saved_print_para.last_print_time;
                    get_sysconf()->SetInt("system", "cumulative_time", cumulative_time);
                    get_sysconf()->WriteIni(SYSCONF_PATH);
                    // aic_tlp_delete(Printer::GetInstance()->m_break_save->s_saved_print_para.tlp_test_path.c_str());

                    //选择取消断电续打，对历史记录进行数据更新
                    print_history_record_t *record;
                    record = &machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE];

                    if(record->print_state == PRINT_RECORD_STATE_START)
                    {
                        printf("cancel resume print: time<%lf>,layer<%d>,filament_used<%lf>\n",
                            Printer::GetInstance()->m_break_save->s_saved_print_para.last_print_time,
                             Printer::GetInstance()->m_break_save->s_saved_print_para.curr_layer_num,
                              Printer::GetInstance()->m_break_save->s_saved_print_para.filament_used);

                        record->print_state = PRINT_RECORD_STATE_ERROR;
                        print_history_update_record(Printer::GetInstance()->m_break_save->s_saved_print_para.last_print_time,
                                                     PRINT_RECORD_STATE_ERROR,
                                                      Printer::GetInstance()->m_break_save->s_saved_print_para.curr_layer_num,
                                                       Printer::GetInstance()->m_break_save->s_saved_print_para.filament_used,
                                                        0);    // todo 停止原因需要补充
                    }
                    Printer::GetInstance()->m_break_save->delete_save_files();
                }
            }
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT:
            if (tip_index == CHECK_BREAK_SAVE)
            {
                char path[100];
                if (Printer::GetInstance()->m_break_save != nullptr)
                {
                    sprintf(path, "%s", Printer::GetInstance()->m_break_save->get_save_file_name().c_str());
                    char *name = strrchr(path, '/');
                    name = name? strdup(name + 1) : NULL;
                    printf("break_save_name = %s\n", name);
                    printf("break_save_path = %s\n", path);
                    strcpy(break_save_item.name, name);
                    strcpy(break_save_item.path, path);
                    break_save_item.userdata = (void *)&calibration_switch;
                    resume_print_aic_state = true;

                    printf("resume print: time<%lf>,layer<%d>,filament_used<%lf>\n",
                            Printer::GetInstance()->m_break_save->s_saved_print_para.last_print_time,
                             Printer::GetInstance()->m_break_save->s_saved_print_para.curr_layer_num,
                              Printer::GetInstance()->m_break_save->s_saved_print_para.filament_used);

                    //选择恢复断电续打，删除上一次的历史记录
                    print_history_record_t *record;
                    record = &machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE];
                    if(record->print_state == PRINT_RECORD_STATE_START)
                    {
                        //保存打印时的开始时间信息
                        break_resume_his_param_t param;
                        param.start_time = record->start_time;
                        param.ntp_status = record->ntp_status;
                        save_last_history_param(param);

                        print_history_set_mask(0);
                        print_history_delete_record();
                    }

                    ui_set_window_index(WINDOW_ID_PRINT, &break_save_item);
                    app_top_update_style(window_get_top_widget_list());
                    app_msgbox_reset_msgbox_queue();
                }
            }
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE:
            if(tip_index == CHECK_HOTBED || tip_index == CHECK_HOTBED_NTC ||
                tip_index == CHECK_NOZZLE || tip_index == CHECK_NOZZLE_NTC || 
                tip_index == CHECK_MCU || tip_index == CHECK_STM32 || 
                tip_index == CHECK_STRAIN_GAUGE_MCU || tip_index == CHECK_BED_MESH_FALT || 
                tip_index == CHECK_AXIS_HOME_FAIL_Z || tip_index == CHECK_SENSOR_DATA_COLLECTION){
                temp_msgbox_step = 0;
                // system("reboot");

                //重启前对打印的历史记录进行更新
                if(app_print_get_print_state() == true) //打印中
                {
                    print_history_record_t *record;
                    record = &machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE];

                    record->print_state = PRINT_RECORD_STATE_ERROR;
                    print_history_update_record(Printer::GetInstance()->m_print_stats->m_print_stats.total_duration,
                                                 PRINT_RECORD_STATE_ERROR,
                                                  Printer::GetInstance()->m_print_stats->m_print_stats.current_layer,
                                                   Printer::GetInstance()->m_print_stats->m_print_stats.filament_used,
                                                    0);    // todo 停止原因需要补充
                }
                
                system_reboot(0, false);
            }
            else if(tip_index == CHECK_MODEL_FAN || tip_index == CHECK_BOARD_COOLING_FAN ||
                        tip_index == CHECK_HOTEND_COOLING_FAN){
                fan_msgbox_step = 0;
            }
            msgbox_push = false;
            ret = true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return ret;
}

static void app_top_dousing_screen_update(widget_t **widget_list)
{
    // 息屏
    if (dousing_screen_state == false && lv_disp_get_inactive_time(NULL) / 1000 > dousing_screen_time * 60 && dousing_screen_time > 0)
    {
        dousing_screen_state = true;
        lv_obj_clear_flag(widget_list[WIDGET_ID_TOP_BTN_DOUSING_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(widget_list[WIDGET_ID_TOP_BTN_DOUSING_CONTAINER]->obj_container[0]);

        utils_vfork_system("echo setbl > /sys/kernel/debug/dispdbg/command");
        utils_vfork_system("echo lcd0 > /sys/kernel/debug/dispdbg/name");
        utils_vfork_system("echo 0 > /sys/kernel/debug/dispdbg/param");
        utils_vfork_system("echo 1 > /sys/kernel/debug/dispdbg/start");
    }

    if(dousing_screen_state)
        lv_obj_move_foreground(widget_list[WIDGET_ID_TOP_BTN_DOUSING_CONTAINER]->obj_container[0]);
}

bool app_top_get_dousing_screen_state(void)
{
    return dousing_screen_state;
}

void app_top_back_dousing_state(void)
{
    if (dousing_screen_state)
    {
        lv_disp_trig_activity(NULL);

        widget_t **widget_list = window_get_top_widget_list();
        lv_event_send(widget_list[WIDGET_ID_TOP_BTN_DOUSING_CONTAINER]->obj_container[0], LV_EVENT_CLICKED, NULL);
    }
}

void update_screen_off_time()
{
    extern ConfigParser *get_sysconf();
    dousing_screen_time = get_sysconf()->GetInt("system", "screen_off_time", 5);
}

int get_screen_off_time()
{
    return dousing_screen_time;
}

void break_resume_msgbox_callback(bool resume_print_switch)
{
    if (Printer::GetInstance()->m_break_save != nullptr && resume_print_switch)
    {
        int ret = Printer::GetInstance()->m_break_save->select_load_variables();
        if (ret != -1)
        {
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_top_routine_msgbox_callback, (void *)CHECK_BREAK_SAVE);
        }
    }
}

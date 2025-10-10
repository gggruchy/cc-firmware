#include "app_device_inspection.h"
#include "configfile.h"
#include "Define_config_path.h"
#include "klippy.h"
#include "ui_api.h"
#include "hl_queue.h"
#include "resonance_tester.h"
#include "auto_leveling.h"
#include "verify_heater.h"
#include "fan.h"
#include "app_control.h"
#define LOG_TAG "app_device_inspection"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

enum
{
    CHECK_ITEM_SHOWER = 0,
    CHECK_ITEM_HOT_BED,
    CHECK_ITEM_PIPE_FAN,
    CHECK_ITEM_BOARD_FAN,
    CHECK_ITEM_VIBRATION,
    CHECK_ITEM_AUTO_LEVEL,

    CHECK_ITEM_TOTAL_NUMBER,
};

enum
{
    MSGBOX_TIP_CHECK_NORMAL = 0,
    MSGBOX_TIP_CHECK_ABNORMAL,
};

enum{
    GUIDE_IDLE_DETECTION,
    GUIDE_TEMP_FAN_DETECTION,
    GUIDE_VIBRATION_DETECTION,
    GUIDE_AUTO_LEVEL_DETECTION,
};

enum{
    detection_idle,
    detection_start,
    detection_completed,
    detection_fail,
};

#define NOZZLE_HEAT_SET_MAX 230
#define NOZZLE_HEAT_SET_MIN 0
#define HOTBED_HEAT_SET_MAX 60
#define HOTBED_HEAT_SET_MIN 0
#define NOZZLE_HEAT_OFFSET 3 
#define HOTBED_HEAT_OFFSET 1 

static int current_page = 0;
static bool engineering_mode = false;  // 添加工程模式标志位
static int check_state[CHECK_ITEM_TOTAL_NUMBER]; // 0未检测 1检测中 2检测正常 3检测异常
static int set_heat_value_local = 0; // 操作的加热值
static double set_heat_value_nozzle = 0.;                 // 喷嘴设置温度
static double heat_real = 0;                                              // 喷嘴真实温度
static double set_hot_bed_value_nozzle = 0.;        // 热床设置温度
static double hot_bed_real = 0;                                     // 热床真实温度
static int detection_type = GUIDE_IDLE_DETECTION;
static bool detection_vibration_flag = false;            // 是否准备检测振动
static bool detection_auto_level_flag = false;         // 是否准备检测自动调平
static uint8_t nozzle_temp_state;             // 喷头温度状态
static uint8_t bed_temp_state;                  // 热床温度状态
static uint8_t board_fan_state;                  // 主板风扇温度状态
static uint8_t hotend_cooling_state;                  // 喉管风扇温度状态
static uint8_t detection_vibration_state;                  // 振动检测状态
static uint8_t detection_auto_level_state;               // 自动调平检测状态
extern ConfigParser *get_sysconf();
static hl_queue_t ui_resonance_tester_event_queue = NULL;
static hl_queue_t ui_fan_event_queue = NULL;

typedef enum
{
    UI_EVENT_ID_RESONANCE_TESTER_ERROR_X = 0,
    UI_EVENT_ID_RESONANCE_TESTER_ERROR_Y = 1,
    UI_EVENT_ID_RESONANCE_TESTER_START = 2,
    UI_EVENT_ID_RESONANCE_TESTER_START_X = 3,
    UI_EVENT_ID_RESONANCE_TESTER_FINISH_X = 4,
    UI_EVENT_ID_RESONANCE_TESTER_START_Y = 5,
    UI_EVENT_ID_RESONANCE_TESTER_FINISH_Y = 6,
    UI_EVENT_ID_RESONANCE_TESTER_FINISH = 7,
    UI_EVENT_ID_RESONANCE_MEASURE_AXES_NOISE_FINISH,
} ui_event_id_resonance_tester_t;

typedef enum
{
    UI_EVENT_ID_PROBE_FALT,
    UI_EVENT_ID_BED_MESH_FALT,
    UI_EVENT_ID_PROBEING,
    UI_EVENT_ID_NUM
} ui_event_id_t;

typedef enum
{
    UI_EVENT_ID_MODEL_ERROR,
    UI_EVENT_ID_BOARD_COOLING_FAN_ERROR,
    UI_EVENT_ID_HOTEND_COOLING_FAN_ERROR,
    UI_EVENT_ID_NUM_FAN_ERROR,
} ui_fan_event_id_t;
typedef enum
{
    UI_EVENT_ID_AUTO_LEVELING_STATE_START = UI_EVENT_ID_NUM,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_PREHEAT,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_EXTURDE,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_PROBE,
    UI_EVENT_ID_AUTO_LEVELING_STATE_FINISH,
    UI_EVENT_ID_AUTO_LEVELING_STATE_ERROR,
    UI_EVENT_ID_AUTO_LEVELING_STATE_RESET,
} ui_event_id_auto_leveling_t;
static hl_queue_t ui_auto_level_event_queue;

static void ui_fan_state_callback(int state)
{
    if (ui_fan_event_queue == NULL) { check_fan_init(&ui_fan_event_queue, ui_fan_state_callback); }
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
};

static void app_device_inspection_update(widget_t **widget_list);
static bool app_device_inspection_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
extern ConfigParser *get_sysconf();

static void ui_resonace_tester_state_callback(int state)
{
    ui_event_id_resonance_tester_t ui_event;
    switch (state)
    {
    case RESONANCE_TESTER_STATE_ERROR_X:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_ERROR_X;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_TESTER_STATE_ERROR_Y:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_ERROR_Y;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_TESTER_STATE_START:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_START;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_TESTER_STATE_START_X:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_START_X;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_TESTER_STATE_FINISH_X:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_FINISH_X;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_TESTER_STATE_START_Y:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_START_Y;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_TESTER_STATE_FINISH_Y:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_FINISH_Y;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_TESTER_STATE_FINISH:
        ui_event = UI_EVENT_ID_RESONANCE_TESTER_FINISH;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
        break;
    case RESONANCE_MEASURE_AXES_NOISE_FINISH:
        ui_event = UI_EVENT_ID_RESONANCE_MEASURE_AXES_NOISE_FINISH;
        hl_queue_enqueue(ui_resonance_tester_event_queue, &ui_event, 1);
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
    case AUTO_LEVELING_STATE_RESET:
        ui_event = UI_EVENT_ID_AUTO_LEVELING_STATE_RESET;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    default:
        break;
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
    case CMD_BEDMESH_PROBE_EXCEPTION:
        ui_event = UI_EVENT_ID_BED_MESH_FALT;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_PROBEING:
        ui_event = UI_EVENT_ID_PROBEING;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

static void vibration_auto_level_update(int detection_type)
{
    if(detection_type == GUIDE_TEMP_FAN_DETECTION){
        int temp_state = Printer::GetInstance()->get_exceptional_temp_status();
        if (temp_state)
        {
            switch (temp_state)
            {
            case CHECK_HOTBED:
                bed_temp_state = detection_fail;
                break;
            case CHECK_NOZZLE:
                nozzle_temp_state = detection_fail;
                break;
            case CHECK_HOTBED_NTC:
                bed_temp_state = detection_fail;
                break;
            case CHECK_NOZZLE_NTC:
                nozzle_temp_state = detection_fail;
                break;
            default:
                break;
            }
        }
        // ui_fan_event_id_t ui_event;
        // if (hl_queue_dequeue(ui_fan_event_queue, &ui_event, 1))
        // {
        //     switch (ui_event)
        //     {
        //     case UI_EVENT_ID_BOARD_COOLING_FAN_ERROR:
        //         board_fan_state = detection_fail;
        //         break;
        //     case UI_EVENT_ID_HOTEND_COOLING_FAN_ERROR:
        //         hotend_cooling_state = detection_fail;
        //         break;
        //     }
        // }
    }
    else if (detection_type == GUIDE_VIBRATION_DETECTION) {
        int temp_state = Printer::GetInstance()->get_exceptional_temp_status();
        if (temp_state)
        {
            switch (temp_state)
            {
            case CHECK_HOTBED:
                detection_vibration_state = detection_fail;
                break;
            case CHECK_NOZZLE:
                detection_vibration_state = detection_fail;
                break;
            case CHECK_HOTBED_NTC:
                detection_vibration_state = detection_fail;
                break;
            case CHECK_NOZZLE_NTC:
                detection_vibration_state = detection_fail;
                break;
            default:
                break;
            }
        }
        
        ui_event_id_resonance_tester_t ui_event;
        if (hl_queue_dequeue(ui_resonance_tester_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_RESONANCE_TESTER_ERROR_X:
                detection_vibration_state = detection_fail;
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_ERROR_Y:
                detection_vibration_state = detection_fail;
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_START:
                detection_vibration_state = detection_start;
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_START_X:
                detection_vibration_state = detection_start;
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_START_Y:
                detection_vibration_state = detection_start;
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_FINISH:
                detection_vibration_state = detection_completed;
                break;
            default:
                break;
            }
            if (ui_event == UI_EVENT_ID_RESONANCE_MEASURE_AXES_NOISE_FINISH 
                && engineering_mode)
            {
                detection_vibration_state = detection_completed;
            }
        }
    }
    else if (detection_type == GUIDE_AUTO_LEVEL_DETECTION) {
        int temp_state = Printer::GetInstance()->get_exceptional_temp_status();
        if (temp_state)
        {
            switch (temp_state)
            {
            case CHECK_HOTBED:
                detection_auto_level_state = detection_fail;
                break;
            case CHECK_NOZZLE:
                detection_auto_level_state = detection_fail;
                break;
            case CHECK_HOTBED_NTC:
                detection_auto_level_state = detection_fail;
                break;
            case CHECK_NOZZLE_NTC:
                detection_auto_level_state = detection_fail;
                break;
            default:
                break;
            }
        }
        
        ui_event_id_auto_leveling_t ui_event;
        if (hl_queue_dequeue(ui_auto_level_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START:
                detection_auto_level_state = detection_start;
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START_PREHEAT:
                detection_auto_level_state = detection_start;
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START_EXTURDE:
                detection_auto_level_state = detection_start;
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START_PROBE:
                detection_auto_level_state = detection_start;
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_FINISH:
                detection_auto_level_state = detection_start;
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_ERROR:
                detection_auto_level_state = detection_fail;
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_RESET:
                detection_auto_level_state = detection_completed;
                break;
            case UI_EVENT_ID_BED_MESH_FALT:
                detection_auto_level_state = detection_fail;
                break;
            default:
                break;
            }
        }
    }
}

void app_device_inspection_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static int check_total_index = -1;
    static uint64_t start_tick = 0;
    double eventtime = 0;
    static auto delay_tick = 0;
    fan_state hot_state;
    fan_state board_state;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_WELCOME]->obj_container[1], ui_get_image_src(356));
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_IMAGE_SECOND_PAGE]->obj_container[0], ui_get_image_src(357));
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_BTN_THIRD_PAGE_IMAGE]->obj_container[1], ui_get_image_src(358));
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FOUR_PAGE]->obj_container[1], ui_get_image_src(359));
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_WELCOME]->obj_container[1], ui_get_image_src(171));
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_IMAGE_SECOND_PAGE]->obj_container[0], ui_get_image_src(183));
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_BTN_THIRD_PAGE_IMAGE]->obj_container[1], ui_get_image_src(184));
        lv_img_set_src(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FOUR_PAGE]->obj_container[1], ui_get_image_src(218));
#endif
        current_page = 4;
        detection_type = GUIDE_IDLE_DETECTION;
        detection_vibration_flag = true;
        detection_auto_level_flag = true;
        if (ui_resonance_tester_event_queue == NULL)
        {
            hl_queue_create(&ui_resonance_tester_event_queue, sizeof(ui_event_id_resonance_tester_t), 8);
            resonace_tester_register_state_callback(ui_resonace_tester_state_callback);
        }

        if (ui_auto_level_event_queue == NULL)
            hl_queue_create(&ui_auto_level_event_queue, sizeof(ui_event_id_t), 8);
        probe_register_state_callback(probe_state_callback);
        auto_leveling_register_state_callback(ui_auto_leveling_state_callback);
        if (ui_fan_event_queue == NULL) { check_fan_init(&ui_fan_event_queue, ui_fan_state_callback); }
        app_device_inspection_update(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        probe_unregister_state_callback(probe_state_callback);
        auto_leveling_unregister_state_callback(ui_auto_leveling_state_callback);
        hl_queue_destory(&ui_auto_level_event_queue);

        hl_queue_destory(&ui_resonance_tester_event_queue);
        resonace_tester_unregister_state_callback(ui_resonace_tester_state_callback);
        if (ui_fan_event_queue != NULL) { hl_queue_destory(&ui_fan_event_queue); }

        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_DEVICE_INSPECTION_BTN_WELCOME_PREV:
            // ui_set_window_index(WINDOW_ID_LANGUAGE, NULL);
            current_page = 4;
            app_device_inspection_update(widget_list);
            break;
        case WIDGET_ID_DEVICE_INSPECTION_BTN_WELCOME_CONFIRM:
            current_page = 2;
            // 检查工程模式文件是否存在
            engineering_mode = (access("/mnt/exUDISK/elegoo_engineering_mode", F_OK) == 0);
            get_sysconf()->SetInt("system", "engineering_mode", engineering_mode);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            app_device_inspection_update(widget_list);
            break;
        case WIDGET_ID_DEVICE_INSPECTION_BTN_SECOND_PAGE_PREV:
            current_page = 1;
            app_device_inspection_update(widget_list);
            break;
        case WIDGET_ID_DEVICE_INSPECTION_BTN_SECOND_PAGE_START:
            current_page = 3;
            // 取螺丝后，热床抬升50，若检测到应变片触发则立即停止
            // printf("lift hot bed begin.....\n");
            // manual_control_sq.push("LIFT_HOT_BED S=50");
            // Printer::GetInstance()->manual_control_signal();
            for (int i = 0; i < CHECK_ITEM_TOTAL_NUMBER; i++){
                check_state[i] = 0;
            }
            check_state[CHECK_ITEM_SHOWER] = 1;
            check_total_index = 0;
            start_tick = utils_get_current_tick();
            app_device_inspection_update(widget_list);
            break;
        case WIDGET_ID_DEVICE_INSPECTION_BTN_FOUR_PAGE_PREV:
            ui_set_window_index(WINDOW_ID_LANGUAGE, NULL);
            break;
        case WIDGET_ID_DEVICE_INSPECTION_BTN_FOUR_PAGE_CONFIRM:
            current_page = 1;
            app_device_inspection_update(widget_list);
            break;
        }
        break;
    case LV_EVENT_UPDATE:

        if(utils_get_current_tick() - start_tick > 100){
            if(current_page == 3 && check_total_index < 6){
                switch (check_total_index)
                {
                case CHECK_ITEM_SHOWER:
                    detection_type = GUIDE_TEMP_FAN_DETECTION;
                    ui_cb[get_extruder1_curent_temp_cb](&heat_real);
                    if(set_heat_value_local != NOZZLE_HEAT_SET_MAX){
                        set_heat_value_local = NOZZLE_HEAT_SET_MAX;
                        ui_cb[extruder1_heat_cb](&set_heat_value_local);
                        check_state[CHECK_ITEM_SHOWER] = 1;
                    }
                    if((int)heat_real > (NOZZLE_HEAT_SET_MAX - NOZZLE_HEAT_OFFSET)){
                        check_state[CHECK_ITEM_SHOWER] = 2;
                        // set_heat_value_local = NOZZLE_HEAT_SET_MIN;
                        // ui_cb[extruder1_heat_cb](&set_heat_value_local);
                        check_total_index++;
                    }
                    else if (nozzle_temp_state == detection_fail){
                        check_state[CHECK_ITEM_SHOWER] = 3;
                        set_heat_value_local = NOZZLE_HEAT_SET_MIN;
                        ui_cb[extruder1_heat_cb](&set_heat_value_local);
                        // check_total_index++;
                        check_total_index = CHECK_ITEM_TOTAL_NUMBER;
                    }
                    break;
                case CHECK_ITEM_HOT_BED:
                    ui_cb[get_bed_curent_temp_cb](&hot_bed_real);

                    if(set_heat_value_local != HOTBED_HEAT_SET_MAX){
                        set_heat_value_local = HOTBED_HEAT_SET_MAX;
                        ui_cb[bed_heat_cb](&set_heat_value_local);
                        check_state[CHECK_ITEM_HOT_BED] = 1;
                    }

                    if((int)hot_bed_real > (HOTBED_HEAT_SET_MAX - HOTBED_HEAT_OFFSET)){
                        check_state[CHECK_ITEM_HOT_BED] = 2;
                        // set_heat_value_local = HOTBED_HEAT_SET_MIN;
                        // ui_cb[bed_heat_cb](&set_heat_value_local);
                        check_total_index++;
                    }
                    else if (bed_temp_state == detection_fail){
                        check_state[CHECK_ITEM_HOT_BED] = 3;
                        set_heat_value_local = HOTBED_HEAT_SET_MIN;
                        ui_cb[bed_heat_cb](&set_heat_value_local);
                        // check_total_index++;
                        check_total_index = CHECK_ITEM_TOTAL_NUMBER;
                    }
                    break;
                case CHECK_ITEM_PIPE_FAN:
                    eventtime = get_monotonic();
                    hot_state = Printer::GetInstance()->m_heater_fans[0]->get_status(eventtime);
                    // std::cout << "hot_state.rpm:" << hot_state.rpm << "，hot_state.speed:" << hot_state.speed << std::endl;
                    if(fabs(hot_state.rpm) < 1e-15 && hot_state.speed > 0.)
                        hotend_cooling_state = detection_fail;

                    // 延时5s
                    delay_tick++;
                    if(delay_tick < 50)
                    {
                        check_state[CHECK_ITEM_PIPE_FAN] = 1;
                    }
                    else
                    {
                        if (hotend_cooling_state == detection_fail)
                        {
                            check_state[CHECK_ITEM_PIPE_FAN] = 3;
                            check_total_index = CHECK_ITEM_TOTAL_NUMBER;
                        }
                        else
                        {
                            delay_tick = 0;
                            check_state[CHECK_ITEM_PIPE_FAN] = 2;
                            check_total_index++;
                        }
                    }
                    break;
                case CHECK_ITEM_BOARD_FAN:
                    eventtime = get_monotonic();
                    board_state = Printer::GetInstance()->controller_fans[0]->get_status(eventtime);
                    // std::cout << "board_state.rpm:" << board_state.rpm << "，board_state.speed:" << board_state.speed << std::endl;
                    if(fabs(board_state.rpm) < 1e-15 && board_state.speed > 0.)
                        board_fan_state = detection_fail;

                    // 延时5s
                    delay_tick++;
                    if(delay_tick < 50)
                    {
                        check_state[CHECK_ITEM_BOARD_FAN] = 1;
                    }
                    else
                    {
                        set_heat_value_local = NOZZLE_HEAT_SET_MIN;
                        ui_cb[extruder1_heat_cb](&set_heat_value_local);
                        set_heat_value_local = HOTBED_HEAT_SET_MIN;
                        ui_cb[bed_heat_cb](&set_heat_value_local);
                        if (board_fan_state == detection_fail)
                        {
                            check_state[CHECK_ITEM_BOARD_FAN] = 3;
                            check_total_index = CHECK_ITEM_TOTAL_NUMBER;
                        }
                        else
                        {
                            delay_tick = 0;
                            check_state[CHECK_ITEM_BOARD_FAN] = 2;
                            check_total_index++;
                        }
                    }
                    break;
                case CHECK_ITEM_VIBRATION:
                    if(detection_vibration_flag){
                        printf("CHECK_ITEM_VIBRATION\n");
                        detection_type = GUIDE_VIBRATION_DETECTION;
                        detection_vibration_flag = false;
                        if (engineering_mode)
                        {
                            manual_control_sq.push("G28");
                            manual_control_sq.push("MEASURE_AXES_NOISE");
                        }
                        else
                        {
                            manual_control_sq.push("SHAPER_CALIBRATE_SCRIPT");
                        }
                        Printer::GetInstance()->manual_control_signal();
                    }

                    if(detection_vibration_state == detection_completed){
                        check_state[CHECK_ITEM_VIBRATION] = 2;
                        check_total_index = CHECK_ITEM_AUTO_LEVEL;
                    }
                    else if(detection_vibration_state == detection_fail){
                        check_state[CHECK_ITEM_VIBRATION] = 3;
                        check_total_index = CHECK_ITEM_TOTAL_NUMBER;
                    }
                    else if(detection_vibration_state == detection_start){
                        check_state[CHECK_ITEM_VIBRATION] = 1;
                    }
                    break;
                case CHECK_ITEM_AUTO_LEVEL:
                    if(detection_auto_level_flag){
                        printf("CHECK_ITEM_AUTO_LEVEL\n");
                        detection_type = GUIDE_AUTO_LEVEL_DETECTION;
                        detection_auto_level_flag = false;
                        manual_control_sq.push("G29");
                        Printer::GetInstance()->manual_control_signal();
                    }

                    if(detection_auto_level_state == detection_completed){
                        check_state[CHECK_ITEM_AUTO_LEVEL] = 2;
                        check_total_index ++;
                    }
                    else if(detection_auto_level_state == detection_fail){
                        check_state[CHECK_ITEM_AUTO_LEVEL] = 3;
                        // check_total_index ++;
                        check_total_index = CHECK_ITEM_TOTAL_NUMBER;
                    }
                    else if(detection_auto_level_state == detection_start){
                        check_state[CHECK_ITEM_AUTO_LEVEL] = 1;
                    }
                    break;
                
                default:
                    break;
                }

                if(check_total_index == CHECK_ITEM_TOTAL_NUMBER)
                {
                    if (engineering_mode)
                    {
                        set_heat_value_local = NOZZLE_HEAT_SET_MAX;
                        ui_cb[extruder1_heat_cb](&set_heat_value_local);
                        set_heat_value_local = HOTBED_HEAT_SET_MAX;
                        ui_cb[bed_heat_cb](&set_heat_value_local);
                    }
                    //自检总界面是否完成
                    static bool check_total_result = true;
                    for (int i = 0; i < CHECK_ITEM_TOTAL_NUMBER; i++)
                    {
                        if (check_state[i] != 2)
                        {
                            check_total_result = false;
                            break;
                        }
                    }
                    if (check_total_result)
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_device_inspection_routine_msgbox_callback, (void *)MSGBOX_TIP_CHECK_NORMAL);
                    else
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_device_inspection_routine_msgbox_callback, (void *)MSGBOX_TIP_CHECK_ABNORMAL);

                }

                app_device_inspection_update(widget_list);
            }

            start_tick = utils_get_current_tick();
        }
        vibration_auto_level_update(detection_type);
        break;
    }
}

static void app_device_inspection_update(widget_t **widget_list)
{
    if (current_page == 1)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FIRST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FOUR_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    }
    else if (current_page == 2)
    {
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FIRST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        if (engineering_mode)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[2], "%s——%s", tr(119),"当前处在工程模式，请注意！！！！");
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[0], lv_color_hex(0xFFFF0000), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[2], "%s", tr(119));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FOUR_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    }
    else if (current_page == 3)
    {
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FIRST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        if (engineering_mode)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[2], "%s——%s", tr(119),"当前处在工程模式，请注意！！！！");
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[0], lv_color_hex(0xFFFF0000), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[2], "%s", tr(119));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FOUR_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    }
    else if (current_page == 4)
    {
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FIRST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_SECOND_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_THIRD_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_DEVICE_INSPECTION_CONTAINER_FOUR_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < CHECK_ITEM_TOTAL_NUMBER; i++)
    {
        int check_widget_item = (i * 2) + WIDGET_ID_DEVICE_INSPECTION_BTN_SHOWER_CONTAINER;
        int check_widget_result = (i * 2) + WIDGET_ID_DEVICE_INSPECTION_BTN_SHOWER_RESULT;

        if (check_state[i] == 0)
        {
            lv_obj_set_style_text_color(widget_list[check_widget_item]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            lv_obj_add_flag(widget_list[check_widget_result]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }
        else if (check_state[i] == 1)
        {
            lv_obj_set_style_text_color(widget_list[check_widget_item]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            lv_obj_clear_flag(widget_list[check_widget_result]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[check_widget_result]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[check_widget_result]->obj_container[2], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(widget_list[check_widget_result]->obj_container[2], tr(107));
            lv_obj_set_style_text_color(widget_list[check_widget_result]->obj_container[2], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else if (check_state[i] == 2)
        {
            lv_obj_set_style_text_color(widget_list[check_widget_item]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            lv_obj_clear_flag(widget_list[check_widget_result]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[check_widget_result]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[check_widget_result]->obj_container[2], LV_OBJ_FLAG_HIDDEN);
        }
        else if (check_state[i] == 3)
        {
            lv_obj_set_style_text_color(widget_list[check_widget_item]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_clear_flag(widget_list[check_widget_result]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[check_widget_result]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[check_widget_result]->obj_container[2], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(widget_list[check_widget_result]->obj_container[2], tr(133));
            lv_obj_set_style_text_color(widget_list[check_widget_result]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
    }
}

static bool app_device_inspection_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_CHECK_NORMAL)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(130));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(132));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(155));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
            if (engineering_mode)
            {
                ui_set_window_index(WINDOW_ID_FEED, NULL);
                app_top_update_style(window_get_top_widget_list());
            }
        }
        else if (tip_index == MSGBOX_TIP_CHECK_ABNORMAL)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(131));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(132));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(156));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT:
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT:
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE:
            if(tip_index == MSGBOX_TIP_CHECK_NORMAL)
            {
                ui_set_window_index(WINDOW_ID_MAIN, NULL);
                app_top_update_style(window_get_top_widget_list());
                get_sysconf()->SetInt("system", "boot", 1);
                get_sysconf()->SetInt("system", "boot_fail_count", 0);
                get_sysconf()->WriteIni(SYSCONF_PATH);
            }
            else if(tip_index == MSGBOX_TIP_CHECK_ABNORMAL)
            {
                int boot_fail_count = get_sysconf()->GetInt("system", "boot_fail_count", 0);
                if (boot_fail_count >= 1)
                {
                    get_sysconf()->SetInt("system", "boot", 1);
                    get_sysconf()->WriteIni(SYSCONF_PATH);
                    ui_set_window_index(WINDOW_ID_MAIN, NULL);
                    app_top_update_style(window_get_top_widget_list());
                }
                else
                {
                    boot_fail_count++;
                    get_sysconf()->SetInt("system", "boot_fail_count", boot_fail_count);
                    get_sysconf()->WriteIni(SYSCONF_PATH);
                    system("reboot");
                }
            }
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_CHECK_NORMAL && engineering_mode)
        {
            if(ui_get_window_index() == WINDOW_ID_FEED)
            {
                get_sysconf()->SetInt("system", "boot", 1);
                get_sysconf()->SetInt("system", "boot_fail_count", 0);
                get_sysconf()->WriteIni(SYSCONF_PATH);
                feed_type_state_callback_call(FEED_TYPE_IN_FEED);
                LOG_I("feed_type_state_callback_call\n");
                return true;
            }
        }
        break;
    }
    return false;
}



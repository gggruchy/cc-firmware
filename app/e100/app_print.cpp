#include "app_print.h"
#include "app_explorer.h"
#include "app_print.h"
#include "app_control.h"
#include "configfile.h"
#include "gcode_preview.h"
#include "gpio.h"
#include "hl_queue.h"
#include "klippy.h"
#include "print_history.h"
#include "simplebus.h"
#include "service.h"
#include "hl_wlan.h"
#include "ai_camera.h"
#include "aic_tlp.h"
#include "app_main.h"
#define LOG_TAG "app_print"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
#include "hl_camera.h"
#include "app_camera.h"

#define LOG_TAG "app_print"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

enum
{
    MSGBOX_TIP_PAUSE = 0,
    MSGBOX_TIP_RESUME,
    MSGBOX_TIP_STOP,
    MSGBOX_TIP_COMPLETED,
    MSGBOX_TIP_ERROR,
    MSGBOX_TIP_CONSUMABLE_REPLACEMENT,
    MSGBOX_TIP_CUTTING_WARN,
    MSGBOX_TIP_BE_LEVELLING,
    MSGBOX_TIP_EXECUTING_OTHER_TASK,
    MSGBOX_TIP_BOX_TEMP,
    MSGBOX_TIP_CHANGE_FILAMENT,
};

enum
{
    PRINT_STATE_PASUE,
    PRINT_STATE_RESUME
};

enum
{
    foreign_capture_idle,
    foreign_capture_start,
    foreign_capture_end,
};

typedef enum
{
    UI_EVENT_PRINT_STATS_STATE_STANDBY = 0,
    UI_EVENT_PRINT_STATS_STATE_PRINTING,
    UI_EVENT_PRINT_STATS_STATE_PAUSING,
    UI_EVENT_PRINT_STATS_STATE_PAUSED,
    UI_EVENT_PRINT_STATS_STATE_COMPLETED,
    UI_EVENT_PRINT_STATS_STATE_CANCELLING,
    UI_EVENT_PRINT_STATS_STATE_CANCELLED,
    UI_EVENT_PRINT_STATS_STATE_ERROR,
    UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING,
    UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING_COMPLETED,
    UI_EVENT_PRINT_STATS_STATE_PREHEATING,
    UI_EVENT_PRINT_STATS_STATE_PREHEATING_COMPLETED,
    UI_EVENT_PRINT_STATS_STATE_HOMING,
    UI_EVENT_PRINT_STATS_STATE_HOMING_COMPLETED,
    UI_EVENT_PRINT_STATS_STATE_PRINT,
    UI_EVENT_PRINT_STATS_STATE_FOREIGN_CAPTURE,  //异物检测中
    UI_EVENT_PRINT_STATS_STATE_FOREIGN_CAPTURE_COMPLETED,  //异物检测完成
    UI_EVENT_PRINT_STATS_STATE_CHANGE_FILAMENT_START,
    UI_EVENT_PRINT_STATS_STATE_CHANGE_FILAMENT_COMPLETED,
} ui_event_stats_id_t;

typedef enum
{
    UI_EVENT_CHANGE_FILAMENT_CHECK_MOVE
} ui_event_change_filament_t;

typedef enum
{
    UI_EVENT_WAIT_MOVE_COMPLETED,
} ui_event_wait_move_t;

aic_print_info_t aic_print_info;


extern bool illumination_light_swtich;

static int calibration_switch = 0;
static bool printing_state = false;
static bool print_busy = false;
static uint8_t stop_state = 0; // 0: 打印中 1：已停止
static bool update_print_speed = false;
static int print_btn_state = PRINT_STATE_PASUE;
static slice_param_t slice_param = {0};
static uint8_t pause_cancel_flag = 0;       // 1：弹窗暂停中/停止中

// static bool feed_msgbox_flag = false;
uint8_t material_break_detection_step = 0; // 0:未弹出断料检测弹窗,可以弹出，1:已弹出暂停弹窗，2:已弹出断料检测弹窗，3:断料检测弹窗已关闭，但处在等待触发阶段，4:仍处于断料状态，准备切换到断料检测弹窗
static bool material_break_detection_flag = 0;    // false: 未检测到断料，true: 检测到断料
extern bool photography_switch;
extern uint8_t aic_detection_switch_flag;
extern bool foreign_detection_switch; //异物检测开关
static bool app_foreign_detection_switch; //实际使用异物检测开关
extern bool resume_print_aic_state;  //断电续打标志位
static int aic_foreign_capture_state = foreign_capture_idle; //0:空闲 1:检测中 2:检测完成
static bool aic_foreign_capture_init = false; //异物检测判断标志位
static bool app_foreign_capture_detection_init = false; //异物检测初始化标志位
bool material_break_clear_msgbox_state = false; // 表示断料弹窗可关闭当前弹窗
static bool start_sdcard_print = false;
static bool foreign_detection_result_handler = false; // 异物前暂停报错专用标志位
static bool change_filament_msgbox_state = false; // 表示换料弹窗可关闭当前弹窗 M600专用
static bool need_extrude = false; // 断料检测暂停后挤出标志位

extern double material_detection_e_length;

static void app_print_init(widget_t **widget_list, explorer_item_t *file_item);
void app_print_update(lv_timer_t *timer);
static bool app_print_double_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_print_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_print_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_print_feed_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static void app_print_update_light(widget_t **widget_list);
static bool app_ui_camera_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
#if CONFIG_SUPPORT_AIC
static void app_print_ai_camera_msg_handler_cb(const void *data, void *user_data);
#endif

static lv_timer_t *print_timer = NULL;
static double hotbed_cur_temp = 0, hotbed_tar_temp = 0, extruder_cur_temp = 0, extruder_tar_temp = 0;
static hl_queue_t ui_printing_stats_queue;
static hl_queue_t ui_change_filament_queue;
static hl_queue_t ui_wait_move_queue;
static void ui_change_filament_callback(int state);
static std::vector<ui_event_stats_id_t> stats_vec;
static void update_stats(widget_t **widget_list, ui_event_stats_id_t ui_print_stats);
static explorer_item_t printing_file_item = {0};
static double nozzle_heat_real;
static int nozzle_heat_target;
static bool box_temp_detect_switch;
extern ConfigParser *get_sysconf();
// #define MAX_HISTORY_SIZE 30
#define EXTRUDER_MOVE_DISTANCE 60
#define EXTRUDER_MOVE_SPEED 240
// static history_item_t history_explorer[MAX_HISTORY_SIZE] = {0};

static void print_load_thumbnail(lv_obj_t *target, char *file_name, double thumbnail_x, double thumbnail_y, const char *origin_src)
{
    // 图片存在才显示，不存在显示默认图片
    char img_path[PATH_MAX_LEN + 1];
    sprintf(img_path, "%s/%s.%s", THUMBNAIL_DIR, file_name, "png");
    if (access(img_path, F_OK) != -1)
    {
        lv_img_set_src(target, img_path);
        int factor = (int)ceil(std::max(slice_param.thumbnail_width / thumbnail_x, slice_param.thumbnail_heigh / thumbnail_y));
        factor = factor < 1 ? 1 : factor;
        printf(">>>>>>>>>>>>factor = %d\n", factor);
        lv_img_set_zoom(target, 256 / factor);
        // lv_obj_align(target, LV_ALIGN_TOP_MID, 0, 18);
        // lv_obj_center(target);
    }
    else
    {
        lv_img_set_src(target, origin_src);
        lv_img_set_zoom(target, 256);
    }
}

void ui_printint_stats_callback(int state)
{
    ui_event_stats_id_t ui_event;
    switch (state)
    {
    case PRINT_STATS_STATE_STANDBY:
        ui_event = UI_EVENT_PRINT_STATS_STATE_STANDBY;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_PRINT_START:
        ui_event = UI_EVENT_PRINT_STATS_STATE_PRINT;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_HOMING:
        ui_event = UI_EVENT_PRINT_STATS_STATE_HOMING;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_HOMING_COMPLETED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_HOMING_COMPLETED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_AUTOLEVELING:
        ui_event = UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_AUTOLEVELING_COMPLETED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING_COMPLETED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_PREHEATING:
        ui_event = UI_EVENT_PRINT_STATS_STATE_PREHEATING;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_PREHEATING_COMPLETED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_PREHEATING_COMPLETED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_PRINTING:
        ui_event = UI_EVENT_PRINT_STATS_STATE_PRINTING;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        aic_print_info.print_state = 4;
        break;
    case PRINT_STATS_STATE_PAUSEING:
        ui_event = UI_EVENT_PRINT_STATS_STATE_PAUSING;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        aic_print_info.print_state = 0;
        break;
    case PRINT_STATS_STATE_PAUSED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_PAUSED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        aic_print_info.print_state = 0;
        break;
    case PRINT_STATS_STATE_CANCELLING:
        ui_event = UI_EVENT_PRINT_STATS_STATE_CANCELLING;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        aic_print_info.print_state = 0;
        break;
    case PRINT_STATS_STATE_CANCELLED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_CANCELLED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        aic_print_info.print_state = 2;
        break;
    case PRINT_STATS_STATE_COMPLETED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_COMPLETED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        aic_print_info.print_state = 1;
        break;
    case PRINT_STATS_STATE_ERROR:
        ui_event = UI_EVENT_PRINT_STATS_STATE_ERROR;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        aic_print_info.print_state = 3;
        break;
    case PRINT_STATS_STATE_FOREIGN_CAPTURE:
        ui_event = UI_EVENT_PRINT_STATS_STATE_FOREIGN_CAPTURE;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_FOREIGN_CAPTURE_COMPLETED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_FOREIGN_CAPTURE_COMPLETED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_CHANGE_FILAMENT_START:
        ui_event = UI_EVENT_PRINT_STATS_STATE_CHANGE_FILAMENT_START;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    case PRINT_STATS_STATE_CHANGE_FILAMENT_COMPLETED:
        ui_event = UI_EVENT_PRINT_STATS_STATE_CHANGE_FILAMENT_COMPLETED;
        hl_queue_enqueue(ui_printing_stats_queue, &ui_event, 1);
        break;
    }
}

static void ui_change_filament_callback(int state)
{
    ui_event_change_filament_t ui_event;
    switch (state)
    {
    case CHANGE_FILAMENT_STATE_CHECK_MOVE:
        ui_event = UI_EVENT_CHANGE_FILAMENT_CHECK_MOVE;
        hl_queue_enqueue(ui_change_filament_queue, &ui_event, 1);
        break;
    }
}

static void wait_move_callback(int state)
{
    ui_event_wait_move_t ui_event;
    switch (state)
    {
    case WAIT_MOVE_COMPLETED:
        ui_event = UI_EVENT_WAIT_MOVE_COMPLETED;
        hl_queue_enqueue(ui_wait_move_queue, &ui_event, 1);
        break;
    }
}

/**
 * @brief 保存累计时间
 *
 */
void save_cumulative_time(void)
{
    uint64_t cumulative_time = 0;
    cumulative_time = get_sysconf()->GetInt("system", "cumulative_time", 0);
    cumulative_time += printing_file_item.time_consumption;
    get_sysconf()->SetInt("system", "cumulative_time", cumulative_time);
    get_sysconf()->WriteIni(SYSCONF_PATH);
}

static bool app_box_temp_detection(char * filament)
{
    // 机箱温度报警机制:每隔60s检测一次，持续5min，五次检测皆超过39℃就报警；一旦有一次掉落39℃，则重新开始计次

    static uint64_t start_tick = 0;
    if(utils_get_current_tick() - start_tick > 60 * 1000)
    {
        start_tick = utils_get_current_tick();
    }
    else
    {
        return true; // 继续检测
    }

    srv_state_t *ss = app_get_srv_state();
    const char *low_temp_filament[] = {"ABS", "PC", "ASA", "PA"};
    int num_filament = sizeof(low_temp_filament) / sizeof(low_temp_filament[0]);
    bool material_switch = true;
    static int anomalies_number = 0;

    for(uint8_t i = 0; i < num_filament; i++)
    {
        if(strcmp(filament, low_temp_filament[i]) == 0)
        {
            material_switch = false;
            break;
        }
    }

    if((ss->heater_state[HEATER_ID_BOX].current_temperature > 39.) && material_switch)
    {
        anomalies_number++;
    }
    else
    {
        anomalies_number = 0;
    }

    if(anomalies_number == 5)
    {
        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_print_routine_msgbox_callback, (void *)MSGBOX_TIP_BOX_TEMP);
        anomalies_number = 0;
        return false; // 检测结束
    }

    return true; // 继续检测
}   

/**
 * @brief 执行打印任务
 * 
 * @param path 
 */
void enqueue_print_job_commands(char * path)
{
    //正常打印
    manual_control_sq.push("RESET_PRINTER_PARAM");
    char control_command[MANUAL_COMMAND_MAX_LENGTH];
    sprintf(control_command, "SDCARD_PRINT_FILE FILESLICE=%d FILENAME=\"%s\"", slice_param.slice_type, path);
    manual_control_sq.push(control_command);
    Printer::GetInstance()->manual_control_signal();
}


/**
 * @brief 异物检测
 * 
 */
#if CONFIG_SUPPORT_AIC
void app_print_foreign_capture_detection(void)
{
    if (app_foreign_detection_switch == false || app_print_get_print_state() == false)
        return;

    int nozzle_temp = 0;
    ui_event_wait_move_t ui_wait_move_stats;
    if (calibration_switch)
    {
        nozzle_temp = 250; // 打印校准喷嘴温度
    }
    else
    {
        nozzle_temp = 200; // 未打印校准喷嘴温度
    }
    if (!app_foreign_capture_detection_init)
    {
        manual_control_sq.push("G28");  
        manual_control_sq.push("G90");  
        manual_control_sq.push("G1 X" + to_string(202) + " F4500");
        manual_control_sq.push("G1 Y" + to_string(264.5) + " F4500");
        manual_control_sq.push("M400");
        manual_control_sq.push("M104 S" + to_string(nozzle_temp));
        manual_control_sq.push("M140 S60");
        manual_control_sq.push("M109 S" + to_string(nozzle_temp));
        manual_control_sq.push("M190 S60");
        Printer::GetInstance()->manual_control_signal();
        app_foreign_capture_detection_init = true;
    }

    if (!aic_foreign_capture_init && app_foreign_capture_detection_init)
    {
        double heat_value_nozzle, heat_value_hotbed;
        ui_cb[get_extruder1_curent_temp_cb](&heat_value_nozzle);
        ui_cb[get_bed_curent_temp_cb](&heat_value_hotbed);

        if (aic_foreign_capture_state == foreign_capture_idle)
        {
            if (hl_queue_dequeue(ui_wait_move_queue, &ui_wait_move_stats, 1))
            {
                int ret = 0;
                switch (ui_wait_move_stats)
                {
                    case UI_EVENT_WAIT_MOVE_COMPLETED:
                        ret = 1;
                        break;
                }
                if (!ret)
                    return; // 未加热完成，不进行异物检测ui更新
            }
            else
            {
                return;
            }
            // 异物监测
            ai_camera_send_cmd_handler(AIC_CMD_FOREIGN_CAPTURE, AIC_CMD_CARRY_ACTIVATE_AI_MONITOR);
            ai_camera_send_cmd_handler(AIC_CMD_FOREIGN_CAPTURE, AIC_CMD_CARRY_NORMAL_AI_MONITOR);
            aic_foreign_capture_state = foreign_capture_start;
            print_stats_state_callback_call(PRINT_STATS_STATE_FOREIGN_CAPTURE);
            return;
        }
        else if(aic_foreign_capture_state == foreign_capture_end)
        {
            aic_foreign_capture_state = foreign_capture_idle;
            print_stats_state_callback_call(PRINT_STATS_STATE_FOREIGN_CAPTURE_COMPLETED);
        }
        else
        {
            return;
        }

        if (aic_print_info.monitor_abnormal_index == 2)
        {
            //弹出检测到异物弹窗
            LOG_I("AI Camera:Foreign objects do not continue to print!\n");
            foreign_detection_result_handler = true; // 异物检测到，弹出暂停打印弹窗
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_ui_camera_routine_msgbox_callback, NULL);
        }
        else
        {
            if (calibration_switch)
            {
                Printer::GetInstance()->m_auto_leveling->m_enable_fast_probe = true;
                manual_control_sq.push("G29");
                Printer::GetInstance()->manual_control_signal();
            } else {
                Printer::GetInstance()->m_auto_leveling->m_enable_fast_probe = false;
            }
            enqueue_print_job_commands(printing_file_item.path);
        }
        aic_foreign_capture_init = true;
        aic_print_info.monitor_abnormal_state = false;
    }
}
#endif

static char file_name[NAME_MAX_LEN + 1];
void app_print_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static uint64_t app_print_abnormal_tick = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        if (app_print_get_print_state() == false)
        {
            ui_cb[manual_control_cb]((char *)"SDCARD_RESET_FILE");
            // utils_init_history_list(history_explorer);
            explorer_item_t *file_item = (explorer_item_t *)lv_event_get_param((lv_event_t *)e);
            if (file_item == NULL)
            {
                LOG_E("[%s] ERROR: file_item == null\n", __FUNCTION__);
                return;
            }
            app_print_init(widget_list, file_item);
            print_btn_state = PRINT_STATE_PASUE;
            memcpy(&printing_file_item, file_item, sizeof(explorer_item_t));
            sprintf(printing_file_item.date, "--/--/-- --:--");
            printing_file_item.print_state = false;
            app_foreign_capture_detection_init = false;
            aic_foreign_capture_init = false;
            aic_foreign_capture_state = foreign_capture_idle;
            ui_cb[clear_print_stats_cb](NULL);
            if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 0) // A面
            { 
                manual_control_sq.push("BED_MESH_SET_INDEX TYPE=standard INDEX=0");
            }
            else
            {
                manual_control_sq.push("BED_MESH_SET_INDEX TYPE=enhancement INDEX=0");
            }
            if (file_item->userdata != NULL)
                calibration_switch = *(int *)file_item->userdata;
            else
                calibration_switch = 0;
            if (hl_camera_get_exist_state() == false)
                app_foreign_detection_switch = false;
            else
                app_foreign_detection_switch = foreign_detection_switch;
            if (!app_foreign_detection_switch || resume_print_aic_state)
            {
                if (calibration_switch)
                {
                    Printer::GetInstance()->m_auto_leveling->m_enable_fast_probe = true;
                    manual_control_sq.push("G29");
                    Printer::GetInstance()->manual_control_signal();
                } else {
                    Printer::GetInstance()->m_auto_leveling->m_enable_fast_probe = false;
                }
                enqueue_print_job_commands(printing_file_item.path);
                app_foreign_capture_detection_init = true;
                aic_foreign_capture_init = true;
                resume_print_aic_state = false;
            }
            strcpy(file_name, file_item->name);
            std::vector<ui_event_stats_id_t>().swap(stats_vec);
            if (ui_printing_stats_queue == NULL)
                hl_queue_create(&ui_printing_stats_queue, sizeof(ui_event_stats_id_t), 64);
            if (ui_change_filament_queue == NULL)
                hl_queue_create(&ui_change_filament_queue, sizeof(ui_event_change_filament_t), 8);
            if (ui_wait_move_queue == NULL)
                hl_queue_create(&ui_wait_move_queue, sizeof(ui_event_wait_move_t), 8);
            print_stats_register_state_callback(ui_printint_stats_callback);
            change_filament_register_state_callback(ui_change_filament_callback);
            M400_register_state_callback(wait_move_callback);
            printing_state = true;
            material_break_detection_step = 0;
            box_temp_detect_switch = true;

            // 不忽略 CHECK_MOVE 事件
            char control_command[MANUAL_COMMAND_MAX_LENGTH];
            sprintf(control_command, "CHANGE_FILAMENT_SET_CHECK_MOVE_IGNORE CHECK_MOVE_IGNORE=%d", false);
            ui_cb[manual_control_cb](control_command);
            material_detection_e_length = 0;

            aic_print_info.print_state = 4;
            aic_print_info.tlp_start_state = true;
            strncpy(aic_print_info.print_name, file_name, sizeof(aic_print_info.print_name));
#if CONFIG_SUPPORT_AIC
            ai_camera_resp_cb_register(app_print_ai_camera_msg_handler_cb, NULL);
#endif

            LOG_I("power_off_resume_print:%d\n", get_sysconf()->GetBool("system", "power_off_resume_print", false));
            if (material_detection_switch)
            {
                if (gpio_is_init(MATERIAL_BREAK_DETECTION_GPIO) < 0)
                {
                    gpio_init(MATERIAL_BREAK_DETECTION_GPIO); // 断料检测IO初始化
                    gpio_set_direction(MATERIAL_BREAK_DETECTION_GPIO, GPIO_INPUT);
                }
                if ((gpio_is_init(MATERIAL_BREAK_DETECTION_GPIO) == 0) &&
                    (gpio_get_value(MATERIAL_BREAK_DETECTION_GPIO) == MATERIAL_BREAK_DETECTION_TRIGGER_LEVEL))
                {
                    material_break_detection_step = 1;
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_PAUSE);
                }
            }
        }
        else
        {
            app_print_init(widget_list, &printing_file_item);
        }
        // 注意不能使用set_text_fmt，文件名会出现%s
        lv_label_set_text(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[2], file_name);
        app_print_abnormal_tick = utils_get_current_tick();
        aic_light_switch_sending = false;
        if (print_timer == NULL)
        {
            print_timer = lv_timer_create(app_print_update, 500, (void *)widget_list);    
            lv_timer_ready(print_timer);
        }
        break;
    case LV_EVENT_DESTROYED:
        if (app_print_get_print_state() == false)
        {
            //打印结束速度切为均衡
            int printing_speed_value = PRINTING_EQUILIBRIUM_SPEED;
            ui_cb[set_print_speed_cb](&printing_speed_value);

            hl_queue_destory(&ui_printing_stats_queue);
            hl_queue_destory(&ui_change_filament_queue);
            hl_queue_destory(&ui_wait_move_queue);
            print_stats_unregister_state_callback(ui_printint_stats_callback);
            change_filament_unregister_state_callback(ui_change_filament_callback);
            M400_unregister_state_callback(wait_move_callback);
            // utils_add_history(&printing_file_item, history_explorer);
            save_cumulative_time();
            // printf("file_item.path: %s\n", printing_file_item.path);
            // printf("file_item.name: %s\n", printing_file_item.name);
            // printf("file_item.date: %s\n", printing_file_item.date);
            // printf("file_item.time_consumption: %"PRIu64"\n", printing_file_item.time_consumption);
            // printf("file_item.print_state: %d\n", printing_file_item.print_state);
#if CONFIG_SUPPORT_AIC
            ai_camrea_resp_cb_unregister(app_print_ai_camera_msg_handler_cb, NULL);
#endif
        }
        if (print_timer != NULL)
        {
            lv_timer_del(print_timer);
            print_timer = NULL;
        }
        break;
    case LV_EVENT_PRESSING:
        break;
    case LV_EVENT_RELEASED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE:
        case WIDGET_ID_PRINT_BTN_HOT_BED:
            ui_set_window_index(WINDOW_ID_CONTROL, (void *)(widget->header.index));
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_PRINT_BTN_FAN:
            ui_set_window_index(WINDOW_ID_FAN, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_PRINT_BTN_LIGHT:
#if 0
            illumination_light_swtich = !illumination_light_swtich;
            if (illumination_light_swtich)
            {
                lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(248));
                manual_control_sq.push("SET_LED_led2 RED=1 GREEN=1 BLUE=1 WHITE=1 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();

            }
            else
            {
                lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(103));
                manual_control_sq.push("SET_LED_led2 RED=0 GREEN=0 BLUE=0 WHITE=0 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();

            }
#endif

#if CONFIG_SUPPORT_AIC
            if (hl_camera_get_exist_state() == false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void *)MSGBOX_TIP_NOT_CAMERA);
            }
            else
            {
                if ((aic_function_switch || foreign_detection_switch) && aic_light_switch_flag == AIC_GET_STATE_LED_ON)
                {
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_aic_function_light_routine_msgbox_callback, NULL);
                }
                else
                {
                    if (aic_light_switch_sending == false)
                    {
                        aic_light_switch_sending = true;
                        if (aic_light_switch_flag == AIC_GET_STATE_LED_ON)
                            ai_camera_send_cmd_handler(AIC_CMD_CAMERA_LIGHT, AIC_CMD_CARRY_OFF_LED);
                        else if (aic_light_switch_flag == AIC_GET_STATE_LED_OFF ||
                                 aic_light_switch_flag == AIC_GET_STATE_LED_ABNORMAL)
                            ai_camera_send_cmd_handler(AIC_CMD_CAMERA_LIGHT, AIC_CMD_CARRY_ON_LED);
                    }
                }
            }
#endif

        { // 新摄像头厂商定义控灯方式
            if (hl_camera_get_exist_state() == false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void *)MSGBOX_TIP_NOT_CAMERA);
            }
            else
            {
                if (get_sysconf()->GetBool("system", "camera_light_switch", false))
                {
                    camera_control_light(0);
                    if(Printer::GetInstance()->m_box_led != nullptr)
                        Printer::GetInstance()->m_box_led->control_light(0);
                    get_sysconf()->SetBool("system", "camera_light_switch", false);
                }
                else
                {
                    camera_control_light(1);
                    if(Printer::GetInstance()->m_box_led != nullptr)
                        Printer::GetInstance()->m_box_led->control_light(1);
                    get_sysconf()->SetBool("system", "camera_light_switch", true);
                }
                app_print_update(print_timer);
            }
        }

            break;
        case WIDGET_ID_PRINT_BTN_PAUSE:
            if (print_btn_state == PRINT_STATE_PASUE)
            {
                app_msgbox_push(WINDOW_ID_DOUBLE_MSGBOX, true, app_print_double_msgbox_callback, (void *)MSGBOX_TIP_PAUSE);
            }
            else if (print_btn_state == PRINT_STATE_RESUME)
            {
                LOG_I("clicked print resume\n");

                if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
                else
                {
                    if (!material_break_detection_flag || !material_detection_switch)
                    {
                        set_filament_out_in_printing_flag(false);
                        manual_control_sq.push("RESUME");
                        Printer::GetInstance()->manual_control_signal();
                        print_btn_state = PRINT_STATE_PASUE;
                        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_PAUSE]->obj_container[1], ui_get_image_src(107));
                        material_break_detection_step = 0;
                        
                        app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_RESUME);
                        aic_print_info.print_state = 4;
    #if CONFIG_SUPPORT_AIC 
                        // 还原AI监测状态
                        aic_print_info.monitor_abnormal_state = false;
                        aic_print_info.monitor_abnormal_index = 0;
    #endif
                    }
                    else{
                        material_break_detection_step = 4;
                    }
                }
            }
            break;
        case WIDGET_ID_PRINT_BTN_STOP:
            if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            else
                app_msgbox_push(WINDOW_ID_DOUBLE_MSGBOX, true, app_print_double_msgbox_callback, (void *)MSGBOX_TIP_STOP);
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        // app_print_update(widget_list, NULL);

#if CONFIG_SUPPORT_AIC
        //打印中只保留炒面检测
        if (app_msgbox_is_active() == false && aic_print_info.print_state == 4) // 没有其他弹窗才弹出AI弹窗
        {
            if (aic_print_info.monitor_abnormal_state && utils_get_current_tick() - app_print_abnormal_tick > 2 * 1000)
            {
                aic_print_info.monitor_abnormal_state = false;
                if (aic_print_info.monitor_abnormal_index == 1)
                {
                    if (aic_abnormal_pause_print) // 触发打印暂停
                    {
                        LOG_I("AI Camera:Chow mein triggers print pause!\n");
                        app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_PAUSE);
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_ui_camera_routine_msgbox_callback, NULL);
                    }
                    else
                    {
                        LOG_I("AI Camera:Chow mein not triggers print pause!\n");
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_ui_camera_routine_msgbox_callback, NULL);
                    }
                }
            }
        }
#endif

        break;
    }
}

void app_print_update(lv_timer_t *timer)
{
    widget_t **widget_list = (widget_t **)timer->user_data;
    static uint64_t start_tick = 0;
    static uint64_t material_detection_start_tick = 0;
    static int printing_speed_value = 100;    // 打印速度值
    if (print_btn_state == PRINT_STATE_PASUE)
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_PAUSE]->obj_container[1], ui_get_image_src(107));
    }
    else if (print_btn_state == PRINT_STATE_RESUME)
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_PAUSE]->obj_container[1], ui_get_image_src(109));
    }
    srv_state_t *ss = app_get_srv_state();
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE]->obj_container[2], "%d℃", (int)ss->heater_state[HEATER_ID_EXTRUDER].target_temperature);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_HOT_BED]->obj_container[2], "%d℃", (int)ss->heater_state[HEATER_ID_BED].target_temperature);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_BOX_TEMPERATURE]->obj_container[2], "%d℃", (int)ss->heater_state[HEATER_ID_BOX].current_temperature);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_SHOWER_TEMPERATURE]->obj_container[0], "%d℃", (int)ss->heater_state[HEATER_ID_EXTRUDER].current_temperature);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_HOT_BED]->obj_container[0], "%d℃", ((int)ss->heater_state[HEATER_ID_BED].current_temperature));

    hl_wlan_connection_t wlan_cur_connection = {0};
    if (hl_wlan_get_status() == HL_WLAN_STATUS_CONNECTED && hl_wlan_get_connection(&wlan_cur_connection) != -1)
    {
        if (wlan_cur_connection.signal <= 100 / 3)
            lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_NETWORK]->obj_container[1], ui_get_image_src(19));
        else if (wlan_cur_connection.signal <= 200 / 3)
            lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_NETWORK]->obj_container[1], ui_get_image_src(20));
        else
            lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_NETWORK]->obj_container[1], ui_get_image_src(21));
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_NETWORK]->obj_container[1], ui_get_image_src(18));
    }

    if (ss->home_state[2] != SRV_STATE_HOME_END_FAILED)     // 弹出z轴归零失败弹窗时打印进度停止不进行变化
    {
        // 进度条
        int alread_print_size = 0;
        int print_file_size = 0;
        ui_cb[get_alread_print_size_cb](&alread_print_size);
        ui_cb[get_print_size_cb](&print_file_size);
        int value = std::min((int)(((double)alread_print_size / (double)print_file_size) * 1000), 1000);
        lv_slider_set_value(widget_list[WIDGET_ID_PRINT_SLIDER_PROGRESS]->obj_container[0], value, LV_ANIM_OFF);
        if (print_file_size == 0)
            print_file_size = 1;
        double value_percent = (double)alread_print_size / (double)print_file_size * 100.0;
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_PROGRESS_LABEL]->obj_container[2], "%.2f%%", value_percent);

        // 层数
        if (slice_param.total_layers != 0)
        {
            int total_layer = 0;
            int current_layer = 0;
            ui_cb[get_total_layer_cb](&total_layer);
            ui_cb[get_current_layer_cb](&current_layer);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_LAYER_LABEL]->obj_container[2], "%d/%d", current_layer, total_layer);
        }
    }

    // 时间
    double print_time = 0;
    ui_cb[get_alread_print_time_cb](&print_time);
    printing_file_item.time_consumption = (uint64_t)print_time;
    if (slice_param.estimated_time != 0)
    {
        uint64_t remaining_time = slice_param.estimated_time - (uint64_t)print_time;
        if ((uint64_t)print_time < slice_param.estimated_time && !printing_file_item.print_state)
        {
            int hours = remaining_time / 3600;
            int minutes = (remaining_time - hours * 3600) / 60;
            int seconds = remaining_time - hours * 3600 - minutes * 60;
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_TIME_LABEL]->obj_container[2], "%02dh%02dm", hours, minutes);
        }
        else
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_TIME_LABEL]->obj_container[2], "00h00m");
        }
    }

    // 温度异常停止打印
    if (Printer::GetInstance()->get_exceptional_temp_status() && !stop_state)
    {
        // ui_cb[manual_control_cb]((char *)"M106 P2 S0"); //关闭辅助风扇
        // ui_cb[manual_control_cb]((char *)"M106 P3 S0"); //关闭机箱风扇
        // highest_priority_cmd_sq.push("CANCEL_PRINT");
        // Printer::GetInstance()->highest_priority_control_signal();
        stop_state = 1;
    }

    if(Printer::GetInstance()->m_break_save->m_break_save_files_status){
        if(!start_tick){
            start_tick = utils_get_current_tick();
            LOG_I("speed slow\n");
        }
        else{
            if((utils_get_current_tick() - start_tick) > 180000){
                printing_speed_value = 100;
                ui_cb[set_print_speed_cb](&printing_speed_value);
                Printer::GetInstance()->m_break_save->m_break_save_files_status = false;
                LOG_I("speed fast\n");
            }
        }
    }

    //摄像头图标
    if (photography_switch && hl_camera_get_exist_state())
    {
        static uint64_t tlp_tick = 0;
        if(utils_get_current_tick() - tlp_tick > 1000)
        {
            if(strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1]), ui_get_image_src(98)) == 0)
                lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1], ui_get_image_src(249));
            else if(strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1]), ui_get_image_src(249)) == 0)
                lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1], ui_get_image_src(98));
            tlp_tick = utils_get_current_tick();
        }

    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1], ui_get_image_src(249));
    }

#if CONFIG_SUPPORT_AIC
    if (aic_light_switch_flag == AIC_GET_STATE_LED_ON && hl_camera_get_exist_state())
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(248));
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(103));
    }
#else
    if (get_sysconf()->GetBool("system", "camera_light_switch", false) && hl_camera_get_exist_state())
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(248));
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(103));
    }
#endif

    // 断料检测开始
    if (material_detection_switch)
    {
        if (gpio_is_init(MATERIAL_BREAK_DETECTION_GPIO) < 0)
        {
            gpio_init(MATERIAL_BREAK_DETECTION_GPIO); // 断料检测IO初始化
            gpio_set_direction(MATERIAL_BREAK_DETECTION_GPIO, GPIO_INPUT);
            LOG_E("material detection gpio error!!!!!\n");
        }
        // LOG_I("print_btn_state：%d\n", print_btn_state);
        if ((gpio_is_init(MATERIAL_BREAK_DETECTION_GPIO) == 0) &&
            (gpio_get_value(MATERIAL_BREAK_DETECTION_GPIO) == MATERIAL_BREAK_DETECTION_TRIGGER_LEVEL))
        {
            if(printing_file_item.print_state != true)      //未弹出打印完成的弹窗
            {  
                if(!material_detection_start_tick){
                material_detection_start_tick = utils_get_current_tick();
                }
                else{
                    if((utils_get_current_tick() - material_detection_start_tick) > 0){
                        material_break_detection_flag = true;
                        widget_t **widget_list = window_get_widget_list();
                        if (material_break_detection_step == 0)
                        {
                            if(print_btn_state == PRINT_STATE_PASUE)
                            {
                                double filament_used = Printer::GetInstance()->m_print_stats->get_status(get_monotonic(), NULL).filament_used;
                                // LOG_D("material_e_length : %lf\n", material_detection_e_length);
                                // LOG_D("e used : %lf\n", filament_used);
                                if (fabs(material_detection_e_length) < 1e-15)
                                {
                                    LOG_I("material_detection triggered, but e_length is 0, wait\n");
                                    material_detection_e_length = filament_used;
                                }
                                else if (fabs(material_detection_e_length - filament_used) > MATERIAL_DETECTION_E_PRINT_LENGTH)
                                {
                                    material_break_detection_step = 1;
                                    LOG_I("material_detection triggered, e_length: %f, used: %f\n", material_detection_e_length, filament_used);
                                    app_msgbox_close_all_avtive_msgbox();
                                    need_extrude = true;
                                    print_history_update_record(Printer::GetInstance()->m_break_save->s_saved_print_para.last_print_time,
                                                                                                PRINT_RECORD_STATE_PAUSE,
                                                                                                Printer::GetInstance()->m_break_save->s_saved_print_para.curr_layer_num,
                                                                                                Printer::GetInstance()->m_break_save->s_saved_print_para.filament_used,
                                                                                                1);    // 停止原因为断料检测，更新到历史记录
                                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_PAUSE);
                                    // app_msgbox_push(WINDOW_ID_MSGBOX_NO_BUTTON, true, app_print_nobtn_msgbox_callback, (void *)print_pausing);
                                }
                            }
                            else if(print_btn_state == PRINT_STATE_RESUME)
                            {
                                material_break_detection_step = 2;
                                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_print_routine_msgbox_callback, (void *)MSGBOX_TIP_CONSUMABLE_REPLACEMENT);
                            }
                        }
                        else if (material_break_detection_step == 4)
                        {
                            material_break_detection_step = 2;
                            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_print_routine_msgbox_callback, (void *)MSGBOX_TIP_CONSUMABLE_REPLACEMENT);
                        }
                    }
                }
            }
        }
        else
        {
            material_detection_e_length = 0;
            material_detection_start_tick = 0;
            material_break_detection_flag = false;
        }
    }

    // if (feed_msgbox_flag)
    // {
    //     app_msgbox_push(WINDOW_ID_PRINTFEED, true, app_print_feed_msgbox_callback, NULL);
    //     feed_msgbox_flag = false;
    // }



    ui_event_stats_id_t ui_print_stats;
    while (hl_queue_dequeue(ui_printing_stats_queue, &ui_print_stats, 1))
    {
        switch (ui_print_stats)
        {
        case UI_EVENT_PRINT_STATS_STATE_STANDBY:
            break;

        case UI_EVENT_PRINT_STATS_STATE_PRINT:
            stats_vec.push_back(ui_print_stats);
            start_sdcard_print = true;
            if (slice_param.total_layers > 0)
            {
                Printer::GetInstance()->m_print_stats->set_slice_param(slice_param);
                Printer::GetInstance()->m_print_stats->set_total_layers(slice_param.total_layers);

                // 创建历史记录时传入的slice_param是空的,此时重新传入
                print_history_record_t *record = print_history_get_record(0);
                memcpy(&record->slice_param, &slice_param, sizeof(slice_param_t));
            }

            update_print_speed = true;
            break;

        case UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING:
            print_busy = true;
            stats_vec.push_back(ui_print_stats);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(36));
            break;

        case UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING_COMPLETED:
            print_busy = false;
            if (stats_vec.size() > 0) { stats_vec.pop_back(); }
            stats_vec.push_back(ui_print_stats);
            break;

        case UI_EVENT_PRINT_STATS_STATE_HOMING:
            stats_vec.push_back(ui_print_stats);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(34));
            break;

        case UI_EVENT_PRINT_STATS_STATE_HOMING_COMPLETED:
            if (stats_vec.size() > 0) { stats_vec.pop_back(); }
            break;

        case UI_EVENT_PRINT_STATS_STATE_PREHEATING:
            stats_vec.push_back(ui_print_stats);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(35));
            break;

        case UI_EVENT_PRINT_STATS_STATE_PREHEATING_COMPLETED:
            if (stats_vec.size() > 0) { stats_vec.pop_back(); }
            break;

        case UI_EVENT_PRINT_STATS_STATE_PRINTING:
            // printing_state = true;
            break;

        case UI_EVENT_PRINT_STATS_STATE_PAUSING:
            break;

        case UI_EVENT_PRINT_STATS_STATE_PAUSED:
            break;

        case UI_EVENT_PRINT_STATS_STATE_CANCELLING:
            break;

        case UI_EVENT_PRINT_STATS_STATE_CANCELLED:
            break;

        case UI_EVENT_PRINT_STATS_STATE_COMPLETED:
        {
            // printing_state = false;
            // ui_set_window_index(WINDOW_ID_MAIN, NULL);
            printing_file_item.print_state = true;
            srv_state_t* ss = app_get_srv_state();
            if (ss->home_state[2] != SRV_STATE_HOME_END_FAILED)     // 弹出z轴归零失败弹窗，不弹打印完成
            {
                app_msgbox_close_all_avtive_msgbox();
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_print_routine_msgbox_callback, (void*)MSGBOX_TIP_COMPLETED);
            }
        }
            break;

        case UI_EVENT_PRINT_STATS_STATE_ERROR:
            printing_state = false;
            start_sdcard_print = false;
            break;

        case UI_EVENT_PRINT_STATS_STATE_FOREIGN_CAPTURE:
            stats_vec.push_back(ui_print_stats);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(335));
            break;
        case UI_EVENT_PRINT_STATS_STATE_FOREIGN_CAPTURE_COMPLETED:
            if (stats_vec.size() > 0) { stats_vec.pop_back(); }
            break;
        case UI_EVENT_PRINT_STATS_STATE_CHANGE_FILAMENT_START:
            app_msgbox_close_all_avtive_msgbox();
            change_filament_msgbox_state = true;
            print_btn_state = PRINT_STATE_RESUME;
            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_CHANGE_FILAMENT);
            break;
        case UI_EVENT_PRINT_STATS_STATE_CHANGE_FILAMENT_COMPLETED:
            change_filament_msgbox_state = false;
            break;
        }
    }
    ui_event_change_filament_t ui_change_filament_stats;
    while (hl_queue_dequeue(ui_change_filament_queue, &ui_change_filament_stats, 1))
    {
        switch (ui_change_filament_stats)
        {
        case UI_EVENT_CHANGE_FILAMENT_CHECK_MOVE:
            LOG_I("change filament check_move pause\n");
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_print_routine_msgbox_callback, (void *)MSGBOX_TIP_CUTTING_WARN);
            // manual_control_sq.push("PAUSE");
            // Printer::GetInstance()->manual_control_signal();
            // print_btn_state = PRINT_STATE_RESUME;
            break;
        }
    }
    
    if(box_temp_detect_switch == true)
    {
        box_temp_detect_switch = app_box_temp_detection(slice_param.filament_type);
    }

    if (stats_vec.size() > 0)
    {
        update_stats(widget_list, stats_vec.back());
    }
}

static void update_stats(widget_t **widget_list, ui_event_stats_id_t ui_print_stats)
{
    switch (ui_print_stats)
    {
    case UI_EVENT_PRINT_STATS_STATE_STANDBY:
        break;
    case UI_EVENT_PRINT_STATS_STATE_PRINT:
        extern int printing_speed_value;
        double speed;
        ui_cb[get_print_actual_speed](&speed);
        speed = speed * printing_speed_value / 100;
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], "%dmm/s", (int)speed / 60);
        break;

    case UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING:
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(36));
        break;

    case UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING_COMPLETED:
        break;

    case UI_EVENT_PRINT_STATS_STATE_HOMING:
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(34));
        break;

    case UI_EVENT_PRINT_STATS_STATE_HOMING_COMPLETED:
        break;

    case UI_EVENT_PRINT_STATS_STATE_PREHEATING:
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(35));
        break;

    case UI_EVENT_PRINT_STATS_STATE_PREHEATING_COMPLETED:
        break;

    case UI_EVENT_PRINT_STATS_STATE_PRINTING:
        break;

    case UI_EVENT_PRINT_STATS_STATE_PAUSING:
        break;

    case UI_EVENT_PRINT_STATS_STATE_PAUSED:
        break;

    case UI_EVENT_PRINT_STATS_STATE_CANCELLING:
        break;

    case UI_EVENT_PRINT_STATS_STATE_CANCELLED:
        break;

    case UI_EVENT_PRINT_STATS_STATE_COMPLETED:
        break;
    case UI_EVENT_PRINT_STATS_STATE_ERROR:
        break;

    case UI_EVENT_PRINT_STATS_STATE_FOREIGN_CAPTURE:
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_STATE]->obj_container[0], tr(335));
        break;
    default:
        break;
    }
}

void adjust_fan_based_on_filament(void)
{
    const char *low_temp_filament[] = {"ABS", "PC", "ASA", "PA"};
    int num_filament = sizeof(low_temp_filament) / sizeof(low_temp_filament[0]);
    bool fan_switch = true;
    srv_state_t *ss = app_get_srv_state();
    double *box_temp = &ss->heater_state[HEATER_ID_BOX].current_temperature;
    static uint64_t adjust_fan_tick = 0;
    double eventtime = get_monotonic();
    struct fan_state box_fan_state = Printer::GetInstance()->m_printer_fans[BOX_FAN]->get_status(eventtime);

    if (utils_get_current_tick() - adjust_fan_tick < 120 * 1000) //两分钟检测一次
    {
        return;
    }
    adjust_fan_tick = utils_get_current_tick();
    char fan_speed_str[64] = {0};
    int fan_speed = 0;
    int real_fan_speed = box_fan_state.speed * 255;

    for(uint8_t i = 0; i < num_filament; i++)
    {
        if(strcmp(slice_param.filament_type, low_temp_filament[i]) == 0)
        {
            fan_switch = false;
            break;
        }
    }
    if(fan_switch)
    {
        // if(*box_temp > 39. && *box_temp <= 45.)
        // {
        //     fan_speed = 255;
        // }
        // else if(*box_temp > 45. && *box_temp <= 50.)
        // {
        //     fan_speed = 255;
        // }
        // else if(*box_temp > 50.)
        // {
        //     fan_speed = 255;
        // }
        // else
        // {
        //     fan_speed = 255;
        // }
        fan_speed = 255;
    }
    else
        fan_speed = 0;

    LOG_I("real_fan_speed = %d，fan speed = %d\n", real_fan_speed, fan_speed);
    if (real_fan_speed != fan_speed)
    {
        snprintf(fan_speed_str, sizeof(fan_speed_str), "M106 P3 S%d", fan_speed);
        ui_cb[manual_control_cb](fan_speed_str);
    }
}

static void app_print_init(widget_t **widget_list, explorer_item_t *file_item)
{
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
    lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_BTN_BOX_TEMPERATURE]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[0],LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_size(widget_list[WIDGET_ID_PRINT_BTN_NETWORK]->obj_container[0], 64, 83);
    lv_obj_set_size(widget_list[WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE]->obj_container[0], 64, 83);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE]->obj_container[0], 0, 84);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE]->obj_container[1], 4, 21);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE]->obj_container[2], 25, 26);
    lv_obj_set_size(widget_list[WIDGET_ID_PRINT_BTN_HOT_BED]->obj_container[0], 64, 83);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_HOT_BED]->obj_container[0], 0, 168);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_HOT_BED]->obj_container[1], 7, 22);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_HOT_BED]->obj_container[2], 30, 26);

    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_LABEL_SHOWER_TEMPERATURE]->obj_container[0], 0, 49);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_LABEL_HOT_BED]->obj_container[0], 0, 49);

    lv_obj_set_size(widget_list[WIDGET_ID_PRINT_BTN_FAN]->obj_container[0], 64, 77);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_FAN]->obj_container[0], 406, 10);
    lv_obj_set_size(widget_list[WIDGET_ID_PRINT_BTN_PAUSE]->obj_container[0], 64, 77);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_PAUSE]->obj_container[0], 406, 97);
    lv_obj_set_size(widget_list[WIDGET_ID_PRINT_BTN_STOP]->obj_container[0], 64, 77);
    lv_obj_set_pos(widget_list[WIDGET_ID_PRINT_BTN_STOP]->obj_container[0], 406, 185);

#endif
    lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_PRINT_SLIDER_PROGRESS]->obj_container[0], 0, LV_PART_KNOB);
    lv_label_set_long_mode(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[2], LV_LABEL_LONG_SCROLL);
    memset(&slice_param, 0, sizeof(slice_param));
    char preview_path[PATH_MAX_LEN + 1];
    gcode_preview(file_item->path, preview_path, 1, &slice_param, file_item->name);
    // printf(">>>>>>>>>>>>print_file_item->name = %s\n", file_item->name);
    LOG_I(">>>>>>>>>>>>print_file_item->name = %s\n", file_item->name);
    print_load_thumbnail(widget_list[WIDGET_ID_PRINT_BTN_PREVIEW]->obj_container[1], file_item->name, 144., 144., ui_get_image_src(215));
    lv_label_set_text(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[2], file_item->name); 
    lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1], ui_get_image_src(98));
    
    if (photography_switch && hl_camera_get_exist_state())
        lv_obj_clear_flag(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(widget_list[WIDGET_ID_PRINT_BTN_CAMERA_NAME]->obj_container[1],LV_OBJ_FLAG_HIDDEN);

    lv_slider_set_value(widget_list[WIDGET_ID_PRINT_SLIDER_PROGRESS]->obj_container[0], 52, LV_ANIM_OFF);
    lv_slider_set_range(widget_list[WIDGET_ID_PRINT_SLIDER_PROGRESS]->obj_container[0], 0, 1000);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_PROGRESS_LABEL]->obj_container[2], "0%%");
    if (slice_param.total_layers != 0)
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_LAYER_LABEL]->obj_container[2], "0/%d", slice_param.total_layers);
    }
    else
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_LAYER_LABEL]->obj_container[2], "-/-");
    }
    if (slice_param.estimated_time != 0)
    {
        int hours = slice_param.estimated_time / 3600;
        int minutes = (slice_param.estimated_time - hours * 3600) / 60;
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_TIME_LABEL]->obj_container[2], "%02dh%02dm", hours, minutes);
    }
    else
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_TIME_LABEL]->obj_container[2], "00h00m");
    }

#if CONFIG_SUPPORT_AIC
    if (aic_light_switch_flag == AIC_GET_STATE_LED_ON && hl_camera_get_exist_state())
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(248));
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(103));
    }
#endif

    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_SHOWER_TEMPERATURE]->obj_container[2], "0℃");
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_HOT_BED]->obj_container[2], "0℃");
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_BTN_BOX_TEMPERATURE]->obj_container[2], "0℃");
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_SHOWER_TEMPERATURE]->obj_container[0], "0℃");
    lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINT_LABEL_HOT_BED]->obj_container[0], "0℃");
    lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_PAUSE]->obj_container[1], ui_get_image_src(107));
}

bool app_print_get_print_state(void)
{
    return printing_state;
}

bool app_print_get_print_busy(void)
{
    return print_busy;
}

/**
 * @brief 获取打印完成弹窗弹出状态
 * 
 * @return  ture: 弹出  false: 未弹出
 */
bool app_print_get_print_completed_msgbox(void)
{
    return printing_file_item.print_state;
}

bool app_print_reset_status(void)
{
    printing_state = false;
    start_sdcard_print = false;
    printing_file_item.print_state = false;
    manual_control_sq.push("RESET_PRINTER_PARAM");
    Printer::GetInstance()->manual_control_signal();
    ui_set_window_index(WINDOW_ID_MAIN, NULL);
    app_top_update_style(window_get_top_widget_list());

    print_stats_set_state(PRINT_STATS_STATE_STANDBY);
}

static bool app_print_double_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_PAUSE)
        {
            material_break_clear_msgbox_state = true;
            lv_img_set_src(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT]->obj_container[1], ui_get_image_src(117));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT]->obj_container[2], tr(52));
        }
        else if (tip_index == MSGBOX_TIP_STOP)
        {
            material_break_clear_msgbox_state = true;
            lv_img_set_src(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT]->obj_container[1], ui_get_image_src(115));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT]->obj_container[2], tr(51));
        }
        lv_img_set_src(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_RIGHT]->obj_container[1], ui_get_image_src(119));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(31));
        break;
    case LV_EVENT_DESTROYED:
        material_break_clear_msgbox_state = false;
        break;
    case LV_EVENT_PRESSING:
        switch (widget->header.index)
        {
        case WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT:
            break;
        case WIDGET_ID_DOUBLE_MSGBOX_BTN_RIGHT:
            break;
        }
        break;
    case LV_EVENT_RELEASED:
        switch (widget->header.index)
        {
        case WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT:
            break;
        case WIDGET_ID_DOUBLE_MSGBOX_BTN_RIGHT:
            break;
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT:
            if (tip_index == MSGBOX_TIP_STOP)
            {
                LOG_I("clicked print stop\n");

                if(Printer::GetInstance()->m_pause_resume->get_status().is_busying)      //正在执行暂停/停止/恢复命令，不允许停止操作
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
                else
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_STOP);
            }
            else if (tip_index == MSGBOX_TIP_PAUSE)
            {
                LOG_I("clicked print pause\n");

                if(Printer::GetInstance()->m_pause_resume->get_status().is_busying)      //正在执行暂停/停止/恢复命令，不允许暂停操作
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
                else
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_PAUSE);
            }
            return true;
        case WIDGET_ID_DOUBLE_MSGBOX_BTN_RIGHT:
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

static bool app_print_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_COMPLETED)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(47));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(49));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(50));
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(155));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 51);
            lv_obj_set_height(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[0], 37);
            lv_obj_set_y(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[0], 67);

            double print_time = 0;
            ui_cb[get_alread_print_time_cb](&print_time);
            int hours = print_time / 3600;
            int minutes = (print_time - hours * 3600) / 60;
            int seconds = print_time - hours * 3600 - minutes * 60;
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], "%s:%02dh%02dm%02ds", tr(48), hours, minutes, seconds);
        }
        else if (tip_index == MSGBOX_TIP_CONSUMABLE_REPLACEMENT)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(42));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            material_detection_e_length = 0;
        }
        else if (tip_index == MSGBOX_TIP_CUTTING_WARN)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(249));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(49));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(340));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_BOX_TEMP)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(316));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT:
            if (tip_index == MSGBOX_TIP_COMPLETED)
            {
                printing_state = false;
                start_sdcard_print = false;
                printing_file_item.print_state = false;
                manual_control_sq.push("RESET_PRINTER_PARAM");
                Printer::GetInstance()->manual_control_signal();
                ui_set_window_index(WINDOW_ID_MAIN, NULL);
                app_top_update_style(window_get_top_widget_list());

                print_stats_set_state(PRINT_STATS_STATE_STANDBY);
            }
            else if(tip_index == MSGBOX_TIP_CUTTING_WARN)
            {
                char control_command[MANUAL_COMMAND_MAX_LENGTH];
                sprintf(control_command, "CHANGE_FILAMENT_SET_CHECK_MOVE_IGNORE CHECK_MOVE_IGNORE=%d", false);
                ui_cb[manual_control_cb](control_command);
            }
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT:
            if (tip_index == MSGBOX_TIP_COMPLETED)
            {
                printing_state = false;
                start_sdcard_print = false;
                widget_t **widget_list = window_get_widget_list();
                lv_event_send(widget_list[0]->obj_container[0], (lv_event_code_t)LV_EVENT_DESTROYED, NULL);
                lv_event_send(widget_list[0]->obj_container[0], (lv_event_code_t)LV_EVENT_CREATED, &printing_file_item);
            }
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE:
            if (tip_index == MSGBOX_TIP_CONSUMABLE_REPLACEMENT)
            {
                print_history_update_record(Printer::GetInstance()->m_break_save->s_saved_print_para.last_print_time,
                                                            PRINT_RECORD_STATE_PAUSE,
                                                            Printer::GetInstance()->m_break_save->s_saved_print_para.curr_layer_num,
                                                            Printer::GetInstance()->m_break_save->s_saved_print_para.filament_used,
                                                            0);    // 停止原因恢复为初始状态避免下次暂停web触发断料弹窗，更新到历史记录
                set_filament_out_in_printing_flag(true);
                ui_set_window_index(WINDOW_ID_FEED, NULL);
                app_top_update_style(window_get_top_widget_list());
                // 需要添加断料检测进出料弹窗
                // feed_msgbox_flag = true;
                // app_msgbox_push(WINDOW_ID_PRINTFEED, true, app_print_feed_msgbox_callback, NULL);
            }
            else if (tip_index == MSGBOX_TIP_BOX_TEMP)
            {
                box_temp_detect_switch = true; // 继续检测
            }
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

// static bool app_print_feed_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
// {
//     widget_t **widget_list = win->widget_list;
//     static int tip_index = 0;
//     srv_control_req_move_t move_request = {};
//     srv_control_res_move_t move_response = {};
//     switch (lv_event_get_code((lv_event_t *)e))
//     {
//     case LV_EVENT_CREATED:
//         nozzle_heat_target = 230;
//         ui_cb[extruder1_heat_cb](&nozzle_heat_target);
//         ui_cb[get_extruder1_curent_temp_cb](&nozzle_heat_real);
//         // ui_cb[get_extruder1_target_temp_cb](&nozzle_heat_target);
//         lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINTFEED_BTN_TEMPERATURE]->obj_container[2], "%.0f/%d℃", nozzle_heat_real, nozzle_heat_target);

//         app_set_widget_item2center_align(widget_list, WIDGET_ID_PRINTFEED_BTN_TEMPERATURE, 3);
//         break;
//     case LV_EVENT_DESTROYED:
//         material_break_detection_step = 3;
//         break;
//     case LV_EVENT_CLICKED:
//         switch (widget->header.index)
//         {
//         // case WIDGET_ID_PRINTFEED_CONTAINER_MASK:
//         //     return true;
//         //     break;
//         case WIDGET_ID_PRINTFEED_BTN_IN_FEED:
//             // move_request.e = EXTRUDER_MOVE_DISTANCE;
//             // move_request.f = EXTRUDER_MOVE_SPEED;
//             // simple_bus_request("srv_control", SRV_CONTROL_STEPPER_MOVE, &move_request, &move_response);
//             set_filament_out_in_printing_flag(true);
//             ui_set_window_index(WINDOW_ID_FEED, NULL);
//             app_top_update_style(window_get_top_widget_list());
//             return true;
//             break;
//         case WIDGET_ID_PRINTFEED_BTN_OUT_FEED:
//             // move_request.e = -EXTRUDER_MOVE_DISTANCE;
//             // move_request.f = EXTRUDER_MOVE_SPEED;
//             // simple_bus_request("srv_control", SRV_CONTROL_STEPPER_MOVE, &move_request, &move_response);
//             set_filament_out_in_printing_flag(true);
//             ui_set_window_index(WINDOW_ID_FEED, NULL);
//             app_top_update_style(window_get_top_widget_list());
//             return true;
//             break;
//         case WIDGET_ID_PRINTFEED_BTN_STOP_FEED:
//             return true;
//             break;
//         }
//         break;
//     case LV_EVENT_UPDATE:
//         ui_cb[get_extruder1_curent_temp_cb](&nozzle_heat_real);
//         lv_label_set_text_fmt(widget_list[WIDGET_ID_PRINTFEED_BTN_TEMPERATURE]->obj_container[2], "%.0f/%d℃", nozzle_heat_real, nozzle_heat_target);
//         break;
//     }
//     return false;
// }

/*
    @breif：网络管理改变灯光状态时，ui进行同步
*/
static void app_print_update_light(widget_t **widget_list)
{
#if 0
    if (illumination_light_swtich == true && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1]), ui_get_image_src(248)) != 0)
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(248));
    }
    else if (illumination_light_swtich == false && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1]), ui_get_image_src(103)) != 0)
    {
        lv_img_set_src(widget_list[WIDGET_ID_PRINT_BTN_LIGHT]->obj_container[1], ui_get_image_src(103));
    }
#endif
}

static bool app_print_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    bool wait_move = true;   //默认为运动阻塞状态
    bool wait_move_reset = true;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        pause_cancel_flag = 0;
        // 调平中
        if (tip_index == MSGBOX_TIP_BE_LEVELLING)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(271));
        else if (tip_index == MSGBOX_TIP_STOP)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(257));
        }
        else if (tip_index == MSGBOX_TIP_PAUSE)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(258));
        }
        else if (tip_index == MSGBOX_TIP_RESUME)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(339));
            pause_cancel_flag = 1;
        }
        else if (tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(313));
            start_tick = utils_get_current_tick();
        }
        else if (tip_index == MSGBOX_TIP_CHANGE_FILAMENT)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(258));
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
        case WIDGET_ID_SINGLE_MSGBOX_BTN_CONTAINER:
            return false;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_BE_LEVELLING)
        {
            return true;
        }
        else if (tip_index == MSGBOX_TIP_STOP)
        {
            auto it = std::find(stats_vec.begin(), stats_vec.end(), UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING_COMPLETED);
            ui_cb[get_printing_wait_cb](&wait_move);
            if (it != stats_vec.end() || !calibration_switch) 
            {
                // 如果 it 不等于 stats_vec.end()，已经自动调平完毕
                if (!pause_cancel_flag)
                {
                    ui_cb[manual_control_cb]((char *)"M106 P2 S0"); //关闭辅助风扇
                    ui_cb[manual_control_cb]((char *)"M106 P3 S0"); //关闭机箱风扇
                    ui_cb[set_printing_wait_cb](&wait_move_reset);
                    highest_priority_cmd_sq.push("CANCEL_PRINT");
                    Printer::GetInstance()->highest_priority_control_signal();
                    printing_state = false;
                    start_sdcard_print = false;
                    pause_cancel_flag = 1;
                    // if (photography_switch)
                    //     aic_tlp_delete(aic_print_info.tlp_test_path);
                }
                else if (wait_move == false)
                {
                    pause_cancel_flag = 0;
                    manual_control_sq.push("RESET_PRINTER_PARAM");
                    Printer::GetInstance()->manual_control_signal();
                    ui_set_window_index(WINDOW_ID_MAIN, NULL);
                    return true;
                }
            }
        }
        else if (tip_index == MSGBOX_TIP_PAUSE)
        {
            auto it = std::find(stats_vec.begin(), stats_vec.end(), UI_EVENT_PRINT_STATS_STATE_AUTOLEVELING_COMPLETED);
            ui_cb[get_printing_wait_cb](&wait_move);

            // 异物前暂停处理
            if (foreign_detection_result_handler)
            {
                foreign_detection_result_handler = false;
                pause_cancel_flag = 0;
                print_btn_state = PRINT_STATE_RESUME;
                return true;
            }

            // 未开始真正打印,暂停无效
            if (start_sdcard_print == false)
            {
                return false;
            }

            if ((it != stats_vec.end() || !calibration_switch) && start_sdcard_print) 
            {
                // 如果 it 不等于 stats_vec.end()，已经自动调平完毕
                if (!pause_cancel_flag)
                {
                    ui_cb[set_printing_wait_cb](&wait_move_reset);
                    if (need_extrude)
                        manual_control_sq.push("PAUSE EXTRUDE=1");
                    else
                        manual_control_sq.push("PAUSE");
                    need_extrude = false;
                    Printer::GetInstance()->manual_control_signal();
                    pause_cancel_flag = 1;
                    print_btn_state = PRINT_STATE_RESUME;
                }
                else if (wait_move == false)
                {
                    pause_cancel_flag = 0;
                    if(material_break_detection_step == 1)
                    {
                        material_break_detection_step = 2;
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_print_routine_msgbox_callback, (void *)MSGBOX_TIP_CONSUMABLE_REPLACEMENT);
                    }
                    return true;
                }
            }
        }
        else if (tip_index == MSGBOX_TIP_RESUME)
        {
            if(!Printer::GetInstance()->m_pause_resume->get_status().is_busying)
            {
                pause_cancel_flag = 0;
                return true;
            }
        }
        else if (tip_index == MSGBOX_TIP_CHANGE_FILAMENT)
        {
            if (change_filament_msgbox_state == false)
            {
                ui_set_window_index(WINDOW_ID_FEED,NULL);
                app_top_update_style(window_get_top_widget_list());
                return true;
            }
        }
        else if (tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
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

// 打印生成延时摄影
void aic_tlp_printing_generate_tlp(void)
{
#if CONFIG_SUPPORT_TLP

    // static uint64_t start1_tick = 0;
    // if (start1_tick == 0)
    //     start1_tick = utils_get_current_tick();
    // if (utils_get_current_tick() - start1_tick > 10 * 1000)
    // {
    //     static bool sta = true;
    //     if (sta)
    //     {
    //         sta = false;
    //         aic_print_info.print_state = 4;
    //         aic_print_info.total_layer = 200;
    //         aic_print_info.tlp_start_state = true;
    //     }
    // }

#define TLP_MIN_PRINT_LAYER 1     // 模型最小层数
#define TLP_MAX_CAPTURE_IMAGE 1200 // 最多拍摄图数
#define TLP_FRAME_RATE 20          // 帧率

    static int interval_capture_layer = 5; // 间隔层拍照
    static int generate_tlp_time = 5;      // 视频时间(s)
    static int model_limit_layer = 500;    // 界限层(小于按时间生成 大于按间隔层生成)

    static int prev_capture_layer = 0;               // 上一次拍照层
    static int interval_capture_practical_layer = 0; // 实际间隔层拍照
    static int interval_capture_image_number;        // 拍照总张数

    static bool tlp_complte_state = false; // 延时生成状态

    static bool video_exist_state_change = false;

    if (app_print_get_print_state() && hl_camera_get_exist_state() == false)
        video_exist_state_change = false;// 摄像头状态变化

    ui_cb[get_total_layer_cb](&aic_print_info.total_layer);
    ui_cb[get_current_layer_cb](&aic_print_info.current_layer);

    if (app_print_get_print_state() && aic_print_info.print_state != 0 && photography_switch)
    {
        if (aic_print_info.tlp_start_state && aic_print_info.total_layer >= TLP_MIN_PRINT_LAYER && aic_print_info.current_layer == 1)
        {
            aic_print_info.tlp_start_state = false;
            tlp_complte_state = false;
            video_exist_state_change = true;

            char name_prefix[256];
            utils_get_prefix(aic_print_info.print_name, name_prefix);
            snprintf(aic_print_info.tlp_test_path, sizeof(aic_print_info.tlp_test_path), "%s-%lld", name_prefix, utils_get_current_tick());
            aic_tlp_init(aic_print_info.tlp_test_path);

            interval_capture_layer = get_sysconf()->GetInt("system", "interval_capture_layer", 5);
            generate_tlp_time = get_sysconf()->GetInt("system", "generate_tlp_time", 5);
            model_limit_layer = get_sysconf()->GetInt("system", "model_limit_layer", 500);

            LOG_I("aic_print_info.tlp_test_path:%s interval_capture_layer:%d generate_tlp_time:%d model_limit_layer:%d\n", aic_print_info.tlp_test_path, interval_capture_layer, generate_tlp_time, model_limit_layer);

            // 实际间隔层拍照
            if (aic_print_info.total_layer < model_limit_layer) // 按时间生成
            {
                float interval_value = aic_print_info.total_layer / (float)(generate_tlp_time * TLP_FRAME_RATE);
                if (interval_value < 1)
                    interval_capture_practical_layer = 1;
                else
                    interval_capture_practical_layer = interval_value;
            }
            else // 按间隔层生成
            {
                float total_capture_layer = (aic_print_info.total_layer / (float)(interval_capture_layer));
                if (total_capture_layer > TLP_MAX_CAPTURE_IMAGE)
                    interval_capture_practical_layer = aic_print_info.total_layer / (float)(TLP_MAX_CAPTURE_IMAGE);
                else
                {
                    if (aic_print_info.total_layer / interval_capture_layer < TLP_MIN_PRINT_LAYER)
                        interval_capture_practical_layer = aic_print_info.total_layer / TLP_MIN_PRINT_LAYER;
                    else
                        interval_capture_practical_layer = interval_capture_layer;
                }
            }
        }

        if (aic_print_info.print_state == 4 && aic_print_info.total_layer >= TLP_MIN_PRINT_LAYER && video_exist_state_change)
        {
            if (prev_capture_layer != aic_print_info.current_layer && aic_print_info.tlp_start_state == false)
            {
                if ((aic_print_info.current_layer == 1) ||
                    (aic_print_info.current_layer >= prev_capture_layer + interval_capture_practical_layer))
                {
                    aic_tlp_capture(aic_print_info.current_layer);
                    prev_capture_layer = aic_print_info.current_layer;
                    interval_capture_image_number++;

#if 0
                    if (aic_print_info.current_layer == 1)
                        interval_capture_image_number = 0;
                    HL_ASS_LOG printf("interval_capture_practical_layer:%d interval_capture_image_number:%d aic_print_info.current_layer:%d\n", interval_capture_practical_layer, interval_capture_image_number, aic_print_info.current_layer);
                    printf("interval_capture_layer:%d generate_tlp_time:%d model_limit_layer:%d\n", interval_capture_layer, generate_tlp_time, model_limit_layer);
                    if (interval_capture_image_number == 180)
                        aic_print_info.print_state = 1;
#endif
                }
            }

            // static uint64_t start_tick = 0;
            // if (utils_get_current_tick() - start_tick > 0.5 * 1000 && aic_print_info.current_layer <= aic_print_info.total_layer)
            // {
            //     start_tick = utils_get_current_tick();
            //     aic_print_info.current_layer++;
            // }
        }
    }

    if (aic_print_info.print_state == 1)
    {
        if (tlp_complte_state == false && aic_print_info.tlp_start_state == false)
        {
            tlp_complte_state = true;

            if (aic_print_info.current_layer >= TLP_MIN_PRINT_LAYER && video_exist_state_change)
                aic_tlp_complte(1, interval_capture_image_number);
            else
                aic_tlp_complte(0, 0);
            aic_print_info.print_state = 0;
            interval_capture_image_number = 0;
        }
        memset(aic_print_info.tlp_test_path, 0, sizeof(aic_print_info.tlp_test_path));  // 初始化为全零
    }
    else if (aic_print_info.print_state == 2 || aic_print_info.print_state == 3)
    {
        aic_print_info.print_state = 0;
        interval_capture_image_number = 0;
        memset(aic_print_info.tlp_test_path, 0, sizeof(aic_print_info.tlp_test_path));  // 初始化为全零
    }

#endif
}

static bool app_ui_camera_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (aic_print_info.monitor_abnormal_index == 1)
        {
            if (aic_abnormal_pause_print) // 已暂停
            {
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(307));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(303));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(340));
            }
            else // 未暂停
            {
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(308));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(304));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(340));
            }

            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else if (aic_print_info.monitor_abnormal_index == 2) // 已暫停
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(306));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(334));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(340));
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            foreign_detection_result_handler = false;
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT:
            if (aic_print_info.monitor_abnormal_index == 1)
            {
                if (aic_abnormal_pause_print) // 已暂停,触发停止
                {
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_STOP);
                }
                else // 未暂停,触发暂停
                {
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_PAUSE);
                }
            }
            else if (aic_print_info.monitor_abnormal_index == 2) // 已暂停,触发停止
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_print_single_msgbox_callback, (void *)MSGBOX_TIP_STOP);
            }
            aic_print_info.monitor_abnormal_index = 0;
            calibration_switch = 0;
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT:
            if (aic_print_info.monitor_abnormal_index == 2)
            {
                if (calibration_switch)
                {
                    Printer::GetInstance()->m_auto_leveling->m_enable_fast_probe = true;
                    manual_control_sq.push("G29");
                    Printer::GetInstance()->manual_control_signal();
                } else {
                    Printer::GetInstance()->m_auto_leveling->m_enable_fast_probe = false;
                }
                enqueue_print_job_commands(printing_file_item.path);
                print_btn_state = PRINT_STATE_PASUE; //点击忽略后将暂停按钮置为可暂停
            }
            aic_print_info.monitor_abnormal_index = 0;
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE:
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

#if CONFIG_SUPPORT_AIC
// 摄像头串口回复
static void app_print_ai_camera_msg_handler_cb(const void *data, void *user_data)
{
    aic_ack_t *resp = (aic_ack_t *)data;
    // std::cout << "aic_foreign_capture_state : " << aic_foreign_capture_state << std::endl;
    switch (resp->cmdid)
    {
    case AIC_CMD_FOREIGN_CAPTURE:
        if (app_print_get_print_state() && app_foreign_detection_switch)
        {
            static bool aic_foreign_capture_skip = false;
            static bool aic_activate_detection_state = false; // 启动检测
            if (resp->is_timeout)
            {
                aic_foreign_capture_skip = true;
                aic_activate_detection_state = false;
                aic_foreign_capture_state = foreign_capture_end;
                LOG_I("AI Camera:Foreign objects detection timeout!\n");
            }
            else
            {
                if (resp->body_size == 2)
                {
                    switch (resp->body[1])
                    {
                    case AIC_GET_STATE_NO_FOREIGN_BODY:
                        if (aic_activate_detection_state == false)
                        {
                            aic_activate_detection_state = true;
                        }
                        else
                        {
                            aic_foreign_capture_skip = true;
                            aic_activate_detection_state = false;
                            aic_foreign_capture_state = foreign_capture_end;
                            LOG_I("AI Camera:Foreign objects detection normal!\n");
                        }
                        break;
                    case AIC_GET_STATE_PRINT_FRONT_CAMER_ABNORMAL:
                    case AIC_GET_STATE_PRINT_FRONT_AI_FUNCTION_OFF:
                        aic_foreign_capture_skip = true;
                        aic_activate_detection_state = false;
                        aic_foreign_capture_state = foreign_capture_end;
                        LOG_I("AI Camera:Foreign objects detection camera abnormal or off!\n");
                        break;
                    case AIC_GET_STATE_HAVE_FOREIGN_BODY:
                        aic_foreign_capture_skip = false; // 异物其他回调处理
                        aic_activate_detection_state = false;
                        aic_foreign_capture_state = foreign_capture_end;
                        LOG_I("AI Camera:Foreign objects detection abnormal!\n");
                        break;
                    }
                }
            }
        }
        break;
    }
}
#endif

uint8_t is_print_pause_cancel_enqueue(void)
{
    return pause_cancel_flag;
}
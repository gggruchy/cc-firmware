#include "app.h"
#include "app_calibration.h"
#include "app_camera.h"
#include "app_control.h"
#include "app_device_inspection.h"
#include "app_explorer.h"
#include "app_keyboard.h"
#include "app_language.h"
#include "app_main.h"
#include "app_network.h"
#include "app_print.h"
#include "app_setting.h"
#if ENABLE_MANUTEST
#include "app_manu.h"
#endif
#include "app_timezone.h"
#include "app_top.h"
#include "hl_ts_queue.h"
#include "ai_camera.h"
#include "Define_config_path.h"
#include "configfile.h"
#include "hl_wlan.h"
#include "ota.h"
#include "service.h"
#include "simplebus.h"
#include "ui_api.h"
#include "aic_tlp.h"
#include "gpio.h"
#include "klippy.h"
#include "app_print_platform.h"
#define LOG_TAG "app"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#include "hl_camera.h"
#include "hl_common.h"
#include "file_manager.h"


static ConfigParser *sysconfig;
bool material_detection_switch = false;
bool keying_sound_switch = false;
bool silent_mode_switch = false;
bool resume_print_switch = false;
int luminance_state = 4;
uint8_t aic_detection_switch_flag = 0;
uint8_t aic_light_switch_flag = 0;
bool photography_switch = false;
// int aic_version_state = 0;
// int aic_version_newer = 0;      //0:没有新版本 1：有新版本
bool aic_abnormal_pause_print = false;
bool aic_detection_switch_sending = false;
bool aic_light_switch_sending = false;
bool aic_function_switch = false; // AI炒面检测
extern int detection_frequency_sec;
bool foreign_detection_switch = false;// AI异物检测
bool calibration_switch = false;// 打印校准
static uint32_t idle_timer_tick = 0;
double material_detection_e_length; // 断料检测长度
int default_gcode_copy_flag = 0; // 默认gcode复制标志位
#define DEFAULT_GCODE_NUM 7
static char default_gcode_name[DEFAULT_GCODE_NUM][128] = {"ECC_0.4_Vase_PLA0.2_4h11m.gcode",
                                                                                                                        "ECC_0.4_The Buddha_PLA0.2_24m17s.gcode",
                                                                                                                        "ECC_0.4_Scraper_PLA0.2_1h10m.gcode",
                                                                                                                        "ECC_0.4_EiffelTower_PLA0.2_2h6m.gcode",
                                                                                                                        "ECC_0.4_CC MINI_PLA0.2_2h0m.gcode",
                                                                                                                        "ECC_0.4_CC Cover Holder_PLA0.2_1h30m.gcode",
                                                                                                                        "ECC_0.4_3DBenchy_PLA0.25_15m29s.gcode"};

extern ConfigParser *get_sysconf();

static void app_get_text_size(lv_point_t *size, lv_obj_t *obj)
{
    int res = 0;
    lv_label_t *label = (lv_label_t *)obj;
    if (label->text == NULL)
        return;

    lv_area_t txt_coords;
    lv_obj_get_content_coords(obj, &txt_coords);
    lv_coord_t max_w = lv_area_get_width(&txt_coords);
    const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
    lv_coord_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);
    lv_coord_t letter_space = lv_obj_get_style_text_letter_space(obj, LV_PART_MAIN);

    lv_text_flag_t flag = LV_TEXT_FLAG_NONE;
    if (label->recolor != 0)
        flag |= LV_TEXT_FLAG_RECOLOR;
    if (label->expand != 0)
        flag |= LV_TEXT_FLAG_EXPAND;
    if (lv_obj_get_style_width(obj, LV_PART_MAIN) == LV_SIZE_CONTENT && !obj->w_layout)
        flag |= LV_TEXT_FLAG_FIT;
    lv_txt_get_size(size, label->text, font, letter_space, line_space, max_w, flag);
}

int app_get_text_height(lv_obj_t *obj)
{
    lv_point_t size;
    app_get_text_size(&size, obj);
    return size.y;
}

int app_get_text_width(lv_obj_t *obj)
{
    lv_point_t size;
    app_get_text_size(&size, obj);
    return size.x;
}

static int app_get_item_align_offset_x(lv_obj_t *btn_obj, lv_obj_t *img_obj, lv_obj_t *label_obj, uint16_t interval, int align_target_index)
{
    int com_offset = (lv_obj_get_style_width(btn_obj, LV_PART_MAIN) - lv_obj_get_self_width(img_obj) - app_get_text_width(label_obj) - interval) / 2;
    // LOG_I("com_offset:%d\n", com_offset);
    if (align_target_index == LEFT_IMG_IN_BTN)
        return com_offset;
    else if (align_target_index == RIGHT_LABEL_IN_BTN)
        return lv_obj_get_self_width(img_obj) + com_offset + interval;
    else
        return 0;
}

/**
 * @brief
 *
 * @param widget_list
 * @param widget_index
 * @param interval 两个控件间的距离
 */
void app_set_widget_item2center_align(widget_t **widget_list, int widget_index, int interval)
{
    uint16_t offset_x = 0;
    offset_x = app_get_item_align_offset_x(widget_list[widget_index]->obj_container[0], widget_list[widget_index]->obj_container[1], widget_list[widget_index]->obj_container[2], interval, RIGHT_LABEL_IN_BTN);
    lv_obj_align(widget_list[widget_index]->obj_container[2], LV_ALIGN_LEFT_MID, offset_x, 0);
    offset_x = app_get_item_align_offset_x(widget_list[widget_index]->obj_container[0], widget_list[widget_index]->obj_container[1], widget_list[widget_index]->obj_container[2], interval, LEFT_IMG_IN_BTN);
    lv_obj_align(widget_list[widget_index]->obj_container[1], LV_ALIGN_LEFT_MID, offset_x, 0);
}

// label设置跑马灯
void label_set_horse_race_lamp(lv_obj_t *label, int horse_race_lamp_width, bool text_align_center)
{
    lv_obj_set_width(label, horse_race_lamp_width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    if (text_align_center)
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    else
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
}

void keyboard_text_check(keyboard_t *keyboard, const char *text)
{
    bool all_space = true;
    if (keyboard != NULL)
    {
        for (int i = 0; i < strlen(keyboard->text); i++)
        {
            if (keyboard->text[i] != ' ')
            {
                all_space = false;
                break;
            }
        }

        if (all_space)
            keyboard->text[0] = '\0';
    }
}

void beeper_ui_handler(lv_event_t *e)
{
    static uint64_t start_tick = 0;
    if ((utils_get_current_tick() - start_tick < 0.3 * 1000) ||
        (get_sysconf()->GetBool("system", "keying_sound", false) == false))
        return;

    if (lv_event_get_current_target(e) == NULL)
        return;
    widget_t *widget = (widget_t *)lv_obj_get_user_data(lv_event_get_current_target(e));
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        switch (widget->window_index)
        {
        case WINDOW_ID_TOP:
            switch (widget->header.index)
            {
            case WIDGET_ID_TOP_BTN_BACKGROUND:
                return;
                break;
            }
            break;
        case WINDOW_ID_CONTROL:
            switch (widget->header.index)
            {
            case WIDGET_ID_CONTROL_CONTAINER_PRINT_SPEED:
            case WIDGET_ID_CONTROL_CONTAINER_DIGITAL_KEYBOARD:
            case WIDGET_ID_CONTROL_CONTAINER_MASK:
                return;
                break;
            }
            break;
        case WINDOW_ID_EXPLORER:
            switch (widget->header.index)
            {
            case WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB:
            case WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY:
                return;
                break;
            }
            break;
        case WINDOW_ID_FILE_INFO:
            switch (widget->header.index)
            {
            case WIDGET_ID_FILE_INFO_CONTAINER_MASK:
                return;
                break;
            }
            break;
        case WINDOW_ID_ROUTINE_MSGBOX:
        case WINDOW_ID_OVER_MSGBOX:
        case WINDOW_ID_DOUBLE_MSGBOX:
        case WINDOW_ID_SINGLE_MSGBOX:
            switch (widget->header.index)
            {
            case 0: // mask
                return;
                break;
            }
            break;
        case WINDOW_ID_VERSION_MSGBOX:
            switch (widget->header.index)
            {
            case WIDGET_ID_VERSION_MSGBOX_CONTAINER_MASK:
            case WIDGET_ID_VERSION_MSGBOX_CONTAINER_CONTENT:
                return;
                break;
            }
            break;
        case WINDOW_ID_KEYBOARD:
            switch (widget->header.index)
            {
            case WIDGET_ID_KEYBOARD_CONTAINER_ALPHABET:
            case WIDGET_ID_KEYBOARD_CONTAINER_SYMBOL:
                return;
                break;
            }
            break;
        case WINDOW_ID_SETTING:
            switch (widget->header.index)
            {
            case WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING:
                return;
                break;
            }
            break;
        case WINDOW_ID_AUTO_LEVEL:
            switch (widget->header.index)
            {
            case WIDGET_ID_AUTO_LEVEL_BTN_BG_CONTAINER:
            case WIDGET_ID_AUTO_LEVEL_CONTAINER_LIST:
                return;
                break;
            }
            break;
        case WINDOW_ID_ONE_CLICK_DETECTION:
            switch (widget->header.index)
            {
            case WIDGET_ID_ONE_CLICK_DETECTION_BTN_FORBIDDEN_LEFT_BAR:
                return;
                break;
            }
            break;
        case WINDOW_ID_PID_VIBRATION:
            switch (widget->header.index)
            {
            case WIDGET_ID_PID_VIBRATION_CONTAINER_BG_DETECTION_CONTAINER:
                return;
                break;
            }
            break;
        case WINDOW_ID_LANGUAGE:
            switch (widget->header.index)
            {
            case WIDGET_ID_LANGUAGE_CONTAINER_MASK:
                return;
                break;
            }
            break;
        case WINDOW_ID_DEVICE_INSPECTION:
            switch (widget->header.index)
            {
            case WIDGET_ID_DEVICE_INSPECTION_CONTAINER_MASK:
                return;
                break;
            }
            break;
        case WINDOW_ID_TIMEZONE:
            switch (widget->header.index)
            {
            case WIDGET_ID_TIMEZONE_CONTAINER_LIST:
                return;
                break;
            }
            break;
        case WINDOW_ID_CAMERA:
            switch (widget->header.index)
            {
            case WIDGET_ID_CAMERA_CONTAINER_LIST:
                return;
                break;
            }
            break;
        case WINDOW_ID_NETWORK:
            switch (widget->header.index)
            {
            case WINDOW_ID_NETWORK_CONTAINER_PASSWORD_PAGE:
            case WINDOW_ID_NETWORK_CONTAINER_LIST:
            case WIDGET_ID_NETWORK_CONTAINER_PASSWORD_KEYBOARD:
                return;
                break;
            }
            break;
        case WINDOW_ID_PRINTFEED:
            switch (widget->header.index)
            {
            case WIDGET_ID_PRINTFEED_BTN_CONTAINER:
                return;
                break;
            }
            break;
        }

        start_tick = utils_get_current_tick();
        set_beeper_status(BEEPER_SHORT_YET, 10);
    }
}

void extruder_retime_handler(lv_event_t *e)
{
   switch (lv_event_get_code(e))
    {
    case LV_EVENT_PRESSED:
    case LV_EVENT_PRESSING:
    case LV_EVENT_PRESS_LOST:
    case LV_EVENT_SHORT_CLICKED:
    case LV_EVENT_LONG_PRESSED:
    case LV_EVENT_LONG_PRESSED_REPEAT:
    case LV_EVENT_CLICKED:
    case LV_EVENT_RELEASED:
        app_update_idle_timer_tick();
        break;
    }
}

srv_state_t app_srv_state[2];
int app_srv_state_changed = 0;
pthread_mutex_t app_srv_state_mutex;

static void srv_state_subscribe_callback(const char *name, void *context, int msg_id, void *msg, uint32_t msg_len)
{
    srv_state_res_t res;
    srv_state_t *backup = &app_srv_state[1];
    switch (msg_id)
    {
    case SRV_STATE_MSG_ID_STATE:
    {
        // LOG_I("srv_state update\n");
        // 拷贝至备份区
        pthread_mutex_lock(&app_srv_state_mutex);
        simple_bus_request("srv_state", SRV_STATE_SRV_ID_STATE, NULL, &res);
        memcpy(backup, &res.state, sizeof(*backup));
        app_srv_state_changed = 1;
        pthread_mutex_unlock(&app_srv_state_mutex);
    }
    break;
    default:
        break;
    }
}

srv_state_t *app_get_srv_state(void)
{
    return &app_srv_state[0];
}

void app_update_srv_state(void)
{
    // 检查备份区状态是否更新,若更新拷贝至前台
    pthread_mutex_lock(&app_srv_state_mutex);
    if (app_srv_state_changed)
    {
        memcpy(&app_srv_state[0], &app_srv_state[1], sizeof(app_srv_state[0]));
        app_srv_state_changed = 0;
    }
    pthread_mutex_unlock(&app_srv_state_mutex);
}
#if CONFIG_SUPPORT_OTA
#include "hl_net.h"
enum
{
    FPO_STEP_INIT = 0,
    FPO_STEP_START,
    FPO_STEP_GET_SYS_V,
    FPO_STEP_RESULT,
    FPO_STEP_FAILED,
};
static int app_ota_state = FPO_STEP_INIT;
static bool poweron_check_enable = false;
static uint64_t _delay_tick = 1000;
static int retry_time = 3;

/**
 * OTA 上电检测流程
 */
void app_ota_version_check_loop()
{
    if (hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) == 1 && get_sysconf()->GetInt("system", "wifi", 0) && app_ota_state == FPO_STEP_INIT && app_print_get_print_state() == false)
    {
        // 触发开机检查更新
        app_ota_state = FPO_STEP_START;
    }

    switch (app_ota_state)
    {
        case FPO_STEP_INIT:
            break;
        case FPO_STEP_START:
            {
                // task start
                if (retry_time <= 0)
                {
                    app_ota_state = FPO_STEP_FAILED;
                }
                else
                {
                    if (_delay_tick == 0 && ota_get_info_request(OTA_FIREMARE_CH_SYS) == 0)
                    {
                        app_ota_state = FPO_STEP_GET_SYS_V;
                    }
                    else
                    {
                        // 延时,刚连上网 DNS 解析失败
                        if ((utils_get_current_tick() - _delay_tick) > 1 * 1000)
                        {
                            _delay_tick = 0;
                        }
                    }
                }
            }
            break;
        case FPO_STEP_GET_SYS_V:
            {
                _ota_info info = {0};
                OTA_API_ST_t rv = ota_get_info_result(OTA_FIREMARE_CH_SYS, &info);
                switch (rv)
                {
                    case OTA_API_STAT_SUCCESS:
                        LOG_I("get sys ota success\n");
                        app_ota_state = FPO_STEP_RESULT;
                        break;
                    case OTA_API_STAT_FAILED:
                    case OTA_API_STAT_TIMEOUT:
                        LOG_I("ota task stage3 failed\n");
                        // 失败了重试
                        app_ota_state = FPO_STEP_START;
                        _delay_tick = utils_get_current_tick();
                        retry_time--;
                        break;
                    case OTA_API_STAT_INIT:
                    case OTA_API_STAT_REQUESTING:
                    default:
                        break;
                }
            }
            break;
        case FPO_STEP_FAILED:
        case FPO_STEP_RESULT: // 获取到服务区上固件版本信息
            {
                if (!hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) || !get_sysconf()->GetInt("system", "wifi", 1))
                {
                    // after newwork disconnected or wifi turned off, try to update ota info
                    LOG_I("ota: try to update ota info next time when wifi reconnected.\n");
                    app_ota_state = FPO_STEP_INIT;
                    retry_time = 3;
                }
            }
            break;
    } // end of switch (app_ota_state)
}
#endif

#if CONFIG_SUPPORT_AIC

// 摄像头串口回复
static void app_ui_camera_msg_handler_cb(const void *data, void *user_data)
{
    aic_ack_t *resp = (aic_ack_t *)data;
    if (resp->is_timeout)
        return;

    switch (resp->cmdid)
    {
    case AIC_CMD_AI_FUNCTION:
        if (resp->body_size == 2)
            aic_detection_switch_flag = resp->body[1];
        aic_detection_switch_sending = false;
        break;
    case AIC_CMD_CAMERA_LIGHT:
        if (resp->body_size == 2)
            aic_light_switch_flag = resp->body[1];
        aic_light_switch_sending = false;
        break;
    case AIC_CMD_NOT_CAPTURE:
    case AIC_CMD_MAJOR_CAPTURE:
        if (app_print_get_print_state() && aic_function_switch)
        {
            if (resp->body_size == 2)
            {
                switch (resp->body[1])
                {
                case AIC_GET_STATE_NOT_CHOW_MEIN:
                    break;
                case AIC_GET_STATE_CHOW_MEIN:
                    aic_print_info.monitor_abnormal_state = true;
                    aic_print_info.monitor_abnormal_index = 1;
                    if (ui_get_window_index() != WINDOW_ID_PRINT)
                    {
                        ui_set_window_index(WINDOW_ID_PRINT, NULL);
                        app_top_update_style(window_get_top_widget_list());
                    }
                    break;
                case AIC_GET_STATE_PRINTING_AI_FUNCTION_OFF:
                    ai_camera_send_cmd_handler(AIC_CMD_AI_FUNCTION, AIC_CMD_CARRY_ON_AI_FUNCTION); // 常开,使用flag控制炒面及异物
                    break;
                }
            }
        }
        break;
    case AIC_CMD_FOREIGN_CAPTURE:
        if (app_print_get_print_state() && foreign_detection_switch)
        {
            if (resp->body_size == 2)
            {
                switch (resp->body[1])
                {
                case AIC_GET_STATE_NO_FOREIGN_BODY:
                    break;
                case AIC_GET_STATE_HAVE_FOREIGN_BODY:
                    aic_print_info.monitor_abnormal_state = true;
                    aic_print_info.monitor_abnormal_index = 2;
                    if (ui_get_window_index() != WINDOW_ID_PRINT)
                    {
                        ui_set_window_index(WINDOW_ID_PRINT, NULL);
                        app_top_update_style(window_get_top_widget_list());
                    }
                    break;
                }
            }
        }
        break;
    }
}

// 获取AI检测结果
static void ai_camera_send_cmd_update(void)
{
    static uint64_t start_tick = 0;
    static uint64_t monitor_tick = 0;
    static uint64_t heartbeat_tick = 0;
    static int start_monitor_number = 0;

#if 0 // 测试串口命令
    aic_print_info.monitor_abnormal_state = true;
    aic_print_info.monitor_abnormal_index = 2;
    aic_abnormal_pause_print = false;

    if (utils_get_current_tick() - start_tick > 30 * 1000)
    {
        start_tick = utils_get_current_tick();
        ai_camera_send_cmd_handler(AIC_CMD_FOREIGN_CAPTURE, AIC_CMD_CARRY_ACTIVATE_AI_MONITOR);
        ai_camera_send_cmd_handler(AIC_CMD_FOREIGN_CAPTURE, AIC_CMD_CARRY_NORMAL_AI_MONITOR);
    }
    return;
#endif

    if (app_print_get_print_state() && aic_function_switch == true && aic_print_info.print_state == 4)
    {
        // 炒面监测 开始打印才检测
        if (utils_get_current_tick() - start_tick > 60 * 1000 && Printer::GetInstance()->m_print_stats->m_print_stats.state == PRINT_STATS_STATE_PRINTING)
        {
            start_tick = utils_get_current_tick();
            if (detection_frequency_sec)
            {
                ai_camera_send_cmd_handler(AIC_CMD_MAJOR_CAPTURE, AIC_CMD_CARRY_ACTIVATE_CHOW_MEIN);
                ai_camera_send_cmd_handler(AIC_CMD_MAJOR_CAPTURE, AIC_CMD_CARRY_NORMAL_CHOW_MEIN);
            }
            else
            {
                ai_camera_send_cmd_handler(AIC_CMD_NOT_CAPTURE, AIC_CMD_CARRY_ACTIVATE_CHOW_MEIN);
                ai_camera_send_cmd_handler(AIC_CMD_NOT_CAPTURE, AIC_CMD_CARRY_NORMAL_CHOW_MEIN);
            }
        }
    }
    else
    {
        // 心跳
        if (utils_get_current_tick() - heartbeat_tick > 10 * 1000)
        {
            heartbeat_tick = utils_get_current_tick();
            ai_camera_send_cmd_handler(AIC_CMD_GET_STATUS, AIC_CMD_CARRY_NULL);
        }
    }
}

// 获取摄像头OTA版本
void app_ota_aic_version_check_info(void)
{
#if CONFIG_SUPPORT_OTA
    static bool ota_get_aic_info_state = false;
    if (hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) && ota_get_aic_info_state == false &&
        strcmp(aic_get_version(), "-") != 0 && app_print_get_print_state() == false)
    {
        static _ota_info aic_info;
        switch (ota_get_info_result(OTA_FIREMARE_CH_AIC, &aic_info))
        {
        case OTA_API_STAT_SUCCESS:
            ota_get_aic_info_state = true;
            if (strcmp(aic_get_version(), aic_info.version) != 0 && strlen(aic_info.version) > 0 && aic_get_online() && hl_camera_get_exist_state())
            {
                aic_version_state = MSGBOX_TIP_AIC_NEW_VERSION;
                aic_version_newer = 1;
            }
            LOG_I("AIC current version:%s\n", aic_get_version());
            LOG_I("AIC  newest version:%s\n", aic_info.version);
            break;
        case OTA_API_STAT_INIT:
        case OTA_API_STAT_FAILED:
        case OTA_API_STAT_TIMEOUT:
            ota_get_info_request(OTA_FIREMARE_CH_AIC);
            break;
        }
    }
#endif
}
#endif

#if CONFIG_SUPPORT_TLP
// 测试延时摄影
static void aic_tlp_test_generate_tlp(void)
{
#define TOTAL_TEST_FRAME 1200
#define CAPTURE_TEST_TIME 0.5

    char tlp_test_path[128];
    static uint64_t start_tick = 0;
    static int frame_tick = -1;
    static bool state = true;
    static int number = 0;

    if (start_tick == 0)
        start_tick = utils_get_current_tick();

    if (frame_tick == -1 && utils_get_current_tick() - start_tick > 5 * 1000)
    {
        sprintf(tlp_test_path, "test_tlp_%d", TOTAL_TEST_FRAME);
        aic_tlp_init(tlp_test_path);
        frame_tick = 0;
        state = true;
        number++;
    }

    if (frame_tick != -1 && utils_get_current_tick() - start_tick > CAPTURE_TEST_TIME * 1000 && frame_tick < TOTAL_TEST_FRAME)
    {
        start_tick = utils_get_current_tick();
        frame_tick++;
        aic_tlp_capture(frame_tick);
        printf("number:%d\n", number);
    }

    if (frame_tick >= TOTAL_TEST_FRAME && state)
    {
        state = false;
        aic_tlp_complte(1, frame_tick);
        frame_tick = -1;
    }
}
#endif

// 非打印界面的 断料检测
static void material_detection_check(void)
{
    extern uint8_t material_break_detection_step;
    if (app_print_get_print_state() == true && ui_get_window_index() != WINDOW_ID_PRINT&& material_break_detection_step == 0 && Printer::GetInstance()->m_change_filament->is_feed_busy() == false)
    {
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
                    LOG_I("material_detection triggered, e_length: %f, used: %f\n", material_detection_e_length, filament_used);
                    ui_set_window_index(WINDOW_ID_PRINT, NULL);
                    app_top_update_style(window_get_top_widget_list());
                }
            }
            else
            {
                material_detection_e_length = 0;
            }
        }
    }
}

// 开机后第一次进入网络界面时才加载wifi列表太慢，此处提前扫描wifi列表
static void app_network_early_scan(void)
{   
    static int early_scan = 0;
    static uint64_t tick = 0;
    if(early_scan == 0)
    {
        if(hl_wlan_get_scan_result_numbers() > 0)   //成功加载过wifi列表后不再扫描
        {
            // LOG_I("-----stop early wlan scan\n");
            early_scan = 1;
            return;
        }
        if(utils_get_current_tick() - tick > 1000)
        {
            if(hl_wlan_get_status() == HL_WLAN_STATUS_DISCONNECTED)
            {
                // LOG_I("-----early wlan scan\n");
                hl_wlan_scan();
            }
            tick = utils_get_current_tick();
        }
    }
}

void app_update()
{
//     app_update_srv_state();
//     app_msgbox_run();
//     material_detection_check();
// #if CONFIG_SUPPORT_NETWORK
//     app_network_early_scan();
// #endif
// #if CONFIG_SUPPORT_AIC
//     ai_camera_send_cmd_update();
//     // aic_tlp_test_generate_tlp();
//     aic_tlp_printing_generate_tlp();
//     app_ota_aic_version_check_info();
// #endif
// #if CONFIG_SUPPORT_OTA
//     app_ota_version_check_loop();
// #endif
// #if CONFIG_TEST_SCREEN_UPDATE_TIME
//     static uint64_t start_tick = 0;
//     static uint32_t screen_update_number = 0;
//     static uint32_t test_screen_time = 60;
//     screen_update_number++;
//     if (utils_get_current_tick() - start_tick > test_screen_time * 1000)
//     {
//         LOG_I("window_index:%d test_screen_time:%ds screen_update_number:%d screen_update_time:%lldms\n\n", ui_get_window_index(), test_screen_time, screen_update_number, (utils_get_current_tick() - start_tick) / screen_update_number);
//         screen_update_number = 0;
//         start_tick = utils_get_current_tick();
//     }
// #endif
}


static void lv_timer_app_update_srv_state(lv_timer_t *timer)
{
    app_update_srv_state();
}
static void lv_timer_app_msgbox_run(lv_timer_t *timer)
{
    app_msgbox_run();
}
static void lv_timer_material_detection_check(lv_timer_t *timer)
{
    material_detection_check();
}
#if CONFIG_SUPPORT_NETWORK
static void lv_timer_app_network_early_scan(lv_timer_t *timer)
{
    app_network_early_scan();
}
#endif
#if CONFIG_SUPPORT_AIC
static void lv_timer_ai_camera_send_cmd_update(lv_timer_t *timer)
{
    ai_camera_send_cmd_update();
}

static void lv_timer_app_ota_aic_version_check_info(lv_timer_t *timer)
{
    app_ota_aic_version_check_info();
}
#endif

#if CONFIG_SUPPORT_TLP
static void lv_timer_aic_tlp_printing_generate_tlp(lv_timer_t *timer)
{
    aic_tlp_printing_generate_tlp();
}
#endif

#if CONFIG_SUPPORT_OTA
static void lv_timer_app_ota_version_check_loop(lv_timer_t *timer)
{
    app_ota_version_check_loop();
}
#endif
static void lv_timer_app_compact_memory(lv_timer_t *timer)
{
    LOG_I("compact_memory start\n");
    system("echo 1 > /proc/sys/vm/compact_memory");
    system("echo 3 > /proc/sys/vm/drop_caches");
    LOG_I("compact_memory finish\n");
}
void app_update_idle_timer_tick(void)
{
    idle_timer_tick = utils_get_current_tick();
}
static void lv_timer_update_idle_timer(lv_timer_t *timer)
{
    if (fabs(Printer::GetInstance()->m_printer_extruder->m_heater->m_target_temp) > 1e-10 ||
        fabs(Printer::GetInstance()->m_bed_heater->m_heater->m_target_temp) > 1e-10)
    {
        if (idle_timer_tick != 0 && utils_get_current_tick() - idle_timer_tick > 60 * 1000)
        {
            idle_timer_tick = 0;
            char cmd[MANUAL_COMMAND_MAX_LENGTH];
            sprintf(cmd, "UPDATE_IDLE_TIMER ACTIVE_TIME=%.3f", (utils_get_current_tick() - idle_timer_tick) / 1000.0 - 60);
            manual_control_sq.push(cmd);
            Printer::GetInstance()->manual_control_signal();
        }
    }
}
int app_init(void)
{

    tr_font_new(10, 0);
    tr_font_new(11, 0);
    tr_font_new(12, 0);
    tr_font_new(14, 0);
    tr_font_new(16, 0);
    tr_font_new(18, 0);
    tr_font_new(20, 0);
    tr_font_new(22, 0);
    tr_font_new(24, 0);
    tr_font_new(26, 0);
    tr_font_new(28, 0);
    tr_font_new(30, 0);
    tr_font_new(32, 0);

    simple_bus_subscribe("srv_state", NULL, srv_state_subscribe_callback);
    tr_set_language(get_sysconf()->GetInt("system", "language", 0));
    material_detection_switch = get_sysconf()->GetBool("system", "material_detection", false);
    get_sysconf()->SetBool("system", "keying_sound", false); // 取消按键声音开关,默认关
    silent_mode_switch = get_sysconf()->GetBool("system", "silent_mode", false);
    // ui_cb[set_silent_mode_cb](&silent_mode_switch);
    resume_print_switch = get_sysconf()->GetBool("system", "power_off_resume_print", false);
    LOG_I("resume_print_switch:%d\n", resume_print_switch);
    Printer::GetInstance()->m_break_save->m_break_save_enable = resume_print_switch;
    luminance_state = get_sysconf()->GetInt("system", "luminance_state", 4);
    #if CONFIG_BOARD_E100 == BOARD_E100
    photography_switch = get_sysconf()->GetBool("system", "tlp_switch", false);
    #endif
    detection_frequency_sec = get_sysconf()->GetInt("system", "detection_frequency_sec", 0);
    aic_abnormal_pause_print = get_sysconf()->GetBool("system", "aic_abnormal_pause_print", false);
    // aic_function_switch = get_sysconf()->GetBool("system", "aic_function_switch", false);
    // foreign_detection_switch = get_sysconf()->GetBool("system", "foreign_detection_switch", false);
    default_gcode_copy_flag = get_sysconf()->GetInt("system", "default_gcode_copy_flag", 0);

    if (!default_gcode_copy_flag)
    {
        for (uint8_t i = 0; i < DEFAULT_GCODE_NUM; i++) //恢复默认gcode文件
        {
            char resources_path[128] = {0};
            char target_path[128] = {0};
            snprintf(resources_path, sizeof(resources_path), "%s/%s", "/app/resources/gcode", default_gcode_name[i]);
            snprintf(target_path, sizeof(target_path), "%s/%s", USER_RESOURCE_PATH, default_gcode_name[i]);
            if (access(resources_path, F_OK) == 0)
            {
                hl_system("cp \"%s\" \"%s\"", resources_path, target_path);
                LOG_I("default_gcode_name = %s\n", default_gcode_name[i]);
            }
        }
        if (access("/app/resources/file_info", F_OK) == 0)
        {
            hl_system("cp -r %s %s", "/app/resources/file_info/", USER_RESOURCE_PATH);
        }
        FileManager::GetInstance()->LoadFileInfo();
        default_gcode_copy_flag = 1;
        get_sysconf()->SetInt("system", "default_gcode_copy_flag", default_gcode_copy_flag); //设置默认gcode复制标志位
    }
    calibration_switch = get_sysconf()->GetBool("system", "calibration_switch", false);

    /* 新摄像头上电控制摄像头灯 */
    // 摄像头灯开机默认关闭
    get_sysconf()->SetBool("system", "camera_light_switch", false);
    if (hl_camera_get_exist_state() != false)
    {
        camera_control_light(0);
        if(Printer::GetInstance()->m_box_led != nullptr)
            Printer::GetInstance()->m_box_led->control_light(0);
    }

#if CONFIG_SUPPORT_AIC
    ai_camera_resp_cb_register(app_ui_camera_msg_handler_cb, NULL);
    ai_camera_send_cmd_handler(AIC_CMD_AI_FUNCTION, AIC_CMD_CARRY_ON_AI_FUNCTION); // 常开,使用flag控制炒面及异物
    if (aic_function_switch || foreign_detection_switch)
        ai_camera_send_cmd_handler(AIC_CMD_CAMERA_LIGHT, AIC_CMD_CARRY_ON_LED); // 灯光与AI功能有关联,AI功能flag开,灯光开
    ai_camera_send_cmd_handler(AIC_CMD_CAMERA_LIGHT, AIC_CMD_CARRY_GET_LED_STATE);
#endif

    // 初始化对话框管理器
    app_msgbox_init();
    ui_register_update_callback(app_update);
    window_register_callback(WINDOW_ID_TOP, app_top_callback);
    window_register_callback(WINDOW_ID_MAIN, app_main_callback);
    window_register_callback(WINDOW_ID_CONTROL, app_control_callback);
    window_register_callback(WINDOW_ID_EXPLORER, app_explorer_callback);
    window_register_callback(WINDOW_ID_PRINT, app_print_callback);
    window_register_callback(WINDOW_ID_SETTING, app_setting_callback);
    window_register_callback(WINDOW_ID_CALIBRATION, app_calibration_callback);
    window_register_callback(WINDOW_ID_LANGUAGE, app_language_callback);
    window_register_callback(WINDOW_ID_DEVICE_INSPECTION, app_device_inspection_callback);
    window_register_callback(WINDOW_ID_LAMPLIGHT_LANGUAGE, app_lamplight_language_callback);
    window_register_callback(WINDOW_ID_CUTTER, app_cutter_callback);
    window_register_callback(WINDOW_ID_TIMEZONE, app_timezone_callback);
    window_register_callback(WINDOW_ID_NETWORK, app_network_callback);
    window_register_callback(WINDOW_ID_INFO, app_info_callback);
    window_register_callback(WINDOW_ID_CAMERA, app_camera_callback);
    window_register_callback(WINDOW_ID_AUTO_LEVEL, app_auto_level_callback);
    window_register_callback(WINDOW_ID_PID_VIBRATION, app_pid_vibration_callback);
    window_register_callback(WINDOW_ID_ONE_CLICK_DETECTION, app_one_click_detection_callback);
    window_register_callback(WINDOW_ID_FAN, app_fan_callback);
    window_register_callback(WINDOW_ID_FEED, app_feed_callback);
    window_register_callback(WINDOW_ID_RESET_FACTORY, app_reset_factory_callback);
    window_register_callback(WINDOW_ID_DETECT_UPDATE, app_detect_update_callback);
    window_register_callback(WINDOW_ID_PRINT_PLATFORM, app_print_platform_callback);
#if ENABLE_MANUTEST
    window_register_callback(WINDOW_ID_MANU_TEST, app_manu_callback);
#endif

#if ENABLE_MANUTEST
        ui_set_window_index(WINDOW_ID_MANU_TEST, NULL);
        LOG_I("Set window id for manufacturing test.\n");
#else
    if (get_sysconf()->GetInt("system", "boot", 0) == 0)
    {
        ui_set_window_index(WINDOW_ID_LANGUAGE, NULL);
    }
    else
    {
        ui_set_window_index(WINDOW_ID_MAIN, NULL);
        app_top_update_style(window_get_top_widget_list());
    }
#endif

    ota_firmware_file_remove();     //上电删除OTA升级相关文件，防止OTA时断电未执行删除操作

    {
        get_sysconf()->SetBool("system", "cutting_mode", 1);      //切刀状态设置为自动 CUTTER_CHOOSE_AUTO
        get_sysconf()->WriteIni(SYSCONF_PATH);
        
        char control_command[MANUAL_COMMAND_MAX_LENGTH];
        sprintf(control_command, "CHANGE_FILAMENT_SET_ACTIVE ACTIVE=%d", get_sysconf()->GetBool("system", "cutting_mode", 0));
        ui_cb[manual_control_cb](control_command);
    }

    if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 0)
    {
        char cmd[MANUAL_COMMAND_MAX_LENGTH] = "BED_MESH_SET_INDEX TYPE=standard INDEX=0";
        ui_cb[manual_control_cb](cmd);
    }
    else if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 1)
    {
        char cmd[MANUAL_COMMAND_MAX_LENGTH] = "BED_MESH_SET_INDEX TYPE=enhancement INDEX=0";
        ui_cb[manual_control_cb](cmd);
    }

    break_resume_msgbox_callback(resume_print_switch);

// 优化代码，但暂未启用
    lv_timer_create(lv_timer_app_update_srv_state, 100, NULL);
    lv_timer_create(lv_timer_app_msgbox_run, 1, NULL);
    lv_timer_create(lv_timer_material_detection_check, 500, NULL);
#if CONFIG_SUPPORT_NETWORK
    lv_timer_create(lv_timer_app_network_early_scan, 1000, NULL);
#endif
#if CONFIG_SUPPORT_AIC
    lv_timer_create(lv_timer_ai_camera_send_cmd_update, 1000, NULL);
    lv_timer_create(lv_timer_app_ota_aic_version_check_info, 1000, NULL);
#endif
#if CONFIG_SUPPORT_TLP
    lv_timer_create(lv_timer_aic_tlp_printing_generate_tlp, 100, NULL);
#endif
#if CONFIG_SUPPORT_OTA
    lv_timer_create(lv_timer_app_ota_version_check_loop, 1000, NULL);
#endif
    // lv_timer_create(lv_timer_app_compact_memory, 60000, NULL);
    lv_timer_create(lv_timer_update_idle_timer, 5 * 1000, NULL);
    return 0;
}

void sysconf_init()
{
    sysconfig = new ConfigParser(SYSCONF_PATH);
    update_screen_off_time();
}

ConfigParser *get_sysconf()
{
    return sysconfig;
}

void key_tone()
{
    if (keying_sound_switch)
    {
        // set_beeper_status(BEEPER_SHORT_YET, 10);
    }
}

void system_reboot(int delay, bool backlight_on)
{
    if (!backlight_on)
    {
        system("echo setbl > /sys/kernel/debug/dispdbg/command");
        system("echo lcd0 > /sys/kernel/debug/dispdbg/name");
        system("echo 0 > /sys/kernel/debug/dispdbg/param");
        system("echo 1 > /sys/kernel/debug/dispdbg/start");
    }
    if (delay)
    {
        char reboot_cmd[32];
        sprintf(reboot_cmd, "reboot -d %d &", delay);
        system(reboot_cmd);
    }
    else
    {
        system("reboot");
    }
}


void system_control_backlight(int pwm_num)
{
    char luminance_str[128] = {0};
    system("echo setbl > /sys/kernel/debug/dispdbg/command");
    system("echo lcd0 > /sys/kernel/debug/dispdbg/name");
    sprintf(luminance_str, "echo %d > /sys/kernel/debug/dispdbg/param", pwm_num);
    system(luminance_str);
    system("echo 1 > /sys/kernel/debug/dispdbg/start");
}

//获取屏幕是否息屏的变量 true息屏，false正常显示
bool screen_state_;

//获取息屏的状态
bool get_screen_state()
{
    return screen_state_;
}

//设置息屏的状态
void set_screen_state(bool state)
{
    screen_state_ = state;
}
#include "app_main.h"
#include "klippy.h"
#include "hl_wlan.h"
#include "hl_common.h"
#include "app_setting.h"
#include "ota.h"
#include "jenkins.h"
#include "ai_camera.h"
#include "hl_disk.h"
#include "hl_camera.h"
#include "app_camera.h"
#include "aic_tlp.h"
#define LOG_TAG "app_main"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

enum
{
    MSGBOX_TIP_HOT_BED_NO_HEAT = 0, // 热床未按预期加热
};

// extern bool model_light_swtich;
extern bool illumination_light_swtich;
// extern int aic_version_state;
extern ConfigParser *get_sysconf();
// static char aic_file_name[64];
// bool aic_update_fail_state = false;
// static bool aic_version_constraint_state = false;

static bool app_main_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_main_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static void app_main_update(widget_t **widget_list);
static void app_main_update_light(widget_t **widget_list);

static double hotbed_cur_temp = 0, extruder_cur_temp = 0;
static void app_main_callback_update(lv_timer_t *timer);
static lv_timer_t *app_main_callback_timer = NULL;

static void main_btn_container_get_image_src(widget_t **widget_list,  int index)
{
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
    lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_CONTAINER]->obj_container[1], ui_get_image_src(353));
#elif CONFIG_BOARD_E100 == BOARD_E100
    lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_CONTAINER]->obj_container[1], ui_get_image_src(index));
#endif
}

void app_main_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static bool light_total_swtich = false;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_obj_add_flag(widget_list[WIDGET_ID_MAIN_BTN_BOX_TEMPERATURE]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(widget_list[WIDGET_ID_MAIN_BTN_FAN]->obj_container[0], 86, 44);
        lv_obj_set_pos(widget_list[WIDGET_ID_MAIN_BTN_NETWORK]->obj_container[0], 22, 44);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_BTN_CONTAINER]->obj_container[2], "Centauri");
        main_btn_container_get_image_src(widget_list, 353);
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_BTN_CONTAINER]->obj_container[2], "Centauri Carbon");
#endif
        aic_light_switch_sending = false;
        lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_LABEL_IP]->obj_container[0], "IP:%s", "192.168.3.128");

        // app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_main_over_msgbox_callback, (void *)MSGBOX_TIP_HOT_BED_NO_HEAT);
        app_main_update(widget_list);
        if (app_main_callback_timer == NULL)
        {
            app_main_callback_timer = lv_timer_create(app_main_callback_update, 500, widget_list);
            lv_timer_ready(app_main_callback_timer);
        }
        break;
    case LV_EVENT_DESTROYED:
        if (app_main_callback_timer != NULL)
        {
            lv_timer_del(app_main_callback_timer);
            app_main_callback_timer = NULL;
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_MAIN_BTN_LIGHT:
#if 0
            light_total_swtich = !light_total_swtich;
            if (light_total_swtich)
            {
                // model_light_swtich = true;
                // manual_control_sq.push("SET_LED_led2 RED=1.0 GREEN=1.0 BLUE=1.0 WHITE=1.0 TRANSMIT=1");
                // Printer::GetInstance()->manual_control_signal();
                illumination_light_swtich = true;
                manual_control_sq.push("SET_LED_led1 RED=1 GREEN=1 BLUE=1 WHITE=1 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();
            }
            else
            {
                // model_light_swtich = false;
                // manual_control_sq.push("SET_LED_led2 RED=0.0 GREEN=0.0 BLUE=0 WHITE=0.0 TRANSMIT=1");
                // Printer::GetInstance()->manual_control_signal();
                illumination_light_swtich = false;
                manual_control_sq.push("SET_LED_led1 RED=0.0 GREEN=0.0 BLUE=0 WHITE=0.0 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();
            }
            if (light_total_swtich)
            {
                lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(217));
                main_btn_container_get_image_src(widget_list, 225);
            }
            else
            {
                lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(12));
                main_btn_container_get_image_src(widget_list, 1);
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
                app_main_update(widget_list);
            }
        }

            break;
        case WIDGET_ID_MAIN_BTN_SHOWER_TEMPERATURE:
        case WIDGET_ID_MAIN_BTN_HOT_BED:
            ui_set_window_index(WINDOW_ID_CONTROL, (void *)(widget->header.index));
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_MAIN_BTN_FAN:
            ui_set_window_index(WINDOW_ID_FAN, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static void app_main_callback_update(lv_timer_t *timer)
{
    widget_t **widget_list = (widget_t **)timer->user_data;
    app_main_update(widget_list);
}

static void app_main_update_light(widget_t **widget_list)
{
    // bool light_total_swtich = (model_light_swtich || illumination_light_swtich) ? true : false;
    bool light_total_swtich = illumination_light_swtich ? true : false;
    if (light_total_swtich == true && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1]), ui_get_image_src(217)) != 0)
    {
        lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(217));
        main_btn_container_get_image_src(widget_list, 225);
    }
    else if (light_total_swtich == false && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1]), ui_get_image_src(12)) != 0)
    {
        lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(12));
        main_btn_container_get_image_src(widget_list, 1);
    }
}

static void app_main_update(widget_t **widget_list)
{
    srv_state_t *ss = app_get_srv_state();
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_BTN_SHOWER_TEMPERATURE]->obj_container[2], "%d℃", (int)ss->heater_state[HEATER_ID_EXTRUDER].current_temperature);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_BTN_HOT_BED]->obj_container[2], "%d℃", (int)ss->heater_state[HEATER_ID_BED].current_temperature);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_BTN_BOX_TEMPERATURE]->obj_container[2], "%d℃", (int)ss->heater_state[HEATER_ID_BOX].current_temperature);

    hl_wlan_connection_t wlan_cur_connection = {0};

#if CONFIG_SUPPORT_AIC
    if (aic_light_switch_flag == AIC_GET_STATE_LED_ON && hl_camera_get_exist_state())
    {
        lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(217));
        main_btn_container_get_image_src(widget_list, 225);
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(12));
        main_btn_container_get_image_src(widget_list, 1);
    }
#else
        // lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(12));
        // main_btn_container_get_image_src(widget_list, 1);

        if (get_sysconf()->GetBool("system", "camera_light_switch", false) && hl_camera_get_exist_state())
        {
            lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(217));
            main_btn_container_get_image_src(widget_list, 225);
        }
        else
        {
            lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_LIGHT]->obj_container[1], ui_get_image_src(12));
            main_btn_container_get_image_src(widget_list, 1);
        }
#endif

#if CONFIG_SUPPORT_OTA && CONFIG_SUPPORT_AIC
    // 检测到强制升级摄像头ota
    static bool aic_version_constraint_init = true;
    if (aic_version_constraint_init)
    {
        aic_version_constraint_init = false;
        char path[PATH_MAX_LEN];
        strncpy(aic_file_name, get_sysconf()->GetString("system", "aic_version_name", "0").c_str(), sizeof(aic_file_name));
        hl_disk_get_default_mountpoint(HL_DISK_TYPE_EMMC, aic_file_name, path, sizeof(path));

        if (strcmp(aic_file_name, "0") != 0 && access(path, F_OK) == 0) // 执行强制摄像头ota
        {
            aic_version_constraint_state = true;
            app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_aic_version_update_msgbox_callback, NULL);
        }
    }

    // AI摄像头固件先于主板固件更新
    if (aic_version_state == MSGBOX_TIP_AIC_NEW_VERSION && aic_version_constraint_state == false)
    {
        aic_version_state = MSGBOX_TIP_AIC_VERSION_ALREADY_TIP;
        app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_aic_version_update_msgbox_callback, NULL);
    }

    // AI摄像头固件更新失败弹窗
    if (aic_update_fail_state)
    {
        aic_update_fail_state = false;
        app_msgbox_close_all_avtive_msgbox();
        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_aic_update_fail_routine_msgbox_callback, NULL);
    }
#endif

    /* check FW updating */
    static bool enable_updateFW_msgbox_pushing = 1;
    static uint64_t start_tick = hl_get_tick_ms();
    if (enable_updateFW_msgbox_pushing)
    {
        /* check if there is a newer version of FirmWare need to be updated */
        if (has_newer_FW())
        {
            LOG_D("[%s] detect newer firmware, go to upgrading window.\n", __FUNCTION__);
            app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_version_update_msgbox_callback, (void *)MSGBOX_TIP_VERSION_LOCAL_UPDATE);
            /* allow pushing updating window only once, so clean the enable flag after message window pushed */
            enable_updateFW_msgbox_pushing = 0;
        }
        #define OVER_TIME 60
        if (hl_tick_is_overtime(start_tick, hl_get_tick_ms(), OVER_TIME*1000))
        {
            LOG_D("[%s] start_tick:%llums curr_tick:%llums\n", __FUNCTION__, start_tick, hl_get_tick_ms());
            LOG_I("[%s] updating FW in the main window overtime, disable it.\n", __FUNCTION__);
            enable_updateFW_msgbox_pushing = 0;
        }
    }

    if (hl_wlan_get_status() == HL_WLAN_STATUS_CONNECTED && hl_wlan_get_connection(&wlan_cur_connection) != -1)
    {
        char ipaddr[16] = {0};
        hl_netif_get_ip_address(HL_NET_INTERFACE_WLAN, ipaddr, sizeof(ipaddr));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_LABEL_IP]->obj_container[0], "IP:%s", ipaddr);

        if (wlan_cur_connection.signal <= 100 / 3)
            lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_NETWORK]->obj_container[1], ui_get_image_src(19));
        else if (wlan_cur_connection.signal <= 200 / 3)
            lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_NETWORK]->obj_container[1], ui_get_image_src(20));
        else
            lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_NETWORK]->obj_container[1], ui_get_image_src(21));
    }
    else
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_MAIN_LABEL_IP]->obj_container[0], "");
        lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_NETWORK]->obj_container[1], ui_get_image_src(18));
    }

    { // 加热图标闪烁
        static uint64_t start_tick = 0;
        static bool image_state = false;
        if (utils_get_current_tick() - start_tick > 2 * 1000)
        {
            start_tick = utils_get_current_tick();
            image_state = !image_state;
        }

        if (ss->heater_state[HEATER_ID_EXTRUDER].target_temperature > 0) // 加热
        {
            if (image_state)
                lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_SHOWER_TEMPERATURE]->obj_container[1], ui_get_image_src(23));
            else
                lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_SHOWER_TEMPERATURE]->obj_container[1], ui_get_image_src(13));
        }
        else
        {
            lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_SHOWER_TEMPERATURE]->obj_container[1], ui_get_image_src(13));
        }

        if (ss->heater_state[HEATER_ID_BED].target_temperature > 0) // 加热
        {
            if (image_state)
                lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_HOT_BED]->obj_container[1], ui_get_image_src(24));
            else
                lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_HOT_BED]->obj_container[1], ui_get_image_src(14));
        }
        else
        {
            lv_img_set_src(widget_list[WIDGET_ID_MAIN_BTN_HOT_BED]->obj_container[1], ui_get_image_src(14));
        }
    }
}

static bool app_main_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_HOT_BED_NO_HEAT)
        {
            lv_img_set_src(widget_list[WIDGET_ID_OVER_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], tr(37));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：101,%s", tr(39));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_BTN_PARTICULARS]->obj_container[2], tr(43));
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_OVER_MSGBOX_BTN_PARTICULARS:
            if (tip_index == MSGBOX_TIP_HOT_BED_NO_HEAT)
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_main_routine_msgbox_callback, (void *)MSGBOX_TIP_HOT_BED_NO_HEAT);
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

static bool app_main_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_HOT_BED_NO_HEAT)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(40));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == MSGBOX_TIP_LOCAL_STORAGE_LACK)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(199));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
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
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT:
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

bool app_aic_update_fail_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;

            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(315));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
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
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

// bool app_aic_version_update_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
// {
// #if CONFIG_SUPPORT_OTA && CONFIG_SUPPORT_AIC
//     widget_t **widget_list = win->widget_list;
//     static _ota_info aic_info;
//     static uint64_t aic_burn_tick; // AI 摄像头假进度，按时间递增
//     static int aic_burn_process;   // AI 摄像头假进度
//     static uint64_t aic_timeout_tick; // AI 升级超时
//     static int aic_process_time;   // AI 摄像头时间进度
//     switch (lv_event_get_code((lv_event_t *)e))
//     {
//     case LV_EVENT_CREATED:
//         aic_process_time = 0;
//         aic_version_state = MSGBOX_TIP_AIC_VERSION_ALREADY_TIP;
//         ota_get_info_result(OTA_FIREMARE_CH_AIC, &aic_info);

//         lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_NO_REMIND]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//         lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_CONTENT]->obj_container[0], LV_PART_INDICATOR);
//         lv_obj_set_style_width(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_CONTENT]->obj_container[0], 0, LV_PART_SCROLLBAR);

//         lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CURRENT]->obj_container[0], "%s:%s", tr(60), aic_get_version()); // 当前版本
//         lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_NEWEST]->obj_container[0], "%s:%s", tr(61), aic_info.version);   // 最新版本
//         lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s", aic_info.log);

//         if (aic_version_constraint_state) // 执行强制摄像头ota
//         {
//             aic_version_constraint_state = false;
//             char path[PATH_MAX_LEN];
//             strncpy(aic_file_name, get_sysconf()->GetString("system", "aic_version_name", "0").c_str(), sizeof(aic_file_name));
//             hl_disk_get_default_mountpoint(HL_DISK_TYPE_EMMC, aic_file_name, path, sizeof(path));
//             if (access(path, F_OK) == 0)
//             {
//                 aic_version_state = MSGBOX_TIP_AIC_VERSION_UPDATING;
//                 aic_timeout_tick = utils_get_current_tick();
//                 aic_process_time = 0;
//                 LOG_I("OTA verify ota upgrade success!\n");

//                 ota_aic_burn_start(aic_file_name);
//                 get_sysconf()->SetValue("system", "aic_version_name", aic_file_name);
//                 get_sysconf()->WriteIni(SYSCONF_PATH);

//                 lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_PART_KNOB);

//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CURRENT]->obj_container[0], "%s:%s", tr(60), "-"); // 当前版本
//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_NEWEST]->obj_container[0], "%s:%s", tr(61), "-"); // 最新版本
//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s", tr(319));
//             }
//         }
//         break;
//     case LV_EVENT_DESTROYED:
//         break;
//     case LV_EVENT_CLICKED:
//         switch (widget->header.index)
//         {
//         case WIDGET_ID_VERSION_MSGBOX_BTN_CANCEL:
//             aic_version_state = MSGBOX_TIP_AIC_VERSION_CANCEL;
//             return true;
//         case WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE:
//             if (hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) && get_sysconf()->GetInt("system", "wifi", 0))
//             {
//                 uint64_t availabel_size = 0;
//                 if(utils_get_dir_available_size("/user-resource") > TLP_FILE_TLP_PARTITION)
//                     availabel_size = utils_get_dir_available_size("/user-resource") - TLP_FILE_TLP_PARTITION;
//                 if(availabel_size < 20 * 1024 * 1024)   //本地可用空间小于20M提示空间不足，不进行摄像头升级
//                 {
//                     app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_main_routine_msgbox_callback, (void *)MSGBOX_TIP_LOCAL_STORAGE_LACK);
//                     return true;
//                 }
//                 else if (ota_aic_fetch_start(&aic_info) == 0)
//                 {
//                     aic_version_state = MSGBOX_TIP_AIC_VERSION_DOWNLOADING;
//                     lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_PART_KNOB);
//                     lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s", aic_info.log);         
//                 }
//                 else
//                 {
//                     aic_version_state = MSGBOX_TIP_AIC_VERSION_DOWNLOAD_FAIL;
//                     lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(269));
//                     lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_ANIM_OFF);

//                     aic_update_fail_state = true;
//                 }
//             }
//             else
//             {
//                 lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(269));
//                 lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_ANIM_OFF);
//             }
//             return false;
//         }
//         break;
//     case LV_EVENT_UPDATE:
//         if (aic_version_state == MSGBOX_TIP_AIC_VERSION_ALREADY_TIP || aic_version_state == MSGBOX_TIP_AIC_VERSION_DOWNLOAD_FAIL || aic_version_state == MSGBOX_TIP_AIC_VERSION_VERIFY_FAIL)
//         {
//             if (hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) && get_sysconf()->GetInt("system", "wifi", 0))
//             {
//                 lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
//                 lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[0], 255, LV_PART_MAIN);
//                 lv_obj_set_style_text_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[2], 255, LV_PART_MAIN);
//             }
//             else
//             {
//                 lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
//                 lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[0], 102, LV_PART_MAIN);
//                 lv_obj_set_style_text_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[2], 102, LV_PART_MAIN);
//             }
//         }
//         else if (aic_version_state == MSGBOX_TIP_AIC_VERSION_DOWNLOADING)
//         {
//             uint64_t download_offset, download_size;
//             switch (get_ota_download_state(&download_offset, &download_size))
//             {
//             case HL_CURL_DOWNLOAD_RUNNING:
//                 if (download_size == 0)
//                 {
//                     LOG_E("[%s] Error!!! download_size == 0\n", __FUNCTION__);
//                     aic_version_state = MSGBOX_TIP_AIC_VERSION_DOWNLOAD_FAIL;
//                     lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(269));
//                     lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_ANIM_OFF);

//                     aic_update_fail_state = true;
//                 }
//                 else
//                 {
//                     lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], tr(264));
//                     lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], (download_offset * 100) / download_size, LV_ANIM_OFF);
//                 }
//                 break;
//             case HL_CURL_DOWNLOAD_COMPLETED:
//                 ota_aic_upgrade_task_destory();
//                 aic_version_state = MSGBOX_TIP_AIC_VERSION_VERIFYING;
//                 aic_burn_process = 0;
//                 LOG_I("OTA aic download completed!\n");
//                 break;
//             case HL_CURL_DOWNLOAD_FAILED:
//                 ota_aic_upgrade_task_destory();
//                 aic_version_state = MSGBOX_TIP_AIC_VERSION_DOWNLOAD_FAIL;
//                 lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(269));
//                 lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_ANIM_OFF);

//                 aic_update_fail_state = true;
//                 LOG_I("OTA aic download failed!\n");
//                 break;
//             }
//         }
//         else if (aic_version_state == MSGBOX_TIP_AIC_VERSION_VERIFYING)
//         {
//             LOG_I("start verify ota upgrade file !\n");
//             char _digest[32];
//             char filepath[PATH_MAX_LEN];
//             sprintf(aic_file_name, "UC021_V%s_ota.bin", aic_info.version);
//             hl_disk_get_default_mountpoint(HL_DISK_TYPE_EMMC, aic_file_name, filepath, sizeof(filepath));
//             otalib_md5sum(filepath, _digest);
//             LOG_I("AIC info packHash: %s\n", aic_info.packHash);

//             if (strcmp(_digest, aic_info.packHash) == 0)
//             {
//                 aic_version_state = MSGBOX_TIP_AIC_VERSION_UPDATING;
//                 aic_timeout_tick = utils_get_current_tick();
//                 aic_process_time = 0; 
//                 LOG_I("OTA verify ota upgrade success!\n");

//                 ota_aic_burn_start(aic_file_name);
//                 get_sysconf()->SetValue("system", "aic_version_name", aic_file_name);
//                 get_sysconf()->WriteIni(SYSCONF_PATH);
//             }
//             else
//             {
//                 aic_version_state = MSGBOX_TIP_AIC_VERSION_VERIFY_FAIL;
//                 LOG_I("OTA verify ota upgrade failed!\n");
//                 lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(266));
//                 lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_ANIM_OFF);

//                 aic_update_fail_state = true;
//             }
//         }
//         else if (aic_version_state == MSGBOX_TIP_AIC_VERSION_UPDATING)
//         {
// // 实际更新时间(65s) + 初始化发现设备时间(20s)
// #define AIC_UPDATE_TIME 85
//             if ((utils_get_current_tick() - aic_burn_tick) > 1 * 1000)
//             {
//                 aic_burn_tick = utils_get_current_tick();
//                 aic_process_time >= AIC_UPDATE_TIME ? AIC_UPDATE_TIME : aic_process_time++;
//                 aic_burn_process = aic_process_time * 100 / AIC_UPDATE_TIME;
//                 LOG_I("aic_process_time:%d aic_burn_process:%d\n",aic_process_time,aic_burn_process);

//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], tr(267));
//                 lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], aic_burn_process, LV_ANIM_OFF);
//             }

//             /* 超出升级时间 + 2分 定为超时  */
//             if (utils_get_current_tick() - aic_timeout_tick > (2 * 60 + AIC_UPDATE_TIME) * 1000)
//             {
//                 aic_version_state = MSGBOX_TIP_AIC_VERSION_UPDATE_FAIL;
//                 LOG_I("OTA aic upgrade timeout!\n");

//                 lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
//                 lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[0], 102, LV_PART_MAIN);
//                 lv_obj_set_style_text_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[2], 102, LV_PART_MAIN);

//                 lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                 lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(315));
//                 lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_ANIM_OFF);

//                 aic_update_fail_state = true;
//             }

//             // if (aic_burn_process == 100)
//             {
//                 int burn_process;
//                 OTA_Ioctl(OTA_FIREMARE_CH_AIC, OTAG_GET_BURN_PROCESS, &burn_process, sizeof(burn_process));
//                 if (burn_process == 100 && aic_burn_process == 100 && hl_camera_get_exist_state() && aic_get_online())
//                 {
//                     aic_version_state = MSGBOX_TIP_AIC_VERSION_UPDATE_FINISH;
//                     get_sysconf()->SetValue("system", "aic_version_name", "0");
//                     get_sysconf()->WriteIni(SYSCONF_PATH);
//                     LOG_I("OTA aic upgrade success!\n");

//                     char path[PATH_MAX_LEN];
//                     hl_disk_get_default_mountpoint(HL_DISK_TYPE_EMMC, aic_file_name, path, sizeof(path));
//                     remove(path);
//                     utils_vfork_system("sync");

//                     system_reboot(3, false);
//                     lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(268));

//                     // lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     // lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_CANCEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     // lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
//                     // lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[2], tr(89));
//                 }
//             }
//         }
//         break;
//     }
// #endif
//     return false;
// }

bool app_aic_function_light_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    extern bool material_break_clear_msgbox_state;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        material_break_clear_msgbox_state = true;
        lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(322));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(45));
        break;
    case LV_EVENT_DESTROYED:
        material_break_clear_msgbox_state = false;
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT:
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT:
#if CONFIG_SUPPORT_AIC
            if (aic_function_switch || foreign_detection_switch)
            {
                aic_cmd_enforce_request();
                ai_camera_send_cmd_handler(AIC_CMD_CAMERA_LIGHT, AIC_CMD_CARRY_OFF_LED);

                aic_function_switch = false;
                foreign_detection_switch = false;
                get_sysconf()->SetBool("system", "aic_function_switch", aic_function_switch);
                get_sysconf()->SetBool("system", "foreign_detection_switch", foreign_detection_switch);
                get_sysconf()->WriteIni(SYSCONF_PATH);
            }
#endif
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

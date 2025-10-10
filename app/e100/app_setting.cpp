#include "app_setting.h"
#include "app_network.h"
#include "app_camera.h"
#include "app_timezone.h"
#include "app_main.h"
#include "hl_common.h"
#include "jenkins.h"
#include "ota_update.h"
#include "configfile.h"
#include "Define_config_path.h"
#include "klippy.h"
#include "ota.h"
#include "hl_disk.h"
#include <sys/statvfs.h>
#include "ui_api.h"
#include "gpio.h"
#include "ai_camera.h"
#include "app_camera.h"
#include "hl_camera.h"
#include "file_manager.h"
#include "hl_boot.h"
#include "aic_tlp.h"


#define LOG_TAG "app_setting"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
#if CONFIG_SUPPORT_OTA
#define LOCAL_UPGRADE_FILE_PATH "/mnt/exUDISK"
#define LOCAL_UPGRADE_FILE_BIN_PATH "/mnt/exUDISK/update.bin"
#define LOCAL_UPGRADE_FILE_FULL_PATH "/mnt/exUDISK/update/update.swu"
#else
#endif
#if CONFIG_SUPPORT_OTA
#include "miniunz.h"
#include "progress_ipc.h"
#include "swupdate_status.h"
#include "hl_net_tool.h"
#include "hl_net.h"
#include "hl_wlan.h"
#define MD5_DEBUG 1
#define OTA_DOWNLOAD_PATH "/user-resource/update.bin"
#define OTA_UNZIP_PATH "/user-resource/update_tmp"
#define OTA_UPDATE_SWU_DIR "/user-resource/update_tmp/update/"
#define OTA_UPDATE_ROOT_PATH "/user-resource"
// #define OTA_UPDATE_ZIP_FILE_PATH "/user-resource/update.bin"
#define OTA_UPDATE_DEC_AES_DOC_PATH "/user-resource/update"
#define OTA_UPDATE_DEC_AES_PATH "/user-resource/update_firmware.zip"
#define SDCARD_UPDATE_FIRMWARE_FILE_PATH "/mnt/exUDISK/update.bin"
#define OTA_UPDATE_MIN_SIZE (1024 * 1024 * 200) // byte,200MB
typedef enum
{
    DOWNLOAD_STATUS_IDLE = 0,
    DOWNLOAD_STATUS_FAILED,
    DOWNLOAD_STATUS_OVERTIME,
    UNZIP_STATUS_FAILED,
    UPDATE_STATUS_FAILED,
    DOWNLOAD_STATUS_START,
    DOWNLOAD_STATUS_DOWNLOADING,
    DOWNLOAD_STATUS_SUCCESS,
    DECRYPT_STATUS_START,
    DECRYPT_STATUS_DECING,
    UNZIP_STATUS_START,
    UNZIP_STATUS_UNZIPING,
    UPDATE_STATUS_START,
    UPDATE_STATUS_UPDATEING,
    UPDATE_STATUS_SUCCESS,
} fw_upgrading_status_t;
static fw_upgrading_status_t fw_upgrading_status = DOWNLOAD_STATUS_IDLE;
static uint64_t download_progress = 0;
static uint64_t gunzip_progress = 0;
static uint64_t ota_updating_progress = 0;
static hl_ota_update_ctx_t update_ctx = NULL;

#define FW_UPDGRADING_NUL 0
#define FW_UPDGRADING_SWU 1
#define FW_UPDGRADING_BIN 2
#define FW_UPDGRADING_OTA 3
static uint8_t upgrading_method = FW_UPDGRADING_NUL;

#endif
enum
{
    CURRENT_WINDOW_SETTING = 0,
    CURRENT_WINDOW_WLAN,
    CURRENT_WINDOW_INFO,
    CURRENT_WINDOW_CAMERA,
    CURRENT_WINDOW_LAMPLIGHT,
    CURRENT_WINDOW_LANGUAGE,
    CURRENT_WINDOW_TIMEZONE,
};

enum
{
    CUTTER_CHOOSE_MANUAL = 0, // 手动切料
    CUTTER_CHOOSE_AUTO,       // 自动切料
};

enum
{
    MSGBOX_TIP_CUTTING_CHECK = 0, // 确认切刀撞击块正确安装
    MSGBOX_TIP_CUTTER_REMOVE_CHECK, //确认撞击块已被拆除
};
typedef enum
{
    UPDATE_STATE_UPDATE_START_DECING,
    UPDATE_STATE_UPDATE_DECING,
    UPDATE_STATE_UPDATING,
    UPDATE_STATE_UPDATE_CANCEL,
    UPDATE_STATE_UPDATE_SUCCESS,
    UPDATE_STATE_UPDATE_FAIL,
    UPDATE_STATE_UPDATE_IDLE
} update_state_t;
static update_state_t update_info = UPDATE_STATE_UPDATE_IDLE;
static int current_window_index = 0;
static bool aod_select_state = false;
static int language_index = 0;
static int other_win_entrance_index = 0;
extern ConfigParser *get_sysconf();
// extern int aic_version_newer;

// static int cutter_choose = CUTTER_CHOOSE_AUTO;  //切料选择项

static void app_setting_update(widget_t **widget_list);
static bool app_setting_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_setting_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_setting_double_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static void language_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void language_listitem_update(void);
static void app_z_offset_callback(widget_t **widget_list, widget_t *widget, void *e);

/* firmware upgrading functions */
static int fw_upgrading_download_start(void);
static int fw_upgrading_downloading(void);
static int fw_upgrading_download_success(void);
static int fw_upgrading_download_failed(void);
static int fw_upgrading_decrypt_start(void);
static int fw_upgrading_decrypting(void);
static int fw_upgrading_unzip_start(void);
static int fw_upgrading_unzipping(void);
static int fw_upgrading_update_start(void);
static int fw_upgrading_updating(void);
extern ConfigParser *get_sysconf();
void app_setting_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    char luminance_str[100];
    static window_t *win_zoffset;
    static int scroll_y = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_Z_OFFSET_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_AOD_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        current_window_index = CURRENT_WINDOW_SETTING;
        app_setting_update(widget_list);

#if ENABLE_MANUTEST
        LOG_D("push a msgbox !!!\n");
        app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_version_update_msgbox_callback, (void *)MSGBOX_TIP_VERSION_LOCAL_UPDATE);
#endif
        if(scroll_y != 0)
        {
            lv_obj_scroll_to(widget_list[WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING]->obj_container[0], 0, scroll_y, LV_ANIM_OFF);
        }
        break;
    case LV_EVENT_DESTROYED:
        if(win_zoffset)
        {
            window_copy_destory(win_zoffset);
            win_zoffset = NULL;
        }
        scroll_y = lv_obj_get_scroll_y(widget_list[WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING]->obj_container[0]);
        aod_select_state = false;
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_SETTING_BTN_LANGUAGE_CONTAINER:
        case WIDGET_ID_SETTING_BTN_LANGUAGE_SELECT:
             ui_set_window_index(WINDOW_ID_LAMPLIGHT_LANGUAGE, (void *)(widget->header.index));
             break;
        case WIDGET_ID_SETTING_BTN_TIME_ZONE_CONTAINER:
        case WIDGET_ID_SETTING_BTN_TIME_ZONE_ENTRANCE:
            ui_set_window_index(WINDOW_ID_TIMEZONE, NULL);
            break;
        case WIDGET_ID_SETTING_BTN_MINUS_LUMINANCE:
        case WIDGET_ID_SETTING_BTN_PLUS_LUMINANCE:
            if (widget->header.index == WIDGET_ID_SETTING_BTN_MINUS_LUMINANCE)
                luminance_state--;
            else if (widget->header.index == WIDGET_ID_SETTING_BTN_PLUS_LUMINANCE)
                luminance_state++;
            app_setting_update(widget_list);
            system("echo setbl > /sys/kernel/debug/dispdbg/command");
            system("echo lcd0 > /sys/kernel/debug/dispdbg/name");
            sprintf(luminance_str, "echo %d > /sys/kernel/debug/dispdbg/param", (int)(luminance_state * 255 / 4));
            system(luminance_str);
            system("echo 1 > /sys/kernel/debug/dispdbg/start");
            get_sysconf()->SetInt("system", "luminance_state", luminance_state);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            break;
        case WIDGET_ID_SETTING_BTN_AOD_CURRENT:
            aod_select_state = !aod_select_state;
            app_setting_update(widget_list);
            break;
        case WIDGET_ID_SETTING_CONTAINER_AOD_MASK:
            aod_select_state = false;
            app_setting_update(widget_list);
            break;
        case WIDGET_ID_SETTING_BTN_AOD_5_TIME:
        case WIDGET_ID_SETTING_BTN_AOD_15_TIME:
        case WIDGET_ID_SETTING_BTN_AOD_STEADY:
            if (widget->header.index == WIDGET_ID_SETTING_BTN_AOD_5_TIME)
            {
                get_sysconf()->SetInt("system", "screen_off_time", 5);
            }
            else if (widget->header.index == WIDGET_ID_SETTING_BTN_AOD_15_TIME)
            {
                get_sysconf()->SetInt("system", "screen_off_time", 15);
            }
            else if (widget->header.index == WIDGET_ID_SETTING_BTN_AOD_STEADY)
            {
                get_sysconf()->SetInt("system", "screen_off_time", -1);
            }
            get_sysconf()->WriteIni(SYSCONF_PATH);
            update_screen_off_time();
            aod_select_state = false;
            app_setting_update(widget_list);
            break;
        case WIDGET_ID_SETTING_BTN_MATERIAL_DETECTION_CONTAINER:
        case WIDGET_ID_SETTING_BTN_MATERIAL_DETECTION_SWITCH:
            material_detection_switch = !material_detection_switch;
            get_sysconf()->SetBool("system", "material_detection", material_detection_switch);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            app_setting_update(widget_list);
            if (app_print_get_print_state() == false)
            {
                if (material_detection_switch)
                {
                    gpio_init(MATERIAL_BREAK_DETECTION_GPIO);
                    gpio_set_direction(MATERIAL_BREAK_DETECTION_GPIO, GPIO_INPUT);
                }
                else
                    gpio_deinit(MATERIAL_BREAK_DETECTION_GPIO);
            }
            break;
        // case WIDGET_ID_SETTING_BTN_KEYING_SOUND_SWITCH:
        //     keying_sound_switch = !keying_sound_switch;
        //     get_sysconf()->SetBool("system", "keying_sound", keying_sound_switch);
        //     get_sysconf()->WriteIni(SYSCONF_PATH);
        //     app_setting_update(widget_list);
        //     break;
        // case WIDGET_ID_SETTING_BTN_SILENT_MODE_SWITCH:
        //     silent_mode_switch = !silent_mode_switch;
        //     get_sysconf()->SetBool("system", "silent_mode", silent_mode_switch);
        //     get_sysconf()->WriteIni(SYSCONF_PATH);
        //     ui_cb[set_silent_mode_cb](&silent_mode_switch);
        //     app_setting_update(widget_list);
        //     if (silent_mode_switch)
        //         app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_SILENT_MODE_ON);
        //     break;
        case WIDGET_ID_SETTING_BTN_RESUME_PRINT_CONTAINER:
        case WIDGET_ID_SETTING_BTN_RESUME_PRINT_SWITCH:
            resume_print_switch = !resume_print_switch;
            Printer::GetInstance()->m_break_save->m_break_save_enable = resume_print_switch;
            get_sysconf()->SetBool("system", "power_off_resume_print", resume_print_switch);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            app_setting_update(widget_list);
            LOG_I("resume print switch:%d\n", resume_print_switch);
            break;
        case WIDGET_ID_SETTING_BTN_LOG_EXPORT_CONTAINER:
        case WIDGET_ID_SETTING_BTN_LOG_EXPORT_ENTRANCE:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                if(hl_disk_default_is_mounted(HL_DISK_TYPE_USB))
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_EXPORT_LOG);
                else
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_NO_USB);
            }
            break;
        case WIDGET_ID_SETTING_BTN_VERSION_UPDATE_CONTAINER:
        case WIDGET_ID_SETTING_BTN_VERSION_UPDATE:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                ui_set_window_index(WINDOW_ID_DETECT_UPDATE, NULL);
                // app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_CURRENT_NEW_VERSION);
                // app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_version_update_msgbox_callback, (void *)MSGBOX_TIP_VERSION_LOCAL_UPDATE);
                // app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_version_update_msgbox_callback, (void *)MSGBOX_TIP_VERSION_OTA_UPDATE);
            }
            break;
        case WIDGET_ID_SETTING_BTN_RESET_CONTAINER:
        case WIDGET_ID_SETTING_BTN_RESET_ENTRANCE:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                ui_set_window_index(WINDOW_ID_RESET_FACTORY, NULL);
                // app_msgbox_push(WINDOW_ID_DOUBLE_MSGBOX, true, app_setting_double_msgbox_callback, (void *)MSGBOX_TIP_RESET_ENTRANCE);
            }
            break;
        case WIDGET_ID_SETTING_BTN_Z_OFFSET_CONTAINER:
        case WIDGET_ID_SETTING_BTN_Z_OFFSET_ENTRANCE:
            lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_Z_OFFSET_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(widget_list[WIDGET_ID_SETTING_CONTAINER_Z_OFFSET_MASK]->obj_container[0]);
            win_zoffset = window_copy(WINDOW_ID_Z_OFFSET, app_z_offset_callback, widget_list[WIDGET_ID_SETTING_CONTAINER_Z_OFFSET_MASK]->obj_container[0], NULL);
            break;
        case WIDGET_ID_SETTING_CONTAINER_Z_OFFSET_MASK:
            lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_Z_OFFSET_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            window_copy_destory(win_zoffset);
            win_zoffset = NULL;
            break;
        case WIDGET_ID_SETTING_BTN_PRINT_PLATFORM_CONTAINER:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                ui_set_window_index(WINDOW_ID_PRINT_PLATFORM, NULL);
            }
            break;
        }      /* end of switch (widget->header.index) */
        break; /* break for case LV_EVENT_CLICKED */
    case LV_EVENT_UPDATE:
#if CONFIG_SUPPORT_OTA
        // ui_ota_event_t ui_event;
// #if CONFIG_SUPPORT_AIC
//         if (has_newer_FW() || (aic_version_newer == 1 && hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) && get_sysconf()->GetInt("system", "wifi", 0)))
//         {
//             /* set the string of the version number clickable and show up the red dot */
//             lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
//             lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
//         }
//         else
//         {
//             lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
//             lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
//         }
// #else
        /* check if there is a newer version of FirmWare need to be updated */
        /* check if there is a newer version of FirmWare need to be updated */
        if (has_newer_FW())
        {
            /* set the string of the version number clickable and show up the red dot */
            lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        }
// #endif
#else

        if (access(LOCAL_UPGRADE_FILE_PATH, F_OK) == 0)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        }
#endif
        
        if(win_zoffset)
            lv_event_send(win_zoffset->widget_list[0]->obj_container[0], (lv_event_code_t)LV_EVENT_UPDATE, NULL);
        
        break; /* break for case LV_EVENT_UPDATE */
    }
}

static void app_setting_update(widget_t **widget_list)
{
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_SETTING]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_WLAN]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_INFO]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_CAMERA]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_SETTING_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_WLAN_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_CAMERA_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_LAMPLIGHT_LANGUAGE_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_TIMEZONE_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    switch (current_window_index)
    {
    case CURRENT_WINDOW_SETTING:
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_CONTAINER_TOP]->obj_container[1], ui_get_image_src(56));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_SETTING]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_SETTING_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        // 设置列表滑动条
        lv_obj_set_style_translate_x(widget_list[WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING]->obj_container[0], 400, LV_PART_SCROLLBAR);
        lv_obj_set_style_width(widget_list[WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING]->obj_container[0], 2, LV_PART_SCROLLBAR);
        lv_obj_set_style_radius(widget_list[WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING]->obj_container[0], 1, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_SCROLLBAR);
        break;
    case CURRENT_WINDOW_WLAN:
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_CONTAINER_TOP]->obj_container[1], ui_get_image_src(57));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_WLAN]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_WLAN_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    case CURRENT_WINDOW_INFO:
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_CONTAINER_TOP]->obj_container[1], ui_get_image_src(75));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_INFO]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    case CURRENT_WINDOW_CAMERA:
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_CONTAINER_TOP]->obj_container[1], ui_get_image_src(76));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_CAMERA]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_CAMERA_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    case CURRENT_WINDOW_LAMPLIGHT:
    case CURRENT_WINDOW_LANGUAGE:
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_CONTAINER_TOP]->obj_container[1], ui_get_image_src(56));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_SETTING]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_LAMPLIGHT_LANGUAGE_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    case CURRENT_WINDOW_TIMEZONE:
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_CONTAINER_TOP]->obj_container[1], ui_get_image_src(56));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_SETTING_BTN_TOP_SETTING]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_TIMEZONE_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    }

    // 语言
    language_index = get_sysconf()->GetInt("system", "language", 0);
    lv_label_set_text(widget_list[WIDGET_ID_SETTING_BTN_LANGUAGE_SELECT]->obj_container[2], language_infomation[language_index]);
    // 版本
    char version[128];
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
    snprintf(version, sizeof(version), "Lite%s", JENKINS_VERSION);
#else
    snprintf(version, sizeof(version), "V%s", JENKINS_VERSION);
#endif
    lv_label_set_text_fmt(widget_list[WIDGET_ID_SETTING_BTN_VERSION_UPDATE]->obj_container[2], version);
    // 断料检测
    if (material_detection_switch)
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_MATERIAL_DETECTION_SWITCH]->obj_container[1], ui_get_image_src(131));
    else
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_MATERIAL_DETECTION_SWITCH]->obj_container[1], ui_get_image_src(132));
    // // 按键声音
    // if (keying_sound_switch)
    //     lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_KEYING_SOUND_SWITCH]->obj_container[1], ui_get_image_src(131));
    // else
    //     lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_KEYING_SOUND_SWITCH]->obj_container[1], ui_get_image_src(132));
    // 静音模式
    // if (silent_mode_switch)
    //     lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_SILENT_MODE_SWITCH]->obj_container[1], ui_get_image_src(131));
    // else
    //     lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_SILENT_MODE_SWITCH]->obj_container[1], ui_get_image_src(132));
    // 断电续打
    if (resume_print_switch)
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_RESUME_PRINT_SWITCH]->obj_container[1], ui_get_image_src(131));
    else
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_RESUME_PRINT_SWITCH]->obj_container[1], ui_get_image_src(132));

    // 屏幕亮度
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_MINUS_LUMINANCE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_PLUS_LUMINANCE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_MINUS_LUMINANCE]->obj_container[1], ui_get_image_src(219));
    lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_PLUS_LUMINANCE]->obj_container[1], ui_get_image_src(222));
    if (luminance_state == 1)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_MINUS_LUMINANCE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_MINUS_LUMINANCE]->obj_container[1], ui_get_image_src(221));
    }
    else if (luminance_state == 4)
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_PLUS_LUMINANCE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_PLUS_LUMINANCE]->obj_container[1], ui_get_image_src(224));
    }
#if 1 // 息屏
    lv_label_set_text_fmt(widget_list[WIDGET_ID_SETTING_BTN_AOD_5_TIME]->obj_container[2], "5%s", tr(70));
    lv_label_set_text_fmt(widget_list[WIDGET_ID_SETTING_BTN_AOD_15_TIME]->obj_container[2], "15%s", tr(70));
    lv_label_set_text_fmt(widget_list[WIDGET_ID_SETTING_BTN_AOD_STEADY]->obj_container[2], tr(71));
    lv_obj_move_foreground(widget_list[WIDGET_ID_SETTING_BTN_AOD_TIME_SELECT]->obj_container[0]);
    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_SETTING_BTN_AOD_5_TIME]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_SETTING_BTN_AOD_15_TIME]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_SETTING_BTN_AOD_STEADY]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_5_TIME]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_15_TIME]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_STEADY]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    if (aod_select_state)
    {
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_AOD_CURRENT]->obj_container[1], ui_get_image_src(125));
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_TIME_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_AOD_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(widget_list[WIDGET_ID_SETTING_CONTAINER_AOD_MASK]->obj_container[0]);
        lv_obj_set_parent(widget_list[WIDGET_ID_SETTING_BTN_AOD_TIME_SELECT]->obj_container[0], widget_list[WIDGET_ID_SETTING_CONTAINER_AOD_MASK]->obj_container[0]);
        lv_obj_align_to(widget_list[WIDGET_ID_SETTING_BTN_AOD_TIME_SELECT]->obj_container[0],widget_list[WIDGET_ID_SETTING_BTN_AOD_CURRENT]->obj_container[0],LV_ALIGN_OUT_BOTTOM_MID,0,0);
    }
    else
    {
        lv_img_set_src(widget_list[WIDGET_ID_SETTING_BTN_AOD_CURRENT]->obj_container[1], ui_get_image_src(124));
        lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_TIME_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_parent(widget_list[WIDGET_ID_SETTING_BTN_AOD_TIME_SELECT]->obj_container[0], widget_list[WIDGET_ID_SETTING_CONTAINER_LSIT_SETTING]->obj_container[0]);
        lv_obj_add_flag(widget_list[WIDGET_ID_SETTING_CONTAINER_AOD_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    }
    if (get_screen_off_time() == 5)
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_SETTING_BTN_AOD_CURRENT]->obj_container[2], "5%s", tr(70));
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_SETTING_BTN_AOD_5_TIME]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_5_TIME]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    }
    else if (get_screen_off_time() == 15)
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_SETTING_BTN_AOD_CURRENT]->obj_container[2], "15%s", tr(70));
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_SETTING_BTN_AOD_15_TIME]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_15_TIME]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    }
    else if (get_screen_off_time() == -1)
    {
        lv_label_set_text_fmt(widget_list[WIDGET_ID_SETTING_BTN_AOD_CURRENT]->obj_container[2], tr(71));
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_SETTING_BTN_AOD_STEADY]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_SETTING_BTN_AOD_STEADY]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

#if 1 // 设置-灯光界面/设置-语言界面

static app_listitem_model_t *language_model = NULL;
// bool model_light_swtich = false;
bool illumination_light_swtich = false;

// static void app_setting_update_light(widget_t **widget_list);

void app_lamplight_language_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        other_win_entrance_index = (int)lv_event_get_param((lv_event_t *)e);
        // if ((ui_get_window_last_index() == WINDOW_ID_PRINT && other_win_entrance_index == WIDGET_ID_PRINT_BTN_LIGHT) ||
        //     (ui_get_window_last_index() == WINDOW_ID_SETTING && other_win_entrance_index == WIDGET_ID_SETTING_BTN_LAMPLIGHT_ENTRANCE) ||
        //     (ui_get_window_last_index() == WINDOW_ID_MAIN && other_win_entrance_index == WIDGET_ID_MAIN_BTN_LIGHT))
        //     current_window_index = CURRENT_WINDOW_LAMPLIGHT;
        if ((ui_get_window_last_index() == WINDOW_ID_SETTING && other_win_entrance_index == WIDGET_ID_SETTING_BTN_LANGUAGE_CONTAINER))
            current_window_index = CURRENT_WINDOW_LANGUAGE;

        if (current_window_index == CURRENT_WINDOW_LAMPLIGHT)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_CONTAINER_LAMPLIGHT_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_CONTAINER_LANGUAGE_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            // if (model_light_swtich)
            // {
            //     lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(131));
            //     manual_control_sq.push("SET_LED_led1 RED=1.0 GREEN=1.0 BLUE=1.0 WHITE=1.0 TRANSMIT=1");
            //     Printer::GetInstance()->manual_control_signal();
            // }
            // else
            // {
            //     lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(132));
            //     manual_control_sq.push("SET_LED_led1 RED=0.0 GREEN=0.0 BLUE=0 WHITE=0.0 TRANSMIT=1");
            //     Printer::GetInstance()->manual_control_signal();
            // }
            if (illumination_light_swtich)
            {
                lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(131));
                manual_control_sq.push("SET_LED_led2 RED=1 GREEN=1 BLUE=1 WHITE=1 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();
            }
            else
            {
                lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(132));
                manual_control_sq.push("SET_LED_led2 RED=0 GREEN=0 BLUE=0 WHITE=0 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();
            }
        }
        else if (current_window_index == CURRENT_WINDOW_LANGUAGE)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_CONTAINER_LAMPLIGHT_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_CONTAINER_LANGUAGE_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            if (language_model == NULL)
            {
                language_model = app_listitem_model_create(WINDOW_ID_LANGUAGE_LIST_TEMPLATE, widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_CONTAINER_LANGUAGE_LIST]->obj_container[0], language_listitem_callback, NULL);
                for (int i = 0; i < 11; i++)
                    app_listitem_model_push_back(language_model);
                language_listitem_update();
            }
        }
        break;
    case LV_EVENT_DESTROYED:
        if (language_model)
        {
            app_listitem_model_destory(language_model);
            language_model = NULL;
        }
        break;
    case LV_EVENT_CHILD_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_LAMPLIGHT_LANGUAGE_CONTAINER_LANGUAGE_LIST:
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

            lv_label_set_text(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_LANGUAGE_BACK]->obj_container[2], tr(46));
            widget_t **top_widget_list = window_get_top_widget_list();
            lv_label_set_text(top_widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_1]->obj_container[2], tr(18));
            lv_label_set_text(top_widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_2]->obj_container[2], tr(19));
            lv_label_set_text(top_widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_3]->obj_container[2], tr(20));
            lv_label_set_text(top_widget_list[WIDGET_ID_TOP_BTN_UPPER_BAR_OPERATION_4]->obj_container[2], tr(21));
        }
        break;
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
//         case WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH:
//             model_light_swtich = !model_light_swtich;
//             if (model_light_swtich)
//             {
//                 lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(131));
//                 manual_control_sq.push("SET_LED_led1 RED=1.0 GREEN=1.0 BLUE=1.0 WHITE=1.0 TRANSMIT=1");
//                 Printer::GetInstance()->manual_control_signal();
//             }
//             else
//             {
//                 lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(132));
//                 manual_control_sq.push("SET_LED_led1 RED=0.0 GREEN=0.0 BLUE=0 WHITE=0.0 TRANSMIT=1");
//                 Printer::GetInstance()->manual_control_signal();
//             }

//             break;
        case WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH:
            illumination_light_swtich = !illumination_light_swtich;
            if (illumination_light_swtich)
            {
                lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(131));
                manual_control_sq.push("SET_LED_led2 RED=1 GREEN=1 BLUE=1 WHITE=1 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();
            }
            else
            {
                lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(132));
                manual_control_sq.push("SET_LED_led2 RED=0 GREEN=0 BLUE=0 WHITE=0 TRANSMIT=1");
                Printer::GetInstance()->manual_control_signal();
            }

            break;
        case WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_LAMPLIGHT_BACK:
            if (app_print_get_print_state())
            {
                ui_set_window_index(WINDOW_ID_PRINT, NULL);
                app_top_update_style(window_get_top_widget_list());
            }
            else
            {
                if (ui_get_window_last_index() == WINDOW_ID_MAIN && other_win_entrance_index == WIDGET_ID_MAIN_BTN_LIGHT)
                    ui_set_window_index(WINDOW_ID_MAIN, NULL);
                else
                    ui_set_window_index(WINDOW_ID_SETTING, NULL);
            }
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_LANGUAGE_BACK:
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static void app_setting_guide_show(widget_t **widget_list, int guide_step)
{
    if (guide_step == 0)
    {
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_CONTAINER]->obj_container[2], tr(243));
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_LABEL_GUIDE_CONTENT]->obj_container[0], tr(252));
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_BTN_CURRENT_PAGE]->obj_container[2], "1/");
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_LABEL_TOTAL_PAGE]->obj_container[0], "2");

        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_LAST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_NEXT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_CONTAINER]->obj_container[1], ui_get_image_src(237));
    }
    else if (guide_step == 1)
    {
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_CONTAINER]->obj_container[2], tr(244));
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_LABEL_GUIDE_CONTENT]->obj_container[0], tr(253));
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_BTN_CURRENT_PAGE]->obj_container[2], "2/");
        lv_label_set_text(widget_list[WIDGET_ID_CUTTER_LABEL_TOTAL_PAGE]->obj_container[0], "2");

        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_NEXT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_LAST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_CONTAINER]->obj_container[1], ui_get_image_src(238));
    }
    lv_obj_move_foreground(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_MASK]->obj_container[0]);
}

static bool app_setting_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_CUTTING_CHECK)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(246));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_CUTTER_REMOVE_CHECK)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(310));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE:
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

// 切刀选项
void app_cutter_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static int guide_step = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_AUTO]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_MANUAL]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
        if (get_sysconf()->GetBool("system", "cutting_mode", CUTTER_CHOOSE_MANUAL) == CUTTER_CHOOSE_AUTO)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_AUTO]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_AUTO]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        }
        else if (get_sysconf()->GetBool("system", "cutting_mode", CUTTER_CHOOSE_MANUAL) == CUTTER_CHOOSE_MANUAL)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_MANUAL]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_MANUAL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CUTTER_BTN_AUTO:
        case WIDGET_ID_CUTTER_BTN_MANUAL:
            if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                uint16_t idx = widget->header.index;
                if(idx == WIDGET_ID_CUTTER_BTN_AUTO)
                {
                    if (get_sysconf()->GetBool("system", "cutting_mode", CUTTER_CHOOSE_MANUAL) != CUTTER_CHOOSE_AUTO)
                    {
                        get_sysconf()->SetBool("system", "cutting_mode", CUTTER_CHOOSE_AUTO);
                        get_sysconf()->WriteIni(SYSCONF_PATH);
                        char control_command[MANUAL_COMMAND_MAX_LENGTH];
                        sprintf(control_command, "CHANGE_FILAMENT_SET_ACTIVE ACTIVE=%d", get_sysconf()->GetBool("system", "cutting_mode", 0));
                        ui_cb[manual_control_cb](control_command);
                        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_MANUAL]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
                        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_MANUAL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                        lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_AUTO]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_AUTO]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_msgbox_callback, (void *)MSGBOX_TIP_CUTTING_CHECK);
                    }
                }
                else if(idx == WIDGET_ID_CUTTER_BTN_MANUAL)
                {
                    if (get_sysconf()->GetBool("system", "cutting_mode", CUTTER_CHOOSE_MANUAL) != CUTTER_CHOOSE_MANUAL)
                    {
                        get_sysconf()->SetBool("system", "cutting_mode", CUTTER_CHOOSE_MANUAL);
                        get_sysconf()->WriteIni(SYSCONF_PATH);
                        char control_command[MANUAL_COMMAND_MAX_LENGTH];
                        sprintf(control_command, "CHANGE_FILAMENT_SET_ACTIVE ACTIVE=%d", get_sysconf()->GetBool("system", "cutting_mode", 0));
                        ui_cb[manual_control_cb](control_command);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_MANUAL]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_MANUAL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_AUTO]->obj_container[1], LV_OBJ_FLAG_HIDDEN);
                        lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_AUTO]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                    
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_msgbox_callback, (void *)MSGBOX_TIP_CUTTER_REMOVE_CHECK);
                    }
                }
            }
            break;
        case WIDGET_ID_CUTTER_BTN_GUIDE:
            lv_obj_clear_flag(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            guide_step = 0;
            app_setting_guide_show(widget_list, guide_step);
            break;
        case WIDGET_ID_CUTTER_BTN_LAST_PAGE:
            guide_step = 0;
            app_setting_guide_show(widget_list, guide_step);
            break;
        case WIDGET_ID_CUTTER_BTN_NEXT_PAGE:
            guide_step = 1;
            app_setting_guide_show(widget_list, guide_step);
            break;
        case WIDGET_ID_CUTTER_BTN_CLOSE_GUIDE:
            lv_obj_add_flag(widget_list[WIDGET_ID_CUTTER_BTN_GUIDE_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            break;
        case WIDGET_ID_CUTTER_BTN_RETURN:
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

// 检测更新页面
static void app_detect_update_update(widget_t **widget_list)
{
    char version[128];
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
    snprintf(version, sizeof(version), "Lite%s", JENKINS_VERSION);
#else
    snprintf(version, sizeof(version), "V%s", JENKINS_VERSION);
#endif
    lv_label_set_text(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE_CONTAINER]->obj_container[2],version);
// #if CONFIG_SUPPORT_AIC
//  lv_label_set_text(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA_CONTAINER]->obj_container[2],aic_get_version());
// #endif

    // Firmware update 
    if(has_newer_FW())
    {
        static _ota_info info;
        ota_get_info_result(OTA_FIREMARE_CH_SYS, &info);
        if (strlen(info.version) == 0) 
        {
            snprintf(info.version, sizeof(info.version), "%s", JENKINS_VERSION);
        }
        lv_obj_clear_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE_CURRENT_VERSION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE_CURRENT_VERSION]->obj_container[1],app_get_text_width(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE_CURRENT_VERSION]->obj_container[2])+5,0);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_DETECT_UPDATE_LABEL_FIRMWARE_VERSION]->obj_container[0],"%s%s%s%s","(",tr(312),")",info.version);
        lv_obj_add_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
    }
    else
    {
        lv_obj_add_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE_CURRENT_VERSION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_DETECT_UPDATE_LABEL_FIRMWARE_VERSION]->obj_container[0],"%s%s%s%s","(",tr(60),")",JENKINS_VERSION);
        lv_obj_clear_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
    }

    // AIC_Camera update
    //隐藏AI摄像头升级信息
    lv_obj_add_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA_CONTAINER]->obj_container[0],LV_OBJ_FLAG_HIDDEN);

// #if CONFIG_SUPPORT_AIC
//     if(aic_version_newer == 1 && hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) && get_sysconf()->GetInt("system", "wifi", 0))
//     {
//         static _ota_info info;
//         ota_get_info_result(OTA_FIREMARE_CH_AIC, &info);
//         lv_obj_clear_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA_CURRENT_VERSION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
//         lv_obj_set_pos(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA_CURRENT_VERSION]->obj_container[1],app_get_text_width(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA_CURRENT_VERSION]->obj_container[2])+5,0);
//         lv_label_set_text_fmt(widget_list[WIDGET_ID_DETECT_UPDATE_LABEL_AICAMERA_VERSION]->obj_container[0],"%s%s%s%s","(",tr(312),")",info.version);
//         lv_obj_add_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
//     }
//     else
//     {
//         lv_obj_add_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA_CURRENT_VERSION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
//         lv_label_set_text_fmt(widget_list[WIDGET_ID_DETECT_UPDATE_LABEL_AICAMERA_VERSION]->obj_container[0],"%s%s%s%s","(",tr(60),")",aic_get_version());
//         lv_obj_clear_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
//     }
// #else
//         lv_obj_add_flag(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA_CONTAINER]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
// #endif
}

// extern bool aic_update_fail_state;
static bool setting_ota_get_info_state = false;
static lv_timer_t *app_detect_update_callback_timer = NULL;
static void app_detect_update_callback_update(lv_timer_t *timer);
void app_detect_update_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_label_set_text(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE]->obj_container[2],"Centauri");
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_label_set_text(widget_list[WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE]->obj_container[2],"Centauri Carbon");
#endif
        app_detect_update_update(widget_list);

        /* 每次进入更新界面都请求更新信息 */
        if(hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) && get_sysconf()->GetInt("system", "wifi", 0))
        {
            ota_get_info_request(OTA_FIREMARE_CH_SYS);
            // ota_get_info_request(OTA_FIREMARE_CH_AIC);
            setting_ota_get_info_state = false;
        }
        else
            setting_ota_get_info_state = true;
        if (app_detect_update_callback_timer == NULL)
        {
            app_detect_update_callback_timer = lv_timer_create(app_detect_update_callback_update, 500, (void *)widget_list);
            lv_timer_ready(app_detect_update_callback_timer);
        }
        app_detect_update_update(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        if (app_detect_update_callback_timer != NULL)
        {
            lv_timer_del(app_detect_update_callback_timer);
            app_detect_update_callback_timer = NULL;
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_DETECT_UPDATE_BTN_FIRMWARE:
            app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_version_update_msgbox_callback, (void *)MSGBOX_TIP_VERSION_LOCAL_UPDATE);
            break;
        // case WIDGET_ID_DETECT_UPDATE_BTN_AICAMERA:
        //     app_msgbox_push(WINDOW_ID_VERSION_MSGBOX, true, app_aic_version_update_msgbox_callback, NULL);
        //     break;
        case WIDGET_ID_DETECT_UPDATE_BTN_RETURN:
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static void app_detect_update_callback_update(lv_timer_t *timer)
{
    widget_t **widget_list = (widget_t **)timer->user_data;
    app_detect_update_update(widget_list);

    // AI摄像头固件更新失败弹窗
//     if (aic_update_fail_state)
//     {
//         aic_update_fail_state = false;
//         app_msgbox_close_all_avtive_msgbox();
//         app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_aic_update_fail_routine_msgbox_callback, NULL);
//     }

    /* 检测摄像头每次进入界面的更新 */
    // _ota_info aic_info;
    // if(setting_ota_get_info_state == false && ota_get_info_result(OTA_FIREMARE_CH_AIC, &aic_info) == OTA_API_STAT_SUCCESS)
    // {
    //     setting_ota_get_info_state = true;
    //     if (strcmp(aic_get_version(), aic_info.version) != 0 && strlen(aic_info.version) > 0 && aic_get_online() && hl_camera_get_exist_state())
    //         aic_version_newer = 1;
    // }
}

// static void app_setting_update_light(widget_t **widget_list)
// {
//     if (illumination_light_swtich == true && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1]), ui_get_image_src(131)) != 0)
//     {
//         lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(131));
//     }
//     else if (illumination_light_swtich == false && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1]), ui_get_image_src(132)) != 0)
//     {
//         lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_ILLUMINATION_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(132));
//     }

//     if (model_light_swtich == true && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1]), ui_get_image_src(131)) != 0)
//     {
//         lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(131));
//     }
//     else if (model_light_swtich == false && strcmp((char *)lv_img_get_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1]), ui_get_image_src(132)) != 0)
//     {
//         lv_img_set_src(widget_list[WIDGET_ID_LAMPLIGHT_LANGUAGE_BTN_MODEL_LIGHT_SWITCH]->obj_container[1], ui_get_image_src(132));
//     }
// }


// 恢复出厂设置
static bool log_select = false;
static bool tlp_vedio_record_select = false;
static bool gcode_file_select = false;

static void reset_factory_update_selectbox(widget_t **widget_list)
{
    if(log_select)
        lv_img_set_src(widget_list[WIDGET_ID_RESET_FACTORY_BTN_LOG_SELECT]->obj_container[1],ui_get_image_src(254));
    else
        lv_img_set_src(widget_list[WIDGET_ID_RESET_FACTORY_BTN_LOG_SELECT]->obj_container[1],ui_get_image_src(253));
    if(tlp_vedio_record_select)
        lv_img_set_src(widget_list[WIDGET_ID_RESET_FACTORY_BTN_TLP_VEDIO_PRINT_RECORD_SELECT]->obj_container[1],ui_get_image_src(254));
    else
        lv_img_set_src(widget_list[WIDGET_ID_RESET_FACTORY_BTN_TLP_VEDIO_PRINT_RECORD_SELECT]->obj_container[1],ui_get_image_src(253));
    if(gcode_file_select)
        lv_img_set_src(widget_list[WIDGET_ID_RESET_FACTORY_BTN_GCODE_FILE_SELECT]->obj_container[1],ui_get_image_src(254));
    else
        lv_img_set_src(widget_list[WIDGET_ID_RESET_FACTORY_BTN_GCODE_FILE_SELECT]->obj_container[1],ui_get_image_src(253));
}

void delete_files(const std::string&delete_files_anme , const std::string& directorypath) {
    DIR* dir = opendir(directorypath.c_str());
    if (dir == nullptr) {
        // std::cerr << "Cannot open directory: " << directorypath << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename.find(delete_files_anme.c_str()) != std::string::npos) {
            std::string filePath = directorypath + "/" + filename;
            struct stat fileStat;
            //获取文件状态信息并检查给定的文件路径是否指向一个普通文件
            if (stat(filePath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                if (unlink(filePath.c_str()) == 0) {
                    // std::cout << "Deleted file: " << filePath << std::endl;
                } else {
                    // std::cerr << "Failed to delete file: " << filePath << std::endl;
                }
            }
        }
    }

    closedir(dir);
}

void app_reset_factory_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_label_set_text_fmt(widget_list[WIDGET_ID_RESET_FACTORY_BTN_TLP_VEDIO_PRINT_RECORD]->obj_container[2],tr(17));
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_label_set_text_fmt(widget_list[WIDGET_ID_RESET_FACTORY_BTN_TLP_VEDIO_PRINT_RECORD]->obj_container[2],tr(298));
#endif
        reset_factory_update_selectbox(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        log_select = false;
        tlp_vedio_record_select = false;
        gcode_file_select = false;
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_RESET_FACTORY_BTN_LOG_SELECT:
            log_select = !log_select;
            reset_factory_update_selectbox(widget_list);
            break;
        case WIDGET_ID_RESET_FACTORY_BTN_TLP_VEDIO_PRINT_RECORD_SELECT:
            tlp_vedio_record_select = !tlp_vedio_record_select;
            reset_factory_update_selectbox(widget_list);
            break;
        case WIDGET_ID_RESET_FACTORY_BTN_GCODE_FILE_SELECT:
            gcode_file_select = !gcode_file_select;
            reset_factory_update_selectbox(widget_list);
            break;
        case WIDGET_ID_RESET_FACTORY_BTN_RETURN:
            ui_set_window_index(WINDOW_ID_SETTING, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_RESET_FACTORY_BTN_RECOVER:
            // wait to do
            if (log_select != false)
            {
                delete_files("log", BREAK_SAVE_PATH);
            }
            if (tlp_vedio_record_select != false)
            {
                hl_system("rm '%s'", HISTORY_PATH);
                hl_system("rm '%s' -rf", HISTORY_IMAGE_PATH);
                hl_system("rm '%s'", HISTORY_PATH1);
                hl_system("rm -rf '%s'", USER_AIC_TLP_PATH);
                
                delete_files(".mp4", USER_RESOURCE_PATH);
                delete_files(".mp4.tmp", USER_RESOURCE_PATH);
            }
            if (gcode_file_select != false)
            {
                delete_files(".gcode", USER_RESOURCE_PATH);
                FileManager::GetInstance()->ClearFileInfo();
                get_sysconf()->SetInt("system", "default_gcode_copy_flag", 0); //清除默认gcode复制标志位
            }
            hl_system("rm '%s'", CONFIG_PATH);
            hl_system("rm '%s'", USER_CONFIG_PATH);
            hl_system("ln -s /usr/share/zoneinfo/%s /etc/localtime -f", "Asia/Shanghai");   //设置时区为中国

#if CONFIG_SUPPORT_AIC
            ai_camera_send_cmd_handler(AIC_CMD_CAMERA_LIGHT, AIC_CMD_CARRY_OFF_LED); //关灯
#endif
            get_sysconf()->SetBool("system", "boot", 0); //开机自检
            get_sysconf()->SetInt("system", "luminance_state", 4); //亮度最大
            get_sysconf()->SetInt("system", "screen_off_time", -1);    //屏幕常亮
            get_sysconf()->SetBool("system", "aic_function_switch", 0); //AI检测关闭
            get_sysconf()->SetBool("system", "foreign_detection_switch", 0); //异物检测关闭
            get_sysconf()->SetBool("system", "power_off_resume_print", 1); //断电续打开启
            get_sysconf()->SetBool("system", "material_detection", 1); // 断料检测开启
            get_sysconf()->SetBool("system", "cutting_mode", CUTTER_CHOOSE_AUTO); //自动切料
            get_sysconf()->SetInt("system", "language", 0); //设置语言为中文
            get_sysconf()->SetInt("system", "timezone", 27); //时区设置
            get_sysconf()->SetBool("system", "calibration_switch", 0); //打印校准关闭
            get_sysconf()->SetInt("system", "detection_frequency_sec", 0); //摄像头模式设置为常规检测
            get_sysconf()->SetBool("system", "print_platform_type", 0); //默认A面
            get_sysconf()->SetBool("system", "camera_light_switch", false); //关闭摄像头灯光
            get_sysconf()->WriteIni(SYSCONF_PATH);
            hl_system("rm '%s'", WLAN_FILE_PATH);
            system("sync");

            system_reboot(0, false);
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        // if(log_select == false && tlp_vedio_record_select == false && gcode_file_select == false)
        // {
        //     lv_obj_clear_flag(widget_list[WIDGET_ID_RESET_FACTORY_BTN_RECOVER]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_set_style_text_opa(widget_list[WIDGET_ID_RESET_FACTORY_BTN_RECOVER]->obj_container[2], 127, LV_PART_MAIN);
        // }
        // else
        // {
        //     lv_obj_add_flag(widget_list[WIDGET_ID_RESET_FACTORY_BTN_RECOVER]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
        //     lv_obj_set_style_text_opa(widget_list[WIDGET_ID_RESET_FACTORY_BTN_RECOVER]->obj_container[2], 255, LV_PART_MAIN);
        // }
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
        lv_obj_set_pos(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], col * 132, 0);
        else
        lv_obj_set_pos(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], col * 132, row * 49);

        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
        // lv_img_set_src(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[1], ui_get_image_src(163 + i));
        // lv_obj_align(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[1], LV_ALIGN_TOP_MID, 0, 10);
        // lv_obj_align(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[2], LV_ALIGN_TOP_MID, 0, 48);
        lv_label_set_text(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[2], language_infomation[i]);

        if (i == language_index)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0]);
            // lv_obj_set_style_border_color(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            // lv_obj_set_style_border_color(widget_list[WIDGET_ID_LANGUAGE_LIST_TEMPLATE_BTN_LANGUAGE_INFO]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
        }
    }
}

#endif

void app_info_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    uint64_t cumulative_time = 0;
    struct statvfs buffer;
    int hours, minutes, ret, memory_percentage;
    double total_space, free_space, used_space;
    char memory_space[64] = {0};
    char b_id[17] = {0};

    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        cumulative_time = get_sysconf()->GetInt("system", "cumulative_time", 0);
        hours = cumulative_time / 3600;
        minutes = (cumulative_time - hours * 3600) / 60;
        ret = statvfs("/user-resource", &buffer);

        if (!ret)
        {
            // LOG_I("文件系统片段的大小: %lu\n", buffer.f_frsize);
            // LOG_I("文件系统块的总数: %lu\n", buffer.f_blocks);
            // LOG_I("文件系统中闲置块的数量: %lu\n", buffer.f_bfree);
            total_space = (double)(buffer.f_blocks * buffer.f_frsize) / 1024. / 1024. / 1024.;
            free_space = (double)(buffer.f_bfree * buffer.f_frsize) / 1024. / 1024. / 1024.;
            used_space = (double)((buffer.f_blocks * buffer.f_frsize) - (buffer.f_bfree * buffer.f_frsize)) / 1024. / 1024. / 1024.;
            // LOG_I("总存储空间(G): %.1lf\n", total_space);
            // LOG_I("已使用的存储空间(G): %.1lf\n", used_space);
            // LOG_I("剩余存储空间(G): %.1lf\n", free_space);
            sprintf(memory_space, "%.1lfG/%.1lfG", used_space, total_space);
            memory_percentage = (int)(used_space / total_space * 100);
            // LOG_I("memory_percentage: %d\n", memory_percentage);
        }
        else
        {
            sprintf(memory_space, "--.--G/--.--G");
            memory_percentage = 0;
            LOG_I("获取文件系统信息失败\n");
        }

#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_EQUIPMENT]->obj_container[0], "Centauri");
        lv_img_set_src(widget_list[WIDGET_ID_INFO_CONTAINER_LEFT]->obj_container[1], ui_get_image_src(355));
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_EQUIPMENT]->obj_container[0], "Centauri Carbon");
        lv_img_set_src(widget_list[WIDGET_ID_INFO_CONTAINER_LEFT]->obj_container[1], ui_get_image_src(186));
#endif
        lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_INFO_SLIDER_STORAGE]->obj_container[0], 0, LV_PART_KNOB);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_PRINT_SIZE]->obj_container[0], "256x256x256mm");
        hl_get_chipid(b_id, sizeof(b_id));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_VERSION]->obj_container[0], b_id);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_PRINT_TIME]->obj_container[0], "%02dh%02dm", hours, minutes);
        if(get_sysconf()->GetInt("system", "language", 0) == 0)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_MANUFACTURER]->obj_container[0], "深圳市智能派科技");
        else
            lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_MANUFACTURER]->obj_container[0], "ELEGOO");
        lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_CONTACT_US]->obj_container[0], "3dp@elegoo.com");
        lv_label_set_text_fmt(widget_list[WIDGET_ID_INFO_LABEL_STORAGE]->obj_container[0], memory_space);
        lv_slider_set_value(widget_list[WIDGET_ID_INFO_SLIDER_STORAGE]->obj_container[0], memory_percentage, LV_ANIM_OFF);
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_INFO_CONTAINER_STORAGE:
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

static bool app_setting_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_CURRENT_NEW_VERSION)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(108));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_SILENT_MODE_ON)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(112));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(31));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_OTA_NETWORK_DISCONNECT)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(269));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_OTA_NETWORK_TIMEOUT)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(270));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_NO_PRINT_Z_OFFSET)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(282));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_OTA_FAIL_RETRY)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(65));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
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
        else if (tip_index == MSGBOX_TIP_Z_OFFSET_SAVE)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(344));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(45));
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
            if (tip_index == MSGBOX_TIP_Z_OFFSET_SAVE)
            {
                ui_cb[manual_control_cb]((char *)"M8823"); 
                LOG_I("[M8823] save Z offset.\n");
            }
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

bool app_version_update_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    _ota_info info;

    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        start_tick = 0;
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_VERSION_LOCAL_UPDATE)
        {
        }
        else if (tip_index == MSGBOX_TIP_VERSION_OTA_UPDATE)
        {
        }
        lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_NO_REMIND]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_CONTENT]->obj_container[0], LV_PART_INDICATOR);
        lv_obj_set_style_width(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_CONTENT]->obj_container[0], 0, LV_PART_SCROLLBAR);

        if (has_newer_localFW())
        {
            LOG_I("[%s] had newer local FW.\n", __FUNCTION__);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CURRENT]->obj_container[0], "%s:%s", tr(60), JENKINS_VERSION);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_NEWEST]->obj_container[0], "%s:%s", tr(61), tr(259));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s", tr(260));
        }
#if CONFIG_SUPPORT_OTA
        else if (has_newer_cloudFW())
        {
            LOG_I("[%s] had newer cloud FW.\n", __FUNCTION__);
            /* get OTA infomation */
            if (ota_get_info_result(OTA_FIREMARE_CH_SYS, &info) == OTA_API_STAT_SUCCESS && info.update == true)
            {
                LOG_D("[%s] get ota info succeed.\n", __FUNCTION__);
                lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CURRENT]->obj_container[0], "%s:%s", tr(60), JENKINS_VERSION); // 当前版本
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
                lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_NEWEST]->obj_container[0], "(%s) Lite%s", tr(61), info.version);     // 最新版本
#else
                lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_NEWEST]->obj_container[0], "(%s) V%s", tr(61), info.version);     // 最新版本
#endif
                lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s", info.log);
            }
            else
            {
                LOG_E("[%s] get ota info error.\n", __FUNCTION__);
            }

            /* check network connection */
            if (!hl_net_wan_is_connected() || !hl_wlan_get_enable())
            {
                LOG_E("[%s] network disconnected or disable.\n", __FUNCTION__);
                /* hid the update button */
                lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s", tr(261));
            }
        }
        else
        {
            LOG_D("[%s]  had neither local FW nor cloud FW.\n", __FUNCTION__);
        }
#endif
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        LOG_D("[%s] clicked.\n", __FUNCTION__);
        switch (widget->header.index)
        {
        case WIDGET_ID_VERSION_MSGBOX_BTN_NO_REMIND:
            return true;
        case WIDGET_ID_VERSION_MSGBOX_BTN_CANCEL:
            return true;
        case WIDGET_ID_VERSION_MSGBOX_BTN_UPDATE:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
                return true;
            }
            else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER_TASK);
                return true;
            }
            else
            {
                // 存储空间大于200M才允许升级
                char diskpath[1024] = {0};
                hl_disk_get_default_mountpoint(HL_DISK_TYPE_EMMC, NULL, diskpath, sizeof(diskpath));
                // LOG_D("availabel size : %lld\n",utils_get_dir_available_size(diskpath));
                
                uint64_t availabel_size = 0;
                if(utils_get_dir_available_size(diskpath) > TLP_FILE_TLP_PARTITION)
                    availabel_size = utils_get_dir_available_size(diskpath) - TLP_FILE_TLP_PARTITION;
                LOG_D("availabel size : %lld\n",availabel_size);

                if(availabel_size < 200 * 1024 *1024)   //200M
                {
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_LOCAL_STORAGE_LACK);
                    return true;
                }
                    
                lv_obj_add_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_FUNCTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_PART_KNOB);
                LOG_I("[%s] start FW updating.\n", __FUNCTION__);


#if CONFIG_SUPPORT_OTA
                /* Updagrating priority: swu > bin > ota */
                if (access(LOCAL_UPGRADE_FILE_FULL_PATH, F_OK) == 0)
                {
                    /* start local swu FW upgrading */
                    lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s:%s", tr(262), " .swu");
                    lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:%s", tr(260), LOCAL_UPGRADE_FILE_FULL_PATH);
                    LOG_E("copy %s to %s and start decrypting \n", LOCAL_UPGRADE_FILE_FULL_PATH, OTA_UPDATE_SWU_DIR);
                    upgrading_method = FW_UPDGRADING_SWU;
                    fw_upgrading_status = UPDATE_STATUS_START;
                }
                else if (access(LOCAL_UPGRADE_FILE_BIN_PATH, F_OK) == 0)
                {
                    /* start local bin FW upgrading */
                    LOG_E("copy %s to %s and start decrypting \n", LOCAL_UPGRADE_FILE_BIN_PATH, OTA_DOWNLOAD_PATH);
                    lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s:%s", tr(262), " .bin");
                    lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:%s", tr(260), LOCAL_UPGRADE_FILE_BIN_PATH);
                    upgrading_method = FW_UPDGRADING_BIN;
                    fw_upgrading_status = DECRYPT_STATUS_START;
                }
                else if (ota_get_info_result(OTA_FIREMARE_CH_SYS, &info) == OTA_API_STAT_SUCCESS && info.update == true && hl_net_wan_is_connected())
                {
                    /* start OTA processes */
                    upgrading_method = FW_UPDGRADING_OTA;
                    fw_upgrading_status = DOWNLOAD_STATUS_START;
                }
                else
                {
                    upgrading_method = FW_UPDGRADING_NUL;
                    LOG_E("Fw upgrading failed.\n");
                    fw_upgrading_status = UPDATE_STATUS_FAILED;
                }
#else
                ota_update(LOCAL_UPGRADE_FILE_PATH);
                // 测
                lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 0, LV_ANIM_OFF);
                lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_PROGRESS]->obj_container[0], "%d%%", 0);
                start_tick = utils_get_current_tick();
#endif
                return false;
            }      
        }
        break;
    case LV_EVENT_UPDATE:
#if CONFIG_SUPPORT_OTA
        /* Firmware upgrading: support both ota and local FW */
        switch (fw_upgrading_status)
        {
        case DOWNLOAD_STATUS_IDLE:
            break;
        case DOWNLOAD_STATUS_START:
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(263));
            fw_upgrading_download_start();
            /* start timeout counting */
            static uint64_t start_tick = 0;
            start_tick = hl_get_tick_ms();
            break;
        case DOWNLOAD_STATUS_DOWNLOADING:
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(264));
            lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], download_progress, LV_ANIM_OFF);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_PROGRESS]->obj_container[0], "%d%%", download_progress);
            if (0 != fw_upgrading_downloading()) { /* it will go to the DOWNLOAD_STATUS_FAILED case */ }

            #define OVER_TIME 60*10 /* 10 minitus timeout */
            if (hl_tick_is_overtime(start_tick, hl_get_tick_ms(), OVER_TIME*1000))
            {
                LOG_D("[%s] start_tick:%llums curr_tick:%llums\n", __FUNCTION__, start_tick, hl_get_tick_ms());
                LOG_I("[%s] Downloading FW overtime.\n", __FUNCTION__);
                fw_upgrading_status = DOWNLOAD_STATUS_OVERTIME;
            }
            break;
        case DOWNLOAD_STATUS_SUCCESS:
            lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 100, LV_ANIM_OFF);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_PROGRESS]->obj_container[0], "%d%%", 100);
            if (0 != fw_upgrading_download_success()) { fw_upgrading_status = DOWNLOAD_STATUS_FAILED; }
            break;
        case DOWNLOAD_STATUS_OVERTIME:
        case DOWNLOAD_STATUS_FAILED:
            int msg_box;
            if (hl_tick_is_overtime(start_tick, hl_get_tick_ms(), OVER_TIME*1000))
            {
                msg_box = MSGBOX_TIP_OTA_NETWORK_TIMEOUT;
            }
            else
            {
                // 判断存储空间是否已满
                char diskpath[1024] = {0};
                hl_disk_get_default_mountpoint(HL_DISK_TYPE_EMMC, NULL, diskpath, sizeof(diskpath));
                // LOG_D("availabel size : %lld\n",utils_get_dir_available_size(diskpath));
                if(utils_get_dir_available_size(diskpath) == 0)
                    msg_box = MSGBOX_TIP_LOCAL_STORAGE_LACK;
                else
                    msg_box = MSGBOX_TIP_OTA_NETWORK_DISCONNECT;
            }
            fw_upgrading_download_failed();
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)msg_box);
            fw_upgrading_status = DOWNLOAD_STATUS_IDLE;
            return true; /* return true to close this msgbox */
            break;
        case DECRYPT_STATUS_START:
            fw_upgrading_decrypt_start();
            break;
        case DECRYPT_STATUS_DECING:
            fw_upgrading_decrypting();
            break;
        case UNZIP_STATUS_START:
            fw_upgrading_unzip_start();
            break;
        case UNZIP_STATUS_UNZIPING:
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(265));
            lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], gunzip_progress, LV_ANIM_OFF);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_PROGRESS]->obj_container[0], "%d%%", gunzip_progress);
            fw_upgrading_unzipping();
            break;
        case UNZIP_STATUS_FAILED:
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(266));    
            {
                // 判断存储空间是否已满
                char diskpath[1024] = {0};
                hl_disk_get_default_mountpoint(HL_DISK_TYPE_EMMC, NULL, diskpath, sizeof(diskpath));
                if(utils_get_dir_available_size(diskpath) == 0)
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_LOCAL_STORAGE_LACK);
                else
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_OTA_FAIL_RETRY);
            }
            fw_upgrading_status = DOWNLOAD_STATUS_IDLE;
            ota_firmware_file_remove();
            return true;
            break;
        case UPDATE_STATUS_START:
            lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 100, LV_ANIM_OFF);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_PROGRESS]->obj_container[0], "%d%%", 100);
            fw_upgrading_update_start();
            break;
        case UPDATE_STATUS_UPDATEING:
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(267));
            lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], ota_updating_progress, LV_ANIM_OFF);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_PROGRESS]->obj_container[0], "%d%%", ota_updating_progress);
            fw_upgrading_updating();
            break;
        case UPDATE_STATUS_SUCCESS:
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(268));
            lv_slider_set_value(widget_list[WIDGET_ID_VERSION_MSGBOX_SLIDER_PROGRESS]->obj_container[0], 100, LV_ANIM_OFF);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_LABEL_PROGRESS]->obj_container[0], "%d%%", 100);
            break;
        case UPDATE_STATUS_FAILED:
            lv_label_set_text_fmt(widget_list[WIDGET_ID_VERSION_MSGBOX_CONTAINER_UPDATE]->obj_container[2], "%s", tr(266));
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_OTA_FAIL_RETRY);
            fw_upgrading_status = DOWNLOAD_STATUS_IDLE;
            ota_firmware_file_remove();
            return true;
            break;
        }
        if (fw_upgrading_status >= DOWNLOAD_STATUS_START)
        {
            return false;
        }
        break;
#else
        if (start_tick != 0)
        {
            if (utils_get_current_tick() - start_tick > 10 * 1000)
                return true;
        }
        break;
#endif
    }
    return false;
}

static bool app_setting_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_NO_USB)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(111));
        else if (tip_index == MSGBOX_TIP_EXPORT_LOG)
        {
            char disk_path[1024];
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s", tr(110));
            // start_tick = utils_get_current_tick();
            // if (hl_disk_get_default_mountpoint(HL_DISK_TYPE_USB, NULL, disk_path, sizeof(disk_path)) == 0)
            // {
            //     log_export_to_path(disk_path);
            //     utils_vfork_system("sync");
            // }
            start_tick = 0;
        }
        else if (tip_index == MSGBOX_TIP_EXPORT_LOG_COMPLETE)
        {
            start_tick = utils_get_current_tick();
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(291));
        }
        else if (tip_index == MSGBOX_TIP_Z_OFFSET_MAX_VALUE)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(283));
        else if (tip_index == MSGBOX_TIP_Z_OFFSET_MIN_VALUE)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(284));
        else if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION)
        {
            start_tick = utils_get_current_tick();
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(290));
        }
        else if (tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
        {
            start_tick = utils_get_current_tick();
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(313));
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
            if (tip_index == MSGBOX_TIP_EXPORT_LOG)
                return false;
            return true;
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_EXPORT_LOG)
        {
            if(start_tick == 0)     //导出时阻塞Ui,等待后续优化
            {
                start_tick = utils_get_current_tick();
                char disk_path[1024];
                if (hl_disk_get_default_mountpoint(HL_DISK_TYPE_USB, NULL, disk_path, sizeof(disk_path)) == 0)
                {
                    log_export_to_path(disk_path);
                    if (access("/board-resource/lastlog", F_OK) == 0)
                    {
                        utils_vfork_system("cp /board-resource/lastlog %s", disk_path);
                        utils_vfork_system("sync");
                    }
                    utils_vfork_system("tar -czf /user-resource/coredumps.tar.gz /user-resource/coredump-*.gz");
                    if (access("/user-resource/coredumps.tar.gz", F_OK) == 0)
                    {
                        utils_vfork_system("rm /user-resource/coredump-*.gz");
                        utils_vfork_system("mv /user-resource/coredumps.tar.gz %s", disk_path);
                        utils_vfork_system("sync");
                    }
                }
            }
            if (utils_get_current_tick() - start_tick > 3 * 1000)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_EXPORT_LOG_COMPLETE);
                return true;
            }
        }
        else if (tip_index == MSGBOX_TIP_EXPORT_LOG_COMPLETE)
        {
            if (utils_get_current_tick() - start_tick > 2 * 1000)
            {
                return true;
            }
        }
        else if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION || tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
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

static bool app_setting_double_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_RESET_ENTRANCE)
        {
            lv_img_set_src(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT]->obj_container[1], ui_get_image_src(159));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_LEFT]->obj_container[2], tr(113));
            lv_img_set_src(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_RIGHT]->obj_container[1], ui_get_image_src(160));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_DOUBLE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(31));
        }
        break;
    case LV_EVENT_DESTROYED:
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
            if (tip_index == MSGBOX_TIP_RESET_ENTRANCE)
            {
                //恢复出厂设置
                hl_system("rm '%s'", CONFIG_PATH);
                hl_system("rm '%s'", SYSCONF_PATH);
                hl_system("rm '%s'", HISTORY_PATH);
                hl_system("rm '%s'", WLAN_FILE_PATH);
                hl_system("rm '%s' -rf", HISTORY_IMAGE_PATH);
                hl_system("rm '%s'", HISTORY_PATH1);
                system("fw_setenv parts_clean UDISK");
                system("sync");
                // system("reboot");
                system_reboot(0, false);
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
void ota_firmware_file_remove(void)
{
#if CONFIG_SUPPORT_OTA
    if (access(OTA_DOWNLOAD_PATH, F_OK) == 0)
    {
        hl_system("rm -r %s", OTA_DOWNLOAD_PATH);
    }
    if (access(OTA_UNZIP_PATH, F_OK) == 0)
    {
        hl_system("rm -r %s", OTA_UNZIP_PATH);
    }
    if (access(OTA_UPDATE_DEC_AES_PATH, F_OK) == 0)
    {
        hl_system("rm -r %s", OTA_UPDATE_DEC_AES_PATH);
    }
    if (access(OTA_UPDATE_DEC_AES_DOC_PATH, F_OK) == 0)
    {
        hl_system("rm -r %s", OTA_UPDATE_DEC_AES_DOC_PATH);
    }
#endif
    hl_system("sync");
}

/* Firmware upgrading processes: compatible with OTA and local firmwares
 * 1. downloan
 * 2. Decryption
 * 3. Unzip
 * 4. update
 */
static hl_curl_download_ctx_t download_ctx = NULL;
static hl_ota_dec_ctx_t dec_ctx = NULL;
static hl_unz_ctx_t unz_ctx = NULL;

static int fw_upgrading_download_start(void)
{
    _ota_info info = {0};

    /* get ota info */
    if ((ota_get_info_result(OTA_FIREMARE_CH_SYS, &info) != OTA_API_STAT_SUCCESS) || (info.update != true))
    {
        LOG_I("ota failed: get info failed.\n");
        fw_upgrading_status = DOWNLOAD_STATUS_IDLE;
        return -1;
    }

    ota_firmware_file_remove();
    if ((hl_curl_download_task_create(&download_ctx, info.packUrl, OTA_DOWNLOAD_PATH) == -1) || download_ctx == NULL)
    {
        LOG_I("ota download failed.\n");
        fw_upgrading_status = DOWNLOAD_STATUS_FAILED;
    }
    else
    {
        fw_upgrading_status = DOWNLOAD_STATUS_DOWNLOADING;
    }

    return 0;
}

static int fw_upgrading_downloading(void)
{
    hl_curl_download_state_t download_state;
    uint64_t download_offset = 0;
    uint64_t download_size = 0;

    /* downloading */
    if (download_ctx != NULL)
    {
        download_state = hl_curl_download_get_state(download_ctx, &download_offset, &download_size);
        if (download_state == HL_CURL_DOWNLOAD_COMPLETED)
        {
            if (download_ctx)
            {
                fw_upgrading_status = DOWNLOAD_STATUS_SUCCESS;
                hl_curl_download_task_destory(&download_ctx);
            }
        }
        else if (download_state == HL_CURL_DOWNLOAD_FAILED)
        {
            LOG_I("ota downloading failed.\n");
            fw_upgrading_status = DOWNLOAD_STATUS_FAILED;
            return -1;
        }
        else
        {
            if (download_size > 0 && download_offset > 0)
            {
                /* print out when download progress changed to reduce redundant logging */
                if (download_progress != download_offset * 100 / download_size)
                {
                    download_progress = download_offset * 100 / download_size;
                    LOG_D("download_progress %llu \n", download_progress);
                }
            }
        }
    }

    return 0;
}

static int fw_upgrading_download_success(void)
{
    /* get ota info */
    _ota_info info = {0};
    if ((ota_get_info_result(OTA_FIREMARE_CH_SYS, &info) != OTA_API_STAT_SUCCESS) || (info.update != true))
    {
        LOG_I("[%s] ota failed: get info failed.\n", __FUNCTION__);
        fw_upgrading_status = DOWNLOAD_STATUS_FAILED;
        return -1;
    }

    fw_upgrading_status = DOWNLOAD_STATUS_IDLE;
    if (download_ctx)
    {
        hl_curl_download_task_destory(&download_ctx);
    }
    LOG_I("ota download succ \n");
    if (access(OTA_DOWNLOAD_PATH, R_OK) == 0)
    {
        uint8_t download_file_md5[64] = {0};
        uint8_t download_info_md5[64] = {0};
        hl_md5(OTA_DOWNLOAD_PATH, download_file_md5);
        hl_str2hex(info.packHash, strlen(info.packHash), download_info_md5);
#if MD5_DEBUG
        char src_md5_str[64] = {0};
        char dst_md5_str[64] = {0};
        for (int i = 0; i < 16; i++)
        {
            sprintf(src_md5_str + i * 2, "%02x", download_file_md5[i]);
            sprintf(dst_md5_str + i * 2, "%02x", download_info_md5[i]);
        }
        LOG_I("src_md5: %s\n", src_md5_str);
        LOG_I("dst_md5: %s\n", dst_md5_str);
#endif
        if (memcmp(download_file_md5, download_info_md5, 16) == 0)
        {
            fw_upgrading_status = DECRYPT_STATUS_START;
            LOG_I("ota start decryption\n");
        }
        else
        {
            remove(OTA_DOWNLOAD_PATH);
            LOG_I("ota MD5 failed\n");
            return -1;
        }
    }
    else
    {
        remove(OTA_DOWNLOAD_PATH);
        LOG_I("access %s failed\n", OTA_DOWNLOAD_PATH);
        return -1;
    }

    return 0;
}

static int fw_upgrading_download_failed(void)
{
    fw_upgrading_status = DOWNLOAD_STATUS_FAILED;
    if (download_ctx)
    {
        hl_curl_download_task_destory(&download_ctx);
    }
    ota_firmware_file_remove();
    return 0;
}

static int fw_upgrading_decrypt_start(void)
{
    char *decrypt_file = (char *)(upgrading_method == FW_UPDGRADING_BIN ? SDCARD_UPDATE_FIRMWARE_FILE_PATH : OTA_DOWNLOAD_PATH);
    if (ota_dec_thread_create(&dec_ctx, decrypt_file, OTA_UPDATE_DEC_AES_PATH) != 0)
    {
        LOG_E("ota_dec_thread_create failed\n");
        // ac_app_report_work_status(AC_APP_STATUS_FREE);
        ota_firmware_file_remove();
        // k2_pro_info_msgbox_push("", language_text[language_state_handle.lagnuage_switch][232], NULL);
        // lv_obj_add_flag(ota_info_page, LV_OBJ_FLAG_HIDDEN);
        fw_upgrading_status = UPDATE_STATUS_FAILED;
        return -1;
    }
    fw_upgrading_status = DECRYPT_STATUS_DECING;
    return 0;
}

static int fw_upgrading_decrypting(void)
{
    hl_ota_dec_state_t dec_state = HL_OTA_DEC_IDLE;
    if (dec_ctx)
    {
        dec_state = ota_dec_get_state(&dec_ctx);
        switch (dec_state)
        {
        case HL_OTA_DEC_COMPLETED:
        {
            LOG_I("ota decryption completed.\n");
            fw_upgrading_status = UNZIP_STATUS_START;
        }
        break;
        case HL_OTA_DEC_FAILED:
        {
            LOG_E("ota update_firmware_check failed\n");
            ota_firmware_file_remove();
            fw_upgrading_status = UPDATE_STATUS_FAILED;
            ota_dec_thread_destory(&dec_ctx);
        }
        break;
        }
    }
    return 0;
}

static int fw_upgrading_unzip_start(void)
{
    if (access(OTA_DOWNLOAD_PATH, F_OK) == 0)
    {
        hl_system("rm -r %s", OTA_DOWNLOAD_PATH);
        hl_system("sync");
    }
    if (access(OTA_UNZIP_PATH, F_OK) == 0)
    {
        hl_system("rm -r %s", OTA_UNZIP_PATH);
        hl_system("sync");
    }
    hl_system("mkdir -p %s", OTA_UNZIP_PATH);
    if (hl_unz_task_create(&unz_ctx, OTA_UPDATE_DEC_AES_PATH, OTA_UNZIP_PATH, NULL) == 0)
    {
        LOG_I("ota unzip create\n");
        fw_upgrading_status = UNZIP_STATUS_UNZIPING;
    }
    else
    {
        fw_upgrading_status = UNZIP_STATUS_FAILED;
    }
    return 0;
}

static int fw_upgrading_unzipping(void)
{
    hl_unz_state_t gunzip_state = HL_UNZ_STATE_IDLE;
    uint64_t gunzip_offset = 0;
    uint64_t gunzip_size = 0;

    gunzip_state = hl_unz_get_state(unz_ctx, &gunzip_offset, &gunzip_size);
    switch (gunzip_state)
    {
    case HL_UNZ_STATE_RUNNING:
    {
        if (gunzip_size > 0 && gunzip_offset > 0)
        {
            gunzip_progress = gunzip_offset * 100 / gunzip_size;
            LOG_D("gunzip_progress %llu \n", gunzip_progress);
        }
    }
    break;

    case HL_UNZ_STATE_COMPLETED: // 解压完成，应该在这开始升级
    {
        LOG_I("ota unzip completed.\n");
        fw_upgrading_status = UPDATE_STATUS_START;
        if (unz_ctx)
            hl_unz_task_destory(&unz_ctx);
    }
    break;

    case HL_UNZ_STATE_FAILED:
    {
        fw_upgrading_status = UNZIP_STATUS_FAILED;
        if (unz_ctx)
            hl_unz_task_destory(&unz_ctx);
        remove(OTA_DOWNLOAD_PATH);
    }
    break;
    }
    return 0;
}

static int fw_upgrading_update_start(void)
{
    char *update_file = (char *)(upgrading_method == FW_UPDGRADING_SWU ? LOCAL_UPGRADE_FILE_PATH : OTA_UNZIP_PATH);
    if (ota_progress_thread_create(&update_ctx) == 0)
    {
        LOG_I("ota start update %s\n", OTA_UNZIP_PATH);
        if (ota_update2(update_file) == -1)
        {
            fw_upgrading_status = UPDATE_STATUS_FAILED;
            ota_progress_thread_destory(&update_ctx);
        }
        else
        {
            fw_upgrading_status = UPDATE_STATUS_UPDATEING;
        }
    }
    else
    {
        LOG_E("ota_progress_thread_create failed\n");
        fw_upgrading_status = UPDATE_STATUS_FAILED;
    }
    return 0;
}

static int fw_upgrading_updating(void)
{
    struct progress_msg *progress_msg_info;

    progress_msg_info = get_ota_progress_msg(&update_ctx);
    if (progress_msg_info != NULL)
    {
        if (progress_msg_info->status == RUN)
        {
            ota_updating_progress = (progress_msg_info->cur_step - 1) * 100.0f / progress_msg_info->nsteps + progress_msg_info->cur_percent / (float)progress_msg_info->nsteps;
            LOG_I("ota progress %llu\n", ota_updating_progress);
        }
        else if (progress_msg_info->status == DONE)
        {
            fw_upgrading_status = UPDATE_STATUS_SUCCESS;
            ota_progress_thread_destory(&update_ctx);
            ota_firmware_file_remove();
            LOG_I("ota update success and try to reboot\n");
            // system("reboot -d3 &");
            hl_system("rm '%s'", CONFIG_PATH);
            hl_system("sync");
            system_reboot(3, false);
        }
        else if (progress_msg_info->status == FAILURE)
        {
            fw_upgrading_status = UPDATE_STATUS_FAILED;
            ota_progress_thread_destory(&update_ctx);
            // ota_firmware_file_remove();
            LOG_I("ota update failed\n");
        }
    }
    return 0;
}

/**
 * @Breif check if there is a newer version of FW in U disk or at cloud server
 *
 * @Returns
 */
bool has_newer_FW(void)
{
    bool has_newer_local_firmware = false;
    bool has_newer_cloud_firmware = false;
    /* currently unable to get version of local FW, so just check if a bin file or a swu file exist */
    has_newer_local_firmware = (access(LOCAL_UPGRADE_FILE_FULL_PATH, F_OK) == 0 || access(LOCAL_UPGRADE_FILE_BIN_PATH, F_OK) == 0);
    /* check if the version number of the FW at cloud is greater then the current FW */
    has_newer_cloud_firmware = is_ota_version_greater(JENKINS_VERSION) && hl_netif_wan_is_connected(HL_NET_INTERFACE_WLAN) && get_sysconf()->GetInt("system", "wifi", 0);

    return (has_newer_local_firmware || has_newer_cloud_firmware);
}

bool has_newer_localFW(void)
{
    return (access(LOCAL_UPGRADE_FILE_FULL_PATH, F_OK) == 0 || access(LOCAL_UPGRADE_FILE_BIN_PATH, F_OK) == 0);
}

bool has_newer_cloudFW(void)
{
    return (is_ota_version_greater(JENKINS_VERSION));
}

static void app_z_offset_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static float Zmax_value = 1.5;
    static float Zmin_value = -1;
    static double Zcurrent_value = 0;
    static int Zscale_index = 0;
    static float Zscale_value = 0;
    static bool init = false; 
    static double position_endstop_z = 0.0; //仅作为保存使用
    static uint64_t z_offset_save_tick = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        // if(!init)
        {
            ui_cb[get_z_offset_cb](&Zcurrent_value);
            Zcurrent_value = 0.0 - Zcurrent_value;
            position_endstop_z = Zcurrent_value;
            init = true;
// 定义一个很小的阈值，用于比较浮点数是否接近零
#define EPSILON 1e-10
            if (fabs(Zcurrent_value) < EPSILON)
                Zcurrent_value = 0;
            printf("z_compensate_callback:compensate_set=%f\n", Zcurrent_value);
        }
        lv_label_set_text_fmt(widget_list[WIDGET_ID_Z_OFFSET_LABEL_VALUE]->obj_container[0], "%.3fmm", Zcurrent_value);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_1]->obj_container[2], "0.025mm");
        lv_label_set_text_fmt(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_2]->obj_container[2], "0.010mm");
        lv_label_set_text_fmt(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_3]->obj_container[2], "0.005mm");

        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_1]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_2]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_3]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
        
        lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_BTN_CLOSE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_LABEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        if (Zscale_index == 0)
        {
            Zscale_value = 0.025;
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_1]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else if (Zscale_index == 1)
        {
            Zscale_value = 0.010;
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_2]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else if (Zscale_index == 2)
        {
            Zscale_value = 0.005;
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_3]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        break;
    case LV_EVENT_DESTROYED:
        if (position_endstop_z != Zcurrent_value)
        {
            ui_cb[save_z_offset_cb](&position_endstop_z);
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_Z_OFFSET_BTN_HELPER:
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_BTN_CLOSE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_LABEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_LABEL_TITLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_1]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_2]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_3]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_UP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_DOWN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_LABEL_VALUE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_HELPER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SAVE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            break;
        case WIDGET_ID_Z_OFFSET_HELPER_BTN_CLOSE:
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_MASK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_BTN_CLOSE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_LABEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_Z_OFFSET_HELPER_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_LABEL_TITLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_1]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_2]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_3]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_UP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_DOWN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_LABEL_VALUE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_HELPER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_Z_OFFSET_BTN_SAVE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            break;
        case WIDGET_ID_Z_OFFSET_BTN_SCALE_1:
        case WIDGET_ID_Z_OFFSET_BTN_SCALE_2:
        case WIDGET_ID_Z_OFFSET_BTN_SCALE_3:
            if (widget->header.index == WIDGET_ID_Z_OFFSET_BTN_SCALE_1)
                Zscale_index = 0;
            else if (widget->header.index == WIDGET_ID_Z_OFFSET_BTN_SCALE_2)
                Zscale_index = 1;
            else if (widget->header.index == WIDGET_ID_Z_OFFSET_BTN_SCALE_3)
                Zscale_index = 2;

            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_1]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_2]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_3]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
            if (Zscale_index == 0)
            {
                Zscale_value = 0.025;
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_1]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            }
            else if (Zscale_index == 1)
            {
                Zscale_value = 0.010;
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_2]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            }
            else if (Zscale_index == 2)
            {
                Zscale_value = 0.005;
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_Z_OFFSET_BTN_SCALE_3]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            }
            break;
        case WIDGET_ID_Z_OFFSET_BTN_DOWN:
            if (app_print_get_print_state() && (!app_print_get_print_busy() || !app_top_get_autoleveling_busy()))
            {
                if (Zcurrent_value >= Zmax_value)
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_Z_OFFSET_MAX_VALUE);
                else
                {
                    Zcurrent_value += Zscale_value;
                    if (Zcurrent_value > Zmax_value)
                        Zcurrent_value = Zmax_value;

                    if (fabs(Zcurrent_value) < 1e-6)
                        lv_label_set_text(widget_list[WIDGET_ID_Z_OFFSET_LABEL_VALUE]->obj_container[0], "0.000mm");
                    else
                        lv_label_set_text_fmt(widget_list[WIDGET_ID_Z_OFFSET_LABEL_VALUE]->obj_container[0], "%.3fmm", Zcurrent_value);
                }
                position_endstop_z = 0.0 - Zcurrent_value;
                ui_cb[set_z_offset_cb](&position_endstop_z);
            }
            else
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_NO_PRINT_Z_OFFSET);
            break;
        case WIDGET_ID_Z_OFFSET_BTN_UP:
            if (app_print_get_print_state() && (!app_print_get_print_busy() || !app_top_get_autoleveling_busy()))
            {
                if (Zcurrent_value <= Zmin_value)
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_setting_single_msgbox_callback, (void *)MSGBOX_TIP_Z_OFFSET_MIN_VALUE);
                else
                {
                    Zcurrent_value -= Zscale_value;
                    if (Zcurrent_value < Zmin_value)
                        Zcurrent_value = Zmin_value;

                    if (fabs(Zcurrent_value) < 1e-6)
                        lv_label_set_text(widget_list[WIDGET_ID_Z_OFFSET_LABEL_VALUE]->obj_container[0], "0.000mm");
                    else
                        lv_label_set_text_fmt(widget_list[WIDGET_ID_Z_OFFSET_LABEL_VALUE]->obj_container[0], "%.3fmm", Zcurrent_value);
                }
                position_endstop_z = 0.0 - Zcurrent_value;
                ui_cb[set_z_offset_cb](&position_endstop_z);
            }
            else
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_NO_PRINT_Z_OFFSET);
            break;
        }
    case LV_EVENT_PRESSED:
        switch (widget->header.index)
        {
            case WIDGET_ID_Z_OFFSET_BTN_SAVE:
                z_offset_save_tick = utils_get_current_tick();
                break;
        }
    case LV_EVENT_RELEASED:
        switch (widget->header.index)
        {
            case WIDGET_ID_Z_OFFSET_BTN_SAVE:
                if ((utils_get_current_tick() - z_offset_save_tick) > 10000)
                {
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_setting_routine_msgbox_callback, (void *)MSGBOX_TIP_Z_OFFSET_SAVE);
                }
                break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
}

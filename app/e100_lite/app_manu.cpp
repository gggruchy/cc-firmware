#if ENABLE_MANUTEST
#include "app_setting.h"
#include "app_network.h"
#include "app_camera.h"
#include "app_timezone.h"
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
#include "manu_test.h"


#define LOG_TAG "app_manu"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

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

extern int has_nofilament;
extern int stepperz_xw;
extern int stepperx_dz;
extern int steppery_dz;
extern int stepperz_dz;
extern int temp_box;
extern int temp_bed;
extern int speed_fan_board;

extern int wifi_strength;
extern int has_camera_uart;
extern int has_camera_usb;
extern int has_usb;
extern int stepperx_com;
extern int steppery_com;
extern int stepperz_com;
extern int steppere_com;
extern int has_extruder;
extern int has_straingauge;
extern int record_success;

extern int sg1;
extern int sg2;
extern int sg3;
extern int sg4;
extern int sg_triggered;

extern int temp_ex;
extern int acc_ex_flag;
extern double acc_ex_x;
extern double acc_ex_y;
extern double acc_ex_z;
extern int speed_fan_model;
extern int speed_fan_hg;

extern int manu_test_mode;



#define STR_SIZE 40
char temp_box_str[STR_SIZE];
char temp_bed_str[STR_SIZE];
char speed_fan_board_str[STR_SIZE];
char wifi_strength_str[STR_SIZE];

char speed_fan_model_str[STR_SIZE];
char speed_fan_hg_str[STR_SIZE];
char temp_ex_str[STR_SIZE];
char acc_ex_str[STR_SIZE];

char sg1_str[STR_SIZE];
char sg2_str[STR_SIZE];
char sg3_str[STR_SIZE];
char sg4_str[STR_SIZE];

static void mainboard_ui_update(widget_t **widget_list);
static void extruder_ui_update(widget_t **widget_list);
static void sg_ui_update(widget_t **widget_list);
static void demo_input();

void app_manu_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    uint64_t cumulative_time = 0;
    struct statvfs buffer;
    int hours, minutes, ret, memory_percentage;
    double total_space, free_space, used_space;
    char memory_space[64] = {0};

    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        LOG_D("!!!! start manu window !!!!\n");
        cumulative_time = get_sysconf()->GetInt("system", "cumulative_time", 0);
        hours = cumulative_time / 3600;
        minutes = (cumulative_time - hours * 3600) / 60;
        ret = statvfs("/user-resource", &buffer);

        //demo_input();

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

        //lv_img_set_src();

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
        switch (manu_test_mode)
        {
            case MANU_TEST_MODE_MAIN:
                mainboard_ui_update(widget_list);
                break;
            case MANU_TEST_MODE_EX:
                extruder_ui_update(widget_list);
                break;
            case MANU_TEST_MODE_SG:
                sg_ui_update(widget_list);
                break;
            default:
                mainboard_ui_update(widget_list);
                break;
        }
        break;
    }
}


static void demo_input()
{
    has_nofilament = 0;
    stepperx_dz = 0;
    steppery_dz = 0;
    stepperz_dz = 0;
    temp_box = 0;
    temp_bed = 0;
    speed_fan_board = 0;

    wifi_strength = 0;
    has_camera_uart = 0;
    has_camera_usb = 0;
    has_usb = 0;
    stepperx_com = 1;
    steppery_com = 1;
    stepperz_com = 1;
    has_extruder = 1;
    has_straingauge = 0;
}

static void mainboard_ui_update(widget_t **widget_list)
{
    char *img_src;
    /* left buttons */
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[2], "        断料检测");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[1], 0, 3);
    img_src = has_nofilament == 0 ? (char *)ui_get_image_src(187) : (char *)ui_get_image_src(188);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[2], "        X轴电机堵转");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[1], 0, 3);
    img_src = stepperx_dz == 0 ? (char *)ui_get_image_src(187) : (char *)ui_get_image_src(188);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[2], "        Y轴电机堵转");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[1], 0, 3);
    img_src = steppery_dz == 0 ? (char *)ui_get_image_src(187) : (char *)ui_get_image_src(188);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[2], "        Z轴电机堵转");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[1], 0, 3);
    img_src = stepperz_dz == 0 ? (char *)ui_get_image_src(187) : (char *)ui_get_image_src(188);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[1], img_src);


    sprintf(temp_box_str, "        箱体温度: %d", temp_box);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L5]->obj_container[2], temp_box_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L5]->obj_container[1], 0, 1);
    img_src = (temp_box < 26 || temp_box > 28) ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L5]->obj_container[1], img_src);

    sprintf(temp_bed_str, "        热床温度: %d", temp_bed);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L6]->obj_container[2], temp_bed_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L6]->obj_container[1], 0, 1);
    img_src = (temp_bed < 26 || temp_bed > 28) ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L6]->obj_container[1], img_src);

    sprintf(speed_fan_board_str, "        主板风扇风速: %d", speed_fan_board);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L7]->obj_container[2], speed_fan_board_str);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L9]->obj_container[2], "        Z轴限位检测");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L9]->obj_container[1], 0, 3);
    img_src = stepperz_xw == 0 ? (char *)ui_get_image_src(187) : (char *)ui_get_image_src(188);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L9]->obj_container[1], img_src);

    /* right buttons */
    sprintf(wifi_strength_str, "        WIFI: %d", wifi_strength);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R1]->obj_container[2], wifi_strength_str);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R1]->obj_container[1], ui_get_image_src(21));
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R2]->obj_container[1], 0, 2);

    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_CHIPID]->obj_container[1], 0, 1);
    img_src = record_success == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_CHIPID]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R2]->obj_container[2], "        摄像头串口");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R2]->obj_container[1], 0, 1);
    img_src = has_camera_uart == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R2]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R3]->obj_container[2], "        摄像头USB");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R3]->obj_container[1], 0, 1);
    img_src = has_camera_usb == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R3]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R4]->obj_container[2], "        USB");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R4]->obj_container[1], 0, 1);
    img_src = has_usb == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R4]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R5]->obj_container[2], "        X电机通信");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R5]->obj_container[1], 0, 1);
    img_src = stepperx_com == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R5]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R6]->obj_container[2], "        Y电机通信");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R6]->obj_container[1], 0, 1);
    img_src = steppery_com == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R6]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R7]->obj_container[2], "        Z电机通信");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R7]->obj_container[1], 0, 1);
    img_src = stepperz_com == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R7]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R8]->obj_container[2], "        喷头板通信");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R8]->obj_container[1], 0, 1);
    img_src = has_extruder == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R8]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_R9]->obj_container[2], "        应变片板通信");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_R9]->obj_container[1], 0, 1);
    img_src = has_straingauge == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_R9]->obj_container[1], img_src);
}

static void extruder_ui_update(widget_t **widget_list)
{
    char *img_src;
    /* left buttons */
    sprintf(temp_ex_str, "        喷头温度: %d", temp_ex);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[2], temp_ex_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[1], 0, 1);
    img_src = (temp_box < 24 || temp_box > 28) ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[1], img_src);

    sprintf(acc_ex_str, "        喷头加速度计通信");
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[2], acc_ex_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[1], 0, 1);
    img_src = acc_ex_flag == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[1], img_src);

    sprintf(speed_fan_model_str, "        模型风扇风速: %d", speed_fan_model);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[2], speed_fan_model_str);
    //lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[1], 0, 1);
    //img_src = speed_fan_model ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    //lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[1], img_src);

    sprintf(speed_fan_hg_str, "        喉管风扇风速: %d", speed_fan_hg);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[2], speed_fan_hg_str);
    //lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[1], 0, 1);
    //img_src = speed_fan_hg ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    //lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[1], img_src);

    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L5]->obj_container[2], "        E电机通信");
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L5]->obj_container[1], 0, 1);
    img_src = steppere_com == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L5]->obj_container[1], img_src);
}

static void sg_ui_update(widget_t **widget_list)
{
    char *img_src;
    /* left buttons */
    sprintf(sg1_str, "        检测到应变片按下");
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[2], sg1_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[1], 0, 1);
    img_src = sg_triggered == 0 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L1]->obj_container[1], img_src);

    /*
    sprintf(sg2_str, "        应变片2: %d", sg2);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[2], sg2_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[1], 0, 1);
    img_src = sg2 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L2]->obj_container[1], img_src);

    sprintf(sg3_str, "        应变片3: %d", sg3);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[2], sg3_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[1], 0, 1);
    img_src = sg3 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L3]->obj_container[1], img_src);

    sprintf(sg4_str, "        应变片4: %d", sg4);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[2], sg4_str);
    lv_obj_set_pos(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[1], 0, 1);
    img_src = sg4 ? (char *)ui_get_image_src(156) : (char *)ui_get_image_src(155);
    lv_img_set_src(widget_list[WIDGET_ID_MANU_BTN_L4]->obj_container[1], img_src);
    */
}
#endif

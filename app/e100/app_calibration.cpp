#include "app_calibration.h"
#include "klippy.h"
#include "hl_queue.h"
#include "auto_leveling.h"
#include "resonance_tester.h"
#include "pid_calibrate.h"


#define LOG_TAG "app_calibration"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#define AUTO_LEVEL_ITEM_NUMBER 36

enum
{
    MSGBOX_TIP_AUTO_LEVEL = 0,
    MSGBOX_TIP_VIBRATION_OPTIMIZE,
    MSGBOX_TIP_RESONANCE_TESTER_ERROR_X,
    MSGBOX_TIP_RESONANCE_TESTER_ERROR_Y,
    MSGBOX_TIP_BED_MESH_FALT,
    MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION,    //打印中暂不允许操作
    MSGBOX_TIP_EXECUTING_OTHER_TASK,
    MSGBOX_TIP_LEVEL_COMPLETE,
};

enum
{
    CALIBRATION_WINDOW_MIAN = 0,
    CALIBRATION_WINDOW_AUTO_LEVEL,
    CALIBRATION_WINDOW_PID_DETECTION,
    CALIBRATION_WINDOW_VIBRATION_OPTIMIZE,
    CALIBRATION_WINDOW_ONE_CLICK_DETECTION,
};
// 一键自检界面
static bool one_click_detection_check_all = false;
static bool one_click_detection_auto_level_state = false;
static bool one_click_detection_pid_detection_state = false;
static bool one_click_detection_vibration_optimization_state = false;
static int one_click_detection_inspection_index = 0; // 0默认 1检查中 2完成
static bool one_click_auto_level_inspection_result = false;
static bool one_click_pid_detection_inspection_resulte = false;
static bool one_click_vibration_optimization_inspection_result = false;
static int one_click_detection_flag = 0;
static int one_click_detection_state_flag = 0;
static bool app_calibration_over_msgbox_flag = false;
static bool app_calibration_routine_msgbox_flag = false;
static uint8_t vibration_optimize_routine_msgbox_err = 0;
static bool app_auto_level_msgbox_flag = false;
static bool app_auto_level_complete_state = false;
static void app_one_click_detection_update(widget_t **widget_list);

// PID检测/振纹优化界面

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
} ui_event_id_resonance_tester_t;

typedef enum
{
    UI_EVENT_ID_PID_CALIBRATE_ERROR = 0,
    UI_EVENT_ID_PID_CALIBRATE_PREHEAT_DONE,
    UI_EVENT_ID_PID_CALIBRATE_FINISH
} ui_event_id_pid_t;

static int calibration_pid_detection_state = 0;      // 0默认 1检查中 2检查结束
static int calibration_vibration_optimize_state = 0; // 0默认 1检查中 2检查结束
static int pid_detection_stage_index = 0;            // PID检测阶段0,1,2
static int vibration_optimize_stage_index = 0;       // 振纹检测阶段0,1,2
static int calibration_pid_detection_target_temperature = 200;
static hl_queue_t ui_resonance_tester_event_queue;
static hl_queue_t ui_pid_calibrate_event_queue;
static void ui_pid_calibrate_state_callback(int state);
void ui_pid_calibrate_update(widget_t **widget_list);
static void ui_resonace_tester_state_callback(int state);
void ui_vibration_compensation_update(widget_t **widget_list);

static void app_pid_vibration_callback(widget_t **widget_list);

// 自动调平界面
static app_listitem_model_t *auto_level_model = NULL;

static void auto_level_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void auto_level_listitem_update(void);
void ui_auto_level_page_update(widget_t **widget_list);
static void set_item_list_seq(void);
static std::vector<double> auto_level_vals;
typedef enum
{
    UI_EVENT_ID_PROBE_FALT,
    UI_EVENT_ID_BED_MESH_FALT,
    UI_EVENT_ID_PROBEING,
    UI_EVENT_ID_NUM
} ui_event_id_t;
typedef enum
{
    UI_EVENT_ID_AUTO_LEVELING_STATE_START = UI_EVENT_ID_NUM,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_PREHEAT,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_EXTURDE,
    UI_EVENT_ID_AUTO_LEVELING_STATE_START_PROBE,
    UI_EVENT_ID_AUTO_LEVELING_STATE_FINISH,
    UI_EVENT_ID_AUTO_LEVELING_STATE_ERROR,
} ui_event_id_auto_leveling_t;
static hl_queue_t ui_auto_level_event_queue = NULL;

static int calibration_window_index = 0;

static bool app_calibration_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_calibration_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_calibration_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
extern ConfigParser *get_sysconf();

void app_calibration_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        lv_obj_add_flag(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 0) // A面
        {
            lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_A]->obj_container[1],ui_get_image_src(268));
            lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_B]->obj_container[1],ui_get_image_src(269));
        }
        else
        {
            lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_B]->obj_container[1],ui_get_image_src(268));
            lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_A]->obj_container[1],ui_get_image_src(269));
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CHILD_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CALIBRATION_BTN_AUTO_LEVEL_ENTRY:
            if(app_print_get_print_state() == true)      //打印中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                // app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_calibration_routine_msgbox_callback, (void *)MSGBOX_TIP_AUTO_LEVEL);
                lv_obj_clear_flag(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
                widget_t **top_widget_list = window_get_top_widget_list();
                lv_obj_add_flag(top_widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case WIDGET_ID_CALIBRATION_BTN_PID_DETECTION_ENTRY:
            if(app_print_get_print_state() == true)    
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                calibration_window_index = CALIBRATION_WINDOW_PID_DETECTION;
                ui_set_window_index(WINDOW_ID_PID_VIBRATION, NULL);
            }
            break;
        case WIDGET_ID_CALIBRATION_BTN_VIBRATION_OPTIMIZATION_ENTRY:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_calibration_routine_msgbox_callback, (void *)MSGBOX_TIP_VIBRATION_OPTIMIZE);
            }
            break;
        case WIDGET_ID_CALIBRATION_BTN_ONE_CLICK_INSPECTION_ENTRY:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_calibration_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                ui_set_window_index(WINDOW_ID_ONE_CLICK_DETECTION, NULL);
            }
            break;
        case WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_A:
        case WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_B:
            // 保存配置
            if (widget->header.index == WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_A)
            {
                get_sysconf()->SetInt("system", "print_platform_type", 0);
                manual_control_sq.push("BED_MESH_SET_INDEX TYPE=standard INDEX=0");
                Printer::GetInstance()->manual_control_signal();
            }
            else
            {
                get_sysconf()->SetInt("system", "print_platform_type", 1);
                manual_control_sq.push("BED_MESH_SET_INDEX TYPE=enhancement INDEX=0");
                Printer::GetInstance()->manual_control_signal();
            }
            get_sysconf()->WriteIni(SYSCONF_PATH);

            //UI显示
            if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 0) // A面
            {
                lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_A]->obj_container[1],ui_get_image_src(268));
                lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_B]->obj_container[1],ui_get_image_src(269));
            }
            else
            {
                lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_B]->obj_container[1],ui_get_image_src(268));
                lv_img_set_src(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM_A]->obj_container[1],ui_get_image_src(269));
            }
            break;
        case WIDGET_ID_CALIBRATION_BTN_CANCEL:
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_CALIBRATION_CONTAINER_PRINT_PLATFORM]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
            widget_t **top_widget_list = window_get_top_widget_list();
            lv_obj_clear_flag(top_widget_list[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
            break;
        }
        case WIDGET_ID_CALIBRATION_BTN_CONFIRM:
        {
            ui_set_window_index(WINDOW_ID_AUTO_LEVEL, NULL);
            manual_control_sq.push("G29");
            Printer::GetInstance()->manual_control_signal();
            std::vector<double>().swap(auto_level_vals);
            break;
        }
        }
        break;
    case LV_EVENT_UPDATE:
        if (app_auto_level_msgbox_flag)
        {
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_calibration_routine_msgbox_callback, (void *)vibration_optimize_routine_msgbox_err);
            app_auto_level_msgbox_flag = false;
        }
        break;
    }
}

#if 1 // 自动调平界面

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
        if (!app_calibration_over_msgbox_flag)
        {
            // app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_calibration_over_msgbox_callback, (void *)MSGBOX_TIP_BED_MESH_FALT);
            app_calibration_over_msgbox_flag = true;
            vibration_optimize_routine_msgbox_err = MSGBOX_TIP_BED_MESH_FALT;
        }
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

void ui_auto_level_page_update(widget_t **widget_list)
{
    ui_event_id_auto_leveling_t ui_event;
    if (one_click_detection_inspection_index == 1)
    {
        while (hl_queue_dequeue(ui_auto_level_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START:
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START_PREHEAT:
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START_PROBE:
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_FINISH:
                one_click_auto_level_inspection_result = true;
                one_click_detection_state_flag |= 0x04;
                app_one_click_detection_update(widget_list);
                break;
            case UI_EVENT_ID_BED_MESH_FALT:
                if (!app_calibration_over_msgbox_flag)
                {
                    // app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_calibration_over_msgbox_callback, (void *)MSGBOX_TIP_BED_MESH_FALT);
                    app_calibration_over_msgbox_flag = true;
                    vibration_optimize_routine_msgbox_err = MSGBOX_TIP_BED_MESH_FALT;
                }
                one_click_auto_level_inspection_result = false;
                one_click_detection_state_flag |= 0x04;
                app_one_click_detection_update(widget_list);
                break;
            case UI_EVENT_ID_PROBEING:
                break;
            default:
                break;
            }
        }
    }
    else
    {
        while (hl_queue_dequeue(ui_auto_level_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START:
                lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LABEL_STATE]->obj_container[0], tr(34));
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START_PREHEAT:
                lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LABEL_STATE]->obj_container[0], tr(35));
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_START_PROBE:
                lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LABEL_STATE]->obj_container[0], tr(87));
                break;
            case UI_EVENT_ID_AUTO_LEVELING_STATE_FINISH:
                lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LABEL_STATE]->obj_container[0], "");
                lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_BTN_STOP]->obj_container[2], tr(89));
                lv_obj_clear_flag(widget_list[WIDGET_ID_AUTO_LEVEL_BTN_STOP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

                lv_slider_set_value(widget_list[WIDGET_ID_AUTO_LEVEL_SLIDER_PROGRESS]->obj_container[0], 100, LV_ANIM_OFF);
                if (!Printer::GetInstance()->get_exceptional_temp_status())
                {
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_calibration_routine_msgbox_callback, (void*)MSGBOX_TIP_LEVEL_COMPLETE);
                }
                LOG_I("Automatic leveling completed!\n");
                app_auto_level_complete_state = true;
                break;
            case UI_EVENT_ID_BED_MESH_FALT:
                if (!app_calibration_over_msgbox_flag)
                {
                    // app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_calibration_over_msgbox_callback, (void *)MSGBOX_TIP_BED_MESH_FALT);
                    app_calibration_over_msgbox_flag = true;
                    vibration_optimize_routine_msgbox_err = MSGBOX_TIP_BED_MESH_FALT;
                }
                one_click_auto_level_inspection_result = false;
                ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
                app_top_update_style(window_get_top_widget_list());
                break;
            case UI_EVENT_ID_PROBEING:
                auto_level_listitem_update();
                break;
            default:
                break;
            }
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
    case CMD_BEDMESH_PROBE_EXCEPTION:
        ui_event = UI_EVENT_ID_BED_MESH_FALT;
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    case AUTO_LEVELING_PROBEING:
        ui_event = UI_EVENT_ID_PROBEING;
        auto_level_vals.push_back(state.value);
        hl_queue_enqueue(ui_auto_level_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

void app_auto_level_callback(widget_t **widget_list, widget_t *widget, void *e)
{
#define TOTAL_TIME_TICK 20
    double hotbed_cur_temp = 0, hotbed_tar_temp = 0, extruder_cur_temp = 0, extruder_tar_temp = 0;
    static uint64_t start_tick = 0, number_tick = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        if (auto_level_model == NULL)
        {
            auto_level_model = app_listitem_model_create(WINDOW_ID_AUTO_LEVEL_LIST_TEMPLATE, widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_LIST]->obj_container[0], NULL, NULL);
            for (int i = 0; i < AUTO_LEVEL_ITEM_NUMBER; i++)
                app_listitem_model_push_back(auto_level_model);
            set_item_list_seq();
            if (ui_auto_level_event_queue == NULL)
                hl_queue_create(&ui_auto_level_event_queue, sizeof(ui_event_id_t), 8);
            auto_level_listitem_update();
            probe_register_state_callback(probe_state_callback);
            auto_leveling_register_state_callback(ui_auto_leveling_state_callback);
        }

#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_img_set_src(widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_PROGRESS]->obj_container[1], ui_get_image_src(354));
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_img_set_src(widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_PROGRESS]->obj_container[1], ui_get_image_src(265));
#endif

        // 设定流式对齐 对齐间隙
        lv_obj_set_flex_flow(widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_LIST]->obj_container[0], LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_row(widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_LIST]->obj_container[0], 7, LV_PART_MAIN);    // 对象之间 row 间隔
        lv_obj_set_style_pad_column(widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_LIST]->obj_container[0], 7, LV_PART_MAIN); // 对象之间 column 间隔
        lv_obj_set_style_pad_top(widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_LIST]->obj_container[0], 0, LV_PART_MAIN);    // 顶部间隙
        lv_obj_set_style_pad_left(widget_list[WIDGET_ID_AUTO_LEVEL_CONTAINER_LIST]->obj_container[0], 0, LV_PART_MAIN);   // 左边间隙

        lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_BTN_STOP]->obj_container[2], tr(88));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LABEL_STATE]->obj_container[0], tr(87));
        lv_obj_add_flag(widget_list[WIDGET_ID_AUTO_LEVEL_BTN_STOP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_AUTO_LEVEL_BTN_BG_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(widget_list[WIDGET_ID_AUTO_LEVEL_SLIDER_PROGRESS]->obj_container[0], 0, LV_PART_KNOB);

        lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LABEL_TIME]->obj_container[0],"%s", tr(342));
        number_tick = 0;
        app_auto_level_complete_state = false;
        break;
    case LV_EVENT_DESTROYED:
        probe_unregister_state_callback(probe_state_callback);
        auto_leveling_unregister_state_callback(ui_auto_leveling_state_callback);
        hl_queue_destory(&ui_auto_level_event_queue);
        app_listitem_model_destory(auto_level_model);
        auto_level_model = NULL;
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_AUTO_LEVEL_BTN_STOP:
            ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        ui_cb[get_bed_curent_temp_cb](&hotbed_cur_temp);
        ui_cb[get_bed_target_temp_cb](&hotbed_tar_temp);
        ui_cb[get_extruder1_curent_temp_cb](&extruder_cur_temp);
        ui_cb[get_extruder1_target_temp_cb](&extruder_tar_temp);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_BTN_SHOWER_TEMPERATURE]->obj_container[2], "%d/%d ℃", (int)extruder_cur_temp, (int)extruder_tar_temp);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_BTN_HOT_BED]->obj_container[2], "%d/%d ℃", (int)hotbed_cur_temp, (int)hotbed_tar_temp);
        ui_auto_level_page_update(widget_list);
        auto_level_listitem_update();

        if(utils_get_current_tick() - start_tick > 1 * 1000 && app_auto_level_complete_state == false)
        {
            start_tick = utils_get_current_tick();
            number_tick++;
            int process = number_tick * 100 / (TOTAL_TIME_TICK * 60);
            /* 经同产品确认,未完成时进度卡在95处等待自动调平完成 */
            if(process > 95)
                process = 95;
            lv_slider_set_value(widget_list[WIDGET_ID_AUTO_LEVEL_SLIDER_PROGRESS]->obj_container[0], process, LV_ANIM_OFF);
        }
        break;
    }
}

static void auto_level_listitem_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_LONG_PRESSED:
        lv_event_send(app_listitem_model_get_parent(auto_level_model), (lv_event_code_t)LV_EVENT_CHILD_LONG_PRESSED, widget);
        break;
    case LV_EVENT_CLICKED:
        lv_event_send(app_listitem_model_get_parent(auto_level_model), (lv_event_code_t)LV_EVENT_CHILD_CLICKED, widget);
        break;
    }
}

static void set_item_list_seq(void)
{
    // // 反转数组
    for (int i = 0; i < 36 / 2; i++)
    {
        app_listitem_t temp = auto_level_model->item_list[i];
        auto_level_model->item_list[i] = auto_level_model->item_list[36 - 1 - i];
        auto_level_model->item_list[36 - 1 - i] = temp;
    }
    int rows = 6;
    int cols = 6;
    for (int i = 0; i < rows; i++)
    {
        if (i % 2 == 0)
        {
            for (int j = 0; j < cols / 2; j++)
            {
                app_listitem_t temp = auto_level_model->item_list[i * cols + j];
                auto_level_model->item_list[i * cols + j] = auto_level_model->item_list[i * cols + cols - 1 - j];
                auto_level_model->item_list[i * cols + cols - 1 - j] = temp;
            }
        }
    }
}

static void auto_level_listitem_update(void)
{
    for (int i = 0; i < AUTO_LEVEL_ITEM_NUMBER; i++)
    {
        app_listitem_t *item = app_listitem_model_get_item(auto_level_model, i);
        window_t *win = item->win;
        widget_t **widget_list = win->widget_list;
        if (i < auto_level_vals.size())
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LIST_TEMPLATE_BTN_ITEM]->obj_container[2], "%.2f", auto_level_vals[i]);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_AUTO_LEVEL_LIST_TEMPLATE_BTN_ITEM]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_AUTO_LEVEL_LIST_TEMPLATE_BTN_ITEM]->obj_container[2], "%d", i + 1);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_AUTO_LEVEL_LIST_TEMPLATE_BTN_ITEM]->obj_container[0], lv_color_hex(0xFF383837), LV_PART_MAIN);
        }
    }
}
#endif

#if 1 // 一键自检界面

void app_one_click_detection_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_CONTAINER]->obj_container[1], ui_get_image_src(360));
#elif CONFIG_BOARD_E100 == BOARD_E100
        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_CONTAINER]->obj_container[1], ui_get_image_src(134));
#endif
        one_click_detection_inspection_index = 0;
        one_click_detection_check_all = false;
        one_click_detection_auto_level_state = false;
        one_click_detection_pid_detection_state = false;
        one_click_detection_vibration_optimization_state = false;
        app_one_click_detection_update(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        one_click_detection_flag = 0;
        one_click_detection_state_flag = 0;
        if (one_click_detection_auto_level_state)
        {
            if(ui_auto_level_event_queue != NULL)
            {
                hl_queue_destory(&ui_auto_level_event_queue);
            }
            probe_unregister_state_callback(probe_state_callback);
            auto_leveling_unregister_state_callback(ui_auto_leveling_state_callback);
        }
        if (one_click_detection_pid_detection_state)
        {
            if(ui_pid_calibrate_event_queue != NULL)
            {
                hl_queue_destory(&ui_pid_calibrate_event_queue);
            }
            pid_calibrate_unregister_state_callback(ui_pid_calibrate_state_callback);
        }
        if (one_click_detection_vibration_optimization_state)
        {
            if(ui_resonance_tester_event_queue != NULL)
            {
                hl_queue_destory(&ui_resonance_tester_event_queue);
            }
            resonace_tester_unregister_state_callback(ui_resonace_tester_state_callback);
        }
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_ONE_CLICK_DETECTION_BTN_BACK:
            ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        case WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION:
            if (one_click_detection_inspection_index == 0)
            {
                if (one_click_detection_auto_level_state || one_click_detection_pid_detection_state || one_click_detection_vibration_optimization_state)
                {
                    one_click_detection_flag = 0;
                    one_click_detection_state_flag = 0;
                    one_click_detection_inspection_index = 1;
                    app_one_click_detection_update(widget_list);
                    if (one_click_detection_vibration_optimization_state)
                    {
                        manual_control_sq.push("SHAPER_CALIBRATE_SCRIPT");
                        if (ui_resonance_tester_event_queue == NULL)
                            hl_queue_create(&ui_resonance_tester_event_queue, sizeof(ui_event_id_resonance_tester_t), 8);
                        resonace_tester_register_state_callback(ui_resonace_tester_state_callback);
                        one_click_detection_flag |= 0x01;
                    }
                    if (one_click_detection_pid_detection_state)
                    {
                        manual_control_sq.push("PID_CALIBRATE_NOZZLE TARGET_TEMP=220");
                        if (ui_pid_calibrate_event_queue == NULL)
                            hl_queue_create(&ui_pid_calibrate_event_queue, sizeof(ui_event_id_pid_t), 8);
                        pid_calibrate_register_state_callback(ui_pid_calibrate_state_callback);
                        one_click_detection_flag |= 0x02;
                    }
                    if (one_click_detection_auto_level_state)
                    {
                        manual_control_sq.push("G29");
                        if (ui_auto_level_event_queue == NULL)
                            hl_queue_create(&ui_auto_level_event_queue, sizeof(ui_event_id_t), 8);
                        probe_register_state_callback(probe_state_callback);
                        auto_leveling_register_state_callback(ui_auto_leveling_state_callback);
                        one_click_detection_flag |= 0x04;
                    }
                    Printer::GetInstance()->manual_control_signal();
                }
            }
            else if (one_click_detection_inspection_index == 2)
            {
                ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
                app_top_update_style(window_get_top_widget_list());
            }
            break;
        case WIDGET_ID_ONE_CLICK_DETECTION_BTN_CHECK_ALL:
            one_click_detection_check_all = !one_click_detection_check_all;
            if (one_click_detection_check_all)
            {
                one_click_detection_auto_level_state = true;
                one_click_detection_pid_detection_state = true;
                one_click_detection_vibration_optimization_state = true;
            }
            else
            {
                one_click_detection_auto_level_state = false;
                one_click_detection_pid_detection_state = false;
                one_click_detection_vibration_optimization_state = false;
            }
            app_one_click_detection_update(widget_list);
            break;
        case WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION:
        case WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION:
        case WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL:
            if (widget->header.index == WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION)
                one_click_detection_vibration_optimization_state = !one_click_detection_vibration_optimization_state;
            else if (widget->header.index == WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION)
                one_click_detection_pid_detection_state = !one_click_detection_pid_detection_state;
            else if (widget->header.index == WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL)
                one_click_detection_auto_level_state = !one_click_detection_auto_level_state;

            if (one_click_detection_auto_level_state && one_click_detection_pid_detection_state && one_click_detection_vibration_optimization_state)
                one_click_detection_check_all = true;
            else
                one_click_detection_check_all = false;
            app_one_click_detection_update(widget_list);
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        if (one_click_detection_inspection_index == 1)
        {
            if (one_click_detection_flag & 0x01)
                ui_vibration_compensation_update(widget_list);
            if (one_click_detection_flag & 0x02)
                ui_pid_calibrate_update(widget_list);
            if (one_click_detection_flag & 0x04)
                ui_auto_level_page_update(widget_list);
            if (one_click_detection_flag == one_click_detection_state_flag)
            {
                one_click_detection_inspection_index = 2;
                app_one_click_detection_update(widget_list);
            }
        }
        if (app_calibration_routine_msgbox_flag)
        {
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_calibration_routine_msgbox_callback, (void *)vibration_optimize_routine_msgbox_err);
            app_calibration_routine_msgbox_flag = false;
        }
        break;
    }
}

static void rotate_animation_cb(void *var, int32_t v)
{
    lv_img_set_angle((lv_obj_t *)var, v);
}

static void app_one_click_detection_update(widget_t **widget_list)
{
    if (one_click_detection_inspection_index == 0)
    {
        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_VIBRATION_OPTIMIZATION]->obj_container[1],ui_get_image_src(136));
        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[1],ui_get_image_src(137));
        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[1],ui_get_image_src(138));
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_VIBRATION_OPTIMIZATION]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);

        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_CHECK_ALL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_IMAGE_FORBIDDEN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[2], tr(93));
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_BACK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[0], 86);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_VIBRATION_OPTIMIZATION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_PID_DETECTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_AUTO_LEVEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_FORBIDDEN_LEFT_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        if (one_click_detection_check_all)
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_CHECK_ALL]->obj_container[1], ui_get_image_src(139));
        else
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_CHECK_ALL]->obj_container[1], ui_get_image_src(140));
        if (one_click_detection_vibration_optimization_state)
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(139));
        else
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(140));
        if (one_click_detection_pid_detection_state)
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(139));
        else
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(140));
        if (one_click_detection_auto_level_state)
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(139));
        else
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(140));

        if (one_click_detection_auto_level_state || one_click_detection_pid_detection_state || one_click_detection_vibration_optimization_state)
            lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        else
            lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
    }
    else if (one_click_detection_inspection_index == 1)
    {
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_CHECK_ALL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_IMAGE_FORBIDDEN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[2], tr(94));
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_BACK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[0], 14);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_FORBIDDEN_LEFT_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        if(one_click_detection_state_flag == 0)
        {
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(143));
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(143));
            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(143));

            lv_anim_t rotate_anim[3];
            lv_obj_t *rotate_obj[3];
            rotate_obj[0] = widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1];
            rotate_obj[1] = widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1];
            rotate_obj[2] = widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1];
            for (int i = 0; i < 3; i++)
            {
                // 设置图片旋转动画
                lv_anim_init(&rotate_anim[i]);
                lv_anim_set_values(&rotate_anim[i], 0, 3600);
                lv_anim_set_time(&rotate_anim[i], 1000);
                lv_anim_set_exec_cb(&rotate_anim[i], rotate_animation_cb);
                lv_anim_set_path_cb(&rotate_anim[i], lv_anim_path_linear);
                lv_anim_set_var(&rotate_anim[i], rotate_obj[i]);
                lv_anim_set_repeat_count(&rotate_anim[i], LV_ANIM_REPEAT_INFINITE);
                lv_anim_start(&rotate_anim[i]);
            }
        }

        // 改变位置与ui显示
        int one_click_detection_flag = 0;
        if(one_click_detection_vibration_optimization_state)
            one_click_detection_flag |= 0x01;
        if(one_click_detection_pid_detection_state)
            one_click_detection_flag |= 0x02;
        if(one_click_detection_auto_level_state)
            one_click_detection_flag |= 0x04;
        switch(one_click_detection_flag)
        {
            case 1: 
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_PID_DETECTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_AUTO_LEVEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                
                if (one_click_detection_state_flag & 0x01)
                {
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], 0);
                    if (one_click_vibration_optimization_inspection_result)
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(141));
                    else
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(142));
                
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], 0);
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], 0);
                }
                break;
            case 2:
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_VIBRATION_OPTIMIZATION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_AUTO_LEVEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_y(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_PID_DETECTION]->obj_container[0],39);
                
                if (one_click_detection_state_flag & 0x02)
                {
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], 0);
                    if (one_click_pid_detection_inspection_resulte)
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(141));
                    else
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(142));
                
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], 0);
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], 0);
                }
                break;
            case 3:
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_AUTO_LEVEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                
                switch(one_click_detection_state_flag)
                {
                    case 0:
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[1],ui_get_image_src(247));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[0], lv_color_hex(0xFF8E8E8F), LV_PART_MAIN);
                        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 1:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], 0);
                        if (one_click_vibration_optimization_inspection_result)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(142));
                        
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[1],ui_get_image_src(137));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 3:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], 0);
                        if (one_click_pid_detection_inspection_resulte)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(142));
                        
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], 0);       
                        break;
                }
                break;
            case 4:
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_VIBRATION_OPTIMIZATION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_PID_DETECTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_y(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_AUTO_LEVEL]->obj_container[0],39);
                
                if (one_click_detection_state_flag & 0x04)
                {
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], 0);
                    if (one_click_auto_level_inspection_result)
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(141));
                    else
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(142));
                
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], 0);
                    lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], &rotate_animation_cb);
                    lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], 0);
                }
                break;
            case 5:
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_PID_DETECTION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_y(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_AUTO_LEVEL]->obj_container[0],76);
                
                switch(one_click_detection_state_flag)
                {
                    case 0:
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[1],ui_get_image_src(247));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[0], lv_color_hex(0xFF8E8E8F), LV_PART_MAIN);
                        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 1:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], 0);
                        if (one_click_vibration_optimization_inspection_result)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(142));
                        
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[1],ui_get_image_src(138));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 5:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], 0);
                        if (one_click_auto_level_inspection_result)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(142));
                        
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], 0);
                        break;
                }
                break;
            case 6:
                lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_VIBRATION_OPTIMIZATION]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_y(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_PID_DETECTION]->obj_container[0],39);
                lv_obj_set_y(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_ITEM_AUTO_LEVEL]->obj_container[0],76);
                
                switch(one_click_detection_state_flag)
                {
                    case 0:
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[1],ui_get_image_src(247));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[0], lv_color_hex(0xFF8E8E8F), LV_PART_MAIN);
                        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 2:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], 0);
                        if (one_click_pid_detection_inspection_resulte)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(142));
                        
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[1],ui_get_image_src(138));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 6:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], 0);
                        if (one_click_auto_level_inspection_result)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(142));
                        
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], 0);
                        break;
                }             
                break;
            case 7:
                switch(one_click_detection_state_flag)
                {
                    case 0:
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[1],ui_get_image_src(246));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[0], lv_color_hex(0xFF8E8E8F), LV_PART_MAIN);
                        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[1],ui_get_image_src(247));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[0], lv_color_hex(0xFF8E8E8F), LV_PART_MAIN);
                        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 1:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], 0);
                        if (one_click_vibration_optimization_inspection_result)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_VIBRATION_OPTIMIZATION]->obj_container[1], ui_get_image_src(142));
                        
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[1],ui_get_image_src(137));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_PID_DETECTION]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 3:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], 0);
                        if (one_click_pid_detection_inspection_resulte)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_PID_DETECTION]->obj_container[1], ui_get_image_src(142));
                        
                        lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[1],ui_get_image_src(138));
                        lv_obj_set_style_text_color(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INFO_AUTO_LEVEL]->obj_container[0], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                        lv_obj_clear_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1],LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 7:
                        lv_anim_del(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], &rotate_animation_cb);
                        lv_img_set_angle(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], 0);
                        if (one_click_auto_level_inspection_result)
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(141));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_STATE_AUTO_LEVEL]->obj_container[1], ui_get_image_src(142));
                        break;
                }
                break;
        }
    }
    else if (one_click_detection_inspection_index == 2)
    {
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_CHECK_ALL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_IMAGE_FORBIDDEN]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[2], tr(89));
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_BACK]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[0], 14);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_INSPECTION]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(widget_list[WIDGET_ID_ONE_CLICK_DETECTION_BTN_FORBIDDEN_LEFT_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    }
}
#endif

#if 1

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
    default:
        break;
    }
}

static void ui_pid_calibrate_state_callback(int state)
{
    ui_event_id_pid_t ui_event;
    switch (state)
    {
    case PID_CALIBRATE_STATE_ERROR:
        ui_event = UI_EVENT_ID_PID_CALIBRATE_ERROR;
        hl_queue_enqueue(ui_pid_calibrate_event_queue, &ui_event, 1);
        break;
    case PID_CALIBRATE_STATE_PREHEAT_DONE:
        ui_event = UI_EVENT_ID_PID_CALIBRATE_PREHEAT_DONE;
        hl_queue_enqueue(ui_pid_calibrate_event_queue, &ui_event, 1);
        break;
    case PID_CALIBRATE_STATE_FINISH:
        ui_event = UI_EVENT_ID_PID_CALIBRATE_FINISH;
        hl_queue_enqueue(ui_pid_calibrate_event_queue, &ui_event, 1);
        break;
    default:
        break;
    }
}

void ui_vibration_compensation_update(widget_t **widget_list)
{
    ui_event_id_resonance_tester_t ui_event;
    if (one_click_detection_inspection_index == 1)
    {

        while (hl_queue_dequeue(ui_resonance_tester_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_RESONANCE_TESTER_ERROR_X:
                if (!app_calibration_over_msgbox_flag)
                {
                    app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_calibration_over_msgbox_callback, (void *)MSGBOX_TIP_RESONANCE_TESTER_ERROR_X);
                    app_calibration_over_msgbox_flag = true;
                    vibration_optimize_routine_msgbox_err = MSGBOX_TIP_RESONANCE_TESTER_ERROR_X;
                }
                one_click_vibration_optimization_inspection_result = false;
                one_click_detection_state_flag |= 0x01;
                app_one_click_detection_update(widget_list);

                break;
            case UI_EVENT_ID_RESONANCE_TESTER_ERROR_Y:
                if (!app_calibration_over_msgbox_flag)
                {
                    app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_calibration_over_msgbox_callback, (void *)MSGBOX_TIP_RESONANCE_TESTER_ERROR_Y);
                    app_calibration_over_msgbox_flag = true;
                    vibration_optimize_routine_msgbox_err = MSGBOX_TIP_RESONANCE_TESTER_ERROR_Y;
                }
                one_click_vibration_optimization_inspection_result = false;
                one_click_detection_state_flag |= 0x01;
                app_one_click_detection_update(widget_list);
                // one_btn_msgbox_push("振动校准异常", "振动校准过程异常，请检查加速度计和接线", NULL);
                // lv_scr_load(ui_menu_bar);
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_START:
                vibration_optimize_stage_index = 1;
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_START_Y:
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_FINISH:
                one_click_vibration_optimization_inspection_result = true;
                one_click_detection_state_flag |= 0x01;
                app_one_click_detection_update(widget_list);
                break;
            default:
                break;
            }
        }
    }
    else
    {
        while (hl_queue_dequeue(ui_resonance_tester_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_RESONANCE_TESTER_ERROR_X:
                if (!app_calibration_over_msgbox_flag)
                {
                    app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_calibration_over_msgbox_callback, (void *)MSGBOX_TIP_RESONANCE_TESTER_ERROR_X);
                    app_calibration_over_msgbox_flag = true;
                    vibration_optimize_routine_msgbox_err = MSGBOX_TIP_RESONANCE_TESTER_ERROR_X;
                }
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_ERROR_Y:
                if (!app_calibration_over_msgbox_flag)
                {
                    app_msgbox_push(WINDOW_ID_OVER_MSGBOX, true, app_calibration_over_msgbox_callback, (void *)MSGBOX_TIP_RESONANCE_TESTER_ERROR_Y);
                    app_calibration_over_msgbox_flag = true;
                    vibration_optimize_routine_msgbox_err = MSGBOX_TIP_RESONANCE_TESTER_ERROR_Y;
                }
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_START:
                vibration_optimize_stage_index = 1;
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_START_Y:
                break;
            case UI_EVENT_ID_RESONANCE_TESTER_FINISH:
                vibration_optimize_stage_index = 2;
                break;
            default:
                break;
            }
        }
    }
}

void ui_pid_calibrate_update(widget_t **widget_list)
{
    ui_event_id_pid_t ui_event;
    if (one_click_detection_inspection_index == 1)
    {
        while (hl_queue_dequeue(ui_pid_calibrate_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_PID_CALIBRATE_ERROR:
                one_click_pid_detection_inspection_resulte = false;
                one_click_detection_state_flag |= 0x02;
                app_one_click_detection_update(widget_list);
                break;
            case UI_EVENT_ID_PID_CALIBRATE_PREHEAT_DONE:
                pid_detection_stage_index = 1;
                break;
            case UI_EVENT_ID_PID_CALIBRATE_FINISH:
                one_click_pid_detection_inspection_resulte = true;
                one_click_detection_state_flag |= 0x02;
                app_one_click_detection_update(widget_list);
                break;
            default:
                break;
            }
        }
    }
    else
    {
        while (hl_queue_dequeue(ui_pid_calibrate_event_queue, &ui_event, 1))
        {
            switch (ui_event)
            {
            case UI_EVENT_ID_PID_CALIBRATE_ERROR:
                break;
            case UI_EVENT_ID_PID_CALIBRATE_PREHEAT_DONE:
                pid_detection_stage_index = 1;
                break;
            case UI_EVENT_ID_PID_CALIBRATE_FINISH:
                pid_detection_stage_index = 2;
                break;
            default:
                break;
            }
        }
    }
}

void app_pid_vibration_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        if (calibration_window_index == CALIBRATION_WINDOW_PID_DETECTION)
        {
            if (ui_pid_calibrate_event_queue == NULL)
                hl_queue_create(&ui_pid_calibrate_event_queue, sizeof(ui_event_id_pid_t), 8);
            calibration_pid_detection_state = 0;
            pid_calibrate_register_state_callback(ui_pid_calibrate_state_callback);
        }
        else if (calibration_window_index == CALIBRATION_WINDOW_VIBRATION_OPTIMIZE)
        {
            if (ui_resonance_tester_event_queue == NULL)
                hl_queue_create(&ui_resonance_tester_event_queue, sizeof(ui_event_id_resonance_tester_t), 8);
            resonace_tester_register_state_callback(ui_resonace_tester_state_callback);
            calibration_vibration_optimize_state = 1;
            vibration_optimize_stage_index = 0;
            
            manual_control_sq.push("SHAPER_CALIBRATE_SCRIPT");
            Printer::GetInstance()->manual_control_signal();

        }
        app_pid_vibration_callback(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        if (calibration_window_index == CALIBRATION_WINDOW_VIBRATION_OPTIMIZE)
        {
            resonace_tester_unregister_state_callback(ui_resonace_tester_state_callback);
            hl_queue_destory(&ui_resonance_tester_event_queue);
        }
        else if (calibration_window_index == CALIBRATION_WINDOW_PID_DETECTION)
        {
            pid_calibrate_unregister_state_callback(ui_pid_calibrate_state_callback);
            hl_queue_destory(&ui_pid_calibrate_event_queue);
        }
        lv_obj_clear_flag(window_get_top_widget_list()[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE1:
            calibration_pid_detection_target_temperature = 200;
            app_pid_vibration_callback(widget_list);
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE2:
            calibration_pid_detection_target_temperature = 220;
            app_pid_vibration_callback(widget_list);
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE3:
            calibration_pid_detection_target_temperature = 240;
            app_pid_vibration_callback(widget_list);
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE4:
            calibration_pid_detection_target_temperature = 260;
            app_pid_vibration_callback(widget_list);
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_MINUS:
            if (calibration_pid_detection_target_temperature > 190)
                calibration_pid_detection_target_temperature -= 10;
            app_pid_vibration_callback(widget_list);
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_PLUS:
            if (calibration_pid_detection_target_temperature < 320)
                calibration_pid_detection_target_temperature += 10;
            app_pid_vibration_callback(widget_list);
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_START_DETECTION:
            if (calibration_window_index == CALIBRATION_WINDOW_PID_DETECTION)
            {
                calibration_pid_detection_state = 1;
                pid_detection_stage_index = 0;
                char cmd[100];
                sprintf(cmd, "PID_CALIBRATE_NOZZLE TARGET_TEMP=%d", calibration_pid_detection_target_temperature);
                manual_control_sq.push(cmd);
                Printer::GetInstance()->manual_control_signal();

                lv_obj_add_flag(window_get_top_widget_list()[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);      
            }
            app_pid_vibration_callback(widget_list);
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE:
            if (calibration_pid_detection_state == 2 || calibration_vibration_optimize_state == 2)
            {
                ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
                app_top_update_style(window_get_top_widget_list());
            }
            break;
        case WIDGET_ID_PID_VIBRATION_BTN_PID_SET_BACK:
            ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
            app_top_update_style(window_get_top_widget_list());
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        lv_obj_move_background(widget_list[WIDGET_ID_PID_VIBRATION_IMAGE_DETECTION_STAGE]->obj_container[0]);
        if (calibration_window_index == CALIBRATION_WINDOW_VIBRATION_OPTIMIZE)
            lv_obj_add_flag(window_get_top_widget_list()[WIDGET_ID_TOP_CONTAINER_UPPER_BAR]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        if (calibration_pid_detection_state == 0)
        {
            app_pid_vibration_callback(widget_list);
        }
        if (calibration_pid_detection_state == 1)
        {
            ui_pid_calibrate_update(widget_list);
            app_pid_vibration_callback(widget_list);
        }
        if (calibration_vibration_optimize_state == 1)
        {
            ui_vibration_compensation_update(widget_list);
            app_pid_vibration_callback(widget_list);
        }
        if (app_calibration_routine_msgbox_flag)
        {
            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_calibration_routine_msgbox_callback, (void *)vibration_optimize_routine_msgbox_err);
            app_calibration_routine_msgbox_flag = false;
        }
        break;
    }
}

static void app_pid_vibration_callback(widget_t **widget_list)
{
    double extruder_cur_temp = 0;
    if (calibration_window_index == CALIBRATION_WINDOW_PID_DETECTION)
    {
        if (calibration_pid_detection_state == 0)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_PID_VIBRATION_CONTAINER_PID_SET_TEMPERATURE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_PID_VIBRATION_CONTAINER_BG_DETECTION_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE1]->obj_container[2], "200℃");
            lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE2]->obj_container[2], "220℃");
            lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE3]->obj_container[2], "240℃");
            lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_PID_SET_TEMPERATURE4]->obj_container[2], "260℃");
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PID_VIBRATION_BTN_PID_TARGET_TEMPERATURE]->obj_container[2], "%d℃", calibration_pid_detection_target_temperature);
            ui_cb[get_extruder1_curent_temp_cb](&extruder_cur_temp);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_PID_VIBRATION_BTN_PID_TEMPERATURE_VALUE]->obj_container[2], "%d/%d℃", (int)extruder_cur_temp, calibration_pid_detection_target_temperature);
        }
        else if (calibration_pid_detection_state == 1)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_PID_VIBRATION_CONTAINER_PID_SET_TEMPERATURE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PID_VIBRATION_CONTAINER_BG_DETECTION_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_ITEM_INDO]->obj_container[1], ui_get_image_src(137));
            lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_ITEM_INDO]->obj_container[2], tr(92));

            if (pid_detection_stage_index == 0)
            {
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_IMAGE_DETECTION_STAGE]->obj_container[0], ui_get_image_src(147));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[2], tr(107));
                lv_obj_clear_flag(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[1], ui_get_image_src(150));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[2], tr(104));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[1], ui_get_image_src(151));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[2], tr(106));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[1], ui_get_image_src(152));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[2], tr(99));
            }
            else if (pid_detection_stage_index == 1)
            {
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_IMAGE_DETECTION_STAGE]->obj_container[0], ui_get_image_src(148));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[2], tr(107));
                lv_obj_clear_flag(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[1], ui_get_image_src(150));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[2], tr(104));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[1], ui_get_image_src(154));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[2], tr(106));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[1], ui_get_image_src(152));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[2], tr(99));
            }
            else if (pid_detection_stage_index == 2)
            {
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_IMAGE_DETECTION_STAGE]->obj_container[0], ui_get_image_src(149));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[2], tr(89));
                lv_obj_add_flag(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[1], ui_get_image_src(150));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[2], tr(104));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[1], ui_get_image_src(154));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[2], tr(106));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[1], ui_get_image_src(155));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[2], tr(99));
                calibration_pid_detection_state = 2;
            }
        }
        else if (calibration_pid_detection_state == 2)
        {
        }
    }
    else if (calibration_window_index == CALIBRATION_WINDOW_VIBRATION_OPTIMIZE)
    {
        if (calibration_vibration_optimize_state == 1)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_PID_VIBRATION_CONTAINER_PID_SET_TEMPERATURE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_PID_VIBRATION_CONTAINER_BG_DETECTION_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_ITEM_INDO]->obj_container[1], ui_get_image_src(136));
            lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_ITEM_INDO]->obj_container[2], tr(97));

            if (vibration_optimize_stage_index == 0)
            {
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_IMAGE_DETECTION_STAGE]->obj_container[0], ui_get_image_src(147));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[2], tr(107));
                lv_obj_clear_flag(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[1], ui_get_image_src(150));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[2], tr(98));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[1], ui_get_image_src(151));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[2], tr(97));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[1], ui_get_image_src(152));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[2], tr(99));
            }
            else if (vibration_optimize_stage_index == 1)
            {
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_IMAGE_DETECTION_STAGE]->obj_container[0], ui_get_image_src(148));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[2], tr(107));
                lv_obj_clear_flag(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[1], ui_get_image_src(155));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[2], tr(100));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[1], ui_get_image_src(154));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[2], tr(97));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[1], ui_get_image_src(152));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[2], tr(99));
            }
            else if (vibration_optimize_stage_index == 2)
            {
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_IMAGE_DETECTION_STAGE]->obj_container[0], ui_get_image_src(149));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[2], tr(89));
                lv_obj_add_flag(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[1], ui_get_image_src(155));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE1]->obj_container[2], tr(100));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[1], ui_get_image_src(154));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE2]->obj_container[2], tr(97));
                lv_img_set_src(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[1], ui_get_image_src(155));
                lv_label_set_text(widget_list[WIDGET_ID_PID_VIBRATION_BTN_DETECTION_STAGE3]->obj_container[2], tr(99));
                calibration_vibration_optimize_state = 2;
            }
        }
        else if (calibration_vibration_optimize_state == 2)
        {
        }
    }
}

#endif

static bool app_calibration_over_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
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
        if (tip_index == MSGBOX_TIP_RESONANCE_TESTER_ERROR_X)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：401,%s", tr(317));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_RESONANCE_TESTER_ERROR_Y)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：402,%s", tr(317));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_BED_MESH_FALT)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_OVER_MSGBOX_LABEL_CONTENT]->obj_container[0], "ErrorCode：502,%s", tr(213));
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
            app_calibration_routine_msgbox_flag = true;
            ret = true;
            if (tip_index == MSGBOX_TIP_BED_MESH_FALT)
                app_auto_level_msgbox_flag = true;
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return ret;
}

static bool app_calibration_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_AUTO_LEVEL)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(86));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_VIBRATION_OPTIMIZE)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(299));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(45));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if (tip_index == MSGBOX_TIP_RESONANCE_TESTER_ERROR_X)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(318));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == MSGBOX_TIP_RESONANCE_TESTER_ERROR_Y)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(318));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == MSGBOX_TIP_BED_MESH_FALT)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(214));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(44));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFF2F14), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(113));
            lv_obj_align(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_ALIGN_TOP_MID, 0, 14);
        }
        else if (tip_index == MSGBOX_TIP_LEVEL_COMPLETE)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(328));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_img_set_src(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], ui_get_image_src(155));
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
            if (tip_index == MSGBOX_TIP_AUTO_LEVEL)
            {
                ui_set_window_index(WINDOW_ID_AUTO_LEVEL, NULL);
                manual_control_sq.push("G29");
                Printer::GetInstance()->manual_control_signal();
                std::vector<double>().swap(auto_level_vals);
            }
            else if (tip_index == MSGBOX_TIP_VIBRATION_OPTIMIZE)
            {
                calibration_window_index = CALIBRATION_WINDOW_VIBRATION_OPTIMIZE;
                ui_set_window_index(WINDOW_ID_PID_VIBRATION, NULL);
            }
            return true;
        case WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE:
            if (tip_index == MSGBOX_TIP_RESONANCE_TESTER_ERROR_X ||
                tip_index == MSGBOX_TIP_RESONANCE_TESTER_ERROR_X ||
                tip_index == MSGBOX_TIP_BED_MESH_FALT)
            {
                // system("reboot");
                system_reboot(0, false);
            }
            else if (tip_index == MSGBOX_TIP_LEVEL_COMPLETE)
            {
                ui_set_window_index(WINDOW_ID_CALIBRATION, NULL);
                app_top_update_style(window_get_top_widget_list());
            }
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

static bool app_calibration_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    bool ret = false;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(290));
        }
        else if (tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(313));
        }
        start_tick = utils_get_current_tick();
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
            case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION || tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
        {
            if (utils_get_current_tick() - start_tick > 2 * 1000)
            {
                return true;
            }
        }
        break;
    }
    return ret;
}

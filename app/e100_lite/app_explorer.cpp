#include "app_explorer.h"
#include "gcode_preview.h"
#include "klippy.h"
#include "hl_disk.h"
#include <sys/statvfs.h>
#include "params.h"
#include "gpio.h"
#include "explorer.h"
#include "ai_camera.h"
#include "hl_camera.h"
#include "aic_tlp.h"
#include "file_manager.h"

#define LOG_TAG "app_explorer"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#define EXPLORER_LIST_ITEMS 8
#define HISTORY_LIST_ITEMS 3

enum
{
    EXPLORER_STATE_DEFAULT,  // 默认状态
    EXPLORER_STATE_SELETING, // 开始选择，但没有按钮按下
    EXPLORER_STATE_SELETED,  // 已经选择了某项文件
    EXPLORER_STATE_COPYING,  // 复制
};

enum
{
    ENTER_EXPLORER_LOCAL = 0,
    ENTER_EXPLORER_USB,
    ENTER_EXPLORER_HISTORY,
};

enum
{
    MSGBOX_TIP_IMPORT = 0,
    MSGBOX_TIP_DELETE,
    MSGBOX_TIP_UDISK_ABNORMAL,
    MSGBOX_TIP_FILE_LOADING,
    MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION,    //打印中暂不允许操作
    MSGBOX_TIP_EXECUTING_OTHER_TASK,
    MSGBOX_TIP_MATERIAL_BREAK_DETECTION, // 断料检测
    MSGBOX_TIP_NO_MEMORY,
    MSGBOX_TIP_NOT_CAMERA,            //摄像头连接异常
    MSGBOX_TIP_COPY_FAIL,
    MSGBOX_TIP_MISSING_DATE,          // 缺失数据,提示调平
};

static int enter_explorer_index = 0;
static app_listitem_model_t *explorer_model = NULL;
static app_listitem_model_t *history_model = NULL;
static window_t *window_file_info = NULL;
static int explorer_total_number = 0;
static int history_total_number = 0;
static int explorer_total_page = 0;
static int explorer_current_page = 0;
static int history_total_page = 0;
static int history_current_page = 0;
static int explorer_state = -1;
static slice_param_t slice_param = {0};
static int explorer_seleted_state = 0;
static int operation_index = -1;
static float operation_progress = 0;
static int operation_status = 0;
static bool prepare_print = false; // 准备打印(文件复制标志位)

// 当快速连续点击更新文件列表时，会出现点击了第一项却触发到第八项情况，当更新文件列表后0.2s内禁止点击文件列表项
static bool filelist_click_forbidden = false;    // 禁止点击文件列表项
                                                
char print_src_name[NAME_MAX_LEN] = {0};
char print_dest_name[NAME_MAX_LEN] = {0};
char file_copy[PATH_MAX_LEN] = {0};             //记录当前正在复制到本地的文件路径（/user_resource/xxx.gcode）
static explorer_item_t file_item = {0};
extern ConfigParser *get_sysconf();
static bool explorer_file_loading_done = false;     // 记录 加载文件/历史记录是否完成
extern bool calibration_switch;

static void app_explorer_init(widget_t **widget_list);
static void explorer_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void history_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void explorer_listitem_update(void);
static void history_listitem_update(void);
static bool app_explorer_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
static bool app_explorer_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);
void load_thumbnail(lv_obj_t *target, char *file_name, double thumbnail_x, double thumbnail_y, const char *origin_src);
static void clear_buffer_img(void);

static explorer_t explorer;
static explorer_item_t **item_list = NULL;
static uint32_t item_list_size = 0;
// #define MAX_HISTORY_SIZE 30
// static history_item_t history_explorer[MAX_HISTORY_SIZE] = {0};

void load_thumbnail(lv_obj_t *target, char *file_name, double thumbnail_x, double thumbnail_y, const char *origin_src)
{
    // 图片存在才显示，不存在显示默认图片
    char img_path[PATH_MAX_LEN + 1];
    sprintf(img_path, "%s/%s.%s", THUMBNAIL_DIR, file_name, "png");
    if (access(img_path, F_OK) != -1)
    {
        lv_img_set_src(target, img_path);
        int factor = (int)ceil(std::max(slice_param.thumbnail_width / thumbnail_x, slice_param.thumbnail_heigh / thumbnail_y));
        factor = factor < 1 ? 1 : factor;
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

void load_thumbnail_local(lv_obj_t *target, char *file_name, int thumb_width, int thumb_height, double thumbnail_x, double thumbnail_y, const char *origin_src)
{
    // 图片存在才显示，不存在显示默认图片
    char img_path[PATH_MAX_LEN + 1] = {0};
    sprintf(img_path, "%s%s.%s", FILEMANAGER_FILEPATH, file_name, "png");

    if (access(img_path, F_OK) != -1)
    {
        lv_img_set_src(target, img_path);
        int factor = (int)ceil(std::max(thumb_width / thumbnail_x, thumb_height / thumbnail_y));
        factor = factor < 1 ? 1 : factor;
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

static void clear_buffer_img(void)
{
    char img_buffer_path[NAME_MAX_LEN];
    sprintf(img_buffer_path, "%s %s/%s", "rm -rf", THUMBNAIL_DIR, "*");
    printf("img_buffer_path = %s\n", img_buffer_path);
    system(img_buffer_path);
}

#if 1 // 多选设置
enum
{
    ITEM_FLAG_SELETED = 0X00000001,
};

static void explorer_set_item_mask(int index, uint32_t mask)
{
    item_list[index]->userdata = (void *)((uint32_t)item_list[index]->userdata | mask);
}

static void explorer_set_item_mask(explorer_item_t *item, uint32_t mask)
{
    item->userdata = (void *)((uint32_t)item | mask);
}

static void explorer_reset_item_mask(int index, uint32_t mask)
{
    item_list[index]->userdata = (void *)((uint32_t)item_list[index]->userdata & (~mask));
}

static bool explorer_get_item_mask(int index, uint32_t mask)
{
    return (uint32_t)item_list[index]->userdata & mask;
}

static void explorer_clear_all_item_mask(void)
{
    for (int i = 0; i < item_list_size; i++)
        item_list[i]->userdata = (void *)(0);
}
#endif

static int compare_date_descend(const void *p1, const void *p2)
{
    const explorer_item_t **item1 = (const explorer_item_t **)p1;
    const explorer_item_t **item2 = (const explorer_item_t **)p2;

    // 返回值：当返回正整数时表示p1应排在p2后面，返回负整数表示p1应排在p2前面
    return (((*item2)->item_mtime < (*item1)->item_mtime) ? (-1) : ((*item2)->item_mtime > (*item1)->item_mtime));
}

static void explorer_set_file_loading_status(bool status)
{
    explorer_file_loading_done = status;
}

static bool explorer_get_file_loading_status(void)
{
    return explorer_file_loading_done;
}

static void explorer_file_loading_done_func(void)
{
    explorer_set_file_loading_status(true);
}

static void explorer_update_callback(explorer_t *explorer)
{
    explorer_file_loading_done_func();
}

static int compar_item_list(const void *p1, const void *p2)
{
    char name1[NAME_MAX_LEN + 1];
    char name2[NAME_MAX_LEN + 1];
    const explorer_item_t **item1 = (const explorer_item_t **)p1;
    const explorer_item_t **item2 = (const explorer_item_t **)p2;
    utils_string_toupper((*item1)->name, name1, sizeof(name1));
    utils_string_toupper((*item2)->name, name2, sizeof(name2));
    return strcmp(name1, name2);
}

static bool explorer_item_callback(struct explorer_tag *explorer, explorer_item_t *item, int status)
{
    static int dir_item_index = 0;
    static int file_item_index = 0;
    static int dir_item_numbers = 0;
    static int total_item_numbers = 0;
    if (status == EXPLORER_ITEM_CALLBACK_STATUS_START)
    {
        if (item_list)
        {
            free(item_list);
            item_list = NULL;
        }

        if (explorer->total_number > 0)
        {
            item_list = (explorer_item_t **)malloc(sizeof(explorer_item_t *) * explorer->total_number);
            memset(item_list, 0, sizeof(explorer_item_t *) * explorer->total_number);
        }

        explorer_total_number = 0;
        item_list_size = 0;
        dir_item_index = 0;
        file_item_index = 0;
        dir_item_numbers = explorer->dir_number;
        total_item_numbers = explorer->total_number;
    }
    else if (status == EXPLORER_ITEM_CALLBACK_STATUS_END)
    {
        // 移除空位保证列表项连续
        if (explorer->dir_number - dir_item_numbers)
        {
            for (int i = 0; i < explorer->total_number - explorer->dir_number; i++)
            {
                item_list[dir_item_numbers + i] = item_list[explorer->dir_number + i];
            }
        }

        if (total_item_numbers > 0)
        {
            if(explorer->depth == 0)
                qsort(item_list, dir_item_numbers, sizeof(explorer_item_t *), compare_date_descend);
            else
                qsort(item_list + 1, dir_item_numbers - 1, sizeof(explorer_item_t *), compare_date_descend);
            qsort(item_list + dir_item_numbers, total_item_numbers - dir_item_numbers, sizeof(explorer_item_t *), compare_date_descend);
        }
    }
    else if (status == EXPLORER_ITEM_CALLBACK_STATUS_CONTINUE)
    {
        if (item_list)
        {
            // 过滤
            if (item->is_dir && (strcmp(item->name, ".") == 0 ||
                                 (explorer->depth == 0 && strcmp(item->name, "..") == 0) ||
                                 (explorer->depth == 0 && strcmp(item->name, "System Volume Information") == 0) ||
                                 (explorer->depth == 0 && strcmp(item->name, "lost+found") == 0) ||
                                 (explorer->depth == 0 && strcmp(item->name, "file_info") == 0) ||
                                 (explorer->depth == 0 && strcmp(item->name, "aic_tlp") == 0)))
            {
                dir_item_numbers--;
                total_item_numbers--;
                return false;
            }

            // 仅显示Gcode文件及文件夹
            const char *suffix = utils_get_suffix(item->name);
            if (!item->is_dir && (!suffix || strcmp(suffix, "gcode") != 0))
            {
                total_item_numbers--;
                return false;
            }

            /* 不显示隐藏文件及文件夹 */
            if (item->name[0] == '.' && strcmp(item->name,"..") != 0)
            {
                if(item->is_dir)
                    dir_item_numbers--;
                total_item_numbers--;
                return false;
            }

            // 文件夹排序
            if (item->is_dir && dir_item_index < dir_item_numbers)
            {
                item_list[dir_item_index] = item;
                dir_item_index++;
                item_list_size++;
                return true;
            }
            else if (!item->is_dir && (file_item_index < total_item_numbers - dir_item_numbers))
            {
                item_list[explorer->dir_number + file_item_index] = item;
                file_item_index++;
                item_list_size++;
                return true;
            }
        }
    }
    return false;
}

static void explorer_operation_callback(struct explorer_tag *explorer, uint64_t size, uint64_t offset, int state)
{

#define RATIO_DOWNLOAD 0.7f
    static float current_progress = 0;

    if (operation_index < 0 || operation_index >= explorer->total_number)
        return;
    switch (state)
    {
    case EXPLORER_OPERATION_COPYING:
        if(fabs(operation_progress) < 1e-6)
            LOG_D("start EXPLORER_OPERATION_COPYING\n");

        if (size > 0)
            operation_progress = (RATIO_DOWNLOAD * offset / size );
        operation_status = 2;

        if(fabs(operation_progress - RATIO_DOWNLOAD) < 1e-6)
        {
            LOG_D("start EXPLORER_OPERATION_VERIFYING\n");
            current_progress = RATIO_DOWNLOAD;
        }
        break;
    case EXPLORER_OPERATION_VERIFYING:
        if (size > 0)
        {
            if(fabs(operation_progress - (RATIO_DOWNLOAD + ((1.0f - RATIO_DOWNLOAD) / 2.0f))) < 1e-6)
                current_progress = RATIO_DOWNLOAD + ((1.0f - RATIO_DOWNLOAD) / 2.0f);
            
            operation_progress = (current_progress + (((1.0f - RATIO_DOWNLOAD) / 2.0f) * offset / size));   //由于进行两次md5计算，MD5校验进度比例除2
        }
        operation_status = 2;
        break;
    case EXPLORER_OPERATION_COPY_DONE:
        LOG_D("EXPLORER_OPERATION_COPY_DONE\n");
        explorer_reset_item_mask(operation_index, ITEM_FLAG_SELETED);
        operation_status = 3;
        break;
    case EXPLORER_OPERATION_COPY_FAIL:
        LOG_D("EXPLORER_OPERATION_COPY_FAIL\n");
        operation_status = 4;
        break;
    }
}

static void explorer_init(void)
{
    utils_explorer_init(&explorer);
    utils_explorer_set_update_callback(&explorer, explorer_update_callback);
    utils_explorer_set_item_callback(&explorer, explorer_item_callback);
    utils_explorer_set_operation_callback(&explorer, explorer_operation_callback);
}

static void explorer_deinit(void)
{
    utils_explorer_deinit(&explorer);
}

static bool app_material_break_detection(void)
{
    bool ret = false;
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
            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_MATERIAL_BREAK_DETECTION);
            ret = true;
        }
    }

    return ret;
}

void app_explorer_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static int udisk_last_state = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        // utils_init_history_list(history_explorer);
        explorer_init();
        app_explorer_init(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        if (window_file_info)
        {
            window_copy_destory(window_file_info);
            window_file_info = NULL;
        }
        if (explorer_model)
        {
            app_listitem_model_destory(explorer_model);
            explorer_model = NULL;
        }
        if (history_model)
        {
            app_listitem_model_destory(history_model);
            history_model = NULL;
        }
        explorer_deinit();
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_EXPLORER_BTN_TOP_LOCAL:
            enter_explorer_index = ENTER_EXPLORER_LOCAL;
            explorer_current_page = 1;
            explorer_state = EXPLORER_STATE_DEFAULT;

            explorer_set_file_loading_status(false);
            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_FILE_LOADING);

            lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_LOCAL]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_USB]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_HISTORY]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN); 
            lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[1], ui_get_image_src(255));
     
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            
            utils_explorer_set_path(&explorer, "/user-resource");
            break;
        case WIDGET_ID_EXPLORER_BTN_TOP_USB:
            enter_explorer_index = ENTER_EXPLORER_USB;
            explorer_current_page = 1;
            explorer_state = EXPLORER_STATE_DEFAULT;
            if (udisk_last_state = hl_disk_default_is_mounted(HL_DISK_TYPE_USB))
            {
                explorer_set_file_loading_status(false);
                app_msgbox_close_avtive_msgbox();
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_FILE_LOADING);

                lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_LOCAL]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_USB]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_HISTORY]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
                lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[1], ui_get_image_src(256));

                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

                utils_explorer_set_path(&explorer, "/mnt/exUDISK");
            }
            else
            {
                item_list_size = 0;
                explorer_listitem_update();

                lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_LOCAL]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_USB]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_HISTORY]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
                lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[1], ui_get_image_src(256));

                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_BTN_DELETE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_height(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], 36);
                lv_label_set_text(widget_list[WIDGET_ID_EXPLORER_BTN_IMPORT]->obj_container[2], tr(225));
            }
            break;
        case WIDGET_ID_EXPLORER_BTN_TOP_HISTORY:
            enter_explorer_index = ENTER_EXPLORER_HISTORY;
            history_current_page = 1;

            explorer_set_file_loading_status(false);
            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_FILE_LOADING);
            
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_LOCAL]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_USB]->obj_container[2], lv_color_hex(0xFFAEAEAE), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_HISTORY]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[1], ui_get_image_src(257));

            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

            utils_explorer_set_path(&explorer, "/user-resource");
            break;
        case WIDGET_ID_EXPLORER_BTN_PREV_PAGE:
            if (enter_explorer_index == ENTER_EXPLORER_LOCAL || enter_explorer_index == ENTER_EXPLORER_USB)
            {
                if (explorer_current_page > 1)
                    explorer_current_page--;
                explorer_listitem_update();
            }
            else if (enter_explorer_index == ENTER_EXPLORER_HISTORY)
            {
                if (history_current_page > 1)
                    history_current_page--;
                history_listitem_update();
            }
            break;
        case WIDGET_ID_EXPLORER_BTN_NEXT_PAGE:
            if (enter_explorer_index == ENTER_EXPLORER_LOCAL || enter_explorer_index == ENTER_EXPLORER_USB)
            {
                if (explorer_current_page < explorer_total_page)
                    explorer_current_page++;
                explorer_listitem_update();
            }
            else if (enter_explorer_index == ENTER_EXPLORER_HISTORY)
            {
                if (history_current_page < history_total_page)
                    history_current_page++;
                history_listitem_update();
            }
            break;
        case WIDGET_ID_EXPLORER_CONTAINER_TOP:
            explorer_state = EXPLORER_STATE_DEFAULT;
            explorer_listitem_update();
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            break;
        case WIDGET_ID_EXPLORER_BTN_IMPORT:
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            if (explorer_seleted_state > 0)
            {
                if(!hl_disk_default_is_mounted(HL_DISK_TYPE_USB))   //不存在U盘
                {
                    explorer_state = EXPLORER_STATE_DEFAULT;
                    explorer_listitem_update();
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_explorer_routine_msgbox_callback, (void *)MSGBOX_TIP_UDISK_ABNORMAL);
                }
                else if(app_print_get_print_state() == true)        // 打印中不允许复制文件
                {
                    explorer_state = EXPLORER_STATE_DEFAULT;
                    explorer_listitem_update();
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
                }
                else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)        // 进退料中
                {
                    explorer_state = EXPLORER_STATE_DEFAULT;
                    explorer_listitem_update();
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER_TASK);
                }
                else
                {
                    explorer_state = EXPLORER_STATE_COPYING;
                    explorer_listitem_update();
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_IMPORT);
                }
            }
            else
            {
                explorer_state = EXPLORER_STATE_DEFAULT;
                explorer_listitem_update();
            }
            break;
        case WIDGET_ID_EXPLORER_BTN_DELETE:
            if(app_print_get_print_state() == true)
            {
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                explorer_clear_all_item_mask();
                explorer_seleted_state = 0;
                explorer_state = EXPLORER_STATE_DEFAULT;
                explorer_listitem_update();

                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false) // 进退料中
            {
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                explorer_clear_all_item_mask();
                explorer_seleted_state = 0;
                explorer_state = EXPLORER_STATE_DEFAULT;
                explorer_listitem_update();

                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else
            {
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                if (explorer_seleted_state > 0)
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_DELETE);
                else
                {
                    explorer_state = EXPLORER_STATE_DEFAULT;
                    explorer_listitem_update();
                }
            }
            break;
        case WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB:
            if (explorer_state == EXPLORER_STATE_SELETING || explorer_state == EXPLORER_STATE_SELETED)
            {
                explorer_state = EXPLORER_STATE_DEFAULT;
                explorer_listitem_update();
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            }
            break;
        }
        break;
    case LV_EVENT_CHILD_PRESSING:
        switch (widget->header.index)
        {
        case WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB:
            if (explorer_state == EXPLORER_STATE_SELETING || explorer_state == EXPLORER_STATE_SELETED)
            {
                widget_t *list_widget = (widget_t *)lv_event_get_param((lv_event_t *)e);
                window_t *win = list_widget->win;
                widget_t **widget_list = win->widget_list;
                for (int i = 0; i < app_listitem_model_count(explorer_model); i++)
                {
                    app_listitem_t *item = app_listitem_model_get_item(explorer_model, i);
                    if (item->win == win)
                    {
                        if (!explorer_get_item_mask(i, ITEM_FLAG_SELETED))
                            lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_SELECT_BOX]->obj_container[1], ui_get_image_src(88));
                        else
                            lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_SELECT_BOX]->obj_container[1], ui_get_image_src(87));
                        break;
                    }
                }
            }
            break;
        }
        break;
    case LV_EVENT_CHILD_LONG_PRESSED:
        switch (widget->header.index)
        {
        case WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB:
            if (explorer_state == EXPLORER_STATE_DEFAULT)
            {
                explorer_state = EXPLORER_STATE_SELETING;
                explorer_seleted_state = 0;
                explorer_clear_all_item_mask();
                explorer_listitem_update();
                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            }
            break;
        }
        break;
    case LV_EVENT_CHILD_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB:
        {
            widget_t *list_widget = (widget_t *)lv_event_get_param((lv_event_t *)e);
            window_t *win = list_widget->win;
            int model_size = app_listitem_model_count(explorer_model);
            app_listitem_t *item;
            int item_index, array_index;
            for (item_index = 0; item_index < model_size; item_index++)
            {
                item = app_listitem_model_get_item(explorer_model, item_index);
                if (item->win == win)
                    break;
            }

            array_index = item_index + (explorer_current_page - 1) * EXPLORER_LIST_ITEMS;
            /* for some unkonw reason, sometimes this pointer can be null. Refer to mn:12635 */
            if (item_list[array_index] == NULL)
            {
                LOG_E("Indexing item_list error!\n");
                break;
            }

            if (explorer_state == EXPLORER_STATE_DEFAULT)
            {
                if (item_list[array_index]->is_dir)
                {
                    explorer_current_page = 1;
                    explorer_set_file_loading_status(false);
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_FILE_LOADING);
                    utils_explorer_opendir(&explorer, item_list[array_index]->name);
                }
                else
                {
                    operation_index = array_index;
                    window_file_info = window_copy(WINDOW_ID_FILE_INFO, app_file_info_callback, widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], item_list[array_index]);
                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                }
            }
            else if (explorer_state == EXPLORER_STATE_SELETING || explorer_state == EXPLORER_STATE_SELETED)
            {
                // 只有文件可以选中
                if (!item_list[array_index]->is_dir)
                {
                    if (explorer_get_item_mask(array_index, ITEM_FLAG_SELETED))
                    {
                        explorer_reset_item_mask(array_index, ITEM_FLAG_SELETED);
                        if (--explorer_seleted_state == 0)
                            explorer_state = EXPLORER_STATE_SELETING;
                    }
                    else
                    {
                        explorer_set_item_mask(array_index, ITEM_FLAG_SELETED);
                        if (explorer_seleted_state++ == 0)
                            explorer_state = EXPLORER_STATE_SELETED;
                    }
                    explorer_listitem_update();
                }
            }
        }
        break;
        case WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY:
        {
            widget_t *list_widget = (widget_t *)lv_event_get_param((lv_event_t *)e);
            window_t *win = list_widget->win;
            int model_size = app_listitem_model_count(history_model);
            app_listitem_t *item;
            int item_index, array_index;
            for (item_index = 0; item_index < model_size; item_index++)
            {
                item = app_listitem_model_get_item(history_model, item_index);
                if (item->win == win)
                    break;
            }
            array_index = item_index + (history_current_page - 1) * HISTORY_LIST_ITEMS;

            if(machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].print_state == PRINT_RECORD_STATE_START)      //如果此时文件正在打印，不显示在历史记录中
                array_index += 1; 

            print_history_record_t *record = &machine_info.print_history_record[(machine_info.print_history_current_index - array_index - 1) % PRINT_HISTORY_SIZE];
            printf("history_explorer[%d].path：%s\n",  array_index, record->filepath);
            // printf("enter_explorer_index：%d\n", enter_explorer_index);

            for(uint8_t i = 0;i < item_list_size;i++){
                printf(" item_list[%d]->path：%s\n", i, item_list[i]->path);
                if(strcmp(record->filepath, item_list[i]->path) == 0){
                    window_file_info = window_copy(WINDOW_ID_FILE_INFO, app_file_info_callback, widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], item_list[i]);
                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    break;
                }
            }

            // history_listitem_update();
        }
        break;
        }
        break;
    case LV_EVENT_CHILD_DESTROYED:
        switch (widget->header.index)
        {
        case WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE:
            if (window_file_info)
            {
                window_copy_destory(window_file_info);
                window_file_info = NULL;
            }
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
      
            lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            
            if (enter_explorer_index == ENTER_EXPLORER_LOCAL || enter_explorer_index == ENTER_EXPLORER_USB)
                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            else if (enter_explorer_index == ENTER_EXPLORER_HISTORY)
                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            break;
        }
        break;
    case LV_EVENT_UPDATE:

        if (enter_explorer_index == ENTER_EXPLORER_USB)
        {
            // U盘状态检测
            if (udisk_last_state != hl_disk_default_is_mounted(HL_DISK_TYPE_USB))
            {
                if(hl_disk_default_is_mounted(HL_DISK_TYPE_USB) == 0 && window_file_info != NULL)
                {
                    window_copy_destory(window_file_info);
                    window_file_info = NULL;
                }
                lv_event_send(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_USB]->obj_container[0], LV_EVENT_CLICKED, NULL);
            }
        }

        // 点击本地文件打印
        if ((enter_explorer_index == ENTER_EXPLORER_LOCAL ||
            enter_explorer_index == ENTER_EXPLORER_HISTORY) &&
            prepare_print == true)
        {
            prepare_print = false;
            if (!app_material_break_detection())
            {
                // sprintf(file_item.path, "%s/%s", "/user-resource", file_item.name);
                ui_set_window_index(WINDOW_ID_PRINT, &file_item);
                app_top_update_style(window_get_top_widget_list());
                // printf("本地文件开始打印");
            }
        }

        // 文件列表项被禁止点击时，0.2s 后允许点击各文件列表项
        if(filelist_click_forbidden == true)
        {
            static uint64_t tick = 0;
            if(tick == 0)
                tick = utils_get_current_tick();
            
            if(tick != 0 && utils_get_current_tick() - tick > 200)
            {
                for (int i = 0; i < EXPLORER_LIST_ITEMS; i++)
                {           
                    app_listitem_t *item = app_listitem_model_get_item(explorer_model, i);
                    window_t *win = item->win;
                    widget_t **widget_list = win->widget_list;
                    
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                }

                //重置参数
                tick = 0;
                filelist_click_forbidden = false;
            }
        }

        if(window_file_info)
            lv_event_send(window_file_info->widget_list[0]->obj_container[0], (lv_event_code_t)LV_EVENT_UPDATE, NULL);
        break;
    }
}

/**
 * @brief 检查当前是否有动作
 * 
 * @return  fasle:繁忙 true:不繁忙
 */
static bool check_action_busy(void)
{
    double eventtime = get_monotonic();
    std::vector<double> ret = Printer::GetInstance()->m_tool_head->check_busy(eventtime);
    return ret[2];
}

static void app_explorer_init(widget_t **widget_list)
{
    enter_explorer_index = ENTER_EXPLORER_LOCAL;
    explorer_state = EXPLORER_STATE_DEFAULT;
    // uint8_t history_index = 0;

    // for (history_index = 0; history_index < MAX_HISTORY_SIZE; history_index++)
    // {
    //     if (history_explorer[history_index].name == NULL)
    //     {
    //         break;
    //     }
    // }
    // history_total_number = history_index;
    if(machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].print_state == PRINT_RECORD_STATE_START)
        history_total_number = machine_info.print_history_valid_numbers - 1;
    else
        history_total_number = machine_info.print_history_valid_numbers;

    explorer_model = app_listitem_model_create(WINDOW_ID_EXPLORER_LIST_TEMPLATE, widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB]->obj_container[0], explorer_listitem_callback, NULL);
    for (int i = 0; i < EXPLORER_LIST_ITEMS; i++)
        app_listitem_model_push_back(explorer_model);
    history_model = app_listitem_model_create(WINDOW_ID_HISTORY_LIST_TEMPLATE, widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY]->obj_container[0], history_listitem_callback, NULL);
    for (int i = 0; i < HISTORY_LIST_ITEMS; i++)
        app_listitem_model_push_back(history_model);

    // 设定流式对齐 对齐间隙
    lv_obj_set_flex_flow(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB]->obj_container[0], LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB]->obj_container[0], 7, LV_PART_MAIN);    // 对象之间 row 间隔
    lv_obj_set_style_pad_column(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB]->obj_container[0], 7, LV_PART_MAIN); // 对象之间 column 间隔
    lv_obj_set_style_pad_top(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB]->obj_container[0], 0, LV_PART_MAIN);    // 顶部间隙
    lv_obj_set_style_pad_left(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_LOCAL_USB]->obj_container[0], 0, LV_PART_MAIN);   // 左边间隙

    // 设定流式对齐 对齐间隙
    lv_obj_set_flex_flow(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY]->obj_container[0], LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY]->obj_container[0], 14, LV_PART_MAIN);   // 对象之间 row 间隔
    lv_obj_set_style_pad_column(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY]->obj_container[0], 0, LV_PART_MAIN); // 对象之间 column 间隔
    lv_obj_set_style_pad_top(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY]->obj_container[0], 0, LV_PART_MAIN);    // 顶部间隙
    lv_obj_set_style_pad_left(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LIST_HISTORY]->obj_container[0], 0, LV_PART_MAIN);   // 左边间隙

    lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[1], ui_get_image_src(255));
    // app_set_widget_item2center_align(widget_list, WIDGET_ID_EXPLORER_BTN_TOP_LOCAL, 0);
    // app_set_widget_item2center_align(widget_list, WIDGET_ID_EXPLORER_BTN_TOP_USB, 0);
    // app_set_widget_item2center_align(widget_list, WIDGET_ID_EXPLORER_BTN_TOP_HISTORY, 0);
    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

    { // 进入本地磁盘
        lv_event_send(widget_list[WIDGET_ID_EXPLORER_BTN_TOP_LOCAL]->obj_container[0],LV_EVENT_CLICKED,NULL);
    }
}

static void explorer_listitem_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_PRESSING:
        lv_event_send(app_listitem_model_get_parent(explorer_model), (lv_event_code_t)LV_EVENT_CHILD_PRESSING, widget);
        break;
    case LV_EVENT_LONG_PRESSED:
        lv_event_send(app_listitem_model_get_parent(explorer_model), (lv_event_code_t)LV_EVENT_CHILD_LONG_PRESSED, widget);
        break;
    case LV_EVENT_CLICKED:
        lv_event_send(app_listitem_model_get_parent(explorer_model), (lv_event_code_t)LV_EVENT_CHILD_CLICKED, widget);
        break;
    }
}

static void history_listitem_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        lv_event_send(app_listitem_model_get_parent(history_model), (lv_event_code_t)LV_EVENT_CHILD_CLICKED, widget);
        break;
    }
}

// static void preview_thread_task(hl_tpool_thread_t thread, void *args)
// {
//     char *path = (char *)args;

// }

std::string modifyString(const std::string& original, const int num) {
    std::string modified;
    size_t i = 0;

    while (i < original.size()) {
        if (original.substr(i, 2) == "00") {
            i += 3;
        } else {
            modified += original[i];
            ++i;
        }

        if(modified.size() >= num*3)
            break;
    }

    return modified;
}

static void explorer_listitem_update(void)
{
    clear_buffer_img();
    printf("enter_explorer_index：%d\n", enter_explorer_index);

    if (enter_explorer_index == ENTER_EXPLORER_LOCAL || enter_explorer_index == ENTER_EXPLORER_USB)
        explorer_total_number = item_list_size;
    // else if (explorer_entry == ENTER_EXPLORER_HISTORY)
    //     total_numbers = machine_info.print_history_valid_numbers;

    if (explorer_total_number % EXPLORER_LIST_ITEMS == 0)
        explorer_total_page = explorer_total_number / EXPLORER_LIST_ITEMS;
    else
        explorer_total_page = (explorer_total_number / EXPLORER_LIST_ITEMS) + 1;

    widget_t **widget_list = window_get_widget_list();
    lv_label_set_text_fmt(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[2], "%d/%d", explorer_current_page, explorer_total_page);
    if (explorer_total_page == 0)
        lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < EXPLORER_LIST_ITEMS; i++)
    {
        app_listitem_t *item = app_listitem_model_get_item(explorer_model, i);
        window_t *win = item->win;
        widget_t **widget_list = win->widget_list;
        int listitem_index = i + (explorer_current_page - 1) * EXPLORER_LIST_ITEMS;
        if (listitem_index > explorer_total_number - 1)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        else
            lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_label_set_long_mode(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_LABEL_LONG_DOT);
        lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_SELECT_BOX]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], item_list[listitem_index]->name);
        if (item_list[listitem_index]->is_dir)
        {
            if (strcmp(item_list[listitem_index]->name, "..") == 0)
            {
                lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], ui_get_image_src(90));
                lv_obj_align(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[0], LV_ALIGN_TOP_MID, 0, 21);
            }
            else
            {
                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], ui_get_image_src(216));
                lv_obj_align(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[0], LV_ALIGN_TOP_MID, 0, 3);
            }
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_TIME]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_LAYER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_ALIGN_TOP_MID, 0, 66);
            lv_img_set_zoom(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], 256);
        }
        else
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_TIME]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_LAYER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_ALIGN_TOP_MID, 0, 59);
            lv_obj_align(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[0], LV_ALIGN_TOP_MID, 0, 0);

            if(enter_explorer_index == ENTER_EXPLORER_USB)
            {
                char preview_path[PATH_MAX_LEN + 1];
                gcode_preview(item_list[listitem_index]->path, preview_path, 1, &slice_param, item_list[listitem_index]->name);
                load_thumbnail(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], item_list[listitem_index]->name, 80., 50., ui_get_image_src(82));
                std::string time_str = modifyString(slice_param.estimeated_time_str, 2);
                lv_label_set_text_fmt(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_TIME]->obj_container[2], time_str.c_str());
                lv_label_set_text_fmt(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_LAYER]->obj_container[2], "%2d", slice_param.total_layers);
            }
            else
            {
                std::vector<FileInfo> file_info = FileManager::GetInstance()->GetFileInfo();
                auto it = file_info.begin();
                for (; it != file_info.end(); ++it) 
                {
                    if ((*it).m_file_name == item_list[listitem_index]->name)
                    {
                        load_thumbnail_local(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], item_list[listitem_index]->name, (*it).m_thumbnail_width, (*it).m_thumbnail_height, 80., 50., ui_get_image_src(82));
                        std::string time_str = modifyString((*it).m_est_time, 2);
                        lv_label_set_text_fmt(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_TIME]->obj_container[2], time_str.c_str());
                        lv_label_set_text_fmt(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_PRINT_LAYER]->obj_container[2], "%2d", (*it).m_total_layers);
                        break;
                    }
                }
                if(it == file_info.end())
                {
                    load_thumbnail_local(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], NULL, 256, 256, 80., 50., ui_get_image_src(82));
                    LOG_E("file_manager have no info about file %s\n",item_list[listitem_index]->name);
                }
            }
        }

        if (explorer_state == EXPLORER_STATE_DEFAULT)
        {
        }
        else if (explorer_state == EXPLORER_STATE_SELETING || explorer_state == EXPLORER_STATE_SELETED)
        {
            if (!item_list[listitem_index]->is_dir)
            {
                lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_SELECT_BOX]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                if (explorer_get_item_mask(listitem_index, ITEM_FLAG_SELETED))
                    lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_SELECT_BOX]->obj_container[1], ui_get_image_src(88));
                else
                    lv_img_set_src(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_BTN_SELECT_BOX]->obj_container[1], ui_get_image_src(87));
            }
        }
    }
}

static void history_listitem_update(void)
{
    if(machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].print_state == PRINT_RECORD_STATE_START)
        history_total_number = machine_info.print_history_valid_numbers - 1;
    else
        history_total_number = machine_info.print_history_valid_numbers;

    if (history_total_number % HISTORY_LIST_ITEMS == 0)
        history_total_page = history_total_number / HISTORY_LIST_ITEMS;
    else
        history_total_page = (history_total_number / HISTORY_LIST_ITEMS) + 1;

    widget_t **widget_list = window_get_widget_list();
    printf("history_current_page = %d，history_total_page = %d\n", history_current_page, history_total_page);
    lv_label_set_text_fmt(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[2], "%d/%d", history_current_page, history_total_page);
    if (history_total_page == 0)
        lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < HISTORY_LIST_ITEMS; i++)
    {
        app_listitem_t *item = app_listitem_model_get_item(history_model, i);
        window_t *win = item->win;
        widget_t **widget_list = win->widget_list;
        int listitem_index = i + ((history_current_page - 1) * HISTORY_LIST_ITEMS);
        if (listitem_index > history_total_number - 1)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        else
            lv_obj_clear_flag(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_long_mode(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_LABEL_LONG_DOT);

        if(machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].print_state == PRINT_RECORD_STATE_START)      //如果此时文件正在打印，不显示在历史记录中
            listitem_index += 1; 

        std::vector<FileInfo> file_info = FileManager::GetInstance()->GetFileInfo();
        auto it = file_info.begin();
        for (; it != file_info.end(); ++it) 
        {
            if ((*it).m_file_name == (char *)utils_get_file_name(machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].filepath))
            {
                load_thumbnail_local(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], (char *)(*it).m_file_name.c_str(), (*it).m_thumbnail_width, (*it).m_thumbnail_height, 80., 50., ui_get_image_src(82));
                break;
            }
        }
        if(it == file_info.end())
        {
            LOG_E("file_manager have no info about file %s\n",(char *)utils_get_file_name(machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].filepath));
            load_thumbnail_local(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_IMAGE_PREVIEW]->obj_container[1], NULL, 256, 256, 80., 50., ui_get_image_src(82));
        }

        lv_label_set_text(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], utils_get_file_name(machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].filepath));

        time_t timestamp = machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].start_time;
        if(machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].ntp_status == 0)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_START_TIME]->obj_container[0], "-/-/- -:-");
        }
        else
        {
            struct tm *timeinfo;
            char buffer[80];
            // 将时间戳转换为本地时间结构体
            timeinfo = localtime(&timestamp);
            // 格式化输出为年月日时分的字符串
            strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M", timeinfo);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_START_TIME]->obj_container[0], buffer);
        }

        int hours = machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].print_duration / 3600;
        int minutes = (machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].print_duration - hours * 3600) / 60;
        lv_label_set_text_fmt(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_TOTAL_TIME]->obj_container[0], "%dh%dm", hours, minutes);

        if (machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].print_state == PRINT_RECORD_STATE_FINISH)
            lv_img_set_src(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_IMAGE_PRINT_STATE]->obj_container[0], ui_get_image_src(85));
        else
            lv_img_set_src(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_IMAGE_PRINT_STATE]->obj_container[0], ui_get_image_src(86));

        if (access(machine_info.print_history_record[(machine_info.print_history_current_index - listitem_index - 1) % PRINT_HISTORY_SIZE].filepath, F_OK) == 0)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_START_TIME]->obj_container[0], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_TOTAL_TIME]->obj_container[0], 255, LV_PART_MAIN);
        }
        else
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], 127, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_START_TIME]->obj_container[0], 127, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_HISTORY_LIST_TEMPLATE_LABEL_TOTAL_TIME]->obj_container[0], 127, LV_PART_MAIN);
        }
    }
}

void app_file_info_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    char preview_path[PATH_MAX_LEN + 1];
    explorer_item_t *tmp = (explorer_item_t *)lv_event_get_param((lv_event_t *)e);
    extern bool photography_switch;
    std::string time_str = "";
    
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
#if CONFIG_BOARD_E100 == BOARD_E100_LITE
        lv_obj_add_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
#endif
        memcpy(&file_item, tmp, sizeof(explorer_item_t));
        sprintf(print_src_name, "%s", file_item.path);
        sprintf(print_dest_name, "%s/%s%s", "/user-resource", file_item.name, ".tmp");
        gcode_preview(file_item.path, preview_path, 1, &slice_param, file_item.name);
        load_thumbnail(widget_list[WIDGET_ID_FILE_INFO_BTN_PREVIEW]->obj_container[1], file_item.name, 144., 144., ui_get_image_src(82));
        lv_label_set_long_mode(widget_list[WIDGET_ID_FILE_INFO_LABEL_NAME]->obj_container[0], LV_LABEL_LONG_DOT);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_FILE_INFO_LABEL_NAME]->obj_container[0], "%s", file_item.name);
        time_str = modifyString(slice_param.estimeated_time_str, 4);
        lv_label_set_text_fmt(widget_list[WIDGET_ID_FILE_INFO_BTN_TIME]->obj_container[2], time_str.c_str());
        // lv_label_set_text_fmt(widget_list[WIDGET_ID_FILE_INFO_BTN_LAYER]->obj_container[2], "%d%s", slice_param.total_layers, tr(33));
        lv_label_set_text_fmt(widget_list[WIDGET_ID_FILE_INFO_BTN_LAYER]->obj_container[2], "%d", slice_param.total_layers);    

        //调整ui
        {
            widget_t **widget_list_cur = window_get_widget_list();
            lv_obj_add_flag(widget_list_cur[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }
                
        if (calibration_switch)
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN); 
        }

        if (photography_switch && hl_camera_get_exist_state())
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN); 
        }

        if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 0) // A面
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], LV_OBJ_FLAG_CLICKABLE); 
            // manual_control_sq.push("BED_MESH_SET_INDEX TYPE=standard INDEX=0");
            // Printer::GetInstance()->manual_control_signal();
        }
        else
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_add_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            // manual_control_sq.push("BED_MESH_SET_INDEX TYPE=enhancement INDEX=0");
            // Printer::GetInstance()->manual_control_signal();
        }

        // 显示耗材
        if(strlen(slice_param.filament_type) > 0)
        {
            char temp[5] = {0};
            strncpy(temp,slice_param.filament_type,sizeof(temp)-1);
            for(int i = 0;i<strlen(temp);i++)
            {
                if(temp[i] == '-')
                {
                    temp[i] = '\0';
                    break;
                }
            }   
            lv_label_set_text_fmt(widget_list[WIDGET_ID_FILE_INFO_BTN_FILAMENT_TYPE]->obj_container[2], "%s", temp);
        }
        else
            lv_obj_add_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_FILAMENT_TYPE]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY:
            if (hl_camera_get_exist_state() == false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_NOT_CAMERA);
            }
            else
            {
                photography_switch = !photography_switch;
                if (photography_switch && hl_camera_get_exist_state())
                {
                    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                    lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                }
                else
                {
                    lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
                    lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN);
                }
                get_sysconf()->SetBool("system", "tlp_switch", photography_switch);
                get_sysconf()->WriteIni(SYSCONF_PATH);
            }
            break;
        case WIDGET_ID_FILE_INFO_BTN_CALIBRATION:
            calibration_switch = !calibration_switch;
            get_sysconf()->SetBool("system", "calibration_switch", calibration_switch);
            get_sysconf()->WriteIni(SYSCONF_PATH);

            if (calibration_switch)
            {
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            }
            else
            {
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
                lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_CALIBRATION]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN);
            }
            break;
        case WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A:
        case WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
                break;
            }
            else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false) // 进退料中
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER_TASK);
                break;
            }
            
            if (widget->header.index == WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A)
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
            if (get_sysconf()->GetInt("system", "print_platform_type", 0) == 0) // A面
            {
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
                lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN);
                lv_obj_clear_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            }
            else
            {
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
                lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN);
                lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                lv_obj_add_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_A]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                lv_obj_clear_flag(widget_list[WIDGET_ID_FILE_INFO_BTN_PRINT_PLATFORM_B]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            }
            break;
        case WIDGET_ID_FILE_INFO_BTN_CANCEL:
            lv_event_send(lv_obj_get_parent(widget_list[WIDGET_ID_FILE_INFO_CONTAINER_MASK]->obj_container[0]), (lv_event_code_t)LV_EVENT_CHILD_DESTROYED, NULL);
            if(enter_explorer_index == ENTER_EXPLORER_HISTORY){
                history_listitem_update();
            }
            break;
        case WIDGET_ID_FILE_INFO_BTN_PRINT:
            if (app_material_break_detection())
            {
                break;
            }
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION);
            }
            else if(Printer::GetInstance()->m_change_filament->is_feed_busy() != false || check_action_busy() == false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_EXECUTING_OTHER_TASK);
            }
            else if(Printer::GetInstance()->m_bed_mesh->get_mesh() == nullptr) // 缺失数据,提示调平
            {
                file_item.userdata = (void *)&calibration_switch;
                app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_explorer_routine_msgbox_callback, (void *)MSGBOX_TIP_MISSING_DATE);
            }
            else
            {
                file_item.userdata = (void *)&calibration_switch;
                if(enter_explorer_index == ENTER_EXPLORER_USB)
                {
                    uint64_t available_size = 0;
                    if(utils_get_dir_available_size("/user-resource") > TLP_FILE_TLP_PARTITION)
                        available_size = utils_get_dir_available_size("/user-resource") - TLP_FILE_TLP_PARTITION;

                    if (utils_get_size(print_src_name) > available_size)
                    {
                        app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_NO_MEMORY);
                        break;
                    }

                    utils_explore_operation_start(&explorer, print_src_name, print_dest_name, 0);
                    snprintf(file_copy, sizeof(file_copy), "/user-resource/%s", utils_get_file_name(print_src_name));
                    explorer_state = EXPLORER_STATE_COPYING;
                    // printf("operation_status%d", operation_status);
                    operation_status = 1;
                    operation_progress = 0;
                    LOG_I("start copy file %s -> %s\n", print_src_name, print_dest_name);
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_IMPORT);
                }
                prepare_print = true;
                // ui_set_window_index(WINDOW_ID_PRINT, &file_item);
                app_top_update_style(window_get_top_widget_list());
            }
            break;
        }
        break;
    case LV_EVENT_UPDATE:
        if (photography_switch && hl_camera_get_exist_state())
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF2F302F), LV_PART_MAIN);
            lv_obj_set_style_border_color(widget_list[WIDGET_ID_FILE_INFO_BTN_PHOTOGRAPHY]->obj_container[0], lv_color_hex(0xFF555555), LV_PART_MAIN); 
        }
        break;
    }
}

static bool app_explorer_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;

    static uint64_t timeout_tick = 0;
    static int progress = 0;
    static int progress_last = 0;

    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_IMPORT)
        {
            if (enter_explorer_index == ENTER_EXPLORER_LOCAL)
                lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:0%%", tr(58));
            else if (enter_explorer_index == ENTER_EXPLORER_USB)
                lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:0%%", tr(227));
        
            timeout_tick = utils_get_current_tick();
            progress = 0;
            progress_last = 0;
        }
        else if (tip_index == MSGBOX_TIP_DELETE)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(57));

            for (int i = 0; i < explorer_total_number; i++)
            {
                if (explorer_get_item_mask(i, ITEM_FLAG_SELETED))
                {
                    explorer_reset_item_mask(i, ITEM_FLAG_SELETED);
                    remove(item_list[i]->path);
                    FileManager::GetInstance()->DeleteFile(item_list[i]->path);
                }
            }
            utils_vfork_system("sync");

            explorer_state = EXPLORER_STATE_DEFAULT;
            utils_explorer_opendir(&explorer, ".");
            explorer_listitem_update();

        }
        else if (tip_index == MSGBOX_TIP_NO_MEMORY)
        {
            if (enter_explorer_index == ENTER_EXPLORER_LOCAL)
                lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(197));
            else if (enter_explorer_index == ENTER_EXPLORER_USB)
                lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(200));
        }
        if(tip_index == MSGBOX_TIP_FILE_LOADING)
        {
            if(enter_explorer_index == ENTER_EXPLORER_LOCAL || enter_explorer_index == ENTER_EXPLORER_USB)
                lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(285));
            else if(enter_explorer_index == ENTER_EXPLORER_HISTORY)
                lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(286));
        }
        else if(tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(290));
        }
        else if(tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(313));
        }
        else if (tip_index == MSGBOX_TIP_MATERIAL_BREAK_DETECTION)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(42));
        }
        else if (tip_index == MSGBOX_TIP_NOT_CAMERA)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(324));
        }
        start_tick = utils_get_current_tick();
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
            if (tip_index == MSGBOX_TIP_IMPORT || tip_index == MSGBOX_TIP_DELETE)
                break;
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_IMPORT)
        {
                if(explorer_model == NULL)      //已切换界面，消除弹窗
                    return true;
            // if (explorer_state == EXPLORER_STATE_COPYING)
            // {
                if (operation_status == 0) // 空闲
                {
                    for (int i = 0; i < explorer_total_number; i++)
                    {
                        if (explorer_get_item_mask(i, ITEM_FLAG_SELETED))
                        {
                            char dst[PATH_MAX_LEN + 1];
                            uint64_t dir_available_size = 0;
                            if (enter_explorer_index == ENTER_EXPLORER_USB) // 导入本地
                            { 
                                snprintf(dst, sizeof(dst), "/user-resource/%s%s", item_list[i]->name, ".tmp");
                                if(utils_get_dir_available_size("/user-resource") > TLP_FILE_TLP_PARTITION)
                                    dir_available_size = utils_get_dir_available_size("/user-resource") - TLP_FILE_TLP_PARTITION;
                                else
                                    dir_available_size = 0;
                            }
                            else if (enter_explorer_index == ENTER_EXPLORER_LOCAL) // 导入U盘
                            {
                                snprintf(dst, sizeof(dst), "/mnt/exUDISK/%s%s", item_list[i]->name, ".tmp");
                                dir_available_size = utils_get_dir_available_size("/mnt/exUDISK");
                            }
                                

                            if (utils_get_size(item_list[i]->path) > dir_available_size && hl_disk_default_is_mounted(HL_DISK_TYPE_USB))
                            {
                              explorer_clear_all_item_mask();
                              explorer_seleted_state = 0;
                              explorer_state = EXPLORER_STATE_DEFAULT;
                              explorer_listitem_update();

                              tip_index = MSGBOX_TIP_NO_MEMORY;
                              if (tip_index == MSGBOX_TIP_NO_MEMORY)
                              {
                                if (enter_explorer_index == ENTER_EXPLORER_LOCAL)
                                    lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(197));
                                else if (enter_explorer_index == ENTER_EXPLORER_USB)
                                    lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(200));
                               }
                               return false;
                            }

                            operation_index = i;
                            operation_status = 1;
                            operation_progress = 0;
                            
                            timeout_tick = utils_get_current_tick();
                            progress = 0;
                            progress_last = 0;

                            if (utils_explore_operation_start(&explorer, item_list[i]->path, dst, 0) != 0)
                                operation_status = 0;
                            else
                            {
                                LOG_I("start copy file %s -> %s\n", item_list[i]->path, dst);
                                if (enter_explorer_index == ENTER_EXPLORER_USB) // 导入本地
                                { 
                                    snprintf(file_copy, sizeof(file_copy), "/user-resource/%s", item_list[i]->name);
                                }
                                break;
                            }
                        }
                    }
                    if (operation_status == 0) // 已经没有任务则切换状态
                    {
                        explorer_state = EXPLORER_STATE_DEFAULT;
                        explorer_listitem_update();
                        return true;
                    }
                }
                else if(operation_status == 1)      
                {
                    if(timeout_tick != 0 && utils_get_current_tick() - timeout_tick > 10 * 1000) //超过10s
                    {
                        LOG_D("operation_status : %d\n",operation_status);
                        timeout_tick = utils_get_current_tick();
                    }
                }
                else if (operation_status == 2) // 进行中,设置进度
                {
                    progress = (int)(operation_progress * 100);
                    if(progress_last != progress)
                    {
                        progress_last = progress;
                        timeout_tick = utils_get_current_tick();
                    }

                    // 防止 复制线程 初始化复制时异常且线程销毁而ui线程不知
                    if(explorer.opertaion_state == EXPLORER_OPERATION_IDLE && operation_status == 2)
                    {
                        LOG_E("cp file error! (progress:%d)\n",progress);
                        operation_status = 4;
                        break;
                    }

                    // 增加超时机制防止复制线程出现问题，ui一直阻塞于此
                    if(timeout_tick != 0 && utils_get_current_tick() - timeout_tick > 60*1000)  //1min
                    {
                        LOG_I("cp file timeout(current progress:%d)!\n",progress);
                        remove(explorer.operation_ctx.dst_path);
                        operation_status = 4;
                        break;
                    }

                    if (enter_explorer_index == ENTER_EXPLORER_LOCAL)
                        lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:%d%%", tr(58), progress);
                    else if (enter_explorer_index == ENTER_EXPLORER_USB)
                        lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:%d%%", tr(227), progress);

                    if(!hl_disk_default_is_mounted(HL_DISK_TYPE_USB))   //不存在U盘
                    {
                        explorer_state = EXPLORER_STATE_DEFAULT;
                        utils_explore_operation_stop(&explorer);
                        explorer_clear_all_item_mask();
                    }

                    if(explorer_state !=  EXPLORER_STATE_COPYING){
                        operation_status = 4;
                    }
                }
                else if (operation_status == 3) // 完成,刷新列表
                {
                    if(strlen(file_copy) > 0)
                        FileManager::GetInstance()->AddFile(file_copy);
                    memset(file_copy,0,sizeof(file_copy));
                    
                    operation_status = 0;
                    explorer_listitem_update();
                    // printf("prepare_print：%d", prepare_print);
                    if(prepare_print){
                        prepare_print = false;
                        sprintf(print_dest_name, "%s/%s", "/user-resource", file_item.name);
                        strcpy(file_item.path, print_dest_name);
                        ui_set_window_index(WINDOW_ID_PRINT, &file_item);
                        app_top_update_style(window_get_top_widget_list());
                        return true;
                    }
                }
                else if(operation_status == 4) //失败,刷新列表
                {
                    explorer_state = EXPLORER_STATE_DEFAULT;
                    memset(file_copy,0,sizeof(file_copy));
                    if(window_file_info)
                    {
                        widget_t ** widget_list = window_get_widget_list();
                        if(ui_get_window_index() == WINDOW_ID_EXPLORER)
                        {
                            lv_event_send(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], (lv_event_code_t)LV_EVENT_CHILD_DESTROYED, NULL);
                        }
                        else
                            return true;                        
                    }

                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_explorer_routine_msgbox_callback, (void *)MSGBOX_TIP_COPY_FAIL);
                    operation_status = 0;
                    prepare_print = false;
                    explorer_listitem_update();
                    return true;
                }
            // }
        }
        else if (tip_index == MSGBOX_TIP_DELETE)
        {
            if(explorer_model == NULL)   //已切换界面，消除弹窗
                return true;

            if (utils_get_current_tick() - start_tick > 2 * 1000)
            {
                explorer_state = EXPLORER_STATE_DEFAULT;
                // explorer_set_file_loading_status(false);
                // app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_FILE_LOADING);
                utils_explorer_opendir(&explorer, ".");
                
                if(item_list_size <= (explorer_current_page - 1) * EXPLORER_LIST_ITEMS)     //删除未页所有文件后刷新到第一页
                    explorer_current_page = 1;
                
                explorer_listitem_update();
                return true;
            }
        }
        else if(tip_index == MSGBOX_TIP_FILE_LOADING)
        {
            if(explorer_model == NULL)      //explorer_model为空表明界面已切换，此时关闭弹窗
                return true;

            if(explorer_file_loading_done == true)
            {
                explorer_file_loading_done = false;
                explorer_listitem_update();
                if(!hl_disk_default_is_mounted(HL_DISK_TYPE_USB))   //不存在U盘
                {
                    if (enter_explorer_index == ENTER_EXPLORER_USB)
                    {
                        explorer_file_loading_done = false;
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_explorer_routine_msgbox_callback, (void *)MSGBOX_TIP_UDISK_ABNORMAL);
                        return true;
                    }
                }

                widget_t ** widget_list = window_get_widget_list();
                if(enter_explorer_index == ENTER_EXPLORER_LOCAL)
                {
                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_BTN_DELETE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_height(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], 74);
                    lv_label_set_text(widget_list[WIDGET_ID_EXPLORER_BTN_IMPORT]->obj_container[2], tr(27));
                }
                else if(enter_explorer_index == ENTER_EXPLORER_USB)
                {
                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_BTN_DELETE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_height(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], 36);
                    lv_label_set_text(widget_list[WIDGET_ID_EXPLORER_BTN_IMPORT]->obj_container[2], tr(225));
                }
                else if(enter_explorer_index == ENTER_EXPLORER_HISTORY)
                {
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_LOCAL_USB_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_HISTORY_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_FILE_INFO_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_IMPORT_DEL]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_CONTAINER_TOP]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                   
                    history_listitem_update();
                }

                // 禁止点击各文件列表项
                for (int i = 0; i < EXPLORER_LIST_ITEMS; i++)
                {           
                    app_listitem_t *item = app_listitem_model_get_item(explorer_model, i);
                    window_t *win = item->win;
                    widget_t **widget_list = win->widget_list;
                    
                    lv_obj_clear_flag(widget_list[WIDGET_ID_EXPLORER_LIST_TEMPLATE_CONTAINER_CONTAINER]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                }
                filelist_click_forbidden = true;
                return true;
            }
        }
        else if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN_OPERATION ||
                 tip_index == MSGBOX_TIP_EXECUTING_OTHER_TASK ||
                 tip_index == MSGBOX_TIP_NOT_CAMERA)
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

static bool app_explorer_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    int ret;
    uint8_t fail_type = 0;
    struct statvfs buffer;
    double total_space, free_space, used_space;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_COPY_FAIL)
        {
            ret = statvfs("/user-resource", &buffer);
            if (!ret) {
                total_space = (double)(buffer.f_blocks * buffer.f_frsize) / 1024. / 1024. / 1024.;
                free_space = (double)(buffer.f_bfree * buffer.f_frsize) / 1024. / 1024. / 1024.;
                used_space = (double)((buffer.f_blocks * buffer.f_frsize) - (buffer.f_bfree * buffer.f_frsize)) / 1024. / 1024. / 1024.;
                if(free_space < 0.01){
                    fail_type = 1;  //空间不足
                }
                // LOG_I("总存储空间(G): %.1lf\n", total_space);
                // LOG_I("已使用的存储空间(G): %.1lf\n", used_space);
                // LOG_I("剩余存储空间(G): %.1lf\n", free_space);
                // LOG_I("memory_percentage: %d\n", memory_percentage);
            } 
            if(fail_type){
                lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(200));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(45));
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            }
            else{
                lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(198));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
                lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(45));
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
                lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
            }
        }
        else if(tip_index == MSGBOX_TIP_UDISK_ABNORMAL)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(198));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], lv_color_hex(0xFFFFFFFF), LV_PART_MAIN);
        }
        else if(tip_index == MSGBOX_TIP_MISSING_DATE)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(333));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[2], tr(31));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[2], tr(49));
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
            if (tip_index == MSGBOX_TIP_MISSING_DATE)
            {
                calibration_switch = true;
                file_item.userdata = (void *)&calibration_switch;
                if (enter_explorer_index == ENTER_EXPLORER_USB)
                {
                    uint64_t available_size = 0;
                    if(utils_get_dir_available_size("/user-resource") > TLP_FILE_TLP_PARTITION)
                        available_size = utils_get_dir_available_size("/user-resource") - TLP_FILE_TLP_PARTITION;
                    if (utils_get_size(print_src_name) > available_size)
                    {
                        app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_NO_MEMORY);
                        return true;
                    }

                    utils_explore_operation_start(&explorer, print_src_name, print_dest_name, 0);
                    snprintf(file_copy, sizeof(file_copy), "/user-resource/%s", utils_get_file_name(print_src_name));
                    explorer_state = EXPLORER_STATE_COPYING;
                    operation_status = 1;
                    app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_explorer_single_msgbox_callback, (void *)MSGBOX_TIP_IMPORT);
                }
                prepare_print = true;
                // ui_set_window_index(WINDOW_ID_PRINT, &file_item);
                app_top_update_style(window_get_top_widget_list());
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

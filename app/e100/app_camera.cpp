#include "app_camera.h"
#include "ai_camera.h"
#include "klippy.h"
#include "params.h"
#include "hl_disk.h"
#include "configfile.h"
#include "hl_common.h"
#include "hl_camera.h"
#include "aic_tlp.h"

#define LOG_TAG "app_camera"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#define CAMERA_LIST_ITEMS 3

enum
{
    COPY_STATE_IDLE = 0,
    COPY_STATE_COPYING,
    COPY_STATE_FAIL,
};

static app_listitem_model_t *camera_model = NULL;
static int camera_total_number = 0;
static int camera_total_page = 0;
static int camera_current_page = 0;
static bool select_state = false;
static int camera_index = 0;
int detection_frequency_sec = 0; //间隔监测 0:常规 1:专业
static int camera_page_index = 0;                    // 0:主页 1:视频列表页 2:ai页
extern uint8_t aic_detection_switch_flag;
static int camera_total_tlp_index[PRINT_HISTORY_SIZE];
static int select_number = 0;
extern bool photography_switch;
extern bool foreign_detection_switch;
extern ConfigParser *get_sysconf();

static void app_camera_update(widget_t **widget_list);
static void camera_listitem_callback(widget_t **widget_list, widget_t *widget, void *e);
static void camera_listitem_update(void);
static bool app_camera_frequency_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e);

static int operation_status = COPY_STATE_IDLE;
static pthread_t thread;
static sem_t stop_sem; // 发送停止命令的信号量
static uint8_t copy_state = COPY_STATE_IDLE;   
static float operation_progress = 0.0f;

#if 1 // 多选设置
enum
{
    ITEM_FLAG_SELETED = 0X00000001,
};

static void tlp_set_item_mask(int index, uint32_t mask)
{
    machine_info.print_history_record[index].mask = ((uint32_t)machine_info.print_history_record[index].mask | mask);
    select_number++;
}

static void tlp_reset_item_mask(int index, uint32_t mask)
{
    machine_info.print_history_record[index].mask = ((uint32_t)machine_info.print_history_record[index].mask & (~mask));
    select_number--;
}

static bool tlp_get_item_mask(int index, uint32_t mask)
{
    return (uint32_t)machine_info.print_history_record[index].mask & mask;
}

static void tlp_clear_all_item_mask(void)
{
    for (int i = 0; i < machine_info.print_history_valid_numbers; i++)
        machine_info.print_history_record[i].mask = (0);
    select_number = 0;
}
#endif



/*
    @brief : 计算文件的md5值
    @param :
        @filename: 文件路径
        @digest：计算的md5值
    @return
        @ -1：异常
        @ 0：正常
*/
static int calculate_md5(const char *filename, unsigned char *digest) 
{
    FILE *file = fopen(filename, "rb");
    if (!file) 
    {
        perror("fopen");
        return -1;
    }

    MD5_CTX md5;
    MD5_Init(&md5);

    unsigned char buffer[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) != 0) 
    {
        if (ferror(file)) 
        {
            perror("fread");
            fclose(file);
            return -1;
        }
        MD5_Update(&md5, buffer, bytesRead);
    }

    MD5_Final(digest, &md5);

    fclose(file);
    return 0;
}

// @brief : 显示md5值
static void print_md5(const unsigned char *md5) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        printf("%02x", md5[i]);
    }
    printf("\n");
}

static void *copy_routine(void *arg)
{
    char *src_path = (char *)arg;
    char dst_path[PATH_MAX_LEN + 1];
    snprintf(dst_path, sizeof(dst_path), "/mnt/exUDISK/%s", utils_get_file_name(src_path));
    printf("cp \"%s\" \"%s\" \n", src_path, dst_path);

    unsigned char digest_src[MD5_DIGEST_LENGTH];
    unsigned char digest_dst[MD5_DIGEST_LENGTH];

    // 计算复制前文件md5
    int ret = calculate_md5(src_path, digest_src);
    if(ret != 0)
    {
        printf("calculate src file(%s) md5 error!\n",src_path);
        copy_state = COPY_STATE_FAIL;
        return NULL;
    }

    uint64_t size = 0;
    uint64_t offset = 0;
    uint64_t ticks;
    uint64_t copy_state_temp = 0;
    ticks = hl_get_tick_ms();
    
    void *ctx;
    if (hl_copy_create(&ctx, src_path, dst_path) != 0)
    {
        remove(dst_path);
        copy_state = COPY_STATE_IDLE;
        return NULL;
    }
    sem_init(&stop_sem, 0, 0);
    do
    {
        copy_state_temp = hl_copy(ctx, &size, &offset);
        
        // 若收到停止信号...或者内存已满
        if ((sem_trywait(&stop_sem) == 0) || (copy_state_temp == -1))
        {
            // 确保已完成操作的文件不会被删除
            if (offset < size)
                remove(dst_path);
            break;
        }
        operation_progress = (float)offset * 100.0f / (float)size;
    } while (offset < size);
    hl_copy_destory(&ctx);

    // 标准库IO带缓存,要考虑是否马上同步内容.如果用户复制完就拔掉U盘不进行同步和umount操作肯定会丢失数据.
    utils_vfork_system("sync");

    // 完成复制
    if (offset >= size)
    {
        //计算复制后文件md5
        int ret = calculate_md5(dst_path, digest_dst);
        if(ret != 0)
        {
            printf("calculate dst file(%s) md5 error!\n",dst_path);
            copy_state = COPY_STATE_FAIL;
            sem_destroy(&stop_sem);
            return NULL;
        }
        else
        {
            if (memcmp((const char*)digest_dst, (const char *)digest_src ,MD5_DIGEST_LENGTH) == 0)    //verify success
            {
                printf("copy successful\n");
            }
            else
            {
                printf("verify md5 failed!\n");
                // print_md5(digest_src);
                // print_md5(digest_dst);
                copy_state = COPY_STATE_FAIL;
                sem_destroy(&stop_sem);
                return NULL;
            }
        }
    }
    // else  if (copy_state_temp == -1)
    else
    {
        printf("copy failed ! copy spent ticks %llu\n", hl_get_tick_ms() - ticks);
        copy_state = COPY_STATE_FAIL;
        sem_destroy(&stop_sem);
        return NULL;
    }
    printf("copy spent ticks %llu\n", hl_get_tick_ms() - ticks);

    copy_state = COPY_STATE_IDLE;
    sem_destroy(&stop_sem);
    return NULL;
}



static bool app_camera_routine_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_UDISK_STORAGE_LACK)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(197));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            tlp_clear_all_item_mask();
        }
        else if (tip_index == MSGBOX_TIP_UDISK_ABNORMAL)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_LEFT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_RIGHT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WIDGET_ID_ROUTINE_MSGBOX_IMAGE_IMAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], tr(53));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_CONTENT]->obj_container[2], tr(198));
            lv_label_set_text_fmt(widget_list[WIDGET_ID_ROUTINE_MSGBOX_BTN_SINGLE]->obj_container[2], tr(49));
            lv_obj_set_style_text_color(widget_list[WIDGET_ID_ROUTINE_MSGBOX_LABEL_TITLE]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            tlp_clear_all_item_mask();
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

static bool app_tlp_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    static uint64_t delay_time = 0;
    static bool is_mounted = false;
    static char mp4_path[1024];
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_EXPORTING)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:%d%%", tr(289), 0);
            for (int i = 0; i < machine_info.print_history_valid_numbers; i++)
            {
                if (tlp_get_item_mask(i, ITEM_FLAG_SELETED))
                {
                    LOG_D("machine_info tlp_path = %s\n", machine_info.print_history_record[camera_total_tlp_index[i]].tlp_path);
                }
            }
            operation_progress = 0.0f;
        }
        else if(tip_index == MSGBOX_TIP_EXPORT_COMPLETE)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(291));
        }
        start_tick = utils_get_current_tick();
        delay_time = utils_get_current_tick();
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
            if (tip_index == MSGBOX_TIP_EXPORT_COMPLETE)
            {
                return true;
            }
            return false;
        }
        break;
    case LV_EVENT_UPDATE:
    if (utils_get_current_tick() - delay_time > 100)
    {
        if(tip_index == MSGBOX_TIP_EXPORTING)
        {
            int copy_index = 0;
            while (1)
            {
                uint8_t progress = (uint8_t)(0.8f * get_aic_tlp_progress() + 0.2f * operation_progress);
                lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], "%s:%d%%", tr(289), progress);
                if (operation_status == COPY_STATE_IDLE) // 空闲
                {
                    if(operation_status == COPY_STATE_IDLE)   //复制完成
                    {
                        if (copy_index > machine_info.print_history_valid_numbers - 1)
                        {
                            LOG_I("copy_index : %d，machine_info.print_history_valid_numbers : %d\n",copy_index, machine_info.print_history_valid_numbers);
                            utils_vfork_system("sync");
                            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_tlp_single_msgbox_callback, (void *)MSGBOX_TIP_EXPORT_COMPLETE);
                            return true;
                        }
                    }
                    if (tlp_get_item_mask(copy_index, ITEM_FLAG_SELETED))
                    {
                        sprintf(mp4_path, "%s.mp4", machine_info.print_history_record[camera_total_tlp_index[copy_index]].tlp_path);
                        
                        if (access(mp4_path, F_OK) != 0)
                        {
                            if (utils_get_current_tick() - start_tick > 1200 * 1000)
                            {
                                return true;
                            }
                            return false;
                        }
                        tlp_reset_item_mask(copy_index, ITEM_FLAG_SELETED);
                        
                    }
                    copy_index++;
                }
                else if (operation_status == COPY_STATE_COPYING) // 复制进行中
                {
                    if(!hl_disk_default_is_mounted(HL_DISK_TYPE_USB))   //不存在U盘
                    {
                        sem_post(&stop_sem);
                        pthread_join(thread, NULL);
                        operation_status = COPY_STATE_FAIL;
                        break;
                    }
                    if(copy_state != COPY_STATE_COPYING)    //复制出错或结束
                    {
                        if(copy_state == COPY_STATE_FAIL)
                        {
                            operation_status = COPY_STATE_FAIL;
                            break;
                        }
                        operation_status = COPY_STATE_IDLE;
                    }
                }
                else if(operation_status == COPY_STATE_FAIL) //失败,刷新列表
                {
                    tlp_clear_all_item_mask();
                    operation_status = COPY_STATE_IDLE;
                    app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_camera_routine_msgbox_callback, (void *)MSGBOX_TIP_UDISK_ABNORMAL);
                    return true;
                }
            }
        }
        else if(tip_index == MSGBOX_TIP_EXPORT_COMPLETE)
        {
            if (utils_get_current_tick() - start_tick > 2 * 1000)
            {
                return true;
            }
        }  
        delay_time = utils_get_current_tick();
    }
        break;
    }
    return false;
}

void app_camera_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    static int last_camera_current_page = -1;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        select_state = false;
        last_camera_current_page = -1;
        camera_current_page = 1;
        camera_page_index = 0;
        select_number = 0;
        aic_detection_switch_sending = false;
        if (camera_model == NULL)
        {
            camera_model = app_listitem_model_create(WINDOW_ID_CAMERA_LIST_TEMPLATE, widget_list[WIDGET_ID_CAMERA_CONTAINER_LIST]->obj_container[0], camera_listitem_callback, NULL);
            for (int i = 0; i < CAMERA_LIST_ITEMS; i++)
                app_listitem_model_push_back(camera_model);
        }

        // 获取成功生成延时摄影总数(倒序排列)
        camera_total_number = 0;
        for (int i = machine_info.print_history_valid_numbers - 1; i >= 0; i--)
        {
            int index = (machine_info.print_history_current_index - (machine_info.print_history_valid_numbers - 1 - i) - 1) % PRINT_HISTORY_SIZE;
            if (machine_info.print_history_record[index].tlp_state == 1 &&
                access(machine_info.print_history_record[index].tlp_path, F_OK) == 0 &&
                strstr(machine_info.print_history_record[index].tlp_path, "/board-resource/") == NULL)
            {
                camera_total_tlp_index[camera_total_number] = index;
                camera_total_number++;
            }
        }

        if(camera_total_number == 0)
        {
            lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT]->obj_container[0],LV_OBJ_FLAG_CLICKABLE);
        }

        // 隐藏AI功能和异物检测功能入口
        lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_BTN_AI_FUNCTION_ITEM]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_BTN_FOREIGN_DETECTION]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_BTN_FOREIGN_LINE]->obj_container[0],LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_BTN_AI_FUNCTION_LINE]->obj_container[0],LV_OBJ_FLAG_HIDDEN);

        camera_listitem_update();
        app_camera_update(widget_list);
        break;
    case LV_EVENT_DESTROYED:
        app_listitem_model_destory(camera_model);
        camera_model = NULL;
        tlp_clear_all_item_mask();
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CAMERA_BTN_CANCEL:
        case WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT:
            if (widget->header.index == WIDGET_ID_CAMERA_BTN_CANCEL)
            {
                if(select_state)
                {
                    select_state = false;
                    tlp_clear_all_item_mask();
                }
                else
                {
                    camera_page_index = 0;
                    app_camera_update(widget_list);
                }
            }
            else if (widget->header.index == WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT)
            {
                if (select_state)       //点击导出
                {
                    if (select_number > 0)
                    {
                        if (app_print_get_print_state() == true)
                        {
                            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN);
                        }
                        else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
                        {
                            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER);
                        }
                        else if (hl_disk_default_is_mounted(HL_DISK_TYPE_USB))
                        {
                            app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_tlp_single_msgbox_callback, (void *)MSGBOX_TIP_EXPORTING);
                        }
                        else
                        {
                            app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_camera_routine_msgbox_callback, (void *)MSGBOX_TIP_UDISK_ABNORMAL);
                        }
                    }
                    select_state = false;
                }
                else                    //点击多选
                    select_state = true;
            }

            if (select_state)
            {
                lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT]->obj_container[2], tr(180));
            }
            else
            {
                lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT]->obj_container[2], tr(179));
            }
            camera_listitem_update();
            break;
        case WIDGET_ID_CAMERA_BTN_PREV_PAGE:
            if (camera_current_page > 1)
                camera_current_page--;
            camera_listitem_update();
            break;
        case WIDGET_ID_CAMERA_BTN_NEXT_PAGE:
            if (camera_current_page < camera_total_page)
                camera_current_page++;
            camera_listitem_update();
            break;
        case WIDGET_ID_CAMERA_BTN_VIDEO_LIST_ITEM:
        // case WIDGET_ID_CAMERA_BTN_VIDEO_LIST_ENTER:
            camera_page_index = 1;
            app_camera_update(widget_list);
            break;
        case WIDGET_ID_CAMERA_BTN_TLP_ITEM:
        // case WIDGET_ID_CAMERA_BTN_TLP_SWITCH:
            if(app_print_get_print_state() == true)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN);
            }
            else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER);
            }
            else if (hl_camera_get_exist_state() == false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void *)MSGBOX_TIP_NOT_CAMERA);
            }
            else
            {
                photography_switch = !photography_switch;  
                get_sysconf()->SetBool("system", "tlp_switch", photography_switch);
                get_sysconf()->WriteIni(SYSCONF_PATH);
            }
            app_camera_update(widget_list);
            break;
        case WIDGET_ID_CAMERA_BTN_AI_FUNCTION_ITEM:
        // case WIDGET_ID_CAMERA_BTN_AI_FUNCTION_ENTER:
            camera_page_index = 2;
            app_camera_update(widget_list);
            break;
        case WIDGET_ID_CAMERA_BTN_FOREIGN_DETECTION:
        case WIDGET_ID_CAMERA_BTN_AI_DETECTION_SWITCH:
            if (hl_camera_get_exist_state() == false)
            {
                app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void *)MSGBOX_TIP_NOT_CAMERA);
            }
            else
            {
#if CONFIG_SUPPORT_AIC
                if (widget->header.index == WIDGET_ID_CAMERA_BTN_FOREIGN_DETECTION)
                {
                    foreign_detection_switch = !foreign_detection_switch;
                    get_sysconf()->SetBool("system", "foreign_detection_switch", foreign_detection_switch);
                    get_sysconf()->WriteIni(SYSCONF_PATH);
                }
                else
                {
                    aic_function_switch = !aic_function_switch;
                    get_sysconf()->SetBool("system", "aic_function_switch", aic_function_switch);
                    get_sysconf()->WriteIni(SYSCONF_PATH);
                }

                if (aic_function_switch || foreign_detection_switch)
                {
                    if (aic_light_switch_flag == AIC_GET_STATE_LED_OFF ||
                        aic_light_switch_flag == AIC_GET_STATE_LED_ABNORMAL)
                        ai_camera_send_cmd_handler(AIC_CMD_CAMERA_LIGHT, AIC_CMD_CARRY_ON_LED);
                }

                app_camera_update(widget_list);
#endif
            }
            break;
        case WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE:
            app_msgbox_push(WINDOW_ID_CAMERA_FREQUENCY, true, app_camera_frequency_msgbox_callback, NULL);
            break;
        case WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM:
            aic_abnormal_pause_print = !aic_abnormal_pause_print;
            get_sysconf()->SetBool("system", "aic_abnormal_pause_print", aic_abnormal_pause_print);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            if (aic_abnormal_pause_print)
                lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[1], ui_get_image_src(139));
            else
                lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[1], ui_get_image_src(140));
            break;
        case WIDGET_ID_CAMERA_BTN_AI_PAGE_RETURN:
            camera_page_index = 0;
            app_camera_update(widget_list);
            break;
        }
        break;
    case LV_EVENT_CHILD_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CAMERA_CONTAINER_LIST:
        {
            widget_t *list_widget = (widget_t *)lv_event_get_param((lv_event_t *)e);
            window_t *win = list_widget->win;
            int model_size = app_listitem_model_count(camera_model);
            app_listitem_t *item;
            int item_index, array_index;
            for (item_index = 0; item_index < model_size; item_index++)
            {
                item = app_listitem_model_get_item(camera_model, item_index);
                if (item->win == win)
                    break;
            }

            array_index = item_index + ((camera_current_page - 1) * CAMERA_LIST_ITEMS);

            switch (list_widget->header.index)
            {
            case WIDGET_ID_CAMERA_LIST_TEMPLATE_CONTAINER_ITEM:
                if (select_state)
                {
                    if (tlp_get_item_mask(array_index, ITEM_FLAG_SELETED))
                        tlp_reset_item_mask(array_index, ITEM_FLAG_SELETED);
                    else
                        tlp_set_item_mask(array_index, ITEM_FLAG_SELETED);
                    camera_listitem_update();
                }
                break;
            case WINDOW_ID_CAMERA_LIST_TEMPLATE_BTN_EXPORT:
                if (select_state == false)
                {
                    if (app_print_get_print_state() == true)
                    {
                        app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void *)MSGBOX_TIP_PRINTING_FORBIDDEN);
                    }
                    else if (Printer::GetInstance()->m_change_filament->is_feed_busy() != false)              //进退料中
                    {
                        app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_camera_single_msgbox_callback, (void*)MSGBOX_TIP_EXECUTING_OTHER);
                    }
                    else if (hl_disk_default_is_mounted(HL_DISK_TYPE_USB))
                    {
                        tlp_set_item_mask(array_index, ITEM_FLAG_SELETED);
                        app_msgbox_push(WINDOW_ID_SINGLE_MSGBOX, true, app_tlp_single_msgbox_callback, (void *)MSGBOX_TIP_EXPORTING);
                    }
                    else
                        app_msgbox_push(WINDOW_ID_ROUTINE_MSGBOX, true, app_camera_routine_msgbox_callback, (void *)MSGBOX_TIP_UDISK_ABNORMAL);
                }
                break;
            }

            break;
        }
        break;
        }

    case LV_EVENT_UPDATE:
        uint8_t video_request = 0;

        // web端删除带有延迟摄影的历史记录时需要进行同步
        {
            // 1. 计算当前的延时摄影文件数
            int camera_total_number_tmp = 0;
            int camera_total_tlp_index_tmp[PRINT_HISTORY_SIZE] = {0};
            for (int i = machine_info.print_history_valid_numbers - 1; i >= 0; i--)
            {
                int index = (machine_info.print_history_current_index - (machine_info.print_history_valid_numbers - 1 - i) - 1) % PRINT_HISTORY_SIZE;
                if (machine_info.print_history_record[index].tlp_state == 1 &&
                    access(machine_info.print_history_record[index].tlp_path, F_OK) == 0 &&
                    strstr(machine_info.print_history_record[index].tlp_path, "/board-resource/") == NULL)
                {
                    camera_total_tlp_index_tmp[camera_total_number_tmp] = index;
                    camera_total_number_tmp++;
                }
            }

            // 2. 文件数发生变化了
            if (camera_total_number_tmp != camera_total_number)
            {
                camera_total_number = camera_total_number_tmp;
                memcpy(camera_total_tlp_index, camera_total_tlp_index_tmp, sizeof(camera_total_tlp_index));

                if (camera_total_number == 0)
                {
                    lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
                }

                
                if (camera_total_number % CAMERA_LIST_ITEMS == 0)
                {
                    if (camera_total_number % CAMERA_LIST_ITEMS == 0)
                        camera_total_page = camera_total_number / CAMERA_LIST_ITEMS;
                    else
                        camera_total_page = (camera_total_number / CAMERA_LIST_ITEMS) + 1;
                    
                    if (camera_current_page != 1)   //处于第二页，当删除后只剩一页
                    {
                        camera_current_page--;
                    }
                    lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_CONTAINER_CUT_PAGE]->obj_container[2], "%d/%d", camera_current_page, camera_total_page);
                    camera_listitem_update();
                }
            }
        }

        if (camera_page_index == 1)
        {
            if (camera_total_page > 0)
            {
                lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
                if(!get_screen_state())
                {
                    lv_obj_move_foreground(widget_list[WIDGET_ID_CAMERA_CONTAINER_CUT_PAGE]->obj_container[0]);
                }
                if (last_camera_current_page != camera_current_page)
                {
                    last_camera_current_page = camera_current_page;
                    lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_CONTAINER_CUT_PAGE]->obj_container[2], "%d/%d", camera_current_page, camera_total_page);
                }
            }
            else
            {
                lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_CUT_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
        }
        app_camera_update(widget_list);
        break;
    }
}

static void app_camera_update(widget_t **widget_list)
{
    lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_VIDEO_LIST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_MAIN_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_AI_FUNCTION_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
    if (camera_page_index == 0) //主页
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_MAIN_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        if (photography_switch && hl_camera_get_exist_state())
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_TLP_SWITCH]->obj_container[1], ui_get_image_src(131));
        else
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_TLP_SWITCH]->obj_container[1], ui_get_image_src(132));

        if (foreign_detection_switch && hl_camera_get_exist_state())
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_FOREIGN_DETECTION]->obj_container[1], ui_get_image_src(131));
        else
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_FOREIGN_DETECTION]->obj_container[1], ui_get_image_src(132));
    }
    if (camera_page_index == 1) //视频列表页
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_VIDEO_LIST_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        if (select_state)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT]->obj_container[2], tr(180));
        }
        else
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_BTN_MULTIPLE_EXPORT]->obj_container[2], tr(179));
        }
    }
    if (camera_page_index == 2) // ai页
    {
        lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_CONTAINER_AI_FUNCTION_PAGE]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        if (detection_frequency_sec)
            lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[2], tr(277));
        else
            lv_label_set_text_fmt(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[2], tr(276));

        if (aic_function_switch && hl_camera_get_exist_state())
        {
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_AI_DETECTION_SWITCH]->obj_container[1], ui_get_image_src(131));
            lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

            if (aic_abnormal_pause_print)
                lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[1], ui_get_image_src(139));
            else
                lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[1], ui_get_image_src(140));
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[1], ui_get_image_src(236));
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_ITEM]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[2], 255, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[2], 255, LV_PART_MAIN);
        }
        else
        {
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_AI_DETECTION_SWITCH]->obj_container[1], ui_get_image_src(132));
            lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[0], LV_OBJ_FLAG_CLICKABLE);

            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[1], ui_get_image_src(262));
            lv_img_set_src(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[1], ui_get_image_src(261));
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_ITEM]->obj_container[2], 127, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_FREQUENCY_STATE]->obj_container[2], 127, LV_PART_MAIN);
            lv_obj_set_style_text_opa(widget_list[WIDGET_ID_CAMERA_BTN_DETECTION_ABNORMAL_ITEM]->obj_container[2], 127, LV_PART_MAIN);
        }
    }
}

static void camera_listitem_callback(widget_t **widget_list, widget_t *widget, void *e)
{
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_LONG_PRESSED:
        lv_event_send(app_listitem_model_get_parent(camera_model), (lv_event_code_t)LV_EVENT_CHILD_LONG_PRESSED, widget);
        break;
    case LV_EVENT_CLICKED:
        lv_event_send(app_listitem_model_get_parent(camera_model), (lv_event_code_t)LV_EVENT_CHILD_CLICKED, widget);
        break;
    }
}

static void camera_listitem_update(void)
{
    if (camera_total_number % CAMERA_LIST_ITEMS == 0)
        camera_total_page = camera_total_number / CAMERA_LIST_ITEMS;
    else
        camera_total_page = (camera_total_number / CAMERA_LIST_ITEMS) + 1;

    for (int i = 0; i < CAMERA_LIST_ITEMS; i++)
    {
        app_listitem_t *item = app_listitem_model_get_item(camera_model, i);
        window_t *win = item->win;
        widget_t **widget_list = win->widget_list;
        int listitem_index = i + ((camera_current_page - 1) * CAMERA_LIST_ITEMS);
        if (listitem_index > camera_total_number - 1)
        {
            lv_obj_add_flag(widget_list[WIDGET_ID_CAMERA_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        else
            lv_obj_clear_flag(widget_list[WIDGET_ID_CAMERA_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_pos(widget_list[WIDGET_ID_CAMERA_LIST_TEMPLATE_CONTAINER_ITEM]->obj_container[0], 0, 5 + 56 * i);
        lv_label_set_text_fmt(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], utils_get_file_name(machine_info.print_history_record[camera_total_tlp_index[listitem_index]].tlp_path));
        int hours = machine_info.print_history_record[camera_total_tlp_index[listitem_index]].tlp_time / 3600;
        int minutes = (machine_info.print_history_record[camera_total_tlp_index[listitem_index]].tlp_time - hours * 3600) / 60;
        int seconds = machine_info.print_history_record[camera_total_tlp_index[listitem_index]].tlp_time - hours * 3600 - minutes * 60;
        lv_label_set_text_fmt(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_TIME]->obj_container[0], "%s:%02dh%02dm%02ds", tr(181), hours, minutes, seconds);

        time_t timestamp = machine_info.print_history_record[camera_total_tlp_index[listitem_index]].start_time;
        if (machine_info.print_history_record[camera_total_tlp_index[listitem_index]].ntp_status == 0)
        {
            lv_label_set_text_fmt(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_DATE]->obj_container[0], "-/-/- -:-");
        }
        else
        {
            struct tm *timeinfo;
            char buffer[80];
            // 将时间戳转换为本地时间结构体
            timeinfo = localtime(&timestamp);
            // 格式化输出为年月日时分的字符串
            strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", timeinfo);
            lv_label_set_text_fmt(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_DATE]->obj_container[0], buffer);
        }

        lv_img_set_src(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_IMAGE_CAMERA]->obj_container[0], ui_get_image_src(193));

        if (select_state)
        {
            if (tlp_get_item_mask(listitem_index, ITEM_FLAG_SELETED))
                lv_img_set_src(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], ui_get_image_src(192));
            else
                lv_img_set_src(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], ui_get_image_src(191));

            lv_obj_clear_flag(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_BTN_EXPORT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_IMAGE_CAMERA]->obj_container[0], 44);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], 108);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_TIME]->obj_container[0], 108);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_DATE]->obj_container[0], 108);
        }
        else
        {
            lv_obj_add_flag(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_IMAGE_SELECT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_BTN_EXPORT]->obj_container[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_IMAGE_CAMERA]->obj_container[0], 7);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], 71);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_TIME]->obj_container[0], 71);
            lv_obj_set_x(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_DATE]->obj_container[0], 71);
        }
        lv_label_set_long_mode(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_NAME]->obj_container[0], LV_LABEL_LONG_DOT);
        lv_label_set_long_mode(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_TIME]->obj_container[0], LV_LABEL_LONG_DOT);
        lv_label_set_long_mode(widget_list[WINDOW_ID_CAMERA_LIST_TEMPLATE_LABEL_DATE]->obj_container[0], LV_LABEL_LONG_DOT);
    }
}

static bool app_camera_frequency_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        if (detection_frequency_sec)
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CAMERA_FREQUENCY_BTN_CONVENTION_TESTING]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CAMERA_FREQUENCY_BTN_PROFESSIONAL_TESTING]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CAMERA_FREQUENCY_BTN_CONVENTION_TESTING]->obj_container[0], lv_color_hex(0xFF49AE35), LV_PART_MAIN);
            lv_obj_set_style_bg_color(widget_list[WIDGET_ID_CAMERA_FREQUENCY_BTN_PROFESSIONAL_TESTING]->obj_container[0], lv_color_hex(0xFF4A4A4A), LV_PART_MAIN);
        }
        break;
    case LV_EVENT_DESTROYED:
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
        case WIDGET_ID_CAMERA_FREQUENCY_CONTAINER_MASK:
            return true;
        case WIDGET_ID_CAMERA_FREQUENCY_BTN_CONVENTION_TESTING:
            detection_frequency_sec = 0;
            get_sysconf()->SetInt("system", "detection_frequency_sec", detection_frequency_sec);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            app_camera_update(window_get_widget_list());
            return true;
        case WIDGET_ID_CAMERA_FREQUENCY_BTN_PROFESSIONAL_TESTING:
            detection_frequency_sec = 1;
            get_sysconf()->SetInt("system", "detection_frequency_sec", detection_frequency_sec);
            get_sysconf()->WriteIni(SYSCONF_PATH);
            app_camera_update(window_get_widget_list());
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        break;
    }
    return false;
}

bool app_camera_single_msgbox_callback(window_t *win, widget_t *widget, void *user_data, void *e)
{
    widget_t **widget_list = win->widget_list;
    static int tip_index = 0;
    static uint64_t start_tick = 0;
    bool ret = false;
    extern bool material_break_clear_msgbox_state;
    switch (lv_event_get_code((lv_event_t *)e))
    {
    case LV_EVENT_CREATED:
        tip_index = (int)user_data;
        if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(290));
            tlp_clear_all_item_mask();
        }
        else if (tip_index == MSGBOX_TIP_EXECUTING_OTHER)
        {
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(313));
            tlp_clear_all_item_mask();
        }
        else if (tip_index == MSGBOX_TIP_NOT_CAMERA)
        {
            material_break_clear_msgbox_state = true;
            lv_label_set_text_fmt(widget_list[WIDGET_ID_SINGLE_MSGBOX_LABEL_CONTENT]->obj_container[0], tr(324));
        }
        start_tick = utils_get_current_tick();
        break;
    case LV_EVENT_DESTROYED:
        material_break_clear_msgbox_state = false;
        break;
    case LV_EVENT_CLICKED:
        switch (widget->header.index)
        {
            case WIDGET_ID_SINGLE_MSGBOX_CONTAINER_MASK:
            return true;
        }
        break;
    case LV_EVENT_UPDATE:
        if (tip_index == MSGBOX_TIP_PRINTING_FORBIDDEN ||
            tip_index == MSGBOX_TIP_EXECUTING_OTHER||
            tip_index == MSGBOX_TIP_NOT_CAMERA)
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

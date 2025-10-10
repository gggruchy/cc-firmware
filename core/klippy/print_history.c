
#include "config.h"
#include "print_history.h"
#include "params.h"
#include "hl_common.h"
#include <pthread.h>

#define LOG_TAG "print_history"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

static break_resume_his_param_t break_resume_param = {0}; //保存断电前打印历史项的开始时间和时间是否同步(断电续打)

// static pthread_rwlock_t glock = PTHREAD_RWLOCK_INITIALIZER;
void print_history_create_record(const char *filepath, char *task_id, slice_param_t *slice_param, char *image_dir)
{
    uint8_t md5[16];
    print_history_record_t *record;
    machine_info.print_history_current_index++;
    machine_info.print_history_valid_numbers++;
    if (machine_info.print_history_valid_numbers > PRINT_HISTORY_SIZE)
    {
        machine_info.print_history_valid_numbers = PRINT_HISTORY_SIZE;

        if (access(machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].image_dir, F_OK) == 0)
        {
            if (remove(machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].image_dir) == 0)
                LOG_I("delete file (%s) success!\n", machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].image_dir);
            else
                LOG_E("delete file (%s) error!\n", machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE].image_dir);
        }
    }
    record = &machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE];
    memset(record, 0, sizeof(print_history_record_t));

    if(break_resume_param.start_time != 0)  //断电续打后的打印,恢复真实开始打印时间
    {
        record->start_time = break_resume_param.start_time;
        record->ntp_status = break_resume_param.ntp_status;
        memset(&break_resume_param, 0, sizeof(break_resume_param));

        // LOG_I("recover last history param: start_time<%lld>,ntp_state<%d>\n",machine_info.print_history_valid_numbers,record->start_time,record->ntp_status);
    }
    else
    {
        record->start_time = hl_get_utc_second();
        if(get_ntpd_result() == 1)
            record->ntp_status = 1;
        else 
            record->ntp_status = 0;

        // printf("history start time<%lld>,ntp_status<%d>\n",record->start_time,record->ntp_status);
    }

    strncpy(record->image_dir, image_dir, PATH_MAX_LEN);
    utils_generate_md5(filepath, md5);
    strncpy(record->filepath, filepath, PATH_MAX_LEN);
    memcpy(record->md5, md5, 16);

    strncpy(record->local_task_id, task_id, sizeof(record->local_task_id));
    memcpy(&record->slice_param, slice_param, sizeof(slice_param_t));
    record->print_state = PRINT_RECORD_STATE_START;
    machine_info_save();
    LOG_I("file path %s task id %s create print history record success!\n", filepath, task_id);
}
void print_history_clean(void)
{
    machine_info.print_history_valid_numbers = 0;
    machine_info.print_history_current_index = 0;
    machine_info_save();
}
/**
 * @brief 打印记录更新
 * time: 打印时长 单位秒
 * state: 打印状态
 * current_layer: 当前层数
 * filament_used: 已用耗材
 * status_r: 状态原因(结束原因)
 */
void print_history_update_record(double time, uint32_t state, int current_layer, double filament_used, int status_r)
{
    print_history_record_t *record;
    record = &machine_info.print_history_record[(machine_info.print_history_current_index - 1) % PRINT_HISTORY_SIZE];
    record->print_duration = time;
    record->print_state = state;
    record->current_layer = current_layer;
    record->status_r = status_r;
    record->filament_used = filament_used;
    record->end_time = hl_get_utc_second();
    machine_info_save();

    LOG_I("print update record: state<%d>,time<%lf>,layer<%d>,filament_used<%lf>,status_r<%d>\n",
                            record->print_state,
                            record->print_duration,
                            record->current_layer,
                            record->filament_used,
                            record->status_r);
}
print_history_record_t *print_history_get_record(uint32_t index)
{
    int record_index = (machine_info.print_history_current_index - index - 1) % PRINT_HISTORY_SIZE;
    return &machine_info.print_history_record[record_index];
}
void print_history_set_mask(uint32_t index)
{
    print_history_get_record(index)->mask |= PRINT_RECORD_MASK_DELETE;
}

void print_history_reset_mask(uint32_t index)
{
    print_history_get_record(index)->mask &= (~PRINT_RECORD_MASK_DELETE);
}

bool print_history_get_mask(uint32_t index)
{
    return print_history_get_record(index)->mask & PRINT_RECORD_MASK_DELETE;
}

void print_history_clear_all_mask(void)
{
    for (int i = 0; i < PRINT_HISTORY_SIZE; i++)
        machine_info.print_history_record[i].mask &= (~PRINT_RECORD_MASK_DELETE);
}

void print_history_delete_record(void)
{
    bool done = true;
    do
    {
        done = true;
        for (int i = 0; i < machine_info.print_history_valid_numbers; i++)
        {
            if (print_history_get_mask(i))
            {

                if (access(print_history_get_record(i)->image_dir, F_OK) == 0)
                {
                    if (remove(print_history_get_record(i)->image_dir) == 0)
                        LOG_I("delete file (%s) success!\n", print_history_get_record(i)->image_dir);
                    else
                        LOG_E("delete file (%s) error!\n", print_history_get_record(i)->image_dir);
                }

                // 删除记录并移动后续元素
                for (int j = i; j < machine_info.print_history_valid_numbers - 1; j++)
                {
                    print_history_record_t *record = print_history_get_record(j);
                    print_history_record_t *next = print_history_get_record(j + 1);
                    memcpy(record, next, sizeof(print_history_record_t));
                }
                machine_info.print_history_valid_numbers--;
                // 清除最后一个元素
                memset(print_history_get_record(machine_info.print_history_valid_numbers), 0, sizeof(print_history_record_t));
                LOG_D("delete record %d\n", i);
                done = false;
                break;
            }
        }
    } while (!done);
    machine_info_save();
}

void save_last_history_param(break_resume_his_param_t param)
{
    break_resume_param.start_time = param.start_time;
    break_resume_param.ntp_status = param.ntp_status;
    
    LOG_I("save last history param: start_time<%lld>,ntp_state<%d>\n",
               break_resume_param.start_time,break_resume_param.ntp_status);
}
#include "config.h"
#include "print_stats.h"
#include "klippy.h"
#include "uuid.h"
#include "hl_common.h"
#include "my_string.h"
#include "print_history.h"
#include "Define_config_path.h"


#define LOG_TAG "print_stats"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define PRINT_STATS_STATE_CALLBACK_SIZE 16
static print_stats_state_callback_t print_stats_state_callback[PRINT_STATS_STATE_CALLBACK_SIZE];
static pthread_rwlock_t glock = PTHREAD_RWLOCK_INITIALIZER;
PrintStats::PrintStats(std::string section_name)
{
    Printer::GetInstance()->m_gcode->register_command("M117", std::bind(&PrintStats::cmd_M117, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("SET_PRINT_STATS_INFO", std::bind(&PrintStats::cmd_SET_PRINT_STATS_INFO, this, std::placeholders::_1));
    reset();
}

PrintStats::~PrintStats()
{
}

void PrintStats::_update_filament_usage(double eventtime)
{
    gcode_move_state_t gc_status = Printer::GetInstance()->m_gcode_move->get_status(eventtime);
    double cur_epos = gc_status.last_position[3];
    m_print_stats.filament_used += (cur_epos - m_last_epos) / gc_status.extrude_factor;
    m_last_epos = cur_epos;
}

void PrintStats::update_filament_usage(double eventtime)
{
    pthread_rwlock_wrlock(&glock);
    gcode_move_state_t gc_status = Printer::GetInstance()->m_gcode_move->get_status(eventtime);
    double cur_epos = gc_status.last_position[3];
    m_print_stats.filament_used += (cur_epos - m_last_epos) / gc_status.extrude_factor;
    m_last_epos = cur_epos;
    pthread_rwlock_unlock(&glock);
}

void PrintStats::set_current_file(std::string filename, int src, std::string taskid)
{
    reset();
    pthread_rwlock_wrlock(&glock);
    m_print_stats.file_src = src;
    m_filename = filename;
    strncpy(m_print_stats.taskid, taskid.c_str(), sizeof(m_print_stats.taskid));
    m_filename_without_path = hl_get_name_from_path(m_filename.c_str());
    if (m_filename_without_path == NULL)
    {
        m_filename_without_path = m_filename.c_str();
    }
    pthread_rwlock_unlock(&glock);
    LOG_I("filename:%s m_filename_without_path %s src %d taskid %s\n", filename.c_str(), m_filename_without_path, m_print_stats.file_src, m_print_stats.taskid);
}
void PrintStats::set_slice_param(slice_param_t slice_param)
{
    pthread_rwlock_wrlock(&glock);
    memcpy(&m_print_stats.slice_param, &slice_param, sizeof(slice_param_t));
    pthread_rwlock_unlock(&glock);
}
void PrintStats::set_task_id(std::string taskid)
{
    pthread_rwlock_wrlock(&glock);
    strncpy(m_print_stats.taskid, taskid.c_str(), sizeof(m_print_stats.taskid));
    pthread_rwlock_unlock(&glock);
}
std::string PrintStats::get_task_id(void)
{
    std::string taskid;
    pthread_rwlock_rdlock(&glock);
    taskid = m_print_stats.taskid;
    pthread_rwlock_unlock(&glock);
    return taskid;
}
void PrintStats::set_total_layers(uint32_t total_layers)
{
    pthread_rwlock_wrlock(&glock);
    m_print_stats.total_layers = total_layers;
    pthread_rwlock_unlock(&glock);
}

void PrintStats::note_start()
{
    pthread_rwlock_wrlock(&glock);
    double curtime = get_monotonic();
    // if (fabs(m_print_stats.print_start_time) <= 1e-15)
    // {
    //     m_print_stats.print_start_time = curtime;
    // }
    // else if (fabs(m_print_stats.last_pause_time) > 1e-15)
    // {
    //     // Update pause time duration
    //     double pause_duration = curtime - m_print_stats.last_pause_time;
    //     m_print_stats.total_pause_duration += pause_duration;
    //     m_print_stats.last_pause_time = 0;
    // }
    // Reset last e-position
    gcode_move_state_t gc_status = Printer::GetInstance()->m_gcode_move->get_status(curtime);
    m_last_epos = gc_status.last_position[3];
    if (m_print_stats.file_src == SDCARD_PRINT_FILE_SRC_LOCAL)
    {
        if ((strcmp(m_print_stats.local_task_id, "0") == 0 || strlen(m_print_stats.local_task_id) == 0))
        {
            uuid_t uuid;
            uuid_generate(uuid);
            uuid_unparse(uuid, m_print_stats.local_task_id); // 生成本地任务ID
#if CONFIG_PRINT_HISTORY
            char thumbnail_path[PATH_MAX_LEN] = {0};     // 此次打印历史记录的缩略图路径
            char thumbnail_path_tmp[PATH_MAX_LEN] = {0}; //  /tmp/中该文件的缩略图路径
            snprintf(thumbnail_path_tmp, sizeof(thumbnail_path_tmp), "/tmp/thumbnail/%s.png", utils_get_file_name((char *)m_filename.c_str()));
            if (access(thumbnail_path_tmp, F_OK) == 0)
            {
                if (access(HISTORY_IMAGE_PATH, F_OK) != 0)
                {
                    hl_system("mkdir -p %s", HISTORY_IMAGE_PATH);
                }
                // 将 /tmp/中 该打印文件缩略图保存到 HISTORY_IMAGE_PATH路径
                snprintf(thumbnail_path, sizeof(thumbnail_path), "%s/%s.png", HISTORY_IMAGE_PATH, m_print_stats.local_task_id);
                hl_system("cp \"%s\" \"%s\"", thumbnail_path_tmp, thumbnail_path);
            }
            else
                LOG_I("can't access thumbnail_path_tmp : %s\n",thumbnail_path_tmp);

            print_history_create_record((char *)m_filename.c_str(), m_print_stats.local_task_id, &m_print_stats.slice_param, thumbnail_path); // 创建打印记录
#endif

            m_print_stats.state = PRINT_STATS_STATE_PRINT_START;
            LOG_I(">>>>>> m_print_stats.state : PRINT_STATS_STATE_PRINT_START (%s)\n", __func__);
            print_stats_state_callback_call(PRINT_STATS_STATE_PRINT_START);
        }
        //         else
        //         {
        //             m_print_stats.state = PRINT_STATS_STATE_PRINTING;
        //             LOG_I(">>>>>> m_print_stats.state : PRINT_STATS_STATE_PRINTING (%s)\n", __func__);
        //             print_stats_state_callback_call(PRINT_STATS_STATE_PRINTING);
        //         }
    }
    else
    {
        strncpy(m_print_stats.local_task_id, "0", sizeof(m_print_stats.local_task_id));
    }
    strncpy(m_print_stats.filename, m_filename_without_path, sizeof(m_print_stats.filename));
    pthread_rwlock_unlock(&glock);
}

void PrintStats::note_actual_start()
{
    pthread_rwlock_wrlock(&glock);
    m_print_stats.state = PRINT_STATS_STATE_PRINTING;
    double curtime = get_monotonic();
    if (fabs(m_print_stats.print_start_time) <= 1e-15)
    {
        m_print_stats.print_start_time = curtime;
    }
    if (fabs(m_print_stats.last_pause_time) > 1e-15)
    {
        // Update pause time duration
        double pause_duration = curtime - m_print_stats.last_pause_time;
        double total_duration = curtime - m_print_stats.print_start_time;
        if (total_duration - pause_duration <= 1e-15)
        {
            pause_duration = 0.;
        }
        m_print_stats.total_pause_duration += pause_duration;
        m_print_stats.last_pause_time = 0;
    }
    // Reset last e-position
    gcode_move_state_t gc_status = Printer::GetInstance()->m_gcode_move->get_status(curtime);
    m_last_epos = gc_status.last_position[3];
    pthread_rwlock_unlock(&glock);
    print_stats_state_callback_call(PRINT_STATS_STATE_PRINTING);
}

void PrintStats::note_pauseing()
{
    if (m_print_stats.state != PRINT_STATS_STATE_ERROR && m_print_stats.state != PRINT_STATS_STATE_CANCELLED)
    {
        pthread_rwlock_wrlock(&glock);
        m_print_stats.state = PRINT_STATS_STATE_PAUSEING;
        pthread_rwlock_unlock(&glock);
        LOG_I(">>>>>> m_print_stats.state : PRINT_STATS_STATE_PAUSEING (%s)\n", __func__);
        // print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE);
        print_stats_state_callback_call(PRINT_STATS_STATE_PAUSEING);
    }
}
void PrintStats::note_pause()
{
    if (fabs(m_print_stats.last_pause_time) <= 1e-15)
    {
        double curtime = get_monotonic();
        pthread_rwlock_wrlock(&glock);
        m_print_stats.last_pause_time = curtime;
        // update filament usage
        if (fabs(m_print_stats.print_start_time) > 1e-15)
        {
            _update_filament_usage(curtime);
        }
        pthread_rwlock_unlock(&glock);
    }
    if (m_print_stats.state != PRINT_STATS_STATE_ERROR && m_print_stats.state != PRINT_STATS_STATE_CANCELLED)
    {
        pthread_rwlock_wrlock(&glock);
        m_print_stats.state = PRINT_STATS_STATE_PAUSED;
        pthread_rwlock_unlock(&glock);
        LOG_I(">>>>>> m_print_stats.state : PRINT_STATS_STATE_PAUSED (%s)\n", __func__);
        // print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE);
        print_stats_state_callback_call(PRINT_STATS_STATE_PAUSED);
    }
}
void PrintStats::note_complete()
{
    print_history_update_record(m_print_stats.total_duration, PRINT_RECORD_STATE_FINISH, m_print_stats.current_layer, m_print_stats.filament_used, 0); // todo 停止原因需要补充

    _note_finish(PRINT_STATS_STATE_COMPLETED);
    print_stats_state_callback_call(PRINT_STATS_STATE_COMPLETED);

    memset(m_print_stats.filename, 0, sizeof(m_print_stats.filename));
    memset(m_print_stats.local_task_id, 0, sizeof(m_print_stats.local_task_id));
    memset(m_print_stats.taskid, 0, sizeof(m_print_stats.taskid));
}
/**
 * todo 错误原因需要补充
 */
void PrintStats::note_error()
{
    print_history_update_record(m_print_stats.total_duration, PRINT_RECORD_STATE_ERROR, m_print_stats.current_layer, m_print_stats.filament_used, 0); // todo 错误原因需要补充

    _note_finish(PRINT_STATS_STATE_ERROR);
    // print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE);
    print_stats_state_callback_call(PRINT_STATS_STATE_ERROR);

    memset(m_print_stats.filename, 0, sizeof(m_print_stats.filename));
    memset(m_print_stats.local_task_id, 0, sizeof(m_print_stats.local_task_id));
    memset(m_print_stats.taskid, 0, sizeof(m_print_stats.taskid));
}
void PrintStats::note_canceling()
{
    pthread_rwlock_wrlock(&glock);
    m_print_stats.state = PRINT_STATS_STATE_CANCELLING;
    pthread_rwlock_unlock(&glock);
    LOG_I(">>>>>> m_print_stats.state : PRINT_STATS_STATE_CANCELLING (%s)\n", __func__);

    // print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE);
    print_stats_state_callback_call(PRINT_STATS_STATE_CANCELLING);
}
void PrintStats::note_cancel()
{
    print_history_update_record(m_print_stats.total_duration, PRINT_RECORD_STATE_CANCEL, m_print_stats.current_layer, m_print_stats.filament_used, 0); // todo 停止原因需要补充

    _note_finish(PRINT_STATS_STATE_CANCELLED);
    // print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE);
    print_stats_state_callback_call(PRINT_STATS_STATE_CANCELLED);

    memset(m_print_stats.filename, 0, sizeof(m_print_stats.filename));
    memset(m_print_stats.local_task_id, 0, sizeof(m_print_stats.local_task_id));
    memset(m_print_stats.taskid, 0, sizeof(m_print_stats.taskid));
}
void PrintStats::note_resuming()
{
    print_history_update_record(m_print_stats.total_duration, PRINT_RECORD_STATE_START, m_print_stats.current_layer, m_print_stats.filament_used, 0); // todo 停止原因需要补充
    pthread_rwlock_wrlock(&glock);
    m_print_stats.state = PRINT_STATS_STATE_RESUMING;
    pthread_rwlock_unlock(&glock);
    LOG_I(">>>>>> m_print_stats.state : PRINT_STATS_STATE_RESUMING (%s)\n", __func__);

    // print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE);
    print_stats_state_callback_call(PRINT_STATS_STATE_RESUMING);
}

void PrintStats::note_change_filament_start()
{
    print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE_FILAMENT_START);
}

void PrintStats::note_change_filament_completed()
{
    print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE_FILAMENT_COMPLETED);
}

void PrintStats::_note_finish(print_stats_state_s state)
{
        // if(fabs(m_print_stats.print_start_time) == 1e-15)
    // {
    //     return;
    // }
    pthread_rwlock_wrlock(&glock);
    double eventtime = get_monotonic();
    if (fabs(m_print_stats.print_start_time) <= 1e-15)
    {
        m_print_stats.total_duration = 0;
    }
    else
    {
        m_print_stats.total_duration = eventtime - m_print_stats.print_start_time + m_print_stats.last_print_time;
    }
    if (fabs(m_print_stats.filament_used) <= 1e-15)
    {
        // No positive extusion detected during print
        m_print_stats.init_duration = m_print_stats.total_duration - m_print_stats.total_pause_duration;
    }
    m_print_stats.print_start_time = 0;
    m_print_stats.last_print_time = 0;
    // memset(&m_print_stats, 0, sizeof(print_stats_t));
    m_print_stats.state = state;
    pthread_rwlock_unlock(&glock);

    LOG_I(">>>>>> m_print_stats.state : %d (%s)\n", m_print_stats.state, __func__);
}

void PrintStats::reset()
{
    pthread_rwlock_wrlock(&glock);
    m_filename = "";
    memset(&m_print_stats, 0, sizeof(print_stats_t));
    pthread_rwlock_unlock(&glock);
    // print_stats_state_callback_call(PRINT_STATS_STATE_CHANGE);
}

double PrintStats::get_print_duration()
{
    pthread_rwlock_rdlock(&glock);
    double eventtime = get_monotonic();
    double print_duration = eventtime - m_print_stats.print_start_time + m_print_stats.last_print_time;
    pthread_rwlock_unlock(&glock);
    return print_duration;
}
print_stats_t PrintStats::get_status(double eventtime, print_stats_t *cur_state)
{
    pthread_rwlock_wrlock(&glock);
    double time_paused = m_print_stats.total_pause_duration;
    if (fabs(m_print_stats.print_start_time) > 1e-15)
    {
        if (fabs(m_print_stats.last_pause_time) > 1e-15)
        {
            // Calculate the total time spent paused during the print
            time_paused += eventtime - m_print_stats.last_pause_time;
        }
        else
        {
            // Accumulate filament if not paused
            _update_filament_usage(eventtime);
        }
        m_print_stats.total_duration = eventtime - m_print_stats.print_start_time + m_print_stats.last_print_time;
        if (fabs(m_print_stats.filament_used) <= 1e-15)
        {
            // Track duration prior to extrusion
            m_print_stats.init_duration = m_print_stats.total_duration - time_paused;
        }
    }
    // m_print_stats.print_duration = m_print_stats.total_duration - m_print_stats.init_duration - time_paused;
    m_print_stats.print_duration = m_print_stats.total_duration - time_paused;

    if (cur_state != NULL)
    {
        memcpy(cur_state, &m_print_stats, sizeof(print_stats_t));
    }
    pthread_rwlock_unlock(&glock);
    return m_print_stats;
}
/**
 * 重新开始计算时间。
 */
void PrintStats::cmd_M117(GCodeCommand &gcode)
{
    pthread_rwlock_wrlock(&glock);
    m_print_stats.total_pause_duration = 0.0f;
    m_print_stats.total_duration = 0.0f;
    m_print_stats.last_pause_time = 0.0f;
    m_print_stats.init_duration = 0.0f;
    m_print_stats.print_start_time = get_monotonic();
    pthread_rwlock_unlock(&glock);
}
void PrintStats::cmd_SET_PRINT_STATS_INFO(GCodeCommand &gcode)
{
    uint32_t current_layer = gcode.get_int("CURRENT_LAYER", 0);
    double last_print_time = gcode.get_double("LAST_PRINT_TIME", 0.0);
    uint32_t total_layers = gcode.get_int("TOTAL_LAYERS", 0);
    pthread_rwlock_wrlock(&glock);
    if (current_layer > 0)
    {
        m_print_stats.current_layer = current_layer;
    }
    if (total_layers > 0)
    {
        if (current_layer != 0)
        {
            m_print_stats.current_layer = current_layer > total_layers ? total_layers : current_layer;
        }
        m_print_stats.slice_param.total_layers = total_layers; // 正常应该在最开始解析出来，如果后面有改变，需要同步
        m_print_stats.total_layers = total_layers;             // 正常应该在最开始解析出来，如果后面有改变，需要同步
    }
    if(last_print_time > 0)
    {
        m_print_stats.last_print_time = last_print_time;
        m_print_stats.total_duration = m_print_stats.last_print_time;
    }
    pthread_rwlock_unlock(&glock);
    LOG_D("current_layer %d total_layers %d\n", m_print_stats.current_layer, m_print_stats.slice_param.total_layers);
}

int print_stats_register_state_callback(print_stats_state_callback_t state_callback)
{
    for (int i = 0; i < PRINT_STATS_STATE_CALLBACK_SIZE; i++)
    {
        if (print_stats_state_callback[i] == NULL)
        {
            print_stats_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}
int print_stats_unregister_state_callback(print_stats_state_callback_t state_callback)
{
    for (int i = 0; i < PRINT_STATS_STATE_CALLBACK_SIZE; i++)
    {
        if (print_stats_state_callback[i] == state_callback)
        {
            print_stats_state_callback[i] = NULL;
            return 0;
        }
    }
    return -1;
}

int print_stats_state_callback_call(int event)
{
    for (int i = 0; i < PRINT_STATS_STATE_CALLBACK_SIZE; i++)
    {
        if (print_stats_state_callback[i] != NULL)
        {
            print_stats_state_callback[i](event);
        }
    }
    return 0;
}
print_stats_state_s print_stats_get_state(void)
{
    print_stats_state_s state;
    pthread_rwlock_rdlock(&glock);
    if (Printer::GetInstance()->m_print_stats == NULL)
    {
        pthread_rwlock_unlock(&glock);
        return PRINT_STATS_STATE_STANDBY;
    }
    state = Printer::GetInstance()->m_print_stats->m_print_stats.state;
    pthread_rwlock_unlock(&glock);
    return state;
}

int print_stats_set_state(print_stats_state_s status)
{
    pthread_rwlock_rdlock(&glock);
    if (Printer::GetInstance()->m_print_stats == NULL)
    {
        pthread_rwlock_unlock(&glock);
        return -1;
    }
    Printer::GetInstance()->m_print_stats->m_print_stats.state = status;
    pthread_rwlock_unlock(&glock);
    return 0;
}
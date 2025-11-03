#ifndef PRINT_STATS_H
#define PRINT_STATS_H
#include <string>
#include "gcode.h"
#include "print_stats_c.h"
typedef enum
{
    PRINT_STATS_INFO_UPDATE = 0,
    PRINT_STATS_STATE_CHANGE = 1,
} print_stats_event_t;
class PrintStats
{
private:
public:
    PrintStats(std::string section_name);
    ~PrintStats();
    std::string m_filename;
    const char *m_filename_without_path;
    double m_last_epos; // 挤出机的位置，用来算出料长度
    print_stats_t m_print_stats;
    void _update_filament_usage(double eventtime);
    void update_filament_usage(double eventtime);
    void set_current_file(std::string filename, int src, std::string taskid);
    double get_print_duration();
    void note_start();
    void note_actual_start();
    void note_pause();
    void note_pauseing();
    void note_canceling();
    void note_resuming();
    void note_complete();
    void note_error();
    void note_cancel();
    void note_change_filament_start();
    void note_change_filament_completed();
    void _note_finish(print_stats_state_s state);
    void reset();
    print_stats_t get_status(double eventtime, print_stats_t *cur_state);
    void cmd_M117(GCodeCommand &gcode);
    void cmd_SET_PRINT_STATS_INFO(GCodeCommand &gcode);
    void set_task_id(std::string taskid);
    std::string get_task_id(void);
    void set_slice_param(slice_param_t slice_param);
    void set_total_layers(uint32_t total_layers);
};
typedef void (*print_stats_state_callback_t)(int event);
int print_stats_register_state_callback(print_stats_state_callback_t state_callback);
int print_stats_unregister_state_callback(print_stats_state_callback_t state_callback);
int print_stats_state_callback_call(int event);
print_stats_state_s print_stats_get_state(void);
int print_stats_set_state(print_stats_state_s status);
int get_slice_param(slice_param_t *slice_param);
#endif
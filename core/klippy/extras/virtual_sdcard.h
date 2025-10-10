#ifndef VIRTUAL_SDCARD_H
#define VIRTUAL_SDCARD_H

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>
#include <malloc.h>
#include <assert.h>
#include <string>
#include "reactor.h"
#include "statistics.h"
#include "gcode.h"
#include <atomic>
#include <time.h>
#include "stdlib.h"
#include <stdio.h>
typedef enum{
    SDCARD_PRINT_FILE_SRC_LOCAL = 0,
    SDCARD_PRINT_FILE_SRC_REMOTE = 1,
}sdcard_print_file_src_t;
typedef struct files_info
{
    std::string name;
    struct stat info;
} file_info_t;

typedef struct virtual_sd_stats_tag
{
    std::string file_path;
    double progress;
    bool is_active;
    int file_position;
    int file_size;
} virtual_sd_stats_t;
class VirtualSD
{
private:
public:
    VirtualSD(std::string section_name);
    ~VirtualSD();

    std::vector<std::string> m_valid_gcode_exts;
    std::deque<GCodeCommand> m_pending_command; // 切料用 在执行G1时暂停会丢掉当前命令，需添加

    std::string m_sdcard_dirname;
    std::string m_current_file_name;
    std::ifstream m_current_file;
    std::string m_taskid;
    int m_file_position;
    int m_file_slicer;
    int m_file_size;
    bool m_cancel; //false:正常打印 true:取消打印
    bool m_must_pause_work;
    bool m_cmd_from_sd;
    bool m_is_active;
    int m_next_file_position;
    int M28_fd;
    int m_sdcard_print_file_src;
    bool custom_zero = false;  // 是否使用自定义归零

    ReactorTimerPtr m_work_timer;
    std::string m_cmd_SDCARD_RESET_FILE_help;
    std::string m_cmd_SDCARD_PRINT_FILE_help;

    std::string m_partial_input = "";
    std::vector<std::string> m_lines;
    bool m_start_print_from_sd = false;

    void load_current_layer(std::string &line, std::string &next_line);
    void handle_shutdown();
    std::string stats(double eventtime);
    std::vector<file_info_t> get_file_list(bool check_subdirs = false);
    virtual_sd_stats_t get_status(double eventtime);
    std::string file_path();
    double progress();
    bool is_active();
    bool start_print_from_sd();
    bool get_cancel_flag();
    bool is_printing();
    void resume_active();
    void do_pause();
    void do_resume();
    void do_cancel();
    void cmd_error(GCodeCommand &gcmd);
    void _reset_file();
    void cmd_SDCARD_RESET_FILE(GCodeCommand &gcmd);
    void cmd_SDCARD_PRINT_FILE(GCodeCommand &gcmd);
    void cmd_SDCARD_PRINT_FILE_from_break(GCodeCommand &gcmd);
    void cmd_M20(GCodeCommand &gcmd);
    void cmd_M21(GCodeCommand &gcmd);
    void cmd_M23(GCodeCommand &gcmd);
    void _load_file(GCodeCommand &gcmd, std::string filename, bool check_subdirs = false);
    void cmd_M24(GCodeCommand &gcmd);
    void cmd_M25(GCodeCommand &gcmd);
    void cmd_M26(GCodeCommand &gcmd);
    void cmd_M27(GCodeCommand &gcmd);
    void cmd_M28(GCodeCommand &gcmd);
    void cmd_M29(GCodeCommand &gcmd);
    void cmd_M30(GCodeCommand &gcmd);
    int get_file_position();
    void set_file_position(int pos);
    bool is_cmd_from_sd();
    double work_handler(double eventtime);

    bool is_work_active();
private:
    bool m_in_work_handler{false};  // 标记是否在work_handler中
};

#endif
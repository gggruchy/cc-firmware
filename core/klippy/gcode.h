#ifndef __NEW_GCODE__H__
#define __NEW_GCODE__H__

#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <queue>
#include "stdio.h"
#include <fstream>
#include <cctype>
#include <float.h>
#include <regex>
#include "reactor.h"
#include <mutex>
#include <atomic>
#define GCODE_COMMAND_QUEUE_SIZE 1024

enum print_mode {
    SDCARD_PRINT = 0,
    MANUAL_CONTROL,
};


class GCodeCommand
{
private:
public:
    std::map<std::string, std::function<void()>> command_callbacks;
    std::map<char, double> command_params;

    std::string m_command;
    std::string m_commandline;
    std::map<std::string, std::string> m_params;
    bool m_need_ack;
    std::function<void(std::string, bool)> m_respond_info;
    std::function<void(std::string)> m_respond_raw;
public:
    GCodeCommand(std::string command, std::string commandline, std::map<std::string, std::string>& params, bool need_ack);
    ~GCodeCommand();
    std::string get_command();
    std::string get_commandline();
    void get_command_parameters(std::map<std::string, std::string> &params);
    std::string get_raw_command_parameters();
    bool ack(std::string msg = "");
    std::string get_string(std::string name, std::string val_default);
    int get_int(std::string name, int val_default, int minval = INT32_MIN, int maxval = INT32_MAX);
    double get_double(std::string name, double val_default, double minval = DBL_MIN, 
        double maxval = DBL_MAX, double above = DBL_MIN, double below = DBL_MAX);
    float get_float(std::string name, float val_default, float minval = FLT_MIN, 
        double maxval = FLT_MAX, double above = FLT_MIN, double below = FLT_MAX);
};

class GCodeDispatch
{
public:
    GCodeDispatch();
    ~GCodeDispatch();

    std::map<std::string, std::function<void(GCodeCommand&)>> ready_gcode_handlers;
    std::map<std::string, std::function<void(GCodeCommand&)>> base_gcode_handlers;
    std::map<std::string, std::function<void(GCodeCommand&)>> gcode_handlers;
    std::map<std::string, std::function<void(GCodeCommand&)>> mux_commands;
    std::vector<std::function<void(std::string)>> output_callbacks;
    std::map<std::string, std::string> gcode_help;
    bool m_is_printer_ready;
    bool m_is_fileinput;
    std::string m_cmd_RESTART_help;
    std::string m_cmd_FIRMWARE_RESTART_help;
    std::string m_cmd_STATUS_help;
    std::string m_cmd_HELP_help;
    std::mutex m_mutex;

    // std::map<std::string, void*> ready_gcode_handlers;
    bool m_gcode_is_lock = false;
    std::mutex get_mutex();
    bool is_traditional_gcode(std::string cmd);
    std::function<void(GCodeCommand&)> register_command(std::string cmd, std::function<void(GCodeCommand&)> func, bool when_not_ready = false, std::string desc = "");
    void register_mux_command(std::string cmd, std::string key, std::string value, std::function<void(GCodeCommand&)> func, std::string desc = "");
    std::map<std::string, std::string> get_command_help();
    void register_output_handler(std::function<void(std::string)> cb);
    void handle_shutdown();
    void handle_disconnect();
    void handle_ready();
    void process_command(std::vector<std::string> &commands, bool need_ack = true);
    void run_script_from_command(std::vector<std::string> &script);
    void run_script(std::vector<std::string> &script);
    void run_script(std::string &script);
    GCodeCommand create_gcode_command(std::string command, std::string commandline, std::map<std::string, std::string> &params);
    void respond_raw(std::string msg);
    void respond_info(std::string msg, bool log = true);
    void respond_error(std::string msg);
    void respond_state(std::string state);
    GCodeCommand& get_extended_params(GCodeCommand &gcmd);
    void cmd_default(GCodeCommand &gcmd);
    void cmd_mux(GCodeCommand &gcmd);
    void cmd_G(GCodeCommand &gcmd);
    void cmd_M(GCodeCommand &gcmd);
    void cmd_M110(GCodeCommand &gcmd);
    void cmd_M112(GCodeCommand &gcmd);
    void cmd_M115(GCodeCommand &gcmd);
    void request_restart(std::string result);
    void cmd_RESTART(GCodeCommand &gcmd);
    void cmd_FIRMWARE_RESTART(GCodeCommand &gcmd);
    void cmd_ECHO(GCodeCommand &gcmd);
    void cmd_STATUS(GCodeCommand &gcmd);
    void cmd_HELP(GCodeCommand &gcmd);

private:
};

class GCodeIO
{
private:
    
public:
    GCodeIO();
    ~GCodeIO();
    void _handle_ready();
    void _dump_debug();
    void _handle_shutdown();
    void _process_data(double eventtime);
    void _respond_raw(std::string msg);
    void stats(double eventtime);

    void process_data(double eventtime);
    void open_file(std::string filename);
    void read_file();
    void single_command(std::string cmd);
    void single_command(const char *fmt, ...);


public:
    int m_fd;
    ReactorFileHandler *m_fd_handle = nullptr;
    bool m_is_printer_ready = false;
    bool m_is_processing_data = false;
    bool m_is_fileinput = false;
    bool m_pipe_is_active = true;
    int m_print_file_size;
    int m_print_file_current_pos;
    std::ifstream m_read_gcode_file;


};

#endif
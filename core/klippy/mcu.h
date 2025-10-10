#ifndef MCU_H
#define MCU_H

#include <vector>
#include <math.h>
#include <sstream>
#include <functional>
#include "clocksync.h"
#include "msgproto.h"
#include "serialhdl.h"
class Printer;
#define HomingLOG2() // std--::cout << "######### File: " << __FILE__ << "  line:" << __LINE__ << "  function:" << __FUNCTION__ << std::endl

extern "C"
{
#include "stepcompress.h"
#include "serialqueue.h"
#include "trdispatch.h"
#include "pyhelper.h"
}

struct pinParams;

struct lastState
{
    double last_value;
    double last_read_time;
};

#define REASON_ENDSTOP_HIT 1
#define REASON_COMMS_TIMEOUT 2
#define REASON_HOST_REQUEST 3
#define REASON_PAST_END_TIME 4

#define TIMEOUT_TIME 5.0
#define RETRY_TIME 0.500
class RetryAsyncCommand
{
private:
    Serialhdl *m_serial;
    std::string m_name;
    int m_oid;
    double m_min_query_time;

public:
    RetryAsyncCommand(Serialhdl *serial, std::string name, int oid = -1);
    ~RetryAsyncCommand();
    void handle_callback(ParseResult &params);
    ParseResult get_response(std::vector<std::string> cmds, command_queue *cmd_queue, uint64_t minclock = 0, uint64_t reqclock = 0);
};

class CommandQueryWrapper
{
private:
    Serialhdl *m_serial;
    std::string m_cmd;
    std::string m_response;
    int m_oid;
    SerialRetryCommand *m_xmit_helper;
    command_queue *m_cmd_queue;

public:
    CommandQueryWrapper(Serialhdl *serial, std::string msgformat, std::string respformat, int oid = -1, command_queue *cmd_queue = nullptr, bool is_async = false);
    ~CommandQueryWrapper();
    void do_send(std::string cmds, uint64_t minclock, uint64_t reqclock);
    void send(uint64_t minclock = 0, uint64_t reqclock = 0);
    void send_with_preface(std::string preface_cmd, uint64_t minclock = 0, uint64_t reqclock = 0);
};

class CommandWrapper
{
private:
    Serialhdl *m_serial;
    std::string m_cmd;
    command_queue *m_cmd_queue;

public:
    CommandWrapper(Serialhdl *serial, std::string msgformat, command_queue *cmd_queue = nullptr);
    ~CommandWrapper()
    {
    }
    void send(uint64_t minclock = 0, uint64_t reqclock = 0);
};
class McuChip
{
private:
public:
    McuChip() {}
    ~McuChip() {}
    virtual void *setup_pin(std::string pin_type, pinParams *pin_params) = 0;
};
class MCU : public McuChip
{
private:
public:
    Serialhdl *m_serial;
    int m_max_pending_blocks;

    // std::vector<std::function<void(int)>> m_callbacks;//void(double, double, double)
    std::vector<std::string> m_config_cmds;
    std::vector<std::string> m_restart_cmds;
    std::vector<std::string> m_init_cmds;
    steppersync *m_steppersync;
    std::vector<stepcompress *> m_stepqueues;
    double m_max_stepper_error;
    double m_mcu_tick_avg;
    double m_stats_sumsq_base;
    double m_mcu_tick_stddev;
    double m_mcu_tick_awake;
    int m_reserved_move_slots;
    double m_report_clock;
    lastState m_last_state;
    int init_config_ok;
    std::string m_restart_method;
    std::vector<std::string> m_power_pin;

    bool m_is_shutdown = false;
    bool m_is_keepalive;
    uint64_t m_shutdown_clock;
    int m_shutdown_msg;
    int use_mcu_spi;
    int m_oid_count;
    int mcu_type;
    double stats_sumsq_base;
    int32_t m_mcu_freq;
    std::string m_chip_name;
    std::string m_name;
    ClockSync *m_clocksync;
    uint32_t m_baud;
    std::string m_canbus_iface;
    std::string m_serialport;
    CommandWrapper *m_emergency_stop_cmd;
    CommandWrapper *m_reset_cmd;
    CommandWrapper *m_config_reset_cmd;
    bool m_is_mcu_bridge;
    bool m_is_timeout;
    std::map<std::string, std::string> m_get_status_info;
    std::vector<std::function<void(int)>> m_config_callbacks;

public:
    MCU(std::string section_name, ClockSync *clocksync);
    ~MCU();
    void handle_mcu_stats(ParseResult params);
    void handle_shutdown(ParseResult params);
    void handle_starting(ParseResult params);
    void check_restart(std::string reason);
    void connect_file(bool pace = false);
    void send_config();
    ParseResult send_get_config();
    std::string log_info();
    void connect();
    void mcu_identify();
    int create_oid();
    void register_config_callback(std::function<void(int)> callback);
    void add_config_cmd(std::string cmd, bool is_init = false, bool on_restart = false);
    uint64_t get_query_slot(int oid);
    void register_stepqueue(stepcompress *stepqueue);
    void request_move_queue_slot();
    uint64_t seconds_to_clock(double time);
    void get_max_stepper_error();
    void *setup_pin(std::string pin_type, pinParams *pin_params);
    std::string get_name();
    void register_response(std::function<void(ParseResult &)> cb, std::string msg, int oid = 0);
    command_queue * alloc_command_queue();
    CommandWrapper* lookup_command(std::string msgformat, command_queue *cq = nullptr);
    CommandQueryWrapper *lookup_query_command(std::string msgformat, std::string respformat, int oid, command_queue *cq = nullptr, bool is_async = false);
    CommandWrapper *try_lookup_command(std::string msgformat);
    void lookup_command_tag(std::string msgformat);
    void get_enumerations();
    void get_constants();
    float get_constant_float(std::string name);
    uint64_t print_time_to_clock(double print_time);
    double clock_to_print_time(uint64_t clock);
    double estimated_print_time(double eventtime);
    uint64_t clock32_to_clock64(uint32_t clock32);
    void disconnect();
    void break_connection();
    void shutdown(bool force = false);
    void restart_arduino();
    void restart_cheetah();
    void restart_via_command();
    void restart_rpi_usb();
    void firmware_restart(bool force = false);
    void firmware_restart_bridge();
    bool is_fileoutput();
    bool is_shutdown();
    uint64_t get_shutdown_clock();
    void flush_moves(double print_time);
    void check_active(double print_time, double eventtime);
    void get_status();
    void stats(double eventtime);
};

void add_printer_mcu();
MCU *get_printer_mcu(Printer *printer, std::string name);
#endif

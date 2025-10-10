#ifndef MOTION_REPORT_H
#define MOTION_REPORT_H

#include "stepper.h"
#include <vector>
#include <map>

const double API_UPDATE_INTERVAL = 0.5;
const double NEVER_TIME = 9999999999999999.;

typedef struct API_data_tag
{
    std::vector<int> data;
    std::vector<double> start_position;
    std::vector<double> start_mcu_position;
    double step_distance;
    uint64_t first_clock;
    double first_step_time;
    uint64_t last_clock;
    double last_step_time;

}API_data;
// Helper to periodically transmit data to a set of API clients
class APIDumpHelper
{
private:
    
public:
    APIDumpHelper(std::function<API_data(void)> data_cb, std::function<void()> startstop_cb = nullptr, double update_interval = API_UPDATE_INTERVAL);
    ~APIDumpHelper();

public:
    std::function<API_data(void)> m_data_cb;
};

// An "internal webhooks" wrapper for using APIDumpHelper internally
class InternalDumpClient
{
private:
    std::vector<std::vector<uint8_t>> m_msgs;
    bool m_is_done = false;
public:
    InternalDumpClient();
    ~InternalDumpClient();
    std::vector<std::vector<uint8_t>> get_message();
    void finalize();
    bool is_closed();
    void send(std::vector<uint8_t> msg);
};

// Extract stepper queue_step messages
class DumpStepper
{
private:
    
public:
    DumpStepper(MCU_stepper* mcu_stepper);
    ~DumpStepper();
    std::vector<struct pull_history_steps> get_step_queue(uint64_t start_clock, uint64_t end_clock);
    void log_steps(std::vector<struct pull_history_steps> data);
    API_data _api_update();

public:
    MCU_stepper* m_mcu_stepper;
    uint64_t m_last_api_clock;
    APIDumpHelper* m_api_dump;
};

// Extract trapezoidal motion queue (trapq)
class DumpTrapQ
{
private:
    
public:
    DumpTrapQ(std::string name, trapq* trapq);
    ~DumpTrapQ();
    void extract_trapq(double start_time, double end_time);
    // void log_trapq(data);
    void get_trapq_position(double print_time);
    API_data _api_update();
};

typedef struct motion_status_tag
{
    std::vector<double> live_position;
    double live_velocity;
    double live_extruder_velocity;
    std::map<std::string, DumpStepper*> steppers;
    std::map<std::string, DumpTrapQ*> trapq;
}motion_status;


class PrinterMotionReport
{
public:
    PrinterMotionReport();
    ~PrinterMotionReport();
    void register_stepper(MCU_stepper* mcu_stepper);
    void _connect();
    void _dump_shutdown();
    void _shutdown();
    void get_status(double eventtime);


public:
    std::map<std::string, DumpStepper*> m_steppers;
    std::map<std::string, DumpTrapQ*> m_trapqs;
    double m_next_status_time;
    motion_status m_last_status;
};




#endif
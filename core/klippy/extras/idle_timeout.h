#ifndef IDLE_TIMEOUT_H
#define IDLE_TIMEOUT_H
#include <string>
#include <vector>
#include <functional>
#include "gcode.h"
#include "reactor.h"

typedef struct idle_timeout_stats_tag{
    std::string state;
    double last_print_start_systime;
}idle_timeout_stats_t;
class IdleTimeout{
    private:

    public:
        IdleTimeout(std::string section_name);
        ~IdleTimeout();

        double m_idle_timeout;
        double m_disp_active_time;
        std::string m_cmd_SET_IDLE_TIMEOUT_help;
        std::string m_cmd_UPDATE_IDLE_TIMER_help;
        std::string m_state;
        double m_last_print_start_systime;
        ReactorTimerPtr m_timeout_timer;

        idle_timeout_stats_t get_status(double eventtime);
        void handle_ready();
        double transition_idle_state(double eventtime);
        double check_idle_timeout(double eventtime);
        double timeout_handler(double eventtime);
        void handle_sync_print_time(double curtime, double print_time, double est_print_time);
        void cmd_SET_IDLE_TIMEOUT(GCodeCommand& gcmd);
        void extruder_off_heater_handler(double eventtime);
        void cmd_UPDATE_IDLE_TIMER(GCodeCommand& gcmd);
};
#endif
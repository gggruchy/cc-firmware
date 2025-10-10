#ifndef FILAMENT_SWITCH_SENSOR_H
#define FILAMENT_SWITCH_SENSOR_H
#include "gcode.h"

typedef struct RunoutHelper_status_tag
{
    bool filament_detected;
    bool enabled;
}RunoutHelper_status_t;


class RunoutHelper
{
private:
    
public:
    std::string m_name;
    bool m_runout_pause;
    double pause_delay;
    double event_delay;
    double m_min_event_systime;
    bool m_filament_present;
    std::string m_runout_gcode;
    std::string m_insert_gcode;
    double m_pause_delay;
    double m_event_delay;
    bool m_sensor_enabled;

public:
    RunoutHelper(std::string section_name);
    ~RunoutHelper();
    void handle_ready();
    double runout_event_handler(double eventtime);
    double insert_event_handler(double eventtime);
    void exec_gcode(std::string prefix, std::string gcode);
    void note_filament_present(bool is_filament_present);
    RunoutHelper_status_t get_status();
    void cmd_QUERY_FILAMENT_SENSOR(GCodeCommand &gcmd);
    void cmd_SET_FILAMENT_SENSOR(GCodeCommand &gcmd);
};


class SwitchSensor
{
private:
    
public:
    RunoutHelper* m_runout_helper;
    std::function<RunoutHelper_status_t(void)> m_get_status;
public:
    SwitchSensor(std::string section_name);
    ~SwitchSensor();
    void button_handler(double eventtime, bool state);
};





#endif
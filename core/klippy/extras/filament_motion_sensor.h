#ifndef FILAMENT_MOTION_SENSOR_H
#define FILAMENT_MOTION_SENSOR_H
#include "gcode.h"
#include "filament_switch_sensor.h"
#include "extruder.h"

class EncoderSensor
{
private:
    
public:
    std::string m_extruder_name;
    double m_detection_length;
    RunoutHelper* m_runout_helper;
    std::function<RunoutHelper_status_t(void)> m_get_status;
    double m_filament_runout_pos;
    std::function<double(double)> m_estimated_print_time;
    ReactorTimerPtr m_extruder_pos_update_timer;
    PrinterExtruder* m_extruder;
    SelectReactor* m_reactor;
public:
    EncoderSensor(std::string section_name);
    ~EncoderSensor();
    void update_filament_runout_pos(double eventtime = 0);
    void handle_ready();
    void handle_printing(double print_time);
    void handle_not_printing(double print_time);
    double get_extruder_pos(double eventtime = 0);
    double extruder_pos_update_event(double eventtime);
    void encoder_event(double eventtime, bool state);
};



#endif
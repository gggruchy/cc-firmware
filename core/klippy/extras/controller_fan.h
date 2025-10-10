#ifndef CONTROLLER_FAN_H
#define CONTROLLER_FAN_H

#include "fan.h"
#include "stepper_enable.h"
#include "heaters.h"

class ControllerFan
{
private:
    std::vector<std::string> m_stepper_names;
    std::vector<Heater*> m_heaters;
    PrinterStepperEnable *m_stepper_enable;
#if !ENABLE_MANUTEST
    Fan* m_fan;
#endif
    double m_fan_speed;
    double m_idle_speed;
    int m_idle_timeout;
    std::vector<std::string> m_heaters_names;
    int m_last_on;
    double m_last_speed = 0.;
    
public:
#if ENABLE_MANUTEST
    Fan* m_fan;
    void set_speed(double speed);
    double get_speed();
#endif
    ControllerFan(std::string section_name);
    ~ControllerFan();
    void handle_connect();
    void handle_ready();
    struct fan_state get_status(double eventtime);
    double callback(double eventtime);
};



#endif

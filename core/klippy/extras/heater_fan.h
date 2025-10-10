#ifndef HEATER_FAN_H
#define HEATER_FAN_H

#include "fan.h"
#include "heaters.h"

#define PIN_MIN_TIME 0.100

class PrinterHeaterFan
{
private:
    std::string m_heater_name;
    double m_heater_temp;
    Fan* m_fan;
    double m_fan_speed;
    double m_last_speed = 0.;
    std::vector<Heater*> m_heaters;
public:
    PrinterHeaterFan(std::string section_name);
    ~PrinterHeaterFan();
    void handle_ready();
    struct fan_state get_status(double eventtime);
    double callback(double eventtime);
#if ENABLE_MANUTEST
    double get_speed();
    void set_speed(double speed);
#endif
};




#endif

#ifndef FAN_GENERIC_H
#define FAN_GENERIC_H
#include "pulse_counter.h"
#include "fan.h"

class PrinterFanGeneric
{
private:
    
public:
    PrinterFanGeneric(std::string section_name);
    ~PrinterFanGeneric();

    Fan *m_fan;
    std::string m_fan_name;
    struct fan_state get_status(double eventtime);
    void cmd_SET_FAN_SPEED(GCodeCommand &gcmd);
    double genericfan_check_event(double eventtime);

};

#endif



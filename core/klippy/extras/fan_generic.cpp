#include "fan_generic.h"
#include "klippy.h"
#include "my_string.h"

PrinterFanGeneric::PrinterFanGeneric(std::string section_name)
{
    std::string cmd_SET_FAN_SPEED_help = "Sets the speed of a fan";
    m_fan = new Fan(section_name);
    m_fan_name = split(section_name, " ")[1];
    Printer::GetInstance()->m_gcode->register_mux_command(m_fan_name + "SET_FAN_SPEED", "FAN", m_fan_name, std::bind(&PrinterFanGeneric::cmd_SET_FAN_SPEED, this, std::placeholders::_1), cmd_SET_FAN_SPEED_help);
    Printer::GetInstance()->m_reactor->register_timer("genericfan_check_timer", std::bind(&PrinterFanGeneric::genericfan_check_event, this, std::placeholders::_1), get_monotonic() + PIN_MIN_TIME);
}

PrinterFanGeneric::~PrinterFanGeneric()
{
}

struct fan_state PrinterFanGeneric::get_status(double eventtime)
{
    return m_fan->get_status(eventtime);
}

double PrinterFanGeneric::genericfan_check_event(double eventtime)
{
    if(Printer::GetInstance()->m_stepper_enable->get_stepper_state("stepper_x") || 
        Printer::GetInstance()->m_stepper_enable->get_stepper_state("stepper_y") || 
        Printer::GetInstance()->m_stepper_enable->get_stepper_state("stepper_z") || 
        Printer::GetInstance()->m_stepper_enable->get_stepper_state("extruder"))
    {
        m_fan->set_speed_from_command(1);
    }
    else 
    {
        m_fan->set_speed_from_command(0);
    }
    return eventtime + 1;
}

void PrinterFanGeneric::cmd_SET_FAN_SPEED(GCodeCommand &gcmd)
{
    double speed = gcmd.get_double("SPEED", 0.) / 255;
    m_fan->set_speed_from_command(speed);
}

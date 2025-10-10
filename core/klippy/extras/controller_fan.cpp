#include "controller_fan.h"
#include "klippy.h"
#include "my_string.h"

#define PIN_MIN_TIME 0.100

//
ControllerFan::ControllerFan(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:ready:ControllerFan" + section_name, std::bind(&ControllerFan::handle_ready, this));
    Printer::GetInstance()->register_event_handler("klippy:connect:ControllerFan" + section_name, std::bind(&ControllerFan::handle_connect, this));
    std::string stepper_str = Printer::GetInstance()->m_pconfig->GetString(section_name, "stepper", "");
    if (stepper_str != "")
        m_stepper_names = split(stepper_str, ",");
    m_stepper_enable = (PrinterStepperEnable *)Printer::GetInstance()->load_object("stepper_enable");
    
    m_fan = new Fan(section_name);
    m_fan_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "fan_speed", 1., 0., 1.);
    m_idle_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "idle_speed", m_fan_speed, 0., 1.);
    m_idle_timeout = Printer::GetInstance()->m_pconfig->GetInt(section_name, "idle_timeout", 30, 0);
    m_heaters_names = split(Printer::GetInstance()->m_pconfig->GetString(section_name, "heater", "extruder,heater_bed"), ",");
    m_last_on = m_idle_timeout;
    m_last_speed = 0;
}

ControllerFan::~ControllerFan()
{
}

void ControllerFan::handle_connect()
{
    // Heater lookup
    for (auto name : m_heaters_names)
    {
        m_heaters.push_back(Printer::GetInstance()->m_pheaters->lookup_heater(name));
    }
    // MCU_stepper lookup
    std::vector<string> all_steppers = m_stepper_enable->get_steppers();
    if (m_stepper_names.size() == 0)
    {
        m_stepper_names = all_steppers;
        return;
    }
    for (auto x : m_stepper_names)
    {
        bool exist = false;
        for (auto y : all_steppers)
        {
            if (x == y)
                exist = true;
        }
        if (exist == false)
        {
            // std::cout << "One or more of these steppers are unknown" << std::endl;
            return;
        }
    }
}

void ControllerFan::handle_ready()
{
#if !ENABLE_MANUTEST
    Printer::GetInstance()->m_reactor->register_timer("controller_fan_timer", std::bind(&ControllerFan::callback, this, std::placeholders::_1), get_monotonic() + PIN_MIN_TIME);
#endif
}

struct fan_state ControllerFan::get_status(double eventtime)
{
    return m_fan->get_status(eventtime);
}


#if ENABLE_MANUTEST
void ControllerFan::set_speed(double speed)
{
    double curtime = get_monotonic();
    double print_time = m_fan->get_mcu()->estimated_print_time(curtime);
    m_fan->m_current_speed = speed;
    m_fan->set_speed(print_time + PIN_MIN_TIME, speed);
    printf("ControllerFan set speed: %f\n", speed);
}
#endif

double ControllerFan::callback(double eventtime)
{
#if !ENABLE_MANUTEST
    double speed = 0;
    bool active = false;
    for (auto name : m_stepper_names)
    {
        active |= m_stepper_enable->lookup_enable(name)->is_motor_enabled();
    }
    for (auto heater : m_heaters)
    {
        if (heater->get_temp(eventtime)[1])
        {
            active = true;
        }
    }
    if (active)
    {
        m_last_on = 0;
        speed = m_fan_speed;
    }
    else if (m_last_on < m_idle_timeout)
    {
        speed = m_idle_speed;
        m_last_on += 1;
    }
    if (speed != m_last_speed)
    {
        m_last_speed = speed;
        double curtime = get_monotonic(); //---??---
        double print_time = m_fan->get_mcu()->estimated_print_time(curtime);
        m_fan->m_current_speed = speed;
        m_fan->set_speed(print_time + PIN_MIN_TIME, speed);
    }
    return eventtime + 1.;
#endif
}

#if ENABLE_MANUTEST
double ControllerFan::get_speed()
{
    struct fan_state fan_s;
    double curtime = get_monotonic();
    fan_s =  m_fan->get_status(curtime);
    printf("controllerfan get speed: %f\n", fan_s.rpm);
    return fan_s.rpm;
}
#endif

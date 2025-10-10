#include "heater_fan.h"
#include "klippy.h"
#include "heaters.h"
#include "my_string.h"

PrinterHeaterFan::PrinterHeaterFan(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:ready:PrinterHeaterFan"+section_name, std::bind(&PrinterHeaterFan::handle_ready, this));
    m_heater_name = "extruder";
    m_heater_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "heater_temp", 50.);
    m_fan = new Fan(section_name, 1.);
    m_fan_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "fan_speed", 1., 0., 1.);
    m_last_speed = 0.;
}

PrinterHeaterFan::~PrinterHeaterFan()
{
        if (m_fan != nullptr) 
        {
            delete m_fan;
            m_fan = nullptr;
        }
}

void PrinterHeaterFan::handle_ready()
{
    if(Printer::GetInstance()->m_pheaters->lookup_heater(m_heater_name) != nullptr)
        m_heaters.push_back(Printer::GetInstance()->m_pheaters->lookup_heater(m_heater_name));
    else
        std::cout << "can not find heater" << std::endl;
#if !ENABLE_MANUTEST
    Printer::GetInstance()->m_reactor->register_timer("heater_fan_timer", std::bind(&PrinterHeaterFan::callback, this, std::placeholders::_1), get_monotonic()+PIN_MIN_TIME);
#endif
}

struct fan_state PrinterHeaterFan::get_status(double eventtime)
{
    return m_fan->get_status(eventtime);
}

double PrinterHeaterFan::callback(double eventtime)
{
    double speed = 0;
    for(auto heater : m_heaters)
    {
        std::vector<double> temp = heater->get_temp(eventtime);
        if(temp[1] || temp[0] > m_heater_temp)
            speed = m_fan_speed;
    }
    if(speed != m_last_speed)
    {
        m_last_speed = speed;
        double curtime = get_monotonic();
        double print_time = m_fan->get_mcu()->estimated_print_time(curtime);
        m_fan->m_current_speed = speed;
        m_fan->set_speed(print_time + PIN_MIN_TIME, speed);
    }
    return eventtime + 1.;
}

#if ENABLE_MANUTEST
void PrinterHeaterFan::set_speed(double speed)
{
    m_last_speed = speed;
    double curtime = get_monotonic();
    double print_time = m_fan->get_mcu()->estimated_print_time(curtime);
    m_fan->m_current_speed = speed;
    m_fan->set_speed(print_time + PIN_MIN_TIME, speed);
    printf("[%s] set speed: %f\n", __FUNCTION__, speed);
}

double PrinterHeaterFan::get_speed()
{
    struct fan_state fan_s;
    double curtime = get_monotonic();
    fan_s =  m_fan->get_status(curtime);
    printf("[%s] get speed: %f\n", __FUNCTION__, fan_s.rpm);
    return fan_s.rpm;
}
#endif

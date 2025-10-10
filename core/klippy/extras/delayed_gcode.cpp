#include "delayed_gcode.h"
#include "klippy.h"

DelayedGcode::DelayedGcode(std::string section_name)
{
    m_name = section_name;
    // m_timer_gcode = gcode_macro.load_template(config, "gcode") //---??---
    m_duration = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "initial_duration", 0., 0.);
    m_timer_handler = nullptr;
    m_inside_timer = false;
    m_repeat = false;
    Printer::GetInstance()->register_event_handler("klippy:ready", std::bind(&DelayedGcode::_handle_ready, this));
    m_cmd_UPDATE_DELAYED_GCODE_help = "Update the duration of a delayed_gcode";
    Printer::GetInstance()->m_gcode->register_mux_command("UPDATE_DELAYED_GCODE", "ID", m_name, std::bind(&DelayedGcode::cmd_UPDATE_DELAYED_GCODE, this, std::placeholders::_1), m_cmd_UPDATE_DELAYED_GCODE_help);
}

DelayedGcode::~DelayedGcode()
{

}
        
void DelayedGcode::_handle_ready()
{
    double waketime = Printer::GetInstance()->m_reactor->m_NEVER;
    if (m_duration)
    {
        waketime = get_monotonic() + m_duration;
    }
    m_timer_handler = Printer::GetInstance()->m_reactor->register_timer("m_timer_handler", std::bind(&DelayedGcode::_gcode_timer_event, this, std::placeholders::_1), waketime);
}
        
double DelayedGcode::_gcode_timer_event(double eventtime)
{
    m_inside_timer = true;
    // Printer::GetInstance()->m_gcode->run_script(m_timer_gcode.render())  //---??---
    double nextwake = Printer::GetInstance()->m_reactor->m_NEVER;
    if (m_repeat)
        nextwake = eventtime + m_duration;
    m_inside_timer = m_repeat = false;
    return nextwake;
}

void DelayedGcode::cmd_UPDATE_DELAYED_GCODE(GCodeCommand& gcmd)
{
    m_duration = gcmd.get_double("DURATION", INT32_MIN, 0.);
    if (m_inside_timer)
        m_repeat = (m_duration != 0.);
    else
    {
        double waketime = Printer::GetInstance()->m_reactor->m_NEVER;
        if (m_duration)
            waketime = get_monotonic() + m_duration;
        Printer::GetInstance()->m_reactor->update_timer(m_timer_handler, waketime);
    }
        
}
    
#include "idle_timeout.h"
#include "Define.h"
#include "klippy.h"

#define LOG_TAG "idle_timeout"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

IdleTimeout::IdleTimeout(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:ready:IdleTimeout", std::bind(&IdleTimeout::handle_ready, this));
    m_idle_timeout = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "timeout", 1800., DBL_MIN, DBL_MAX, 0.);
    // gcode_macro = m_printer.load_object(config, "gcode_macro") //---??---IdleTimeout
    // m_idle_gcode = gcode_macro.load_template(config, "gcode", DEFAULT_IDLE_GCODE)
    m_cmd_SET_IDLE_TIMEOUT_help = "Set the idle timeout in seconds";
    m_cmd_UPDATE_IDLE_TIMER_help = "update idle timer help";
    Printer::GetInstance()->m_gcode->register_command("SET_IDLE_TIMEOUT", std::bind(&IdleTimeout::cmd_SET_IDLE_TIMEOUT, this, std::placeholders::_1), false, m_cmd_SET_IDLE_TIMEOUT_help);
    Printer::GetInstance()->m_gcode->register_command("UPDATE_IDLE_TIMER", std::bind(&IdleTimeout::cmd_UPDATE_IDLE_TIMER, this, std::placeholders::_1), false, m_cmd_UPDATE_IDLE_TIMER_help);
    m_state = "Idle";
    m_last_print_start_systime = 0.;
}

IdleTimeout::~IdleTimeout()
{

}

        
idle_timeout_stats_t IdleTimeout::get_status(double eventtime)
{
    double printing_time = 0.;
    if (m_state == "Printing")
        printing_time = eventtime - m_last_print_start_systime;
    idle_timeout_stats_t ret = {
        .state = m_state,
        .last_print_start_systime = m_last_print_start_systime
    };
    return ret;
}
        
void IdleTimeout::handle_ready()
{
    m_disp_active_time = get_monotonic();
    m_timeout_timer = Printer::GetInstance()->m_reactor->register_timer("timeout_timer", std::bind(&IdleTimeout::timeout_handler, this, std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NOW);
    Printer::GetInstance()->register_event_double3_handler("toolhead:sync_print_time:IdleTimeout", std::bind(&IdleTimeout::handle_sync_print_time, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void IdleTimeout::extruder_off_heater_handler(double eventtime)
{
    if (fabs(Printer::GetInstance()->m_printer_extruder->m_heater->m_target_temp) > 1e-10 ||
        fabs(Printer::GetInstance()->m_bed_heater->m_heater->m_target_temp) > 1e-10)
    {
        if (Printer::GetInstance()->m_print_stats->m_print_stats.state == PRINT_STATS_STATE_PAUSEING || Printer::GetInstance()->m_print_stats->m_print_stats.state == PRINT_STATS_STATE_PAUSED)
        {
        }
        else if (Printer::GetInstance()->m_virtual_sdcard->is_printing())
        {
        }
        else
        {
            LOG_I("IdleTimeout. Turn off heaters.\n");
            Printer::GetInstance()->m_gcode_io->single_command("M104 S0");
            Printer::GetInstance()->m_gcode_io->single_command("M140 S0");
        }
    }
}

double IdleTimeout::transition_idle_state(double eventtime)
{
    m_state = "Printing";
    try
    {
        // script = m_idle_gcode.render();
        // res = m_gcode.run_script(script);
        extruder_off_heater_handler(eventtime);
    } 
    catch(...)
    {
        std::cout << "idle timeout gcode execution" << std::endl;
        m_state = "Ready";
        return eventtime + 1.;
    }
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    m_state = "Idle";
    Printer::GetInstance()->send_event("idle_timeout:idle", print_time);
    // return Printer::GetInstance()->m_reactor->m_NEVER;
    return eventtime + m_idle_timeout;
}
        
double IdleTimeout::check_idle_timeout(double eventtime)
{
    // Make sure toolhead class isn"t busy
    std::vector<double> ret = Printer::GetInstance()->m_tool_head->check_busy(eventtime);
    double print_time = ret[0];
    double est_print_time = ret[1];
    bool lookahead_empty = (bool)ret[2];
    double idle_time = est_print_time - print_time;
    LOG_D("print_time: %f, est_print_time: %f, lookahead_empty: %d, idle_time: %f\n", print_time, est_print_time, lookahead_empty, idle_time);
    if (!lookahead_empty || idle_time < 1.)
    {
        // Toolhead is busy
        return eventtime + m_idle_timeout;
    }
    if (idle_time < m_idle_timeout)
    {
        // Wait for idle timeout
        return eventtime + m_idle_timeout - idle_time;
    }
    if (eventtime - m_disp_active_time < m_idle_timeout)
    {
        // Display is active
        return m_idle_timeout + m_disp_active_time;
    }
    // if (m_gcode.get_mutex().test())  //---??---
    // {
    //     // Gcode class busy
    //     return eventtime + 1.;
    // }
    // Idle timeout has elapsed
    return transition_idle_state(eventtime);
}
        
double IdleTimeout::timeout_handler(double eventtime)
{
    LOG_D("IdleTimeout m_state: %s\n", m_state.c_str());
    if (m_state == "Ready")
        return check_idle_timeout(eventtime);
    // Check if need to transition to "ready" state
    std::vector<double> ret = Printer::GetInstance()->m_tool_head->check_busy(eventtime);
    double print_time = ret[0];
    double est_print_time = ret[1];
    bool lookahead_empty = (bool)ret[2];
    double buffer_time = std::min(2., print_time - est_print_time);
    if (!lookahead_empty)
    {
        // Toolhead is busy
        return eventtime + READY_TIMEOUT + std::max(0., buffer_time);
    }
    if (buffer_time > -READY_TIMEOUT)
    {
        // Wait for ready timeout
        return eventtime + READY_TIMEOUT + buffer_time;
    } 
    // if (m_gcode.get_mutex().test()) //---??---
    // {
    //     // Gcode class busy
    //     return eventtime + READY_TIMEOUT;
    // }
    // Transition to "ready" state
    m_state = "Ready";
    Printer::GetInstance()->send_event("idle_timeout:ready", est_print_time + PIN_MIN_TIME);
    return eventtime + m_idle_timeout;
}
        
void IdleTimeout::handle_sync_print_time(double curtime, double print_time, double est_print_time)
{
    if (m_state == "Printing")
        return;
    // Transition to "printing" state
    m_state = "Printing";
    m_last_print_start_systime = curtime;
    double check_time = READY_TIMEOUT + print_time - est_print_time;
    Printer::GetInstance()->m_reactor->update_timer(m_timeout_timer, curtime + check_time);
    Printer::GetInstance()->send_event("idle_timeout:printing", est_print_time + PIN_MIN_TIME);
}
    
void IdleTimeout::cmd_SET_IDLE_TIMEOUT(GCodeCommand& gcmd)
{
    double timeout = gcmd.get_double("TIMEOUT", m_idle_timeout, DBL_MIN, DBL_MAX, 0.);
    m_idle_timeout = timeout;
    // gcmd.respond_info("idle_timeout: Timeout set to %.2f s" % (timeout,))  //---??---
    if (m_state == "Ready")
    {
        double checktime = get_monotonic() + timeout;
        Printer::GetInstance()->m_reactor->update_timer(m_timeout_timer, checktime);
    }
        
}

void IdleTimeout::cmd_UPDATE_IDLE_TIMER(GCodeCommand &gcmd)
{
    LOG_D("cmd_UPDATE_IDLE_TIMER!\n");
    m_disp_active_time = gcmd.get_double("ACTIVE_TIME", get_monotonic(), DBL_MIN, DBL_MAX, 0.);
    Printer::GetInstance()->m_reactor->update_timer(m_timeout_timer, Printer::GetInstance()->m_reactor->m_NOW);
}

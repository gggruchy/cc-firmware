#include "display_status.h"
#include "klippy.h"
#include "my_string.h"

#define M73_TIMEOUT 5.

DisplayStatus::DisplayStatus(std::string section_name)
{
    double m_expire_progress = 0.;
    double m_progress = 0.;
    std::string m_message = "";
    // Register commands
    Printer::GetInstance()->m_gcode->register_command("M73",  std::bind(&DisplayStatus::cmd_M73, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M117", std::bind(&DisplayStatus::cmd_M117, this, std::placeholders::_1));
}

DisplayStatus::~DisplayStatus()
{

}
        
display_status_t DisplayStatus::get_status(double eventtime)
{
    double progress = m_progress;
    if (progress != 0 && eventtime > m_expire_progress)
    {
        idle_timeout_stats_t idle_timeout_info = Printer::GetInstance()->m_idle_timeout->get_status(eventtime);
        if (idle_timeout_info.state != "Printing")
            m_progress = progress = 0.;
    }
    if (progress == 0)
    {
        progress = 0.;
        if (Printer::GetInstance()->m_virtual_sdcard != nullptr)
            progress = Printer::GetInstance()->m_virtual_sdcard->get_status(eventtime).progress;
    }
    display_status_t ret = {
        .progress = progress,
        .message = m_message,
    };
    return ret;
}
    
void DisplayStatus::cmd_M73(GCodeCommand& gcmd)
{
    double progress = gcmd.get_double("P", 0.) / 100.;
    m_progress = std::min(1., std::max(0., progress));
    double curtime = get_monotonic();
    m_expire_progress = curtime + M73_TIMEOUT;
}
    
void DisplayStatus::cmd_M117(GCodeCommand& gcmd)
{
    std::string msg = gcmd.get_commandline();
    std::string umsg;
    transform(msg.begin(),msg.end(),umsg.begin(),::toupper);
    if (!startswith(umsg, "M117"))
    {
        // Parse out additional info if M117 recd during a print
        int start = umsg.find("M117");
        int end = msg.rfind("*");
        if (end >= 0)
            msg = msg.substr(0, end);
        msg = msg.substr(start);
    }
    if (msg.length() > 5)
        m_message = msg.substr(5);
    else
        m_message = "";
}
        
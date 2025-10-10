#include "firmware_retraction.h"
#include "klippy.h"

FirmwareRetraction::FirmwareRetraction(std::string section_name)
{
    m_retract_length = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "retract_length", 0, 0);
    m_retract_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "retract_speed", 20, 1);
    m_unretract_extra_length = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "unretract_extra_length", 0, 0);
    m_unretract_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "unretract_speed", 10, 1);
    m_unretract_length = m_retract_length + m_unretract_extra_length;
    m_is_retracted = false;
    std::string cmd_SET_RETRACTION_help = "Set firmware retraction parameters";
    std::string cmd_GET_RETRACTION_help = "Report firmware retraction paramters";
    Printer::GetInstance()->m_gcode->register_command("SET_RETRACTION", std::bind(&FirmwareRetraction::cmd_SET_RETRACTION, this, std::placeholders::_1), false, cmd_SET_RETRACTION_help);
    Printer::GetInstance()->m_gcode->register_command("GET_RETRACTION", std::bind(&FirmwareRetraction::cmd_GET_RETRACTION, this, std::placeholders::_1), false, cmd_GET_RETRACTION_help);
    Printer::GetInstance()->m_gcode->register_command("G10", std::bind(&FirmwareRetraction::cmd_G10, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("G22", std::bind(&FirmwareRetraction::cmd_G10, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("G11", std::bind(&FirmwareRetraction::cmd_G11, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("G23", std::bind(&FirmwareRetraction::cmd_G11, this, std::placeholders::_1));
}

FirmwareRetraction::~FirmwareRetraction()
{
}

void FirmwareRetraction::cmd_SET_RETRACTION(GCodeCommand &gcmd)
{
    m_retract_length = gcmd.get_double("RETRACT_LENGTH", m_retract_length, 0);
    m_retract_speed = gcmd.get_double("RETRACT_SPEED", m_retract_speed, 1);
    m_unretract_extra_length = gcmd.get_double("UNRETRACT_EXTRA_LENGTH", m_unretract_extra_length, 0);
    m_unretract_speed = gcmd.get_double("UNRETRACT_SPEED", m_unretract_speed, 1);
    m_unretract_length = m_retract_length + m_unretract_extra_length;
    m_is_retracted = false;
}

void FirmwareRetraction::cmd_GET_RETRACTION(GCodeCommand &gcmd)
{
    std::stringstream ss;
    ss << "RETRACT_LENGTH=" << m_retract_length << " RETRACT_SPEED=" << m_retract_speed
        << "\n UNRETRACT_EXTRA_LENGTH=" << m_unretract_extra_length << " UNRETRACT_SPEED=" << m_unretract_speed;
    gcmd.m_respond_info(ss.str(), true);
}

void FirmwareRetraction::cmd_G10(GCodeCommand &gcmd)
{
    if(!m_is_retracted)
    {
        std::vector<std::string> commands;
        commands.push_back("SAVE_GCODE_STATE NAME=_retract_state");
        commands.push_back("G91");
        std::stringstream ss;
        ss << "G1 E-" << m_retract_length << " F" << m_retract_speed * 60;
        commands.push_back(ss.str());
        commands.push_back("RESTORE_GCODE_STATE NAME=_retract_state");
        Printer::GetInstance()->m_gcode->run_script_from_command(commands);
        m_is_retracted = true;
    }
}

void FirmwareRetraction::cmd_G11(GCodeCommand &gcmd)
{
    if(m_is_retracted)
    {
        std::vector<std::string> commands;
        commands.push_back("SAVE_GCODE_STATE NAME=_retract_state");
        commands.push_back("G91");
        std::stringstream ss;
        ss << "G1 E" << m_retract_length << " F" << m_retract_speed * 60;
        commands.push_back(ss.str());
        commands.push_back("RESTORE_GCODE_STATE NAME=_retract_state");
        Printer::GetInstance()->m_gcode->run_script_from_command(commands);
        m_is_retracted = false;
    }
}
    
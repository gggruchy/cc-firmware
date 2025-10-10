#include "manual_probe.h"
#include "Define.h"
#include "klippy.h"

ManualProbe::ManualProbe(std::string section_name)
{
    // Register commands
    m_cmd_MANUAL_PROBE_help = "Start manual probe helper script";
    Printer::GetInstance()->m_gcode->register_command("MANUAL_PROBE", std::bind(&ManualProbe::cmd_MANUAL_PROBE, this, std::placeholders::_1), false, m_cmd_MANUAL_PROBE_help);
    m_z_position_endstop = Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "position_endstop", DBL_MIN);
    if (m_z_position_endstop != DBL_MIN)
    {
        m_cmd_Z_ENDSTOP_CALIBRATE_help = "Calibrate a Z endstop";
        Printer::GetInstance()->m_gcode->register_command("Z_ENDSTOP_CALIBRATE", std::bind(&ManualProbe::cmd_Z_ENDSTOP_CALIBRATE, this, std::placeholders::_1), false, m_cmd_Z_ENDSTOP_CALIBRATE_help);
    }
}

ManualProbe::~ManualProbe()
{

}
        
void ManualProbe::manual_probe_finalize(std::vector<double> kin_pos)
{
    if (kin_pos.size() != 0)
    {
        // m_gcode.respond_info("Z position is %.3f" % (kin_pos[2],))  //---??---
    }
}
        

void ManualProbe::cmd_MANUAL_PROBE(GCodeCommand& gcmd)
{
    ManualProbeHelper(gcmd, std::bind(&ManualProbe::manual_probe_finalize, this, std::placeholders::_1));
}
        
void ManualProbe::z_endstop_finalize(std::vector<double> kin_pos)
{
    if (kin_pos.size() == 0)
        return;
    double z_pos = m_z_position_endstop - kin_pos[2];
    // m_gcode.respond_info(
    //     "stepper_z: position_endstop: %.3f\n"
    //     "The SAVE_CONFIG command will update the printer config file\n"
    //     "with the above and restart the printer." % (z_pos,))  //---??---
    Printer::GetInstance()->m_pconfig->SetDouble("stepper_z", "position_endstop", z_pos);
}
    
void ManualProbe::cmd_Z_ENDSTOP_CALIBRATE(GCodeCommand& gcmd)
{
    ManualProbeHelper(gcmd, std::bind(&ManualProbe::z_endstop_finalize, this, std::placeholders::_1));
}
        

//Verify that a manual probe isn"t already in progress
void verify_no_manual_probe()
{
    // gcode = printer.lookup_object("gcode")
    // try:
    //     gcode.register_command("ACCEPT", "dummy")
    // except printer.config_error as e:
    //     raise gcode.error(
    //         "Already in a manual Z probe. Use ABORT to abort it.")
    // gcode.register_command("ACCEPT", None)  //---??---
}

// Helper script to determine a Z height
ManualProbeHelper::ManualProbeHelper(GCodeCommand& gcmd, std::function<void(std::vector<double>)> finalize_callback)
{
    std::function<void(std::vector<double>)> m_finalize_callback = finalize_callback;
    double m_speed = gcmd.get_float("SPEED", 5.);
    std::vector<double> m_past_positions;
    std::vector<double> m_last_toolhead_pos;
    std::vector<double> m_last_kinematics_pos;
    // Register commands
    verify_no_manual_probe();
    std::string m_cmd_ACCEPT_help = "Accept the current Z position";
    std::string m_cmd_ABORT_help = "Abort manual Z probing tool";
    std::string m_cmd_TESTZ_help = "Move to new Z height";
    Printer::GetInstance()->m_gcode->register_command("ACCEPT", std::bind(&ManualProbeHelper::cmd_ACCEPT, this, std::placeholders::_1), false, m_cmd_ACCEPT_help);
    Printer::GetInstance()->m_gcode->register_command("NEXT", std::bind(&ManualProbeHelper::cmd_ACCEPT, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("ABORT", std::bind(&ManualProbeHelper::cmd_ABORT, this, std::placeholders::_1), false, m_cmd_ABORT_help);
    Printer::GetInstance()->m_gcode->register_command("TESTZ", std::bind(&ManualProbeHelper::cmd_TESTZ, this, std::placeholders::_1), false, m_cmd_TESTZ_help);
    // m_gcode.respond_info(
    //     "Starting manual Z probe. Use TESTZ to adjust position.\n"
    //     "Finish with ACCEPT or ABORT command.")  //---??---
    std::vector<double> m_start_position = Printer::GetInstance()->m_tool_head->get_position();
    report_z_status();
}

ManualProbeHelper::~ManualProbeHelper()
{

}

std::vector<double> ManualProbeHelper::get_kinematics_pos()
{
    std::vector<double> toolhead_pos = Printer::GetInstance()->m_tool_head->get_position();
    if (toolhead_pos == m_last_toolhead_pos)
        return m_last_kinematics_pos;
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::vector<std::vector<MCU_stepper*>> steppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
    std::map<std::string, double> kin_spos;
    for(int i = 0; i < steppers.size(); i++)
    {
        for (int j = 0; j < steppers[i].size(); j++)
        {
            kin_spos[steppers[i][j]->get_name()] = steppers[i][j]->get_commanded_position();
        }
    }
    std::vector<double> kin_pos = Printer::GetInstance()->m_tool_head->m_kin->calc_position(kin_spos);
    m_last_toolhead_pos = toolhead_pos;
    m_last_kinematics_pos = kin_pos;
    return kin_pos;
}
        
void ManualProbeHelper::move_z(double z_pos)
{
    std::vector<double> curpos = Printer::GetInstance()->m_tool_head->get_position();
    double z_bob_pos = z_pos + Z_BOB_MINIMUM;
    if (curpos[2] < z_bob_pos)
    {
        std::vector<double> pos = {DBL_MIN, DBL_MIN, z_bob_pos};
        Printer::GetInstance()->m_tool_head->manual_move(pos, m_speed);
    }
    std::vector<double> pos = {DBL_MIN, DBL_MIN, z_bob_pos};
    Printer::GetInstance()->m_tool_head->manual_move(pos, m_speed);
}
        
void ManualProbeHelper::report_z_status(bool warn_no_change, double prev_pos)
{
    // Get position
    std::vector<double> kin_pos = get_kinematics_pos();
    double z_pos = kin_pos[2];
    if (warn_no_change && z_pos == prev_pos)
    {
        // m_gcode.respond_info("WARNING: No change in position (reached stepper resolution)");//---??---
    }
    // Find recent positions that were tested
    std::vector<double> pp = m_past_positions;
    std::vector<double>::iterator it = std::find(pp.begin(), pp.end(), z_pos);
    int next_pos = it - pp.begin();
    prev_pos = next_pos - 1; 
    if (next_pos < pp.size() && pp[next_pos] == z_pos)
        next_pos += 1;
    std::string prev_str = "??????";
    std::string next_str = "??????";
    if (prev_pos >= 0)
        prev_str = std::to_string(prev_pos);
    if (next_pos < pp.size())
        next_str = std::to_string(next_pos);
    // Find recent positions
    // m_gcode.respond_info("Z position: %s --> %.3f <-- %s" % (prev_str, z_pos, next_str)) //---??---
}

void ManualProbeHelper::cmd_ACCEPT(GCodeCommand& gcmd)
{
    std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
    std::vector<double> start_pos = m_start_position;
    if ((pos[0] != start_pos[0] && pos[1] != start_pos[1]) || pos[2] >= start_pos[2])
    {
        // gcmd.respond_info(
        //     "Manual probe failed! Use TESTZ commands to position the\n"
        //     "nozzle prior to running ACCEPT.")  //---??---
        finalize(false);
        return;
    }
    finalize(true);
}
        
    
void ManualProbeHelper::cmd_ABORT(GCodeCommand& gcmd)
{
    finalize(false);
}
        
    
void ManualProbeHelper::cmd_TESTZ(GCodeCommand& gcmd)
{
    // Store current position for later reference
    std::vector<double> kin_pos = get_kinematics_pos();
    double z_pos = kin_pos[2];
    std::vector<double> pp = m_past_positions;
    std::vector<double>::iterator it = std::find(pp.begin(), pp.end(), z_pos);
    int insert_pos = it - pp.begin();
    if (insert_pos >= pp.size() || pp[insert_pos] != z_pos)
    {
        pp.insert(pp.begin() + insert_pos, z_pos);
    } 
    // Determine next position to move to
    std::string req = gcmd.get_string("Z", "");
    double check_z = 0.;
    double next_z_pos = 0.;
    if (req == "+" || req == "++")
    {
        check_z = 9999999999999.9;
        if (insert_pos < m_past_positions.size() - 1)
            check_z = m_past_positions[insert_pos + 1];
        if (req == "+")
            check_z = (check_z + z_pos) / 2.;
        next_z_pos = std::min(check_z, z_pos + BISECT_MAX);
    }
    else if (req == "-" || req == "--")
    {
        check_z = -9999999999999.9;
        if (insert_pos > 0)
            check_z = m_past_positions[insert_pos - 1];
        if (req == "-")
            check_z = (check_z + z_pos) / 2.;
        next_z_pos = std::max(check_z, z_pos - BISECT_MAX);
    }
    else
        next_z_pos = z_pos + gcmd.get_double("Z", DBL_MIN);
    // Move to given position and report it
    move_z(next_z_pos);
    report_z_status(next_z_pos != z_pos, z_pos);
}
        
void ManualProbeHelper::finalize(bool success)
{
    Printer::GetInstance()->m_gcode->register_command("ACCEPT", nullptr);
    Printer::GetInstance()->m_gcode->register_command("NEXT", nullptr);
    Printer::GetInstance()->m_gcode->register_command("ABORT", nullptr);
    Printer::GetInstance()->m_gcode->register_command("TESTZ", nullptr);
    std::vector<double> kin_pos;
    if (success)
        kin_pos = get_kinematics_pos();
    m_finalize_callback(kin_pos);
}
        
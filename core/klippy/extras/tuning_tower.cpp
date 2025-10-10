#include "tuning_tower.h"
#include "klippy.h"
#include "Define.h"

TuningTower::TuningTower(std::string section_name)
{
    m_last_position = {0., 0., 0., 0.};
    m_last_z = 0.;
    m_start = 0.;
    m_factor = 0.;
    m_band = 0.;
    m_last_command_value = 0.;
    m_command_fmt = "";
    // Register command
    m_cmd_TUNING_TOWER_help = "Tool to adjust a parameter at each Z height";
    // Printer::GetInstance()->m_gcode->register_command("TUNING_TOWER", std::bind(&TuningTower::cmd_TUNING_TOWER, this, std::placeholders::_1), false, m_cmd_TUNING_TOWER_help);

}

TuningTower::~TuningTower()
{

}  
    
void TuningTower::cmd_TUNING_TOWER(GCodeCommand& gcmd)
{
    if (m_normal_move_transform != nullptr)
    {
        end_test();
    }
    // Get parameters
    std::string command = gcmd.get_string("COMMAND", "");
    std::string parameter = gcmd.get_string("PARAMETER", "");
    m_start = gcmd.get_double("START", 0.);
    m_factor = gcmd.get_double("FACTOR", DBL_MIN);
    m_band = gcmd.get_double("BAND", 0., 0.);
    
    // Enable test mode
    if (Printer::GetInstance()->m_gcode->is_traditional_gcode(command))
        m_command_fmt = command + " " + parameter;
    else
        m_command_fmt = command + " " + parameter + "=";
    m_normal_move_transform = Printer::GetInstance()->m_gcode_move->set_move_transform(std::bind(&TuningTower::move, this, std::placeholders::_1, std::placeholders::_2), true);
    // m_normal_get_position_transform = Printer::GetInstance()->m_gcode_move->set_get_position_transform(std::bind(&TuningTower::get_position, this), true);
    m_last_z = -99999999.9;
    m_last_command_value = DBL_MIN;
    this->get_position();
    // gcmd.respond_info("Starting tuning test (start=%.6f factor=%.6f)"
    //                     % (m_start, m_factor))  //---??---TuningTower
}
        
std::vector<double> TuningTower::get_position()
{
    std::vector<double> pos = m_normal_get_position_transform();
    m_last_position = pos;
    return pos;
}
        
double TuningTower::calc_value(double z)
{
    if (m_band)
        z = (floor(z / m_band) + .5) * m_band;
    return m_start + z * m_factor;
}
        
bool TuningTower::move(std::vector<double> newpos, double speed)
{
    if (newpos[3] > m_last_position[3] && newpos[2] != m_last_z && (newpos[0] != m_last_position[0] || newpos[1] != m_last_position[1] || newpos[2] != m_last_position[2]))
    {
        // Extrusion move at new z height
        double z = newpos[2];
        if (z < m_last_z - CANCEL_Z_DELTA)
        {
            // Extrude at a lower z height - probably start of new print
            end_test();
        }
        else
        {
            //Process update
            double gcode_z = Printer::GetInstance()->m_gcode_move->get_status(0.).base_position[2]; 
            double newval = calc_value(gcode_z);
            m_last_z = z;
            if (newval != m_last_command_value)
            {
                m_last_command_value = newval;
                std::vector<std::string> cmds = {m_command_fmt + std::to_string(newval)};
                Printer::GetInstance()->m_gcode->run_script_from_command(cmds);
            }
                
        }
            
    }
    // Forward move to actual handler
    m_last_position = newpos;
    return m_normal_move_transform(newpos, speed);
}
        
void TuningTower::end_test()
{
    Printer::GetInstance()->m_gcode->respond_info("Ending tuning test mode");
    // Printer::GetInstance()->m_gcode_move->set_move_transform(std::bind(&TuningTower::move, this, std::placeholders::_1, std::placeholders::_2), true);
    // Printer::GetInstance()->m_gcode_move->set_get_position_transform(std::bind(&TuningTower::get_position, this), true);
    Printer::GetInstance()->m_gcode_move->set_move_transform(m_normal_move_transform, true);
    
    m_normal_move_transform = nullptr;
    // m_normal_get_position_transform = nullptr;
}

bool TuningTower::is_active()
{
    return m_normal_move_transform != nullptr;
}
        

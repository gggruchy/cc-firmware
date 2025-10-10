#include "bed_screws.h"
#include "klippy.h"
#include "Define.h"

std::vector<double> parse_coord(std::string param)
{
    std::string value_str =  Printer::GetInstance()->m_pconfig->GetString("bed_screws", param);
    std::vector<double> ret;
    std::istringstream iss(value_str);	// 输入流
    string token;			// 接收缓冲区
    while (getline(iss, token, ','))	// 以split为分隔符
    {
        ret.push_back(atof(token.c_str()));
    }
    return ret;
}
    

BedScrews::BedScrews(std::string section_name){
    m_state = -1;
    m_current_screw = 0;
    m_adjust_again = false; 
    // Read config
    
    for (int i = 0; i < 99; i++)
    {
        std::string prefix = "screw" + std::to_string(i + 1);
        if (Printer::GetInstance()->m_pconfig->GetString(section_name, prefix) == "")
        {
            break;
        }
        std::vector<double> screw_coord = parse_coord(prefix);
        std::string screw_name = "screw at " +  std::to_string(screw_coord[0]) + "," + std::to_string(screw_coord[1]);
        screw_name = Printer::GetInstance()->m_pconfig->GetString(section_name, prefix + "_name", screw_name);
        m_screws.push_back(screw_coord);
        m_screws_index_name.push_back(screw_name);
        if (Printer::GetInstance()->m_pconfig->GetString(section_name, prefix + "_fine_adjust") != "")
        {
            std::vector<double> fine_coord = parse_coord(prefix + "_fine_adjust");
            m_fine_adjust.push_back(fine_coord);
        } 
    }
        
    if (m_screws.size() < 3)
    {
        std::cout << "bed_screws: Must have at least three screws" << std::endl;
    }
    // m_states = {"adjust": screws, "fine": fine_adjust}
    m_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "speed", 50., DBL_MIN, DBL_MAX, 0.);
    m_lift_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "probe_speed", 5., DBL_MIN, DBL_MAX, 0.);
    m_horizontal_move_z = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "horizontal_move_z", 5.);
    m_probe_z = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "probe_height", 0.);
    // Register command
    m_cmd_BED_SCREWS_ADJUST_help = "Tool to help adjust bed leveling screws";
    Printer::GetInstance()->m_gcode->register_command("BED_SCREWS_ADJUST", std::bind(&BedScrews::cmd_BED_SCREWS_ADJUST, this, std::placeholders::_1), false, m_cmd_BED_SCREWS_ADJUST_help);
}

BedScrews::~BedScrews(){

}
        
bool BedScrews::move(std::vector<double> coord, double speed)
{
    std::vector<double> pos = {coord[0], coord[1], coord[2], coord[3]};
    return Printer::GetInstance()->m_tool_head->manual_move(pos, speed);
}

void BedScrews::move_to_screw(int state, int screw)
{
    // Move up, over, and then down
    std::vector<double> pos = {DBL_MIN, DBL_MIN, m_horizontal_move_z, DBL_MIN};
    move(pos, m_lift_speed);
    if (state == adjust)
    {
        std::vector<double> coord = m_screws[screw];
        std::vector<double> coord1 = {coord[0], coord[1], m_horizontal_move_z, DBL_MIN};
        move(coord1, m_speed);
        std::vector<double> coord2 = {coord[0], coord[1], m_probe_z, DBL_MIN};
        move(coord2, m_lift_speed);
    }
    else if (state == fine)
    {
        std::vector<double> coord = m_fine_adjust[screw];
        std::vector<double> coord1 = {coord[0], coord[1], m_horizontal_move_z, DBL_MIN};
        move(coord1, m_speed);
        std::vector<double> coord2 = {coord[0], coord[1], m_probe_z, DBL_MIN};
        move(coord2, m_lift_speed);
    }
    
    // Update state
    m_state = state;
    m_current_screw = screw;
    std::string name = m_screws_index_name[screw];
    // Register commands
    Printer::GetInstance()->m_gcode->respond_info(
        "Adjust " + name + ". Then run ACCEPT, ADJUSTED, or ABORT\n \
        Use ADJUSTED if a significant screw adjustment is made");

    m_cmd_ACCEPT_help = "Accept bed screw position";
    m_cmd_ADJUSTED_help = "Accept bed screw position after notable adjustment";
    m_cmd_ABORT_help = "Abort bed screws tool";
    Printer::GetInstance()->m_gcode->register_command("ACCEPT", std::bind(&BedScrews::cmd_ACCEPT, this, std::placeholders::_1), false, m_cmd_ACCEPT_help);
    Printer::GetInstance()->m_gcode->register_command("ADJUSTED", std::bind(&BedScrews::cmd_ADJUSTED, this, std::placeholders::_1), false, m_cmd_ADJUSTED_help);
    Printer::GetInstance()->m_gcode->register_command("ABORT", std::bind(&BedScrews::cmd_ABORT, this, std::placeholders::_1), false, m_cmd_ABORT_help);
}
        
void BedScrews::unregister_commands()
{
    Printer::GetInstance()->m_gcode->register_command("ACCEPT", NULL);
    Printer::GetInstance()->m_gcode->register_command("ADJUSTED", NULL);
    Printer::GetInstance()->m_gcode->register_command("ABORT", NULL);
}
        
void BedScrews::cmd_BED_SCREWS_ADJUST(GCodeCommand& gcmd)
{
    if (m_state != -1)
    {
        std::cout << "Already in bed_screws helper; use ABORT to exit" << std::endl;
    }
    m_adjust_again = false;
    std::vector<double> pos = {DBL_MIN, DBL_MIN, m_horizontal_move_z, DBL_MIN};
    move(pos, m_speed);
    move_to_screw(adjust, 0);
}
        
    
void BedScrews::cmd_ACCEPT(GCodeCommand& gcmd)
{
    unregister_commands();
    if (m_state == adjust)
    {
        if (m_current_screw + 1 < m_screws.size())
        {
            // Continue with next screw
            move_to_screw(m_state, m_current_screw + 1);
            return;
        }
    }
    else if(m_state == fine)
    {
        if (m_current_screw + 1 < m_fine_adjust.size())
        {
            // Continue with next screw
            move_to_screw(m_state, m_current_screw + 1);
            return;
        }
    }    
    if (m_adjust_again)
    {
        // Retry coarse adjustments
        m_adjust_again = false;
        move_to_screw(adjust, 0);
        return;
    }
    if (m_state == adjust && m_fine_adjust.size() != 0)
    {
        // Perform fine screw adjustments
        move_to_screw(fine, 0);
        return;
    }
    // Done
    m_state = -1;
    std::vector<double> pos = {DBL_MIN, DBL_MIN, m_horizontal_move_z, DBL_MIN};
    move(pos, m_lift_speed);
    // gcmd.m_respond_info("Bed screws tool completed successfully");
}
        
    
void BedScrews::cmd_ADJUSTED(GCodeCommand& gcmd)
{
    unregister_commands();
    m_adjust_again = true;
    cmd_ACCEPT(gcmd);
}
        
    
void BedScrews::cmd_ABORT(GCodeCommand& gcmd)
{
    unregister_commands();
    m_state = -1;
}
        


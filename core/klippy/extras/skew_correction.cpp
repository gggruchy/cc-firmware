#include "skew_correction.h"
#include "Define.h"
#include "klippy.h"

double calc_skew_factor(double ac, double bd, double ad)
{
    double side = sqrt(2*ac*ac + 2*bd*bd - 4*ad*ad) / 2.;
    return tan(M_PI/2 - acos((ac*ac - side*side - ad*ad) / (2*side*ad)));
}

SkewCorrection::SkewCorrection()
{
    // m_name = SKEW_CORRECTION_NAME;
    m_xy_factor = 0.;
    m_xz_factor = 0.;
    m_yz_factor = 0.;
    
    _load_storage();
    Printer::GetInstance()->register_event_handler("klippy:connect:SkewCorrection", std::bind(&SkewCorrection::_handle_connect, this)); //---??---SkewCorrection
    // m_next_transform = None

    m_cmd_GET_CURRENT_SKEW_help = "Report current printer skew";
    m_cmd_CALC_MEASURED_SKEW_help = "Calculate skew from measured print";
    m_cmd_SET_SKEW_help = "Set skew based on lengths of measured object";
    m_cmd_SKEW_PROFILE_help = "Profile management for skew_correction";
    Printer::GetInstance()->m_gcode->register_command("GET_CURRENT_SKEW", std::bind(&SkewCorrection::cmd_GET_CURRENT_SKEW, this, std::placeholders::_1), false, m_cmd_GET_CURRENT_SKEW_help);
    Printer::GetInstance()->m_gcode->register_command("CALC_MEASURED_SKEW", std::bind(&SkewCorrection::cmd_CALC_MEASURED_SKEW, this, std::placeholders::_1), false, m_cmd_CALC_MEASURED_SKEW_help);
    Printer::GetInstance()->m_gcode->register_command("SET_SKEW", std::bind(&SkewCorrection::cmd_SET_SKEW, this, std::placeholders::_1), false, m_cmd_SET_SKEW_help);
    Printer::GetInstance()->m_gcode->register_command("SKEW_PROFILE", std::bind(&SkewCorrection::cmd_SKEW_PROFILE, this, std::placeholders::_1), false, m_cmd_SKEW_PROFILE_help);
}

SkewCorrection::~SkewCorrection()
{

}

void SkewCorrection::_handle_connect()
{
    m_next_move_transform = Printer::GetInstance()->m_gcode_move->set_move_transform(std::bind(&SkewCorrection::move, this, std::placeholders::_1, std::placeholders::_2), true);
    m_next_get_position_transform = Printer::GetInstance()->m_gcode_move->set_get_position_transform(std::bind(&SkewCorrection::get_position, this), true);
}
        
void SkewCorrection::_load_storage()
{
    std::vector<std::string> stored_profs = Printer::GetInstance()->Printer::GetInstance()->m_pconfig->get_prefix_sections(m_name);
    // Remove primary skew_correction section, as it is not a stored profile
    for (int i = 0; i < stored_profs.size(); i++)
    {
        if (stored_profs[i] == m_name)
        {
            stored_profs.erase(stored_profs.begin() + i);
        }
    }
    for (int i = 0; i < stored_profs.size(); i++)
    {
        m_skew_profiles_xy[stored_profs[i]] = Printer::GetInstance()->m_pconfig->GetDouble(stored_profs[i], "xy_skew");
        m_skew_profiles_xz[stored_profs[i]] = Printer::GetInstance()->m_pconfig->GetDouble(stored_profs[i], "xz_skew");
        m_skew_profiles_yz[stored_profs[i]] = Printer::GetInstance()->m_pconfig->GetDouble(stored_profs[i], "yz_skew");
    }
}
        
std::vector<double> SkewCorrection::calc_skew(std::vector<double> pos)
{
    double skewed_x = pos[0] - pos[1] * m_xy_factor - pos[2] * (m_xz_factor - (m_xy_factor * m_yz_factor));
    double skewed_y = pos[1] - pos[2] * m_yz_factor;
    std::vector<double> ret = {skewed_x, skewed_y, pos[2], pos[3]};
    return ret;
}
        
std::vector<double> SkewCorrection::calc_unskew(std::vector<double> pos)
{
    double skewed_x = pos[0] + pos[1] * m_xy_factor + pos[2] * m_xz_factor;
    double skewed_y = pos[1] + pos[2] * m_yz_factor;
    std::vector<double> ret = {skewed_x, skewed_y, pos[2], pos[3]};
    return ret;
}
        
std::vector<double> SkewCorrection::get_position()
{
    return calc_unskew(m_next_get_position_transform());
}
        
bool SkewCorrection::move(std::vector<double> newpos, double speed)
{
    std::vector<double> corrected_pos = calc_skew(newpos);
    return m_next_move_transform(corrected_pos, speed);
}
        
void SkewCorrection::_update_skew(double xy_factor, double xz_factor, double yz_factor)
{
    m_xy_factor = xy_factor;
    m_xz_factor = xz_factor;
    m_yz_factor = yz_factor;
    Printer::GetInstance()->m_gcode_move->reset_last_position();
}
        
void SkewCorrection::cmd_GET_CURRENT_SKEW(GCodeCommand& gcmd)
{
    std::string out = "Current Printer Skew:";
    std::vector<std::string> planes = {"XY", "XZ", "YZ"};
    std::vector<double> factors = {m_xy_factor, m_xz_factor, m_yz_factor};
    for (int i = 0; i < 3; i++)
    {
        out += "\n" + planes[i];
        out += " Skew: " + std::to_string(factors[i]) + " radians" + std::to_string(factors[i] * 180 / M_PI) + "degrees";
    }
    gcmd.m_respond_info(out); //---??---SkewCorrection::
}

void SkewCorrection::cmd_CALC_MEASURED_SKEW(GCodeCommand& gcmd)
{
    double ac = gcmd.get_double("AC", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    double bd = gcmd.get_double("BD", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    double ad = gcmd.get_double("AD", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    double factor = calc_skew_factor(ac, bd, ad);
    gcmd.m_respond_info("Calculated Skew: " + std::to_string(factors[i]) + " radians" + std::to_string(factors[i] * 180 / M_PI) + " degrees"); //---??---SkewCorrection
}
        
void SkewCorrection::cmd_SET_SKEW(GCodeCommand& gcmd)
{
    if (gcmd.get_int("CLEAR", 0))
    {
        _update_skew(0., 0., 0.);
        return;
    }
        
    std::vector<std::string> planes = {"XY", "XZ", "YZ"};
    for (int i = 0; i < planes.size(); i++)
    {
        std::string lengths = gcmd.get_string(planes[i], "");
        if (lengths != "")
        {
            std::vector<std::string> lengths_temp;
            std::istringstream iss(lengths);	// 输入流
            string token;			// 接收缓冲区
            while (getline(iss, token, ','))	// 以split为分隔符
            {
                lengths_temp.push_back(token);
            }
            if (lengths_temp.size() != 3)
            {
                std::cout << "skew_correction: improperly formatted entry for plane" << planes[i] << std::endl;
            }
            std::string factor = planes[i] + "_factor";
            if (factor == "xy_factor")
            {
                m_xy_factor = calc_skew_factor(atof(lengths_temp[0].c_str()), atof(lengths_temp[1].c_str()), atof(lengths_temp[2].c_str()));
            }
            else if (factor == "xz_factor")
            {
                m_xz_factor = calc_skew_factor(atof(lengths_temp[0].c_str()), atof(lengths_temp[1].c_str()), atof(lengths_temp[2].c_str()));
            }
            else if (factor == "yz_factor")
            {
                m_yz_factor = calc_skew_factor(atof(lengths_temp[0].c_str()), atof(lengths_temp[1].c_str()), atof(lengths_temp[2].c_str()));
            }
            
        }
    }
            
}
        
void SkewCorrection::cmd_SKEW_PROFILE(GCodeCommand& gcmd)
{
    if (gcmd.get_string("LOAD", "") != "")
    {
        std::string name = gcmd.get_string("LOAD", "");
        double xy_factor = m_skew_profiles_xy[name];
        double xz_factor = m_skew_profiles_xz[name];
        double yz_factor = m_skew_profiles_yz[name];
        if (xy_factor == DBL_MIN || xz_factor == DBL_MIN || yz_factor == DBL_MIN)
        {
            gcmd.m_respond_info("skew_correction:  Load failed, unknown profile " + name); //---??---
            return;
        }
        _update_skew(xy_factor, xz_factor, yz_factor);
    }   
    else if (gcmd.get_string("SAVE", "") != "")
    {
        std::string name = gcmd.get_string("SAVE", "");
        std::string cfg_name = m_name + " " + name;
        Printer::GetInstance()->Printer::GetInstance()->m_pconfig->SetDouble(cfg_name, "xy_skew", m_xy_factor);
        Printer::GetInstance()->Printer::GetInstance()->m_pconfig->SetDouble(cfg_name, "xz_skew", m_xz_factor);
        Printer::GetInstance()->Printer::GetInstance()->m_pconfig->SetDouble(cfg_name, "yz_skew", m_yz_factor);
        // Copy to local storage
        m_skew_profiles_xy[name] = Printer::GetInstance()->m_pconfig->GetDouble(cfg_name, "xy_skew");
        m_skew_profiles_xz[name] = Printer::GetInstance()->m_pconfig->GetDouble(cfg_name, "xz_skew");
        m_skew_profiles_yz[name] = Printer::GetInstance()->m_pconfig->GetDouble(cfg_name, "yz_skew");
        gcmd.m_respond_info("Skew Correction state has been saved to profile " + name + "\n \
            for the current session.  The SAVE_CONFIG command will\n \
            update the printer config file and restart the printer.");  //---??---
    }
    else if (gcmd.get_string("REMOVE", "") != "")
    {
        std::string name = gcmd.get_string("REMOVE", "");
        if (m_skew_profiles_xy.find(name) != m_skew_profiles_xy.end())
        {
            Printer::GetInstance()->m_pconfig->DeleteSection("skew_correction " + name);
            m_skew_profiles_xy.erase(name);
            // gcmd.m_respond_info(
            //     "Profile " + name + " removed from storage for this session.\n \
            //     The SAVE_CONFIG command will update the printer\n \
            //     configuration and restart the printer")  //---??---
        }
        else
        {
            // gcmd.m_respond_info("skew_correction: No profile named " + name + "to remove");  //---??---
        }
            
            
        if (m_skew_profiles_xy.find(name) != m_skew_profiles_xy.end())
        {
            Printer::GetInstance()->m_pconfig->DeleteSection("skew_correction " + name);
            m_skew_profiles_xy.erase(name);
            // gcmd.m_respond_info(
            //     "Profile " + name + " removed from storage for this session.\n \
            //     The SAVE_CONFIG command will update the printer\n \
            //     configuration and restart the printer") //---??---
        }
        else
        {
            // gcmd.m_respond_info("skew_correction: No profile named " + name + "to remove"); //---??---
        }
        if (m_skew_profiles_xy.find(name) != m_skew_profiles_xy.end())
        {
            Printer::GetInstance()->m_pconfig->DeleteSection("skew_correction " + name);
            m_skew_profiles_xy.erase(name);
            // gcmd.m_respond_info(
            //     "Profile " + name + " removed from storage for this session.\n \
            //     The SAVE_CONFIG command will update the printer\n \
            //     configuration and restart the printer")  //---??---
        }
        else
        {
            // gcmd.m_respond_info("skew_correction: No profile named " + name + "to remove");  //---??---
        }
           
    }
        
}
        



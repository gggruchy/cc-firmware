#include "print_rail.h"
#include "Define.h"
#include "klippy.h"
MCU_stepper* PrinterStepper(std::string section_name, bool units_in_radians)
{
    // MCU_stepper definition
    std::string step_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "step_pin", "");
    pinParams *step_pin_params = Printer::GetInstance()->m_ppins->lookup_pin(step_pin, true);
    std::string dir_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "dir_pin", "");
    pinParams *dir_pin_params = Printer::GetInstance()->m_ppins->lookup_pin(dir_pin, true);
    double step_dist = parse_step_distance(section_name, units_in_radians, true);
    MCU_stepper *mcu_stepper = new MCU_stepper(section_name, step_pin_params, dir_pin_params, step_dist, units_in_radians);
    // Support for stepper enable pin handling
    Printer::GetInstance()->load_object("stepper_enable");
    Printer::GetInstance()->m_stepper_enable->register_stepper(mcu_stepper, Printer::GetInstance()->m_pconfig->GetString(section_name, "enable_pin", ""));
    // Register STEPPER_BUZZ command
    Printer::GetInstance()->load_object("force_move");
    Printer::GetInstance()->m_force_move->register_stepper(mcu_stepper);  
    return mcu_stepper;
}

double parse_gear_ratio(std::string section_name, bool note_valid)
{
    std::string gear_ratio = Printer::GetInstance()->m_pconfig->GetString(section_name, "gear_ratio", "");
    if (gear_ratio == "")
        return 1.;
    double result = 1.;
    std::istringstream iss(gear_ratio);	// 输入流
    std::string token1;			// 接收缓冲区
    while (getline(iss, token1, ','))	// 以split为分隔符
    {
        std::string token2;	
        // std::cout << "token1--" << token1<< std::endl;
        std::istringstream iss1(token1);	// 输入流
        std::vector<double> out;
        while (getline(iss1, token2, ':'))
        {
            // std::cout << "token2--" << token2<< std::endl;
            out.push_back(atof(token2.c_str()));
        }
        double g1 = out[0];
        double g2 = out[1];
        result = result * (g1 / g2);
        // std::cout << "result--" << result<< std::endl;
    }
    return result;
}

double parse_step_distance(std::string section_name, bool units_in_radians, bool note_valid)         //计算步距
{
    std::string rd = "";
    std::string gr = "";
    std::string sd = "";
    double rotation_dist = 0.;
    if (units_in_radians == false)
    {
        // Caller doesn"t know if units are in radians - infer it
        rd = Printer::GetInstance()->m_pconfig->GetString(section_name, "rotation_distance", "");
        gr = Printer::GetInstance()->m_pconfig->GetString(section_name, "gear_ratio", "");
        units_in_radians = (rd == "" && gr != "");
    }
    if (units_in_radians)
    {
        rotation_dist = 2. * M_PI;
        Printer::GetInstance()->m_pconfig->GetString(section_name, "gear_ratio", "");
    }
    else
    {
        rd = Printer::GetInstance()->m_pconfig->GetString(section_name, "rotation_distance", "");
        sd = Printer::GetInstance()->m_pconfig->GetString(section_name, "step_distance", "");
        if (rd == "" && sd != "")
        {
            // Older config format with step_distance
            return Printer::GetInstance()->m_pconfig->GetDouble(section_name, "step_distance", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
        } 
        rotation_dist = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "rotation_distance", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    }
        
    // Newer config format with rotation_distance
    int microsteps = Printer::GetInstance()->m_pconfig->GetInt(section_name, "microsteps", INT32_MIN, 1);
    int full_steps = Printer::GetInstance()->m_pconfig->GetInt(section_name, "full_steps_per_rotation", 200, 1);
    if (full_steps % 4)
    {
        std::stringstream eor;
        eor << "full_steps_per_rotation invalid in section " << section_name;
        // config.error(eor.str);  //---??---
    }
    double gearing = parse_gear_ratio(section_name, note_valid); 
    std::cout << "rotation_dist--" <<  rotation_dist / (full_steps * microsteps * gearing)<< std::endl;
    return rotation_dist / (full_steps * microsteps * gearing); //(20*2.032)/((360/1.8)*16)
}

std::vector<std::string> stringSplit( std::string& str, char delim) {
    std::size_t previous = 0;
    std::size_t current = str.find_first_of(delim);
    std::vector<std::string> elems;
    while (current != std::string::npos) {
        if (current > previous) {
            elems.push_back(str.substr(previous, current - previous));
        }
        previous = current + 1;
        current = str.find_first_of(delim, previous);
    }
    if (previous != str.size()) {
        elems.push_back(str.substr(previous));
    }
    return elems;
}

PrinterRail* LookupMultiRail(std::string section_name)
{
    PrinterRail *rail = new PrinterRail(section_name);
    for (int i = 1; i < 99; i++)
    {
        std::stringstream section;
        section << section_name << i;
        if (Printer::GetInstance()->m_pconfig->GetSection(section.str()) == nullptr)
        {
            break;
        }
        rail->add_extra_stepper(section.str());
    }
    return rail;
}
    

PrinterRail::PrinterRail(std::string section_name, bool need_position_minmax, double default_position_endstop, bool units_in_radians)
{
    // Primary stepper and endstop
    m_stepper_units_in_radians = false;     //步进单位是mm还是角度
    cur_stepper_slect = 0;
    add_extra_stepper(section_name);
    // Primary endstop position
    if (default_position_endstop == DBL_MIN)
        m_position_endstop = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "position_endstop", DBL_MIN);
    else
        m_position_endstop = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "position_endstop", default_position_endstop);
    m_position_endstop_extra = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "position_endstop_extra", default_position_endstop);
    printf("%s: m_position_endstop--%f\n",section_name.c_str(), m_position_endstop);
    // Axis range
    if (need_position_minmax)
    {
        m_position_min = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "position_min", 0.);
        m_position_max = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "position_max", DBL_MIN, DBL_MIN, DBL_MAX, m_position_min);
    } 
    else
    {
        m_position_min = 0.;
        m_position_max = m_position_endstop;
    }
        
    if (m_position_endstop < m_position_min || m_position_endstop > m_position_max)
    {
        std::stringstream eor;
        eor << "position_endstop in section " << section_name << " position_min and position_max";
        // config.error(eor.str); //---??---PrinterRail
    }
        
    // Homing mechanics
    m_homing_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "homing_speed", 5.0, DBL_MIN, DBL_MAX, 0.);
    m_second_homing_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "second_homing_speed", m_homing_speed/2., DBL_MIN, DBL_MAX, 0.);
    m_homing_retract_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "homing_retract_speed", m_homing_speed, DBL_MIN, DBL_MAX, 0.);
    m_homing_retract_dist = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "homing_retract_dist", 5., 0.);
    m_homing_force_retract = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "homing_force_retract", 0., 0.);
    m_homing_positive_dir = Printer::GetInstance()->m_pconfig->GetBool(section_name, "homing_positive_dir", false);
    m_homing_accel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "homing_accel", DBL_MIN, 0., 50000., 0.);
    m_homing_current = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "homing_current", DBL_MIN, 0., 2., 0.);
    if (m_homing_positive_dir == false)
    {
        int axis_len = m_position_max - m_position_min;
        if (m_position_endstop <= m_position_min + axis_len / 4.)
            m_homing_positive_dir = false;
        else if (m_position_endstop >= m_position_max - axis_len / 4.)
            m_homing_positive_dir = true;
        else
        {
            std::stringstream eor;
            eor << "Unable to infer homing_positive_dir in section " << section_name;
        }
        Printer::GetInstance()->m_pconfig->GetBool(section_name, "homing_positive_dir", m_homing_positive_dir);
    }  
    else if ((m_homing_positive_dir && m_position_endstop == m_position_min) || (not m_homing_positive_dir && m_position_endstop == m_position_max))
    {
        std::stringstream eor;
        eor << "Invalid homing_positive_dir / position_endstop in " << section_name;
    }
}
PrinterRail::~PrinterRail()
{
    for (int i = 0; i <= m_steppers.size(); i++)
    {
        if (m_steppers[i] != nullptr)
        {
            delete m_steppers[i];
        }
    }
    for (int i = 0; i < m_endstops.size(); i++)
    {
        
        if (m_endstops[i] != nullptr)
        {
            delete m_endstops[i];        
        }
        
    }
}


std::vector<double> PrinterRail::get_range()
{
    std::vector<double> ret = {m_position_min, m_position_max};
    return ret;
}

homingInfo PrinterRail::get_homing_info()
{
    homingInfo homing_info;
    homing_info.speed = m_homing_speed;
    homing_info.position_endstop = m_position_endstop;
    homing_info.retract_speed = m_homing_retract_speed;
    homing_info.retract_dist = m_homing_retract_dist;
    homing_info.force_retract = m_homing_force_retract;
    homing_info.positive_dir = m_homing_positive_dir;
    homing_info.second_homing_speed = m_second_homing_speed;
    homing_info.homing_accel = m_homing_accel;
    homing_info.homing_current = m_homing_current;
    return homing_info;
}

std::vector<MCU_stepper*> PrinterRail::get_steppers()
{
    return m_steppers;
}

std::vector<MCU_endstop*> PrinterRail::get_endstops()
{
    return m_endstops;
}

void PrinterRail::add_extra_stepper(std::string section_name)
{
    MCU_stepper *stepper = PrinterStepper(section_name, m_stepper_units_in_radians);
    m_steppers.push_back(stepper);
    if (m_endstops.size() != 0 && Printer::GetInstance()->m_pconfig->GetString(section_name, "endstop_pin", "") == "")
    {
        // No endstop defined - use primary endstop
        m_endstops[0]->add_stepper(stepper);
        return;
    }
    MCU_endstop *mcu_endstop;
    if(Printer::GetInstance()->m_pconfig->GetInt(section_name, "virtrual_endstop", 0))
    {
        mcu_endstop = (MCU_endstop *)Printer::GetInstance()->m_ppins->setup_pin("endstop", "tmc2209_" + section_name + ":virtrual_endstop");
    }
    else
    {
        mcu_endstop = (MCU_endstop *)Printer::GetInstance()->m_ppins->setup_pin("endstop", Printer::GetInstance()->m_pconfig->GetString(section_name, "endstop_pin", ""));
    }
    mcu_endstop->add_stepper(stepper);
    std:string name = stepper->get_name();
    m_endstops_name.push_back(name);
    m_endstops.push_back(mcu_endstop);
// #include "debug.h"
//  DisTraceMsg();
// QueryEndstops* query_endstops = (QueryEndstops*)Printer::GetInstance()->load_object("query_endstops");
// query_endstops->register_endstop(mcu_endstop, name); //---??---
//  DisTraceMsg();
    cur_stepper_slect = (1 << m_steppers.size()) - 1;    
}

void PrinterRail::setup_itersolve(char axis)
{
    for (int i = 0; i < m_steppers.size(); i++)
    {
        m_steppers[i]->setup_itersolve(axis);
    }
}

void PrinterRail::setup_itersolve(double arm2, double tower_x, double tower_y)
{
    for (int i = 0; i < m_steppers.size(); i++)
    {
        m_steppers[i]->setup_itersolve(arm2, tower_x, tower_y);
    }
}

void PrinterRail::set_trapq(trapq* trapq)
{
    for (int i = 0; i < m_steppers.size(); i++)
    {
        m_steppers[i]->set_trapq(trapq);
    }
}
        
void PrinterRail::set_position(double coord[3])
{
    for (int i = 0; i < m_steppers.size(); i++)
    {
        m_steppers[i]->set_position(coord);
    }
}

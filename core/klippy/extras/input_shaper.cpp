#include "input_shaper.h"
#include "klippy.h"
#include "Define.h"

//shaper type "zv" "zvd" "mzv" "ei" "2hump_ei" "3hump_ei"

InputShaper::InputShaper(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:connect:InputShaper", std::bind(&InputShaper::connect, this));
    m_damping_ratio_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "damping_ratio_x", 0.1, 0., 1.);
    m_damping_ratio_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "damping_ratio_y", 0.1, 0., 1.);
    m_shaper_freq_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "shaper_freq_x", 0., 0.);
    m_shaper_freq_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "shaper_freq_y", 0., 0.);
    m_shapers = {{"zv", 0}, {"zvd", 1}, {"mzv", 2}, {"ei", 3}, {"2hump_ei", 4}, {"3hump_ei", 5}};
    std::string shaper_type = Printer::GetInstance()->m_pconfig->GetString(section_name, "shaper_type", "mzv");
    m_shaper_type_x = m_shapers[Printer::GetInstance()->m_pconfig->GetString(section_name, "shaper_type_x", shaper_type)];
    m_shaper_type_y = m_shapers[Printer::GetInstance()->m_pconfig->GetString(section_name, "shaper_type_y", shaper_type)];
    m_saved_shaper_freq_x = 0.;
    m_saved_shaper_freq_y = 0.;
    // Register gcode commands
    m_cmd_SET_INPUT_SHAPER_help = "Set cartesian parameters for input shaper";
    Printer::GetInstance()->m_gcode->register_command("M593", std::bind(&InputShaper::cmd_SET_INPUT_SHAPER, this, std::placeholders::_1), false, m_cmd_SET_INPUT_SHAPER_help);
    Printer::GetInstance()->m_gcode->register_command("SET_INPUT_SHAPER", std::bind(&InputShaper::cmd_SET_INPUT_SHAPER, this, std::placeholders::_1), false, m_cmd_SET_INPUT_SHAPER_help);
}

InputShaper::~InputShaper()
{
}

void InputShaper::connect()
{
    for(int i = 0; i < Printer::GetInstance()->m_tool_head->m_kin->m_rails.size(); i++)
    {
        for (int j = 0; j < Printer::GetInstance()->m_tool_head->m_kin->m_rails[i]->m_steppers.size(); j++)
        {
            stepper_kinematics *sk = input_shaper_alloc();
            stepper_kinematics *orig_sk = Printer::GetInstance()->m_tool_head->m_kin->m_rails[i]->m_steppers[j]->set_stepper_kinematics(sk);
            int res = input_shaper_set_sk(sk, orig_sk);
            if(res < 0)
            {
                Printer::GetInstance()->m_tool_head->m_kin->m_rails[i]->m_steppers[j]->set_stepper_kinematics(orig_sk);
                continue;
            }
            m_stepper_kinematics.push_back(sk);
            m_orig_stepper_kinematics.push_back(orig_sk);
        }
        
    }
    m_old_delay = 0;
    set_input_shaper(m_shaper_type_x, m_shaper_type_y,
                    m_shaper_freq_x, m_shaper_freq_y,
                    m_damping_ratio_x, m_damping_ratio_y);
}

void InputShaper::set_input_shaper(int shaper_type_x, int shaper_type_y,
                        double shaper_freq_x, double shaper_freq_y,
                        double damping_ratio_x, double damping_ratio_y)
{
    if(shaper_type_x != m_shaper_type_x || shaper_type_y != m_shaper_type_y)
    {
        Printer::GetInstance()->m_tool_head->flush_step_generation();
    }   
    double new_delay = 0;
    double delay_x, delay_y;
    delay_x = input_shaper_get_step_generation_window(shaper_type_x, shaper_freq_x, damping_ratio_x);
    delay_y = input_shaper_get_step_generation_window(shaper_type_y, shaper_freq_y, damping_ratio_x);
    new_delay = std::max(delay_x, delay_y);
    Printer::GetInstance()->m_tool_head->note_step_generation_scan_time(new_delay, m_old_delay);
    m_old_delay = new_delay;
    m_shaper_type_x = shaper_type_x;
    m_shaper_type_y = shaper_type_y;
    m_shaper_freq_x = shaper_freq_x;
    m_shaper_freq_y = shaper_freq_y; 
    m_damping_ratio_x = damping_ratio_x;
    m_damping_ratio_y = damping_ratio_y;
    
    for(int i = 0; i < m_stepper_kinematics.size(); i++)
    {
        input_shaper_set_shaper_params(m_stepper_kinematics[i]
            , shaper_type_x, shaper_type_y
            , shaper_freq_x, shaper_freq_y
            , damping_ratio_x, damping_ratio_y);
    }
}

void InputShaper::disable_shaping()
{
    if ((m_saved_shaper_freq_x || m_saved_shaper_freq_y) && !(m_shaper_freq_x || m_shaper_freq_y))
    {
        //Input shaper is already disabled
        return;
    }
    m_saved_shaper_freq_x = m_shaper_freq_x;
    m_saved_shaper_freq_y = m_shaper_freq_y;
    set_input_shaper(m_shaper_type_x, m_shaper_type_y, 0., 0.,m_damping_ratio_x, m_damping_ratio_y);
}

bool InputShaper::enable_shaping()
{
    bool saved = m_saved_shaper_freq_x || m_saved_shaper_freq_y;
    if(saved)
    {
        set_input_shaper(m_shaper_type_x, m_shaper_type_y, m_saved_shaper_freq_x, m_saved_shaper_freq_y, m_damping_ratio_x, m_damping_ratio_y);
    }
    m_saved_shaper_freq_x = m_saved_shaper_freq_y = 0;
    return saved;
}
        
int InputShaper::parse_shaper(std::string shaper_type_str)
{
    if (m_shapers.find(shaper_type_str) == m_shapers.end())
    {
        // gcmd.error("Requested shaper type " + shaper_type_str + " is not supported");
        return -1;
    }
    return m_shapers[shaper_type_str];
}

void InputShaper::cmd_SET_INPUT_SHAPER(GCodeCommand& gcmd)
{
    double damping_ratio_x = gcmd.get_double("DAMPING_RATIO_X", m_damping_ratio_x, 0., 1.);
    double damping_ratio_y = gcmd.get_double("DAMPING_RATIO_Y", m_damping_ratio_y, 0., 1.);
    double shaper_freq_x = gcmd.get_double("SHAPER_FREQ_X", m_shaper_freq_x, 0.);
    double shaper_freq_y = gcmd.get_double("SHAPER_FREQ_Y", m_shaper_freq_y, 0.);
    int shaper_type = parse_shaper(gcmd.get_string("SHAPER_TYPE", ""));
    int shaper_type_x;
    int shaper_type_y;
    if (shaper_type == -1)
    {
        shaper_type_x = parse_shaper(gcmd.get_string("SHAPER_TYPE_X", ""));
        if (shaper_type_x = -1)
            shaper_type_x = m_shaper_type_x;
        shaper_type_y = parse_shaper(gcmd.get_string("SHAPER_TYPE_Y", ""));
        if (shaper_type_y = -1)
            shaper_type_y = m_shaper_type_y;
    }
    else
    {
        shaper_type_x = shaper_type_y = shaper_type;
    }
    set_input_shaper(shaper_type_x, shaper_type_y,shaper_freq_x, shaper_freq_y,damping_ratio_x, damping_ratio_y);

    // id_to_name = {v: n for n, v in m_shapers.items()}
    // gcmd.m_respond_info("shaper_type_x:%s shaper_type_y:%s "
    //                     "shaper_freq_x:%.3f shaper_freq_y:%.3f "
    //                     "damping_ratio_x:%.6f damping_ratio_y:%.6f"
    //                     % (id_to_name[shaper_type_x],
    //                         id_to_name[shaper_type_y],
    //                         shaper_freq_x, shaper_freq_y,
    //                         damping_ratio_x, damping_ratio_y))
}
        



        
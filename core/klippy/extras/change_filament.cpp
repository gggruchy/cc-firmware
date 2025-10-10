#include "change_filament.h"
#include "klippy.h"
#define LOG_TAG "change_filament"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

#define TEMPOFFSET (2)  // 温度偏差值，在作温度比较时允许有个温度偏差

ChangeFilament::ChangeFilament(std::string section_name)
{
    m_max_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_x", 0);
    m_max_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_y", 0);
    m_min_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "min_x", 0);
    m_min_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "min_y", 0);
    m_extrude_fan_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "extrude_fan_speed", 0);
    m_pos_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pos_x", 202);
    m_pos_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pos_y", 265);

    m_active = false;
    m_busy = false;
    m_check_move_ignore = false;
    set_feet_out_state(false);
    Printer::GetInstance()->m_gcode->register_command("MOVE_TO_EXTRUDE", std::bind(&ChangeFilament::cmd_MOVE_TO_EXTRUDE, this, std::placeholders::_1), false, "Move to extrude");
    Printer::GetInstance()->m_gcode->register_command("CHANGE_FILAMENT_SET_ACTIVE", std::bind(&ChangeFilament::cmd_CHANGE_FILAMENT_SET_ACTIVE, this, std::placeholders::_1), false, "Change filament status");
    Printer::GetInstance()->m_gcode->register_command("CHANGE_FILAMENT_SET_BUSY", std::bind(&ChangeFilament::cmd_CHANGE_FILAMENT_SET_BUSY, this, std::placeholders::_1), false, "Change filament busy");
    Printer::GetInstance()->m_gcode->register_command("CHANGE_FILAMENT_SET_CHECK_MOVE_IGNORE", std::bind(&ChangeFilament::cmd_CHANGE_FILAMENT_SET_CHECK_MOVE_IGNORE, this, std::placeholders::_1), false, "Change filament check_move_ignore");
    Printer::GetInstance()->m_gcode->register_command("CUT_OFF_FILAMENT", std::bind(&ChangeFilament::cmd_CUT_OFF_FILAMENT, this, std::placeholders::_1), false, "Cut off filament");
    Printer::GetInstance()->m_gcode->register_command("EXTRUDE_FILAMENT", std::bind(&ChangeFilament::cmd_EXTRUDE_FILAMENT, this, std::placeholders::_1), false, "Extrude filament");
}

ChangeFilament::~ChangeFilament()
{
}

bool ChangeFilament::is_active()
{
    return m_active;
}

bool ChangeFilament::is_feed_busy()
{
    return m_busy;
}

bool ChangeFilament::check_move(std::vector<double> &pos)
{
    if (m_active)
    {
        // if ((pos[0] > m_min_x && pos[0] < m_max_x) || (pos[1] > m_min_y && pos[1] < m_max_y))
        if (pos[0] > m_min_x && pos[1] < m_max_y)
        {
            if(m_check_move_ignore)     //防止发送多次事件导致多次弹窗
            {
                return false;
            }
            std::cout << "m_min_x : " << m_min_x << " m_max_x : " << m_max_x << " m_min_y : " << m_min_y << " m_max_y : " << m_max_y << std::endl;
            LOG_W("Can't move to %f, %f\n", pos[0], pos[1]);
            change_filament_state_callback_call(CHANGE_FILAMENT_STATE_CHECK_MOVE);
            m_check_move_ignore = true;
            return false;
        }
        else
        {
            return true;
        }
    }
    return true;
}

void ChangeFilament::cmd_MOVE_TO_EXTRUDE(GCodeCommand &gcmd)
{
    LOG_D("cmd_MOVE_TO_EXTRUDE start\n");
    int tar_temp = gcmd.get_int("TARGET_TEMP", 220);
    int move = gcmd.get_int("MOVE", 1, 0, 1);
    int zero_z = gcmd.get_int("ZERO_Z", 1); // 0：z不归零，1：z归零
    std::vector<std::string> script;
    std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(get_monotonic());
    if (zero_z)
    {
        if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos || kin_status["homed_axes"].find("z") == std::string::npos)
        {
            script.push_back("G28");                      // xyz归零
        }
        else
        {
            script.push_back("Z_AXIS_OFF_LIMIT_ACTION");
        }
    }
    else
    {
        if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos)
        {
            script.push_back("G28 X Y");                      // xy归零
        }
        else
        {
            script.push_back("Z_AXIS_OFF_LIMIT_ACTION");
        }
    }
    if (move)
    {
        script.push_back("G90");
        script.push_back("G1 X" + to_string(m_pos_x) + " F4500");
        script.push_back("G1 Y" + to_string(m_pos_y) + " F4500");
    }
    script.push_back("M109 S" + to_string(tar_temp)); // 设置温度
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    change_filament_state_callback_call(CHANGE_FILAMENT_STATE_MOVE_TO_EXTRUDE);
    LOG_D("cmd_MOVE_TO_EXTRUDE end\n");
}

void ChangeFilament::cmd_CHANGE_FILAMENT_SET_ACTIVE(GCodeCommand &gcmd)
{
    m_active = gcmd.get_int("ACTIVE", 0, 0, 1);
    LOG_I("ChangeFilament status : %d\n", m_active);
}

void ChangeFilament::cmd_CHANGE_FILAMENT_SET_BUSY(GCodeCommand &gcmd)
{
    m_busy = gcmd.get_int("BUSY", 0, 0, 1);
    LOG_I("ChangeFilament busy : %d\n", m_busy);
}

void ChangeFilament::cmd_CHANGE_FILAMENT_SET_CHECK_MOVE_IGNORE(GCodeCommand &gcmd)
{
    m_check_move_ignore = gcmd.get_int("CHECK_MOVE_IGNORE", 0, 0, 1);
    LOG_I("check_move_ignore : %d\n", m_check_move_ignore);
}

void ChangeFilament::cmd_CUT_OFF_FILAMENT(GCodeCommand &gcmd)
{
    LOG_D("cmd_CUT_OFF_FILAMENT start\n");
    int zero_z = gcmd.get_int("ZERO_Z", 1); // 0：不归零，1：归零
    std::vector<std::string> script;

    std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(get_monotonic());
    if (zero_z)
    {
        if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos || kin_status["homed_axes"].find("z") == std::string::npos)
        {
            script.push_back("G28");                      // xyz归零
        }
        else
        {
            double extruder_curr_temp = Printer::GetInstance()->m_printer_extruder->get_heater()->get_status(get_monotonic()).smoothed_temp;
            // LOG_I("-------------%llf,%d\n", extruder_curr_temp, Printer::GetInstance()->m_safe_z_homing->temp - TEMPOFFSET);
            if (extruder_curr_temp < Printer::GetInstance()->m_safe_z_homing->temp - TEMPOFFSET)
            {
                std::stringstream ss;
                ss << "M109 S" << Printer::GetInstance()->m_safe_z_homing->temp;
                std::string extruder_command = ss.str();
                script.push_back(extruder_command); //加热软化耗材(G28有加热则不执行)
                script.push_back("Z_AXIS_OFF_LIMIT_ACTION");
            }
        }
    }
    else
    {
        if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos)
        {
            script.push_back("G28 X Y");                      // xy归零
        }
        else
        {
            double extruder_curr_temp = Printer::GetInstance()->m_printer_extruder->get_heater()->get_status(get_monotonic()).smoothed_temp;
            if (extruder_curr_temp < Printer::GetInstance()->m_safe_z_homing->temp - TEMPOFFSET)
            {
                std::stringstream ss;
                ss << "M109 S" << Printer::GetInstance()->m_safe_z_homing->temp;
                std::string extruder_command = ss.str();
                script.push_back(extruder_command); //加热软化耗材(G28有加热则不执行)
                script.push_back("Z_AXIS_OFF_LIMIT_ACTION");
            }
        }
    }
    
    // script.push_back("G28 X Y"); // xy归零
    script.push_back("G90");
    script.push_back("G1 Y30 F4500");
    script.push_back("G1 X255 F4500"); // 移动到切割位置
    script.push_back("G1 Y3 F600"); // 切割
    script.push_back("G1 Y30 F4500");
    script.push_back("G1 X" + to_string(m_pos_x) + " F4500");
    script.push_back("G1 Y" + to_string(m_pos_y) + " F4500");
    script.push_back("M104 S0"); // 关闭喷头加热
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    change_filament_state_callback_call(CHANGE_FILAMENT_STATE_CUT_OFF_FILAMENT);
    LOG_D("cmd_CUT_OFF_FILAMENT end\n");
}

void ChangeFilament::cmd_EXTRUDE_FILAMENT(GCodeCommand &gcmd)
{
    LOG_D("cmd_EXTRUDE_FILAMENT start\n");
    double e = gcmd.get_double("E", 60);
    double f = gcmd.get_double("F", 240);
    int fan = gcmd.get_int("FAN_ON", 0, 0, 1);
    int report = gcmd.get_int("REPORT", 1);
    if(fan)
    {
        Printer::GetInstance()->m_gcode_io->single_command("M106 S%d", (int)m_extrude_fan_speed);
    }
    Printer::GetInstance()->m_gcode_io->single_command("M83");
    if (e < 1e-15)
    {
        //m_min_extrude_temp设置为0，表示不限制喷头温度
        Printer::GetInstance()->m_gcode_io->single_command("SET_MIN_EXTRUDE_TEMP S0");
        Printer::GetInstance()->m_gcode_io->single_command("G1 E%d F%d", (int)e, (int)f);
        //m_min_extrude_temp设置为默认值，表示限制喷头温度
        Printer::GetInstance()->m_gcode_io->single_command("SET_MIN_EXTRUDE_TEMP RESET");
    }
    else
    {
        Printer::GetInstance()->m_gcode_io->single_command("G1 E%d F%d", (int)e, (int)f);
        Printer::GetInstance()->m_gcode_io->single_command("M729");
    }
    Printer::GetInstance()->m_gcode_io->single_command("M82");
    Printer::GetInstance()->m_tool_head->wait_moves();
    if(fan)
    {
        Printer::GetInstance()->m_gcode_io->single_command("M106 S0");
    }
    Printer::GetInstance()->m_pause_resume->m_last_pause = 0;

    // if(get_feet_out_state() == true)
    // {
    //     m_busy = false;
    //     set_feet_out_state(false);
    // }

    if (report)
    {
        change_filament_state_callback_call(CHANGE_FILAMENT_STATE_EXTRUDE_FILAMENT);
    }
    LOG_D("cmd_EXTRUDE_FILAMENT end\n");
}

#define CHANGE_FILAMEN_STATE_CALLBACK_SIZE 16
static change_filament_state_callback_t change_filament_state_callback[CHANGE_FILAMEN_STATE_CALLBACK_SIZE];

int change_filament_register_state_callback(change_filament_state_callback_t state_callback)
{
    for (int i = 0; i < CHANGE_FILAMEN_STATE_CALLBACK_SIZE; i++)
    {
        if (change_filament_state_callback[i] == NULL)
        {
            change_filament_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}

int change_filament_unregister_state_callback(change_filament_state_callback_t state_callback)
{
    for (int i = 0; i < CHANGE_FILAMEN_STATE_CALLBACK_SIZE; i++)
    {
        if (change_filament_state_callback[i] == state_callback)
        {
            change_filament_state_callback[i] = NULL;
            return 0;
        }
    }
    return -1;
}

int change_filament_state_callback_call(int state)
{
    // printf("homing_state_callback_call state:%d\n", state);
    for (int i = 0; i < CHANGE_FILAMEN_STATE_CALLBACK_SIZE; i++)
    {
        if (change_filament_state_callback[i] != NULL)
        {
            change_filament_state_callback[i](state);
        }
    }
    return 0;
}
void ChangeFilament::set_feet_out_state(bool state)
{
    feed_out_state = state;
}
bool ChangeFilament::get_feet_out_state()
{
    return feed_out_state;
}
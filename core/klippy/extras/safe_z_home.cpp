#include "safe_z_home.h"
#include "klippy.h"
#include "Define.h"
#include "srv_state.h"
#include "simplebus.h"
#define LOG_TAG "safe_z_home"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_WARN
#include "log.h"

#define TEMPOFFSET (2)  // 温度偏差值，在归零前温度比较时允许有个温度偏差
SafeZHoming::SafeZHoming(std::string section_name)
{
    m_home_x_pos = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "home_x_position", 0.0f);
    m_home_y_pos = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "home_y_position", 0.0f);
    m_z_lift = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_lift", 10);
    m_z_hop = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_hop", 0.0);
    m_z_hop_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_hop_speed", 15., DBL_MIN, DBL_MAX, 0.);
    m_max_z = Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "position_max", DBL_MIN);
    m_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "speed", 50.0, DBL_MIN, DBL_MAX, 0.);
    m_move_to_previous = Printer::GetInstance()->m_pconfig->GetBool(section_name, "move_to_previous", false);
    temp = Printer::GetInstance()->m_pconfig->GetInt(section_name, "temp", 140);
    Printer::GetInstance()->load_object("homing");
    m_prev_G28 = Printer::GetInstance()->m_gcode->register_command("G28", nullptr);
    Printer::GetInstance()->m_gcode->register_command("G28", std::bind(&SafeZHoming::cmd_G28, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("CUSTOM_ZERO", std::bind(&SafeZHoming::cmd_CUSTOM_ZERO, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("Z_AXIS_OFF_LIMIT_ACTION", std::bind(&SafeZHoming::cmd_Z_AXIS_OFF_LIMIT_ACTION, this, std::placeholders::_1));
    if (Printer::GetInstance()->m_pconfig->GetSection("homing_override") != nullptr)
    {
        printf("config error: homing_override and safe_z_homing cannot be used simultaneously\n");
        // config.error("homing_override and safe_z_homing cannot be used simultaneously"); //---??---
    }
}

SafeZHoming::~SafeZHoming()
{
}
/**
 * @brief //使用safe_z_home之后，G28 X Y Z或G28或没归零X和Y就调用G28 Z，之后会调用两次home接口，分别归零1:(X Y),2:Z
 * 
 * @param gcmd 
 */
void SafeZHoming::cmd_G28(GCodeCommand &gcmd)
{
    print_stats_state_callback_call(PRINT_STATS_STATE_HOMING);
    bool need_x = gcmd.get_string("X", "") != "";
    bool need_y = gcmd.get_string("Y", "") != "";
    bool need_z = gcmd.get_string("Z", "") != "";
    std::vector<bool> need_axis = {need_x, need_y, need_z};
    for (uint8_t i = 0; i < 3; i++)
    {
        LOG_I("home_msg axis[%d]=%d", i, need_axis[i]);
        if (need_axis[i])
        {
            srv_state_home_msg_t home_msg;
            home_msg.axis = AXIS_X + i;
            home_msg.st = SRV_STATE_HOME_HOMING;
            simple_bus_publish_async("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));
        }
    }
    // Perform Z Hop if necessary
    LOG_D("m_z_hop:%f\n", m_z_hop);
    double extruder_curr_temp = Printer::GetInstance()->m_printer_extruder->get_heater()->get_status(get_monotonic()).smoothed_temp;
    double temp_tmp = -1.0f;
    if (extruder_curr_temp < temp - TEMPOFFSET)
    {
        // 暂存目标温度，温度设为配置文件温度，G28执行后恢复目标温度
        temp_tmp = Printer::GetInstance()->m_printer_extruder->get_heater()->get_status(get_monotonic()).target_temp;
        Printer::GetInstance()->m_gcode_io->single_command("M109 S%d", temp);
    }
    
    if (m_z_hop != 0.0)
    {
        // Check if Z axis is homed and its last known position
        double curtime = get_monotonic();
        std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(curtime);
        std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
        if (kin_status["homed_axes"].find("z") == std::string::npos)
        {
#if 1
            LOG_D("Z轴没有归位,先抬升z_hop\n");
            // Always perform the z_hop if the Z axis is not homed
            pos[2] = 0;
            std::vector<int> axes = {2};
            Printer::GetInstance()->m_tool_head->set_position(pos, axes);
            std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_hop};
            Printer::GetInstance()->m_tool_head->manual_move(temp_pos, m_z_hop_speed);
            // if (Printer::GetInstance()->m_tool_head->get_kinematics()->note_z_not_homed() != nullptr)
            // {
            Printer::GetInstance()->m_tool_head->m_kin->note_z_not_homed();
            // }
#else
            LOG_D("Z轴没有归位,先抬升z_hop:%f\n",m_z_hop);
            // Always perform the z_hop if the Z axis is not homed
            pos[2] = 0;
            std::vector<int> axes = {2};
            Printer::GetInstance()->m_tool_head->set_position(pos, axes);
            std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_hop}; 
            int step_cnt = (int)(m_max_z / (Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->m_steppers[0]->get_step_dist() * Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->m_step_base));
            int step_us = (int)(((m_max_z / m_z_hop_speed) * 1000 * 1000) / step_cnt);
            Printer::GetInstance()->m_hx711s->CalibrationStart(30, false); 
            Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStart(0, 0, 0); 
            Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->check_and_run(1, step_us, step_cnt, false, false);
            Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->delay_s(0.015);
            std::cout << "PROBE_BY_STEP x= " << 0 << " y= " << 0 << " z= " << 0 << " speed_mm= " << m_z_hop_speed << " step_us= " << step_us
                    << " step_cnt= " << step_cnt << std::endl;
            std::cout << "******************************probe_by_step****************************" << std::endl;
            int32_t loop=0; 
            while (Printer::GetInstance()->m_strain_gauge->ck_sys_sta())
            {
                loop++;
                if (Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->m_params.size() == 2)
                {
                    LOG_E("axis z wait home timeout \n"); 
                    break;
                }
                double running_dis = m_z_hop_speed*loop/100.f;
                if (loop%100==0){
                    std::cout << "running_dis: " << running_dis  << std::endl;
                }
                if (running_dis > m_z_hop ){
                    std::cout << "回退结束。。。。。。。。" << std::endl;
                    break;
                }
                if (Printer::GetInstance()->m_hx711s->m_is_trigger > 0)
                {
                    std::cout << "回退中 probe_by_step trigger!!!" << std::endl;
                    break;;
                }
                usleep(10 * 1000);
            }
            Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true);
            Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->check_and_run(0, 0, 0, false);
            Printer::GetInstance()->m_tool_head->m_kin->note_z_not_homed();
#endif
        }
        else if (pos[2] < m_z_hop)
        {
            LOG_D("Z轴已归位,但是当前坐标不等于z_hop\n");
            // If the Z axis is homed, and below z_hop, lift it to z_hop
            std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_hop};
            Printer::GetInstance()->m_tool_head->manual_move(temp_pos, m_z_hop_speed);
        }
    }
    else
    {
        std::vector<MCU_endstop *> endstops;
        for (int i = 0; i < Printer::GetInstance()->m_tool_head->m_kin->m_rails.size(); i++)
        {
            std::vector<MCU_endstop *> ret_endstops = Printer::GetInstance()->m_tool_head->m_kin->m_rails[i]->get_endstops();
            for (int j = 0; j < ret_endstops.size(); j++)
            {
                endstops.push_back(ret_endstops[j]);
            }
        }
        homingInfo hi = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_homing_info();
        HomingMove hzmove(endstops);
        std::stringstream endstop_query_state_str;
        endstop_query_state_str << "endstop_query_state id=" << hzmove.m_endstops[2]->m_oid;
        ParseResult params = Printer::GetInstance()->m_mcu->m_serial->send_with_response(endstop_query_state_str.str(), "endstop_state", hzmove.m_endstops[2]->m_trsync->m_cmd_queue, hzmove.m_endstops[2]->m_oid);
        if (params.PT_uint32_outs["pin_value"])
        {
            std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
            std::vector<int> axes = {2};
            Printer::GetInstance()->m_tool_head->set_position(pos, axes);
            std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_lift + pos[2]};
            Printer::GetInstance()->m_tool_head->manual_move(temp_pos, m_z_hop_speed);
        }
    }
    // Determine which axes we need to home
    if (!need_x && !need_y && !need_z)
    {
        need_x = need_y = need_z = true;
    }
    // Home XY axes if necessary
    std::map<std::string, std::string> new_params;
    if (need_x)
        new_params["X"] = "0";
    if (need_y)
        new_params["Y"] = "0";
    if (new_params.size() > 0)
    {
        GCodeCommand g28_gcmd = Printer::GetInstance()->m_gcode->create_gcode_command("G28", "G28", new_params);
        m_prev_G28(g28_gcmd);
    }
    // Home Z axis if necessary
    if (need_z)
    {
        // Throw an error if X or Y are not homed
        double curtime = get_monotonic();
        std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(curtime);
        if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos)
        {
            LOG_D("X轴或Y轴没有归位,先归X和Y\n");
            // printf("Must home X and Y axes first\n");
            new_params.clear();
            new_params["X"] = "0";
            new_params["Y"] = "0";
            GCodeCommand g28_gcmd = Printer::GetInstance()->m_gcode->create_gcode_command("G28", "G28", new_params);
            m_prev_G28(g28_gcmd);
            // raise gcmd.error("Must home X and Y axes first") //---??---
        }

        // Move to safe XY homing position
        std::vector<double> prevpos = Printer::GetInstance()->m_tool_head->get_position();
        std::vector<double> xy_temp_pos = {m_home_x_pos, m_home_y_pos, DBL_MIN};
        Printer::GetInstance()->m_tool_head->manual_move(xy_temp_pos, m_speed);
        // Home Z
        std::map<std::string, std::string> z_map = {{"Z", "0"}};
        GCodeCommand g28_gcmd = Printer::GetInstance()->m_gcode->create_gcode_command("G28", "G28", z_map);
        m_prev_G28(g28_gcmd);
        // Perform Z Hop again for pressure-based probes
        std::vector<double> z_temp_pos = {DBL_MIN, DBL_MIN, m_z_hop};
        if (m_z_hop)
            Printer::GetInstance()->m_tool_head->manual_move(z_temp_pos, m_z_hop_speed);
        // Move XY back to previous positions
        if (m_move_to_previous)
        {
            std::vector<double> prev_pos = {prevpos[0], prevpos[1], DBL_MIN};
            Printer::GetInstance()->m_tool_head->manual_move(prev_pos, m_speed);
        }
    }

    // G28完成后恢复暂存的目标温度
    if (!(temp_tmp < 0))
    {
        Printer::GetInstance()->m_gcode_io->single_command("M109 S%d", temp_tmp);
    }
    Printer::GetInstance()->m_printer_extruder->unregister_fan_callback();
    print_stats_state_callback_call(PRINT_STATS_STATE_HOMING_COMPLETED);
}
/**
 * @brief //自定义的G28命令
 * 
 * @param gcmd 
 */
void SafeZHoming::cmd_CUSTOM_ZERO(GCodeCommand &gcmd)
{
#define POS_X 202.0
#define POS_Y 264.5
#define HOMING_SPEED 80
    print_stats_state_callback_call(PRINT_STATS_STATE_HOMING);
    // Perform Z Hop if necessary
    LOG_D("m_z_hop:%f\n", m_z_hop);
    double extruder_curr_temp = Printer::GetInstance()->m_printer_extruder->get_heater()->get_status(get_monotonic()).smoothed_temp;
    double temp_tmp = -1.0f;
    if (extruder_curr_temp < temp - TEMPOFFSET)
    {
        // 暂存目标温度，温度设为配置文件温度，G28执行后恢复目标温度
        temp_tmp = Printer::GetInstance()->m_printer_extruder->get_heater()->get_status(get_monotonic()).target_temp;
        Printer::GetInstance()->m_gcode_io->single_command("M109 S%d", temp);
    }
    
    if (m_z_hop != 0.0)
    {
        // Check if Z axis is homed and its last known position
        double curtime = get_monotonic();
        std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(curtime);
        std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
        if (kin_status["homed_axes"].find("z") == std::string::npos)
        {
#if 1
            LOG_D("Z轴没有归位,先抬升z_hop\n");
            // Always perform the z_hop if the Z axis is not homed
            pos[2] = 0;
            std::vector<int> axes = {2};
            Printer::GetInstance()->m_tool_head->set_position(pos, axes);
            std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_hop};
            Printer::GetInstance()->m_tool_head->manual_move(temp_pos, m_z_hop_speed);
            // if (Printer::GetInstance()->m_tool_head->get_kinematics()->note_z_not_homed() != nullptr)
            // {
            Printer::GetInstance()->m_tool_head->m_kin->note_z_not_homed();
            // }
#else
            LOG_D("Z轴没有归位,先抬升z_hop:%f\n",m_z_hop);
            // Always perform the z_hop if the Z axis is not homed
            pos[2] = 0;
            std::vector<int> axes = {2};
            Printer::GetInstance()->m_tool_head->set_position(pos, axes);
            std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_hop}; 
            int step_cnt = (int)(m_max_z / (Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->m_steppers[0]->get_step_dist() * Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->m_step_base));
            int step_us = (int)(((m_max_z / m_z_hop_speed) * 1000 * 1000) / step_cnt);
            Printer::GetInstance()->m_hx711s->CalibrationStart(30, false); 
            Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStart(0, 0, 0); 
            Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->check_and_run(1, step_us, step_cnt, false, false);
            Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->delay_s(0.015);
            std::cout << "PROBE_BY_STEP x= " << 0 << " y= " << 0 << " z= " << 0 << " speed_mm= " << m_z_hop_speed << " step_us= " << step_us
                    << " step_cnt= " << step_cnt << std::endl;
            std::cout << "******************************probe_by_step****************************" << std::endl;
            int32_t loop=0; 
            while (Printer::GetInstance()->m_strain_gauge->ck_sys_sta())
            {
                loop++;
                if (Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->m_params.size() == 2)
                {
                    LOG_E("axis z wait home timeout \n"); 
                    break;
                }
                double running_dis = m_z_hop_speed*loop/100.f;
                if (loop%100==0){
                    std::cout << "running_dis: " << running_dis  << std::endl;
                }
                if (running_dis > m_z_hop ){
                    std::cout << "回退结束。。。。。。。。" << std::endl;
                    break;
                }
                if (Printer::GetInstance()->m_hx711s->m_is_trigger > 0)
                {
                    std::cout << "回退中 probe_by_step trigger!!!" << std::endl;
                    break;;
                }
                usleep(10 * 1000);
            }
            Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true);
            Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->check_and_run(0, 0, 0, false);
            Printer::GetInstance()->m_tool_head->m_kin->note_z_not_homed();
#endif
        }
        else if (pos[2] < m_z_hop)
        {
            LOG_D("Z轴已归位,但是当前坐标不等于z_hop\n");
            // If the Z axis is homed, and below z_hop, lift it to z_hop
            std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_hop};
            Printer::GetInstance()->m_tool_head->manual_move(temp_pos, m_z_hop_speed);
        }
    }
    // Determine which axes we need to home
    bool need_x = gcmd.get_string("X", "") != "";
    bool need_y = gcmd.get_string("Y", "") != "";
    bool need_z = gcmd.get_string("Z", "") != "";
    if (!need_x && !need_y && !need_z)
    {
        need_x = need_y = need_z = true;
    }
    // Home XY axes if necessary
    std::map<std::string, std::string> new_params;
    if (need_x)
        new_params["X"] = "0";
    if (need_y)
        new_params["Y"] = "0";
    if (new_params.size() > 0)
    {
        GCodeCommand g28_gcmd = Printer::GetInstance()->m_gcode->create_gcode_command("G28", "G28", new_params);
        m_prev_G28(g28_gcmd);
    }
    // Home Z axis if necessary
    if (need_z)
    {
        // Throw an error if X or Y are not homed
        double curtime = get_monotonic();
        std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(curtime);
        if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos)
        {
            LOG_D("X轴或Y轴没有归位,先归X和Y\n");
            // printf("Must home X and Y axes first\n");
            new_params.clear();
            new_params["X"] = "0";
            new_params["Y"] = "0";
            GCodeCommand g28_gcmd = Printer::GetInstance()->m_gcode->create_gcode_command("G28", "G28", new_params);
            m_prev_G28(g28_gcmd);
            // raise gcmd.error("Must home X and Y axes first") //---??---
        }

        // Move to safe XY homing position
        std::vector<double> prevpos = Printer::GetInstance()->m_tool_head->get_position();
        // std::vector<double> xy_temp_pos = {m_home_x_pos, m_home_y_pos, DBL_MIN};
        // Printer::GetInstance()->m_tool_head->manual_move(xy_temp_pos, m_speed);
        std::vector<double> x_temp_pos = {POS_X, DBL_MIN, DBL_MIN};
        Printer::GetInstance()->m_tool_head->manual_move(x_temp_pos, HOMING_SPEED);
        std::vector<double> y_temp_pos = {DBL_MIN, POS_Y, DBL_MIN};
        Printer::GetInstance()->m_tool_head->manual_move(y_temp_pos, HOMING_SPEED);
        // Home Z
        std::map<std::string, std::string> z_map = {{"Z", "0"}};
        GCodeCommand g28_gcmd = Printer::GetInstance()->m_gcode->create_gcode_command("G28", "G28", z_map);
        m_prev_G28(g28_gcmd);
        // Perform Z Hop again for pressure-based probes
        std::vector<double> z_temp_pos = {DBL_MIN, DBL_MIN, m_z_hop};
        if (m_z_hop)
            Printer::GetInstance()->m_tool_head->manual_move(z_temp_pos, m_z_hop_speed);
        // Move XY back to previous positions
        if (m_move_to_previous)
        {
            std::vector<double> prev_pos = {prevpos[0], prevpos[1], DBL_MIN};
            Printer::GetInstance()->m_tool_head->manual_move(prev_pos, HOMING_SPEED);
        }
    }

    // G28完成后恢复暂存的目标温度
    if (!(temp_tmp < 0))
    {
        Printer::GetInstance()->m_gcode_io->single_command("M109 S%d", temp_tmp);
    }
    Printer::GetInstance()->m_printer_extruder->unregister_fan_callback();
    print_stats_state_callback_call(PRINT_STATS_STATE_HOMING_COMPLETED);
}

/**
 * @brief 检测z轴限位并在触发限位状态下离开限位位置
 * 
 * @param gcmd 
 */
void SafeZHoming::cmd_Z_AXIS_OFF_LIMIT_ACTION(GCodeCommand &gcmd)
{
    std::vector<MCU_endstop *> endstops;
    for (int i = 0; i < Printer::GetInstance()->m_tool_head->m_kin->m_rails.size(); i++)
    {
        std::vector<MCU_endstop *> ret_endstops = Printer::GetInstance()->m_tool_head->m_kin->m_rails[i]->get_endstops();
        for (int j = 0; j < ret_endstops.size(); j++)
        {
            endstops.push_back(ret_endstops[j]);
        }
    }
    homingInfo hi = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_homing_info();
    HomingMove hzmove(endstops);
    std::stringstream endstop_query_state_str;
    endstop_query_state_str << "endstop_query_state id=" << hzmove.m_endstops[2]->m_oid;
    ParseResult params = Printer::GetInstance()->m_mcu->m_serial->send_with_response(endstop_query_state_str.str(), "endstop_state", hzmove.m_endstops[2]->m_trsync->m_cmd_queue, hzmove.m_endstops[2]->m_oid);
    if (params.PT_uint32_outs["pin_value"])
    {
        std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
        std::vector<int> axes = {2};
        Printer::GetInstance()->m_tool_head->set_position(pos, axes);
        std::vector<double> temp_pos = {DBL_MIN, DBL_MIN, m_z_lift + pos[2]};
        Printer::GetInstance()->m_tool_head->manual_move(temp_pos, m_z_hop_speed);
    }
}
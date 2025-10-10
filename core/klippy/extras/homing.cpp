#include "homing.h"
#include "klippy.h"
#include "Define.h"
#include "configfile.h"
#include "hl_assert.h"
#include "hl_tpool.h"
#include "simplebus.h"
#include "srv_state.h"

#include "config.h"

#define LOG_TAG "homing"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
#define homing_min(a, b) (((a) < (b)) ? (a) : (b))
#define HOMING_STATE_CALLBACK_SIZE 16
#define HOMING_Z_CONTROL_FAN 0
static homing_state_callback_t homing_state_callback[HOMING_STATE_CALLBACK_SIZE];
// Return a completion that completes when all completions in a list complete
// void multi_complete(printer, completions)
// {
//     if len(completions) == 1:
//         return completions[0]
//     # Build completion that waits for all completions
//     reactor = printer.get_reactor()
//     cp = reactor.register_callback(lambda e: [c.wait() for c in completions])
//     # If any completion indicates an error, then exit main completion early
//     for c in completions:
//         reactor.register_callback(lambda e: cp.complete(1) if c.wait() else 0)
//     return cp
// }  //---??---

static hl_tpool_thread_t wait_z_home_thread;
static void wait_z_home_process(hl_tpool_thread_t thread, void *args)
{
    LOG_I("wait_z_home_process start at: %f\n",get_monotonic());
    Printer::GetInstance()->m_strain_gauge->wait_home();
}

Homing::Homing()
{
}
Homing::~Homing()
{
}

void Homing::set_axes(std::vector<int> axes)
{
    m_changed_axes = axes;
}

std::vector<int> Homing::get_axes()
{
    return m_changed_axes;
}
std::map<std::string, double> Homing::get_stepper_trigger_positions()
{
    return m_kin_spos;
}
void Homing::set_homed_position(std::vector<double> pos)
{
    // Printer::GetInstance()->m_tool_head->set_position(fill_coord(pos));
}

std::vector<double> Homing::fill_coord(std::vector<double> coord)
{
    std::vector<double> thcoord = Printer::GetInstance()->m_tool_head->m_commanded_pos;
    if (coord[0] != DO_NOT_MOVE_F)
        thcoord[0] = coord[0];
    if (coord[1] != DO_NOT_MOVE_F)
        thcoord[1] = coord[1];
    if (coord[2] != DO_NOT_MOVE_F)
        thcoord[2] = coord[2];
    if (coord[3] != DO_NOT_MOVE_F)
        thcoord[3] = coord[3];
    return thcoord;
}
/**
 * @brief 个人理解，不一定对。该函数会把当前位置设置为最大行程，然后往回走，走的目标位置是当前轴的position_endstop，直到找到限位。
 *
 * @param rails 轴列表，目前看起来调用时候就是一个轴，只有一项（2023/5/9）
 * @param forcepos 最大行程
 * @param movepos 归零成功后的位置
 * @return true
 * @return false
 */
bool Homing::home_rails(std::vector<PrinterRail *> rails, std::vector<double> forcepos, std::vector<double> movepos, int axis) // 归零操作
{
    double in_homing_print_fan_speed = 0.0f; /*--hao--*/

    // Notify of upcoming homing operation
    Printer::GetInstance()->send_event("homing:home_rails_begin", this, rails);
    // Alter kinematics class to think printer is at forcepos
    std::vector<int> homing_axes;
    for (int i = 0; i < 3; i++)
    {
        if (forcepos[i] != DO_NOT_MOVE_F)
        {
            homing_axes.push_back(i);
        }
    }
    forcepos = fill_coord(forcepos);
    movepos = fill_coord(movepos);

    LOG_D("forcepos: %f %f %f %f\n", forcepos[0], forcepos[1], forcepos[2], forcepos[3]);
    LOG_D("movepos: %f %f %f %f\n", movepos[0], movepos[1], movepos[2], movepos[3]);

    Printer::GetInstance()->m_tool_head->set_position(forcepos, homing_axes);
    std::vector<MCU_endstop *> endstops;
    for (int i = 0; i < rails.size(); i++)
    {
        std::vector<MCU_endstop *> ret_endstops = rails[i]->get_endstops();
        for (int j = 0; j < ret_endstops.size(); j++)
        {
            endstops.push_back(ret_endstops[j]);
        }
    }

    srv_state_home_msg_t home_msg;
    home_msg.axis = axis;
    home_msg.st = SRV_STATE_HOME_HOMING;
    simple_bus_publish_async("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));

    homingInfo hi = rails[0]->get_homing_info();
    HomingMove hmove(endstops);
    double accel = Printer::GetInstance()->m_tool_head->get_max_velocity()[1];
    if (hi.homing_accel != DBL_MIN)
    {
        Printer::GetInstance()->m_gcode_io->single_command("M204 S%.2f", hi.homing_accel);
    }
    if (hi.homing_current != DBL_MIN)
    {
        std::string cmd = "SET_TMC_CURRENT_" + rails[0]->get_steppers()[0]->get_name();
        cmd += " CURRENT=" + to_string(hi.homing_current);
        Printer::GetInstance()->m_gcode_io->single_command(cmd.c_str());
    }
#if HOMING_Z_CONTROL_FAN
    if (std::find(homing_axes.begin(), homing_axes.end(), 2) != homing_axes.end()) /*--hao--Z轴向下归零，关闭风扇*/
    {
        in_homing_print_fan_speed = Printer::GetInstance()->m_printer_fan->get_status(0).speed;
        Printer::GetInstance()->m_gcode_io->single_command("M106 S0");
    }
#endif

    homing_state_callback_call(HOMING_STATE_SEED_LIMIT); // 回调
    hmove.homing_move(movepos, hi.speed);                // 找限位

    if (fabs(hi.retract_dist) > 1e-15)
    {
        std::vector<double> axes_d = {movepos[0] - forcepos[0], movepos[1] - forcepos[1], movepos[2] - forcepos[2], movepos[3] - forcepos[3]};
        double move_d = sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
        double retract_r = std::min(1.0, hi.retract_dist / move_d);
        std::vector<double> retractpos = {movepos[0] - axes_d[0] * retract_r, movepos[1] - axes_d[1] * retract_r, movepos[2] - axes_d[2] * retract_r, movepos[3] - axes_d[3] * retract_r};
        Printer::GetInstance()->m_tool_head->move(retractpos, hi.retract_speed); // 回退
#if HOMING_Z_CONTROL_FAN
        if (std::find(homing_axes.begin(), homing_axes.end(), 2) != homing_axes.end()) /*--hao--Z轴回退，开启风扇*/
        {
            Printer::GetInstance()->m_gcode_io->single_command("M106 S255");
        }
#endif
        homing_state_callback_call(HOMING_STATE_OUT_LIMIT); // 回调
        Printer::GetInstance()->m_tool_head->wait_moves();  // 等待回退完成

        std::vector<double> forcepos = {retractpos[0] - axes_d[0] * retract_r, retractpos[1] - axes_d[1] * retract_r, retractpos[2] - axes_d[2] * retract_r, retractpos[3] - axes_d[3] * retract_r};
        Printer::GetInstance()->m_tool_head->set_position(forcepos);
        HomingMove homemove(endstops);
#if HOMING_Z_CONTROL_FAN
        if (std::find(homing_axes.begin(), homing_axes.end(), 2) != homing_axes.end()) /*--hao--*/
        {
            Printer::GetInstance()->m_gcode_io->single_command("M106 S0");
        }
#endif
        homing_state_callback_call(HOMING_STATE_SEED_LIMIT); // 再次找限位
        hmove.homing_move(movepos, hi.second_homing_speed);  // 再次找限位
        if (homemove.check_no_movement() != "")
        {
            // raise self.printer.command_error( "Endstop %s still triggered after retract" % (hmove.check_no_movement(),))
        }
    }
    if (fabs(hi.force_retract) > 1e-15)
    {
        std::vector<double> axes_d = {movepos[0] - forcepos[0], movepos[1] - forcepos[1], movepos[2] - forcepos[2], movepos[3] - forcepos[3]};
        double move_d = sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
        double retract_r = std::min(1.0, hi.force_retract / move_d);
        std::vector<double> retractpos = {movepos[0] - axes_d[0] * retract_r, movepos[1] - axes_d[1] * retract_r, movepos[2] - axes_d[2] * retract_r, movepos[3] - axes_d[3] * retract_r};
        Printer::GetInstance()->m_tool_head->move(retractpos, hi.retract_speed); // 回退
        // homing_state_callback_call(HOMING_STATE_OUT_LIMIT); // 回调
        Printer::GetInstance()->m_tool_head->wait_moves(); // 等待回退完成
        Printer::GetInstance()->m_tool_head->set_position(retractpos);
    }

    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::map<std::string, double> kin_spos;
    std::vector<std::vector<MCU_stepper *>> kin_steppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
    for (int i = 0; i < kin_steppers.size(); i++)
    {
        for (int j = 0; j < kin_steppers[i].size(); j++)
        {
            kin_spos[kin_steppers[i][j]->get_name()] = kin_steppers[i][j]->get_commanded_position();
        }
    }
    m_kin_spos = kin_spos;
    Printer::GetInstance()->send_event("homing:home_rails_end", this, rails);
    if (kin_spos != m_kin_spos)
    {
        // Apply any homing offsets
        std::vector<double> adjustpos = Printer::GetInstance()->m_tool_head->m_kin->calc_position(m_kin_spos);
        for (int axis = 0; axis < homing_axes.size(); axis++)
        {
            movepos[axis] = adjustpos[axis];
        }
        Printer::GetInstance()->m_tool_head->set_position(movepos);
    }
    if (fabs(hi.homing_accel) != DBL_MIN)
    {
        Printer::GetInstance()->m_gcode_io->single_command("M204 S%.2f", accel);
    }
    if (hi.homing_current != DBL_MIN)
    {
        std::string cmd = "SET_TMC_CURRENT_" + rails[0]->get_steppers()[0]->get_name();
        Printer::GetInstance()->m_gcode_io->single_command(cmd.c_str());

    }
#if HOMING_Z_CONTROL_FAN
    if (std::find(homing_axes.begin(), homing_axes.end(), 2) != homing_axes.end()) /*--hao--*/ /*--hao--Z轴归零，恢复风扇状态*/
    {
        Printer::GetInstance()->m_gcode_io->single_command("M106 S%.0f", homing_min(in_homing_print_fan_speed * 255.0, 255.0));
    }
#endif
    home_msg.axis = axis;
    if (hmove.is_succ)
        home_msg.st = SRV_STATE_HOME_END_SUCCESS;
    else
    {
        home_msg.st = SRV_STATE_HOME_END_FAILED;
        if (home_msg.axis == AXIS_Z)
        {
            std::string msg = "Z-axis zeroing failed";
            Printer::GetInstance()->invoke_shutdown(msg);
            LOG_I("Z-axis zeroing failed mcu shutdown\n");
        }
    }
    simple_bus_publish_async("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));

    return hmove.is_succ;
}

HomingMove::HomingMove(std::vector<MCU_endstop *> endstops)
{
    // HomingLOG();
    m_endstops = endstops;
    // std-----::cout << "m_endstops[0]->m_oid 130 " << m_endstops[0]->m_oid << std::endl;
    // std-----::cout << "m_endstops[0]->m_invert 130 " << m_endstops[0]->m_invert << std::endl;
    // HomingLOG();
}

HomingMove::~HomingMove()
{
}

std::vector<MCU_endstop *> HomingMove::get_mcu_endstops()
{
    return m_endstops;
}

double HomingMove::calc_endstop_rate(MCU_endstop *mcu_endstop, std::vector<double> movepos, double speed)
{

    std::vector<double> startpos = Printer::GetInstance()->m_tool_head->m_commanded_pos;
    // std-----::cout << "movepos 150 " << movepos[0] << " " << movepos[1] << " " << movepos[2] << " " << movepos[3] << std::endl;
    // std-----::cout << "startpos 150 " << startpos[0] << " " << startpos[1] << " " << startpos[2] << " " << startpos[3] << std::endl;
    // std-----::cout << "speed 152 " << speed << std::endl;
    std::vector<double> axes_d = {movepos[0] - startpos[0], movepos[1] - startpos[1], movepos[2] - startpos[2], movepos[3] - startpos[3]};
    double move_d = std::sqrt((axes_d[0] * axes_d[0]) + (axes_d[1] * axes_d[1]) + (axes_d[2] * axes_d[2]));
    double move_t = move_d / speed;
    // std-----::cout << "move_d 157 " << move_d << std::endl;
    // std-----::cout << "move_t 157 " << move_t << std::endl;

    double max_steps = 0;
    for (int i = 0; i < mcu_endstop->m_trsync->m_steppers.size(); i++)
    {
        double start_pos = mcu_endstop->m_trsync->m_steppers[i]->calc_position_from_coord(startpos);
        // std-----::cout << "start_pos 164 " << start_pos << std::endl;
        double move_pos = mcu_endstop->m_trsync->m_steppers[i]->calc_position_from_coord(movepos);
        // std-----::cout << "move_pos 164 " << move_pos << std::endl;
        double step_dist = mcu_endstop->m_trsync->m_steppers[i]->m_step_dist;
        // std-----::cout << "step_dist 164 " << step_dist << std::endl;
        double temp_max_steps = (start_pos - move_pos) / step_dist;
        // std-----::cout << "temp_max_steps 164 " << temp_max_steps << std::endl;
        max_steps = std::max(temp_max_steps, max_steps);
    }
    // std-----::cout << "max_steps 169 " << max_steps << std::endl;
    if (max_steps <= 0)
    {
        return 0.001;
    }
    return move_t / max_steps;
}

double HomingMove::homing_z_move(std::vector<double> movepos, double homing_speed, bool probe_pos, bool triggered, bool check_triggered)
{
    double in_homing_print_fan_speed = 0.0f;
    Printer::GetInstance()->send_event("homing:homing_move_begin", this);
    is_succ = true;
    // Note start location
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::map<std::string, double> kin_spos;
    std::map<std::string, MCU_stepper *> steppers;
    std::vector<std::vector<MCU_stepper *>> kin_stepppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
    for (int i = 0; i < kin_stepppers.size(); i++)
    {
        for (int j = 0; j < kin_stepppers[i].size(); j++)
        {
            kin_spos[kin_stepppers[i][j]->get_name()] = kin_stepppers[i][j]->get_commanded_position();
        }
    }
    for (int i = 0; i < m_endstops.size(); i++)
    {
        for (int j = 0; j < m_endstops[i]->get_steppers().size(); j++)
        {
            steppers[m_endstops[i]->get_steppers()[j]->get_name()] = m_endstops[i]->get_steppers()[j];
            m_start_mcu_pos[m_endstops[i]->get_steppers()[j]->get_name()] = m_endstops[i]->get_steppers()[j]->get_mcu_position();
        }
    }
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (int i = 0; i < m_endstops.size(); i++)
    {
        double rest_time = calc_endstop_rate(m_endstops[i], movepos, homing_speed);
        m_endstops[i]->home_start_z(print_time, ENDSTOP_SAMPLE_TIME, ENDSTOP_SAMPLE_COUNT, rest_time);
        // Printer::GetInstance()->m_hx711s->read_base(40 / 2, 6000);
        // Printer::GetInstance()->m_hx711s->query_start(32 * 2, 65535, true, false, true);
        //TODO: 坐标值
        Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStart(movepos[0],movepos[1],movepos[2]);
    }
    // if (probe_pos == true) /*--hao--*/
    // {
    //     in_homing_print_fan_speed = Printer::GetInstance()->m_printer_fan->get_status(0).speed;
    //     Printer::GetInstance()->m_gcode_io->single_command("M106 S0");
    // }

    int all_endstop_trigger = 0; //---??---
    Printer::GetInstance()->m_tool_head->dwell(HOMING_START_DELAY);
    // std::cout << "hl_tpool_create_thread start " << get_monotonic() << std::endl; 
    HL_ASSERT(hl_tpool_create_thread(&wait_z_home_thread, wait_z_home_process, NULL, 0, 0, 0, 0) == 0);
    // std::cout << "hl_tpool_create_thread done " << get_monotonic() << std::endl;
    // double start_time = get_monotonic();
    Printer::GetInstance()->m_tool_head->drip_move(movepos, homing_speed, all_endstop_trigger); // 归0运动命令
    // std::cout << "movepos: " << movepos[0] << " " << movepos[1] << " " << movepos[2] << std::endl;
    // double end_time = get_monotonic();
    double move_end_print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (int i = 0; i < m_endstops.size(); i++)
    {
        bool did_trigger = m_endstops[i]->home_wait_z(move_end_print_time);
        if (!did_trigger && check_triggered)
        {
            is_succ = false;
            // 输出错误信息
            LOG_E("Failed to home did_trigger:%d  check_triggered:%d is_succ:%d\n", did_trigger, check_triggered, is_succ);
        }
    }
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::map<std::string, MCU_stepper *>::iterator it = steppers.begin();
    while (it != steppers.end())
    {
        m_end_mcu_pos[it->first] = steppers[it->first]->get_mcu_position();
        it++;
    }
    double move_z = 0;
    if (probe_pos)
    {
        double start_z = 0;
        double end_z = 0;
        std::vector<double> now_pos1 = Printer::GetInstance()->m_tool_head->get_position();
        // std::cout << "now_pos1: " << now_pos1[0] << " " << now_pos1[1] << " " << now_pos1[2] << std::endl;
        std::map<std::string, int64_t>::iterator iter;
        for (iter = m_end_mcu_pos.begin(); iter != m_end_mcu_pos.end(); iter++)
        {
            std::string sname = iter->first;
            if (kin_spos.find(sname) != kin_spos.end())
            {
                // kin_spos[sname] += (m_end_mcu_pos[sname] - m_start_mcu_pos[sname]) * steppers[sname]->get_step_dist();
                start_z = m_start_mcu_pos[sname] * steppers[sname]->get_step_dist();
                end_z = m_end_mcu_pos[sname] * steppers[sname]->get_step_dist();
            }
        }
        // std::vector<double> calc_pos_ret = Printer::GetInstance()->m_tool_head->m_kin->calc_position(kin_spos);
        double start_time = Printer::GetInstance()->m_tool_head->m_start_z_time;
        double end_time = Printer::GetInstance()->m_tool_head->m_end_z_time;
        double delta_compensation_z = Printer::GetInstance()->m_strain_gauge->cal_z(start_z, start_time, end_z, end_time);
        // std::cout << "calc_pos_ret: " << calc_pos_ret[2] << std::endl;
        // movepos = {calc_pos_ret[0], calc_pos_ret[1], calc_pos_ret[2], movepos[3]};
        // std::cout << "start_z: " << start_z << std::endl;
        // std::cout << "end_z: " << end_z << std::endl;
        // movepos[2] += (now_pos1[2] - end_z - start_z);
        // movepos[2] += z;

        double ori_end_z = end_z;
        if (Printer::GetInstance()->m_strain_gauge->m_cfg->m_enable_ts_compense) {
            end_z = end_z - delta_compensation_z;
            // 时间戳补偿值，用于计算z轴实际触发坐标 
            LOG_I("enable timestamp compensation!!!!!!! ori-z:%f compensation-z:%f delta_compensation_z:%f\n", ori_end_z,end_z,delta_compensation_z); 
        }else { 
            LOG_I("disable timestamp compensation!!!!!!! ori-z:%f compensation-z:%f delta_compensation_z:%f\n", ori_end_z,end_z,delta_compensation_z); 
        }

        move_z = end_z - start_z;
        LOG_I("homing_z_move move_z:%f",move_z);
        // movepos[2] = now_pos1[2] - move_z;
    }
    Printer::GetInstance()->m_tool_head->set_position(movepos);
    // std::cout << "set_position movepos: " << movepos[0] << " " << movepos[1] << " " << movepos[2] << " " << movepos[3] << std::endl;
    Printer::GetInstance()->send_event("homing:homing_move_end", this);
    // std::cout << "is_succ: " << is_succ << std::endl;
    if (is_succ == false)
    {
        Printer::GetInstance()->m_tool_head->m_kin->motor_off(0); // 标记归零失败
    }
    return move_z;
}

std::vector<double> HomingMove::G29_z_move(std::vector<double> movepos, double homing_speed, bool probe_pos, bool triggered, bool check_triggered)
{
    movepos[2] = -20;
    double in_homing_print_fan_speed = 0.0f;
    Printer::GetInstance()->send_event("homing:homing_move_begin", this);
    is_succ = true;
    // Note start location
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::map<std::string, double> kin_spos;
    std::map<std::string, MCU_stepper *> steppers;
    std::vector<std::vector<MCU_stepper *>> kin_stepppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
    for (int i = 0; i < kin_stepppers.size(); i++)
    {
        for (int j = 0; j < kin_stepppers[i].size(); j++)
        {
            kin_spos[kin_stepppers[i][j]->get_name()] = kin_stepppers[i][j]->get_commanded_position();
        }
    }
    for (int i = 0; i < m_endstops.size(); i++)
    {
        for (int j = 0; j < m_endstops[i]->get_steppers().size(); j++)
        {
            steppers[m_endstops[i]->get_steppers()[j]->get_name()] = m_endstops[i]->get_steppers()[j];
            m_start_mcu_pos[m_endstops[i]->get_steppers()[j]->get_name()] = m_endstops[i]->get_steppers()[j]->get_mcu_position();
        }
    }
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (int i = 0; i < m_endstops.size(); i++)
    {
        double rest_time = calc_endstop_rate(m_endstops[i], movepos, homing_speed);
        m_endstops[i]->home_start_z(print_time, ENDSTOP_SAMPLE_TIME, ENDSTOP_SAMPLE_COUNT, rest_time);
        // Printer::GetInstance()->m_hx711s->read_base(40 / 2, 6000);
        // Printer::GetInstance()->m_hx711s->query_start(32 * 2, 65535, true, false, true); 
        Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStart(movepos[0],movepos[1],movepos[2]);
    }

    int all_endstop_trigger = 0; //---??---
    Printer::GetInstance()->m_tool_head->dwell(HOMING_START_DELAY); 
    HL_ASSERT(hl_tpool_create_thread(&wait_z_home_thread, wait_z_home_process, NULL, 0, 0, 0, 0) == 0);
    Printer::GetInstance()->m_tool_head->drip_move(movepos, homing_speed, all_endstop_trigger); // 归0运动命令
    double move_end_print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (int i = 0; i < m_endstops.size(); i++)
    {
        bool did_trigger = m_endstops[i]->home_wait_z(move_end_print_time);
        if (!did_trigger && check_triggered)
        {
            is_succ = false;
            // 输出错误信息
            LOG_E("Failed to home did_trigger:%d  check_triggered:%d is_succ:%d\n", did_trigger, check_triggered, is_succ);
        }
    }

    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::map<std::string, MCU_stepper *>::iterator it = steppers.begin();
    while (it != steppers.end())
    {
        m_end_mcu_pos[it->first] = steppers[it->first]->get_mcu_position();
        it++;
    }
    if (probe_pos)
    {
        double start_z = 0;
        double end_z = 0;
        std::map<std::string, int64_t>::iterator iter;
        for (iter = m_end_mcu_pos.begin(); iter != m_end_mcu_pos.end(); iter++)
        {
            std::string sname = iter->first;
            if (kin_spos.find(sname) != kin_spos.end())
            {
                kin_spos[sname] += (m_end_mcu_pos[sname] - m_start_mcu_pos[sname]) * steppers[sname]->get_step_dist();
                start_z = m_start_mcu_pos[sname] * steppers[sname]->get_step_dist();
                end_z = m_end_mcu_pos[sname] * steppers[sname]->get_step_dist();
            }
        }
        // 绝对坐标转换？
        std::vector<double> calc_pos_ret = Printer::GetInstance()->m_tool_head->m_kin->calc_position(kin_spos);

        // g29 end_z
        std::cout << "---------g29---- start_z: " << start_z << " end_z: " << end_z << std::endl;
        double start_time = Printer::GetInstance()->m_tool_head->m_start_z_time;
        double end_time = Printer::GetInstance()->m_tool_head->m_end_z_time;
        double delta_compensation_z = Printer::GetInstance()->m_strain_gauge->cal_z(start_z, start_time, end_z, end_time);

        if (Printer::GetInstance()->m_strain_gauge->m_cfg->m_enable_ts_compense) {
            movepos[2] = calc_pos_ret[2] - delta_compensation_z;
            // 时间戳补偿值，用于计算z轴实际触发坐标 
            LOG_I("enable timestamp compensation!!!!!!! ori-z:%f compensation-z:%f delta_compensation_z:%f\n", calc_pos_ret[2],movepos[2],delta_compensation_z); 
        }else {
            movepos[2] = calc_pos_ret[2];
            LOG_I("disable timestamp compensation!!!!!!! ori-z:%f compensation-z:%f delta_compensation_z:%f\n", calc_pos_ret[2],movepos[2],delta_compensation_z); 
        }

        movepos = {calc_pos_ret[0], calc_pos_ret[1], calc_pos_ret[2], movepos[3]};
    }
    Printer::GetInstance()->m_tool_head->set_position(movepos);
    std::cout << "g29 movepos: " << movepos[0] << " " << movepos[1] << " " << movepos[2] << " " << movepos[3] << std::endl;
    Printer::GetInstance()->send_event("homing:homing_move_end", this);
    return movepos;
}

std::vector<double> HomingMove::homing_move(std::vector<double> movepos, double homing_speed, bool probe_pos, bool triggered, bool check_triggered)
{
    double in_homing_print_fan_speed = 0.0f; /*--hao--*/
    // Notify start of homing/probing move
    Printer::GetInstance()->send_event("homing:homing_move_begin", this);
    is_succ = true;
    // Note start location
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::map<std::string, double> kin_spos;
    std::map<std::string, MCU_stepper *> steppers;
    std::vector<std::vector<MCU_stepper *>> kin_stepppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
    for (int i = 0; i < kin_stepppers.size(); i++)
    {
        for (int j = 0; j < kin_stepppers[i].size(); j++)
        {
            kin_spos[kin_stepppers[i][j]->get_name()] = kin_stepppers[i][j]->get_commanded_position();
        }
    }
    for (int i = 0; i < m_endstops.size(); i++)
    {
        for (int j = 0; j < m_endstops[i]->get_steppers().size(); j++)
        {
            steppers[m_endstops[i]->get_steppers()[j]->get_name()] = m_endstops[i]->get_steppers()[j];
            m_start_mcu_pos[m_endstops[i]->get_steppers()[j]->get_name()] = m_endstops[i]->get_steppers()[j]->get_mcu_position();
        }
    }
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (int i = 0; i < m_endstops.size(); i++)
    {
        double rest_time = calc_endstop_rate(m_endstops[i], movepos, homing_speed);
        m_endstops[i]->home_start(print_time, ENDSTOP_SAMPLE_TIME, ENDSTOP_SAMPLE_COUNT, rest_time);
        // endstop_triggers.append(wait)  //---??---
    }
    if (probe_pos == true) /*--hao--*/
    {
        in_homing_print_fan_speed = Printer::GetInstance()->m_printer_fan->get_status(0).speed;
        Printer::GetInstance()->m_gcode_io->single_command("M106 S0");
    }
    // endstop_triggers = []
    // all_endstop_trigger = multi_complete(self.printer, endstop_triggers)
    int all_endstop_trigger = 0; //---??---
    Printer::GetInstance()->m_tool_head->dwell(HOMING_START_DELAY);
    // std::cout << "before_drip_move movepos: " << movepos[0] << " " << movepos[1] << " " << movepos[2] << " " << movepos[3] << std::endl;
    Printer::GetInstance()->m_tool_head->drip_move(movepos, homing_speed, all_endstop_trigger); // 归0运动命令
    double move_end_print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (int i = 0; i < m_endstops.size(); i++)
    {
        bool did_trigger = m_endstops[i]->home_wait(move_end_print_time);
        if (!did_trigger && check_triggered)
        {
            is_succ = false;
            // 输出错误信息
            LOG_E("Failed to home did_trigger:%d  check_triggered:%d is_succ:%d\n", did_trigger, check_triggered, is_succ);
        }
    }
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    std::map<std::string, MCU_stepper *>::iterator it = steppers.begin();
    while (it != steppers.end())
    {
        m_end_mcu_pos[it->first] = steppers[it->first]->get_mcu_position();
        it++;
    }
    if (probe_pos)
    {
        std::map<std::string, int64_t>::iterator iter;
        for (iter = m_end_mcu_pos.begin(); iter != m_end_mcu_pos.end(); iter++)
        {
            std::string sname = iter->first;
            if (kin_spos.find(sname) != kin_spos.end())
            {
                kin_spos[sname] += (m_end_mcu_pos[sname] - m_start_mcu_pos[sname]) * steppers[sname]->get_step_dist();
            }
        }
        std::vector<double> calc_pos_ret = Printer::GetInstance()->m_tool_head->m_kin->calc_position(kin_spos);
        movepos = {calc_pos_ret[0], calc_pos_ret[1], calc_pos_ret[2], movepos[3]};
        Printer::GetInstance()->m_gcode_io->single_command("M106 S%.0f", homing_min(in_homing_print_fan_speed * 255.0f, 255.0f));
    }
    Printer::GetInstance()->m_tool_head->set_position(movepos);
    // std::cout << " X Y ---- movepos: " << movepos[0] << " " << movepos[1] << " " << movepos[2] << " " << movepos[3] << std::endl;
    Printer::GetInstance()->send_event("homing:homing_move_end", this);
    if (is_succ == false)
    {
        Printer::GetInstance()->m_tool_head->m_kin->motor_off(0); // 标记归零失败
    }
    return movepos;
}

std::string HomingMove::check_no_movement()
{
    // if self.printer.get_start_args().get('debuginput') is not None:
    //     return None
    for (std::map<std::string, int64_t>::iterator it = m_end_mcu_pos.begin(); it != m_end_mcu_pos.end(); it++)
    {
        if (m_end_mcu_pos[it->first] == m_start_mcu_pos[it->first])
        {
            return it->first;
        }
    }
    return "";
}
PrinterHoming::PrinterHoming(std::string section_name)
{
    Printer::GetInstance()->m_gcode->register_command("G28", std::bind(&PrinterHoming::cmd_G28, this, std::placeholders::_1));
}

PrinterHoming::~PrinterHoming()
{
}

void PrinterHoming::manual_home(std::vector<MCU_endstop *> endstops, std::vector<double> pos, double speed, bool triggered, bool check_triggered)
{
    HomingMove hmove = HomingMove(endstops);
    hmove.homing_move(pos, speed, false, triggered, check_triggered);
}

std::vector<double> PrinterHoming::probing_move(MCU_endstop *mcu_probe_endstop, std::vector<double> pos, double speed)
{
    std::vector<MCU_endstop *> endstops = {mcu_probe_endstop};
    HomingMove hmove = HomingMove(endstops);
    std::vector<double> epos;
    if (Printer::GetInstance()->m_strain_gauge != nullptr)
    {
        epos = Printer::GetInstance()->m_strain_gauge->run_G29_Z(&hmove);
    }
    else
    {
        epos = hmove.homing_move(pos, speed, true);
    }
    if (hmove.check_no_movement() != "")
    {
        printf("Probe triggered prior to movement\n"); // 探头在移动之前被触发
    }
    return epos;
}

void PrinterHoming::cmd_G28(GCodeCommand &gcmd)
{
    Printer::GetInstance()->m_tool_head->wait_moves();

    std::map<std::string, std::string> params;
    gcmd.get_command_parameters(params);
    std::vector<int> axes;
    std::vector<std::string> axiss = {"X", "Y", "Z"};
    bool need_z_home = false;
    for (int i = 0; i < axiss.size(); i++)
    {
        auto iter = params.find(axiss[i]);
        if (iter != params.end())
        {
            if (iter->first == "Z")
            {
                need_z_home = true;
            }
            else
            {
                axes.push_back(i);
            }
        }
    }

    if (axes.size() == 0 && need_z_home == false)
    {
        axes = {0, 1};
        need_z_home = true;
    }

    Homing homing_state;
    homing_state.set_axes(axes);
    for (int i = 0; i < 2; i++)
    {
        for (auto a : axes)
        {
            homing_state.set_axes({a});
            Printer::GetInstance()->m_tool_head->m_kin->home(homing_state);
        }
    }
    if (need_z_home)
    {
        homing_state.set_axes({2});  
        if (Printer::GetInstance()->m_strain_gauge != nullptr && Printer::GetInstance()->m_strain_gauge->m_cfg->m_enable_z_home)
        {
            std::cout << "strain gauge home....";
            Printer::GetInstance()->m_strain_gauge->run_G28_Z(homing_state);
        }
        else
        {
            std::cout << "Photoelectric home....";
            Printer::GetInstance()->m_tool_head->m_kin->home(homing_state);
        }
    }

}

int homing_register_state_callback(homing_state_callback_t state_callback)
{
    for (int i = 0; i < HOMING_STATE_CALLBACK_SIZE; i++)
    {
        if (homing_state_callback[i] == NULL)
        {
            homing_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}

int homing_state_callback_call(int state)
{
    // printf("homing_state_callback_call state:%d\n", state);
    for (int i = 0; i < HOMING_STATE_CALLBACK_SIZE; i++)
    {
        if (homing_state_callback[i] != NULL)
        {
            homing_state_callback[i](state);
        }
    }
    return 0;
}

#include "toolhead.h"
#include "configfile.h"
#include "Define.h"
#include "klippy.h"
#include "float.h"
#include "debug.h"
#include "hl_common.h"
#include "hl_boot.h"
#define LOG_TAG "toolhead"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

ToolHead::ToolHead(std::string section_name)
{
    // m_printer = config.get_printer();
    // m_reactor = self.printer.get_reactor(); //---??---ToolHead
    m_mcu = Printer::GetInstance()->m_mcu;
    m_all_mcus.push_back(m_mcu);
    for (auto mcu_map : Printer::GetInstance()->m_mcu_map)
    {
        m_all_mcus.push_back(mcu_map.second);
    }
    m_can_pause = true;
    // if (m_mcu->is_fileoutput())
    // {
    //     m_can_pause = false;
    // }
    m_movequeue = new MoveQueue();
    is_home = false;
    is_trigger = false;
    is_drip_move = false;
    // m_stop_move = false;
    // m_stop_bed_mesh = false;
    m_commanded_pos = {0., 0., 0., 0.};
    m_move_speed = 20;
    Printer::GetInstance()->register_event_handler("klippy:shutdown:ToolHead", std::bind(&ToolHead::handle_shutdown, this));
    Printer::GetInstance()->register_event_bool_handler("set_silent_mode" + section_name, std::bind(&ToolHead::set_silent_mode, this, std::placeholders::_1));

    // 速度和加速度控制
    m_max_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_velocity", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    m_max_velocity_limit = m_max_velocity;
    m_max_accel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_accel", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    m_max_accel_limit = m_max_accel;
    m_requested_accel_to_decel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_accel_to_decel", m_max_accel * 0.5, DBL_MIN, DBL_MAX, 0.);
    m_max_accel_to_decel = m_requested_accel_to_decel;
    m_square_corner_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "square_corner_velocity", 5., 0.);
    ;
    m_junction_deviation = 0.0;
    calc_junction_deviation();
    // 打印时间跟踪
    m_buffer_time_low = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "buffer_time_low", 1.000, DBL_MIN, DBL_MAX, 0.);
    m_buffer_time_high = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "buffer_time_high", 2.000, DBL_MIN, DBL_MAX, m_buffer_time_low);
    m_buffer_time_start = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "buffer_time_start", 0.25, DBL_MIN, DBL_MAX, 0.);
    m_move_flush_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "move_flush_time", 0.05, DBL_MIN, DBL_MAX, 0.);
    m_print_time = 0.0;
    m_special_queuing_state = Flushed;
    m_need_check_stall = -1.0;
    m_flush_timer = Printer::GetInstance()->m_reactor->register_timer("flush_timer", std::bind(&ToolHead::flush_handler, this, std::placeholders::_1));
    m_stats_timer = Printer::GetInstance()->m_reactor->register_timer("stats_timer", std::bind(&ToolHead::stats, this, std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NEVER);
    // Printer::GetInstance()->m_reactor->register_timer(std::bind(&ToolHead::check_chip_state, this,std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NOW);

    m_movequeue->set_flush_time(m_buffer_time_high);
    m_idle_flush_print_time = 0.0;
    m_print_stall = 0;
    // self.drip_completion = None //---??---ToolHead
    // 运动学步生成扫描窗口时间跟踪
    m_kin_flush_delay = SDS_CHECK_TIME; // 0.008576076596340965 SDS_CHECK_TIME
    // self.kin_flush_times = []  //---??---ToolHead
    m_last_kin_flush_time = m_last_kin_move_time = 0.0;
    m_last_flush_time = m_need_flush_time = 0.;
    m_min_restart_time = 0.;
    m_step_gen_time = 0.;
    // 设置迭代求解器
    m_trapq = trapq_alloc();

    // 注册命令
    m_cmd_SET_VELOCITY_LIMIT_help = ""; // "Set printer velocity limits"
    Printer::GetInstance()->m_gcode->register_command("G4", std::bind(&ToolHead::cmd_G4, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M400", std::bind(&ToolHead::cmd_M400, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("SET_VELOCITY_LIMIT", std::bind(&ToolHead::cmd_SET_VELOCITY_LIMIT, this, std::placeholders::_1), false, m_cmd_SET_VELOCITY_LIMIT_help);
    Printer::GetInstance()->m_gcode->register_command("RESET_PRINTER_PARAM", std::bind(&ToolHead::cmd_RESET_PRINTER_PARAM, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M204", std::bind(&ToolHead::cmd_M204, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M222", std::bind(&ToolHead::cmd_M222, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M223", std::bind(&ToolHead::cmd_M223, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M211", std::bind(&ToolHead::cmd_M211, this, std::placeholders::_1));
    // Load some default modules
    std::vector<std::string> modules = {"gcode_move", "homing", "idle_timeout", "statistics", "manual_probe", "tuning_tower"};
    for (int i = 0; i < modules.size(); i++)
    {
        Printer::GetInstance()->load_object(modules[i]);
    }
}
ToolHead::~ToolHead()
{
    // if (m_kin != nullptr)
    // {
    //     delete m_kin;
    // }
    // delete m_trapq;
    if (m_trapq != nullptr)
    {
        trapq_free(m_trapq);
    }
}

double ToolHead::check_chip_state(double eventtime)
{
    char temp[12] = {0};
    hl_get_chiptemp(temp, sizeof(temp));
    LOG_I("TEMP:%s\n", temp);
    return eventtime + 60.;
}

void ToolHead::load_kinematics(std::string section_name)
{
    // 创建运动学类
    m_kin_name = Printer::GetInstance()->m_pconfig->GetString(section_name, "kinematics", "");
    // std::cout << "m_kin_name " << m_kin_name << std::endl;
    if (m_kin_name == "cartesian")
    {
        m_kin = new CartKinematics(section_name);
    }
    else if (m_kin_name == "corexy")
    {
        m_kin = new CoreXYKinematics(section_name);
    }
    else if (m_kin_name == "corexz")
    {
        m_kin = new CoreXZKinematics(section_name);
    }
    else if (m_kin_name == "delta")
    {
        m_kin = new DeltaKinematics(section_name);
    }
    else if (m_kin_name == "hybrid_corexy")
    {
    }
    else if (m_kin_name == "rotary_delta")
    {
    }
    else if (m_kin_name == "winch")
    {
    }
}

void ToolHead::set_silent_mode(bool silent_mode)
{
    if (silent_mode)
    {
        m_max_velocity = Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_velocity_silent", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    }
    else
    {
        m_max_velocity = Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_velocity", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    }
}

void ToolHead::update_move_time(double next_print_time) //--7-home-2task-G-G--UI_control_task--         //--15-move-2task-G-G--UI_control_task--
{
    double batch_time = MOVE_BATCH_TIME;        // 0.5S冲刷一次 避免太长时间积累太多 超时
    double kin_flush_delay = m_kin_flush_delay; // 一个避免 step+dir+step 的过滤器
    double lkft = m_last_kin_flush_time;
    // GAM_DEBUG_send_UI("2-271-\n" );
    while (1)
    {
        // if (Printer::GetInstance()->m_tool_head->m_stop_move)
        // {
        //     // std::cout << "m_print_time5 " << m_stop_move << std::endl;
        //     break;
        // }
        // else
        // {
        //     // std::cout << "m_print_time6 " << m_print_time <<  " next_print_time "<< next_print_time << " TIME:"<< get_monotonic() << std::endl;
        // }
        LOG_D("m_print_time : %lf\n", m_print_time);
        LOG_D("update_move_time. now : %lf\n", get_monotonic());
        m_print_time = std::min(m_print_time + batch_time, next_print_time); // 保证至少0.5S 调用 flush_moves 一次
        double sg_flush_time = std::max(lkft, m_print_time - kin_flush_delay);
        for (int i = 0; i < m_step_generators.size(); i++)
        {
            m_step_generators[i](sg_flush_time); //-把移动命令变成每一步的运动时间------G-G-2023-05-11  //---10-add_move-G-G---
        }
        double free_time = std::max(lkft, sg_flush_time - kin_flush_delay);
        trapq_free_moves(m_trapq, free_time); // 删除已经处理完成的命令
        m_extruder->update_move_time(free_time);
        double mcu_flush_time = std::max(lkft, sg_flush_time - m_move_flush_time);
        for (int i = 0; i < m_all_mcus.size(); i++)
        {
            double start_time = get_monotonic();
            // if (m_all_mcus[i]->m_name == "stm32")
            //     usleep(100000);
            m_all_mcus[i]->flush_moves(mcu_flush_time); // 把每一步的运动时间发出去给MCU执行
            double end_time = get_monotonic();
            LOG_D("m_all_mcus[i]->m_name : %s mcu_flush_time : %lf\n", m_all_mcus[i]->m_name.c_str(), end_time - start_time);
        }
        if (m_print_time >= next_print_time)
        {
            break;
        }
    }
}

void ToolHead::advance_flush_time(double flush_time)
{
    flush_time = std::max(flush_time, m_last_flush_time);
    double sg_flush_ceil = std::max(flush_time, m_print_time - m_kin_flush_delay);
    double sg_flush_time = std::min(flush_time + m_move_flush_time, sg_flush_ceil);

    // LOG_D("advance_flush_time: flush_time=%.6f, print_time=%.6f, kin_flush_delay=%.6f\n",
    //       flush_time, m_print_time, m_kin_flush_delay);
    // LOG_D("sg_flush_ceil=%.6f, sg_flush_time=%.6f, last_flush_time=%.6f\n",
    //       sg_flush_ceil, sg_flush_time, m_last_flush_time);

    // Generate steps via itersolve
    for (auto sg : m_step_generators)
    {
        sg(sg_flush_time);
    }
    m_min_restart_time = std::max(m_min_restart_time, sg_flush_time);
    // Free trapq entries that are no longer needed
    double free_time = sg_flush_time - m_kin_flush_delay;
    // LOG_D("Freeing trapq moves before time %.6f\n", free_time);
    trapq_free_moves(m_trapq, free_time);
    m_extruder->update_move_time(free_time);

    // Flush stepcompress and mcu steppersync
    for (auto m : m_all_mcus)
    {
        // LOG_D("Flushing MCU %s at time %.6f\n", m->m_name.c_str(), flush_time);
        double start_time = get_monotonic();
        m->flush_moves(flush_time);
        double end_time = get_monotonic();
        if (end_time - start_time > 0.001)
        {
            LOG_D("m->flush_moves(flush_time) time: %lf\n", end_time - start_time);
        }
    }
    m_last_flush_time = flush_time;
}

void ToolHead::advance_move_time(double next_print_time)
{
    double pt_delay = m_kin_flush_delay + 0.05;
    double flush_time = std::max(m_last_flush_time, m_print_time - pt_delay);
    m_print_time = std::max(m_print_time, next_print_time);
    double want_flush_time = std::max(flush_time, m_print_time - pt_delay);
    while (1)
    {
        flush_time = std::min(flush_time + MOVE_BATCH_TIME, want_flush_time);
        advance_flush_time(flush_time);
        if (flush_time >= want_flush_time)
            break;
    }
}

void ToolHead::calc_print_time() //---home-2task-G-G--UI_control_task--         //--9-move-2task-G-G--UI_control_task--   //计算预估MCU计数器计数时间 s
{
    double curtime = get_monotonic();
    double est_print_time = m_mcu->m_clocksync->estimated_print_time(curtime); // 计算预估MCU计数器计数时间 s
    double kin_time = std::max(est_print_time + MIN_KIN_TIME, m_min_restart_time);
    kin_time += m_kin_flush_delay;
    double min_print_time = std::max(est_print_time + m_buffer_time_start, kin_time);
    if (min_print_time > m_print_time)
    {
        m_print_time = min_print_time;
        Printer::GetInstance()->send_event("toolhead:sync_print_time", curtime, est_print_time, m_print_time);
    }
}

#define GAM_DEBUG_SQ_STATE(fmt, ...) // printf(fmt, ##__VA_ARGS__)

void ToolHead::process_moves(std::vector<Move> &moves) //--13-home-2task-G-G--UI_control_task--        //--8-move-2task-G-G--UI_control_task--  //--处理一个移动命令，一个命令分为T形的3段运动--
{
    // Resync print_time if necessary  如有必要，重新同步 print_time
    double next_move_time = 0.0;
    if (m_special_queuing_state != main_state) //  GAM_DEBUG_send_UI("2-197-\n" );
    {
        if (m_special_queuing_state != Drip)
        {
            m_special_queuing_state = main_state; // 从“Flushed”/“Priming”状态转换到主状态
            // GAM_DEBUG_SQ_STATE("2-202-m_special_queuing_state main_state\n" );
            m_need_check_stall = -1.0;
            Printer::GetInstance()->m_reactor->update_timer(m_flush_timer, Printer::GetInstance()->m_reactor->m_NOW);
        }
        // GAM_DEBUG_send_UI("2-205-calc_print_time \n" );
        calc_print_time(); //  //计算预估MCU计数器计数时间 s  归0后进来一次
    }
    // 队列移动到梯形运动队列（trapq）
    next_move_time = m_print_time;
    for (int i = 0; i < moves.size(); i++)
    {
        if (moves[i].m_is_kinematic_move)
        {
            // LOG_D("Move %d: accel=%.6f cruise=%.6f decel=%.6f start_v=%.6f cruise_v=%.6f\n",
            //       i, moves[i].m_accel_t, moves[i].m_cruise_t, moves[i].m_decel_t,
            //       moves[i].m_start_v, moves[i].m_cruise_v);

            trapq_append(m_trapq, next_move_time,
                         moves[i].m_accel_t, moves[i].m_cruise_t, moves[i].m_decel_t,
                         moves[i].m_start_pos[0], moves[i].m_start_pos[1], moves[i].m_start_pos[2],
                         moves[i].m_axes_r[0], moves[i].m_axes_r[1], moves[i].m_axes_r[2],
                         moves[i].m_start_v, moves[i].m_cruise_v, moves[i].m_accel);
        }
        if (moves[i].m_axes_d[3])
        {
            m_extruder->move(next_move_time, moves[i]); //---11-2task-G-G--UI_control_task--
        }
        next_move_time = (next_move_time + moves[i].m_accel_t + moves[i].m_cruise_t + moves[i].m_decel_t);
        if (isnan(next_move_time))
        {
            LOG_E("Invalid next_move_time in move %d: accel=%.6f cruise=%.6f decel=%.6f\n",
                  i, moves[i].m_accel_t, moves[i].m_cruise_t, moves[i].m_decel_t);
        }

        for (auto cb : moves[i].m_timing_callbacks)
        {
            if (cb != nullptr)
                cb(next_move_time, 0.);
        }
    }
    // 生成移动步骤
    if (m_special_queuing_state != main_state)
    {
        update_drip_move_time(next_move_time);
        next_move_time = m_print_time;
    }
    // update_move_time(next_move_time); //--G-G-2023-05-11         //---9-add_move-G-G---
    note_kinematic_activity(next_move_time + m_kin_flush_delay);
    advance_move_time(next_move_time);


    m_last_kin_move_time = next_move_time; // 这个需要更新  flush_step_generation 才能正确处理剩余移动数据
    // if (Printer::GetInstance()->m_tool_head->m_stop_move)
    // {
    //     m_last_kin_move_time = m_print_time;
    //     stop_move_clear();
    // }
}

void ToolHead::stop_move_clear()
{
    // std::cout << "stop_move_clear !!!   " << m_stop_move << std::endl;
    m_movequeue->reset();
    trapq_free_moves(m_trapq, Printer::GetInstance()->m_reactor->m_NEVER);
    trapq_free_moves(Printer::GetInstance()->m_printer_extruder->m_trapq, Printer::GetInstance()->m_reactor->m_NEVER);
    // m_stop_move = false;
    wait_moves();
    std::map<std::string, double> kin_spos;
    std::map<std::string, MCU_stepper *> steppers;
    std::vector<std::vector<MCU_stepper *>> kin_stepppers = m_kin->get_steppers();
    for (int i = 0; i < kin_stepppers.size(); i++)
    {
        for (int j = 0; j < kin_stepppers[i].size(); j++)
        {
            kin_spos[kin_stepppers[i][j]->get_name()] = kin_stepppers[i][j]->get_commanded_position();
            // std::cout << "i: " << i << " j: " << j << kin_stepppers[i][j]->get_name() << " value: " << kin_spos[kin_stepppers[i][j]->get_name()] << std::endl;
        }
    }
    std::vector<double> adjustpos = m_kin->calc_position(kin_spos);
    adjustpos.push_back(Printer::GetInstance()->m_printer_extruder->m_stepper->get_commanded_position());
    Printer::GetInstance()->m_tool_head->set_position(adjustpos);

    Printer::GetInstance()->m_tool_head->set_extruder(Printer::GetInstance()->m_printer_extruder, Printer::GetInstance()->m_printer_extruder->m_stepper->get_commanded_position());
    Printer::GetInstance()->send_event("extruder:activate_extruder");
}

void ToolHead::stop_move()
{
    // m_stop_move = true;
    // m_stop_bed_mesh = true;
    // std::cout << "stop move !!!   " << m_stop_move << std::endl;
}

void ToolHead::flush_step_generation() //--5-home-2task-G-G--UI_control_task--
{
    // if (Printer::GetInstance()->m_tool_head->m_stop_move)
    // {
    // }
    // else
    // {
    // }
    // m_special_queuing_state = Drip;
    m_movequeue->flush(); //---7-add_move-G-G---
    m_special_queuing_state = Flushed;
    m_need_check_stall = -1.0;
    Printer::GetInstance()->m_reactor->update_timer(m_flush_timer, Printer::GetInstance()->m_reactor->m_NEVER);
    m_movequeue->set_flush_time(m_buffer_time_high);
    m_idle_flush_print_time = 0.0;
    double flush_time = m_last_kin_move_time + m_kin_flush_delay;
    flush_time = std::max(flush_time, m_print_time - m_kin_flush_delay);
    m_last_kin_flush_time = std::max(m_last_kin_flush_time, flush_time); //-----G-G-2023-05-10--------------
    advance_flush_time(m_need_flush_time);
    m_min_restart_time = std::max(m_min_restart_time, m_print_time);
    // update_move_time(std::max(m_print_time, m_last_kin_flush_time));     //--7-home-2task-G-G--UI_control_task--
    // if (Printer::GetInstance()->m_tool_head->m_stop_move)
    // {
    //     m_stop_move = false;
    // }
}

void ToolHead::flush_lookahead()
{
    if (m_special_queuing_state != main_state)
    {
        flush_step_generation(); //---6-add_move-G-G---
    }
    m_movequeue->flush();
}

double ToolHead::get_last_move_time()
{
    flush_lookahead(); //---5-add_move-G-G---
    if (m_special_queuing_state != main_state)
    {
        calc_print_time();
    }
    return m_print_time;
}

void ToolHead::check_stall()
{
    double est_print_time = 0.0;
    double eventtime = get_monotonic();
    if (m_special_queuing_state)
    {
        if (m_idle_flush_print_time != 0)
        {
            est_print_time = m_mcu->estimated_print_time(eventtime);
            if (est_print_time < m_idle_flush_print_time)
            {
                m_print_stall += 1;
            }
            m_idle_flush_print_time = 0.;
        }
        m_special_queuing_state = Priming;
        m_need_check_stall = -1.0;
        Printer::GetInstance()->m_reactor->update_timer(m_flush_timer, eventtime + 0.100);
    }
    while (1)
    {
        est_print_time = m_mcu->estimated_print_time(eventtime);
        double buffer_time = m_print_time - est_print_time;
        double stall_time = buffer_time - m_buffer_time_high;
        if (stall_time <= 0)
        {
            break;
        }
        if (!m_can_pause)
        {
            m_need_check_stall = Printer::GetInstance()->m_reactor->m_NEVER;
            return;
        }
        eventtime = get_monotonic();
        // double min_stall_time = std::min(stall_time, 1.0);
        Printer::GetInstance()->m_reactor->pause(eventtime);
    }
    if (m_special_queuing_state == main_state)
    {
        m_need_check_stall = est_print_time + m_buffer_time_high + 0.1;
    }
}

double ToolHead::flush_handler(double eventtime)
{
    double print_time = m_print_time;
    double buffer_time = m_print_time - Printer::GetInstance()->m_mcu->estimated_print_time(eventtime);

    // LOG_D("-------------------------flush_handler-------------------------\n");
    // LOG_D("print_time=%.6f est_time=%.6f buffer_time=%.6f\n",
    //       m_print_time, Printer::GetInstance()->m_mcu->estimated_print_time(eventtime), buffer_time);
    // LOG_D("buffer_time_low=%.6f buffer_time_high=%.6f special_state=%d\n",
    //       m_buffer_time_low, m_buffer_time_high, m_special_queuing_state);
    // LOG_D("movequeue_size=%zu\n",
    //       m_movequeue->moveq.size());

    if (buffer_time > m_buffer_time_low)
    {
        // LOG_D("Buffer time %.6f > low threshold %.6f, postponing flush\n",
            //   buffer_time, m_buffer_time_low);
        return eventtime + buffer_time - m_buffer_time_low;
    }

    // LOG_D("Initiating flush_step_generation\n");
    flush_step_generation();

    if (print_time != m_print_time)
    {
        m_idle_flush_print_time = m_print_time;
        // LOG_D("Print time updated: %.6f -> %.6f\n", print_time, m_print_time);
    }

    return Printer::GetInstance()->m_reactor->m_NEVER;
}

std::vector<double> ToolHead::get_position()
{
    return m_commanded_pos;
}

void ToolHead::set_position(std::vector<double> new_pos, std::vector<int> homing_axes) //--4-home-2task-G-G--UI_control_task--
{
    flush_step_generation(); //--5-home-2task-G-G--UI_control_task--
    trapq_free_moves(m_trapq, Printer::GetInstance()->m_reactor->m_NEVER);
    m_commanded_pos = new_pos; // 归零后会将每一轴当前位置，都设置为某一轴参数里面的position_endstop。
    // printf("ToolHead::set_position %f\t%f\t%f\t%f\n",new_pos[0],new_pos[1],new_pos[2],new_pos[3]);
    double new_pos_temp[3] = {new_pos[0], new_pos[1], new_pos[2]};
    m_kin->set_position(new_pos_temp, homing_axes);
    Printer::GetInstance()->send_event("toolhead:set_position");
}

bool ToolHead::move(std::vector<double> &new_pos, double speed) //--1-move-2task-G-G--UI_control_task---插入一条移动指令到-m_movequeue--
{
    //  GAM_DEBUG_send_UI("2-115-\n");
    Move move(m_commanded_pos, new_pos, speed); // 新建move对象;起始坐标，结束坐标，速度。
    // printf("ToolHead::move m_commanded_pos %f\t%f\t%f\t%f\n", m_commanded_pos[0], m_commanded_pos[1], m_commanded_pos[2], m_commanded_pos[3]);
    // printf("ToolHead::move new_pos %f\t%f\t%f\t%f\n", new_pos[0], new_pos[1], new_pos[2], new_pos[3]);
    // printf("ToolHead::move speed %f\n", speed);
    if (fabs(move.m_move_d) <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        return false;
    }
    if (move.m_is_kinematic_move)
    {
        if (!m_kin->check_move(move)) //--2-move-2task-G-G--UI_control_task--
        {
            return false;
        }
    }
    if (move.m_axes_d[3])
    {
        if (!m_extruder->check_move(move))
        {
            return false;
        }
    }
    if (fabs(move.m_move_d) <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        return false;
    }
    m_commanded_pos = move.m_end_pos;
    m_move_speed = speed;
    m_movequeue->add_move(move); //--5-move-2task-G-G--UI_control_task--

    if (m_print_time > m_need_check_stall)
    {
        check_stall();
    }
    return true;
}

bool ToolHead::move1(double *new_pos, double speed) //--1-move-2task-G-G--UI_control_task---插入一条移动指令到-m_movequeue--
{
    std::vector<double> pos = {new_pos[0], new_pos[1], new_pos[2], new_pos[3]};
    Move move(m_commanded_pos, pos, speed); // 新建move对象;

    if (fabs(move.m_move_d) <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        return true;
    }
    if (move.m_is_kinematic_move)
    {
        if (!m_kin->check_move(move)) //--2-move-2task-G-G--UI_control_task--
        {
            return false;
        }
    }
    if (move.m_axes_d[3])
    {
        if (!m_extruder->check_move(move))
        {
            return false;
        }
    }
    if (fabs(move.m_move_d) <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        return false;
    }
    m_commanded_pos = move.m_end_pos;
    m_move_speed = speed;
    m_movequeue->add_move(move); //--5-move-2task-G-G--UI_control_task--
    if (m_print_time > m_need_check_stall)
    {
        check_stall();
    }
    return true;
}

bool ToolHead::manual_move(std::vector<double> coord, double speed)
{
    double curpos[4] = {m_commanded_pos[0], m_commanded_pos[1], m_commanded_pos[2], m_commanded_pos[3]};
    if (coord[0] != DBL_MIN)
    {
        curpos[0] = coord[0];
    }
    if (coord[1] != DBL_MIN)
    {
        curpos[1] = coord[1];
    }
    if (coord[2] != DBL_MIN)
    {
        curpos[2] = coord[2];
    }
    bool ret = move1(curpos, speed);
    Printer::GetInstance()->send_event("toolhead:manual_move");
    return ret;
}

void ToolHead::dwell(double delay)
{
    double next_print_time = get_last_move_time() + std::max(0.0, delay);
    // update_move_time(next_print_time);
    advance_move_time(next_print_time);
    check_stall();
}

void ToolHead::wait_moves()
{
    this->flush_lookahead();
    double eventtime = get_monotonic();
    while (!m_special_queuing_state || m_print_time >= m_mcu->estimated_print_time(eventtime))
    {
        if (!m_can_pause)
        {
            break;
        }
        // double poll_time = Printer::GetInstance()->m_reactor->_check_timers(get_monotonic(), true);
        // usleep(100000);
        // eventtime = get_monotonic() + 0.1;
        eventtime = Printer::GetInstance()->m_reactor->pause(get_monotonic());
    }
    return;
}

void ToolHead::set_extruder(PrinterExtruder *extruder, double extrude_pos)
{
    m_extruder = extruder;
    // printf("set_extruder 1 m_commanded_pos %f\t%f\t%f\t%f\t%d\n", m_commanded_pos[0], m_commanded_pos[1], m_commanded_pos[2], m_commanded_pos[3], m_commanded_pos.size());

    m_commanded_pos[3] = extrude_pos;
    // printf("set_extruder 2 m_commanded_pos %f\t%f\t%f\t%f\t%d\n", m_commanded_pos[0], m_commanded_pos[1], m_commanded_pos[2], m_commanded_pos[3], m_commanded_pos.size());
}

PrinterExtruder *ToolHead::get_extruder()
{
    return m_extruder;
}

void ToolHead::update_drip_move_time(double next_print_time)
{
    double flush_delay = DRIP_TIME + m_move_flush_time + m_kin_flush_delay;
    m_start_z_time = m_mcu->m_clocksync->estimated_print_time(get_monotonic());

    // std::cout << "update_drip_move_time m_print_time:" << m_print_time <<  " next_print_time:" << next_print_time << std::endl;
    while (m_print_time < next_print_time) //&& in_temp
    {
        if (is_trigger)
        {
            // usleep(100 * 1000);
            // GAM_DEBUG_printf("is_trigger:break \n");
            // std::cout << "update_drip_move_time is_trigger" << std::endl;
            break;
        }
        // if (Printer::GetInstance()->m_tool_head->m_stop_move)
        // {

        //     // std::cout << "update_drip_move_time m_stop_move" << std::endl;
        //     break;
        // }
        double curtime = get_monotonic();
        double est_print_time = m_mcu->m_clocksync->estimated_print_time(curtime);
        double wait_time = m_print_time - est_print_time - flush_delay;
        if (wait_time > 0 && m_can_pause)
        {
            usleep(wait_time * 1e6); // 在发送更多步骤之前暂停
            continue;
        }
        double npt = std::min(m_print_time + DRIP_SEGMENT_TIME, next_print_time);
        // update_move_time(npt);
        note_kinematic_activity(npt + m_kin_flush_delay);
        advance_move_time(npt);
    }
    m_end_z_time = m_mcu->m_clocksync->estimated_print_time(get_monotonic());
}

void ToolHead::drip_move(std::vector<double> newpos, double speed, int drip_completion) // home Endstop
{
    dwell(m_kin_flush_delay);
    m_movequeue->flush();
    m_special_queuing_state = Drip;
    GAM_DEBUG_SQ_STATE("2-417-m_special_queuing_state Drip\n");
    m_need_check_stall = Printer::GetInstance()->m_reactor->m_NEVER;
    Printer::GetInstance()->m_reactor->update_timer(m_flush_timer, Printer::GetInstance()->m_reactor->m_NEVER);
    m_movequeue->set_flush_time(m_buffer_time_high);
    m_idle_flush_print_time = 0.0;
    double newpos1[4] = {newpos[0], newpos[1], newpos[2], newpos[3]};
    move1(newpos1, speed); // 归0移动
    is_drip_move = true;
    m_movequeue->flush(); // 发送归0移动命令
    is_drip_move = false;
    if (Printer::GetInstance()->m_tool_head->is_trigger) // 归0限位后删除剩余的队列
    {
        m_movequeue->reset();
        trapq_free_moves(m_trapq, Printer::GetInstance()->m_reactor->m_NEVER); //-----G-G-2023-05-10--------------------
    }
    flush_step_generation(); // 有限位就无意义 无限位就把最后0.03S数据发送出去
}

double ToolHead::stats(double eventtime)
{
    double max_queue_time = std::max(m_print_time, m_last_flush_time);
    for (int i = 0; i < m_all_mcus.size(); i++)
    {
        m_all_mcus[i]->check_active(max_queue_time, eventtime);
    }
    double buffer_time = m_print_time - m_mcu->estimated_print_time(eventtime);
    bool is_active = (buffer_time > -60.0f || !m_special_queuing_state);
    if (m_special_queuing_state == Drip)
        buffer_time = 0.0f;
    return eventtime + 1.0f;
    // std::stringstream ret;
    // ret << "print_time=" << m_print_time << " buffer_time=" << std::max(buffer_time, 0.) << " print_stall=" << m_print_stall << " is_active" << is_active;
    // return ret.str();
}

std::vector<double> ToolHead::check_busy(double eventtime)
{
    double est_print_time = m_mcu->estimated_print_time(eventtime);
    double lookahead_empty = (!m_movequeue->moveq.size());
    std::vector<double> ret = {m_print_time, est_print_time, lookahead_empty};
    return ret;
}

std::map<std::string, std::string> ToolHead::get_status(double eventtime)
{
    double print_time = m_print_time;
    double estimated_print_time = m_mcu->estimated_print_time(eventtime);
    std::map<std::string, std::string> res = m_kin->get_status(eventtime);
    res["print_time"] = std::to_string(print_time);
    res["stalls"] = std::to_string(m_print_stall);
    res["estimated_print_time"] = std::to_string(estimated_print_time);
    res["extruder"] = m_extruder->get_name();
    res["position"] = std::to_string(m_commanded_pos[0]) + "," + std::to_string(m_commanded_pos[1]) + "," + std::to_string(m_commanded_pos[2]) + "," + std::to_string(m_commanded_pos[3]);
    res["position_e"] = std::to_string(m_commanded_pos[3]);
    res["max_velocity"] = std::to_string(m_max_velocity);
    res["max_accel"] = std::to_string(m_max_accel);
    res["max_accel_to_decel"] = std::to_string(m_requested_accel_to_decel);
    res["square_corner_velocity"] = std::to_string(m_square_corner_velocity);
    return res;
}

void ToolHead::handle_shutdown()
{
    m_can_pause = false;
    m_movequeue->reset();
}

Kinematics *ToolHead::get_kinematics()
{
    return m_kin;
}

trapq *ToolHead::get_trapq()
{
    return m_trapq;
}

void ToolHead::register_step_generator(std::function<void(double)> handler)
{
    m_step_generators.push_back(handler);
}

void ToolHead::note_step_generation_scan_time(double delay, double old_delay)
{
    flush_step_generation();
    double cur_delay = m_kin_flush_delay;
    if (old_delay)
    {
        std::vector<double>::iterator iter = find(m_kin_flush_times.begin(), m_kin_flush_times.end(), old_delay);
        if (iter != m_kin_flush_times.end())
        {
            // m_kin_flush_times.erase(iter);
            std::vector<double> kin_flush_times(m_kin_flush_times.begin(), iter);
            kin_flush_times.insert(kin_flush_times.end(), iter + 1, m_kin_flush_times.end());
            m_kin_flush_times.swap(kin_flush_times);
        }
    }
    if (delay)
    {
        m_kin_flush_times.push_back(delay);
    }
    std::vector<double>::iterator max = max_element(m_kin_flush_times.begin(), m_kin_flush_times.end());
    double new_delay = 0;
    if (max != m_kin_flush_times.end())
    {
        new_delay = *max + SDS_CHECK_TIME;
        m_kin_flush_delay = new_delay;
    }
}

void ToolHead::register_lookahead_callback(std::function<void(double, double)> callback)
{
    Move *last_move = m_movequeue->get_last();
    if (last_move == nullptr)
    {
        if (callback != nullptr)
        {
            callback(get_last_move_time(), 0.);
        }
        else
        {
            get_last_move_time();
        }
    }
    else
    {
        last_move->m_timing_callbacks.push_back(callback); // 有些条件下 m_timing_callbacks释放的时候报错
    }
}

void ToolHead::note_kinematic_activity(double kin_time)
{
    m_need_flush_time = std::max(m_need_flush_time, kin_time);
}

std::vector<double> ToolHead::get_max_velocity()
{
    std::vector<double> ret = {m_max_velocity, m_max_accel};
    return ret;
}

void ToolHead::calc_junction_deviation()
{
    double scv2 = pow(m_square_corner_velocity, 2);
    m_junction_deviation = scv2 * (sqrt(2) - 1) / m_max_accel;
    m_max_accel_to_decel = std::min(m_requested_accel_to_decel, m_max_accel);
}

void ToolHead::cmd_G4(GCodeCommand &gcmd)
{
    double delay = gcmd.get_double("P", 0., 0.) / 1000.;
    std::cout << ">>>>G4 delay = " << delay << std::endl;
    this->dwell(delay);
}

void ToolHead::cmd_M400(GCodeCommand &gcmd)
{
    // Wait for current moves to finish
    this->wait_moves();
    M400_state_callback_call(WAIT_MOVE_COMPLETED);
}

void ToolHead::cmd_SET_VELOCITY_LIMIT(GCodeCommand &gcmd)
{
    double max_velocity = gcmd.get_double("VELOCITY", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    double max_accel = gcmd.get_double("ACCEL", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    double square_corner_velocity = gcmd.get_double("SQUARE_CORNER_VELOCITY", DBL_MIN, 0.);
    double requested_accel_to_decel = gcmd.get_double("ACCEL_TO_DECEL", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    int force = gcmd.get_int("FORCE", 0);
    // std::cout << "ACCEL = " << max_accel << std::endl;
    // std::cout << "ACCEL_TO_DECEL = " << requested_accel_to_decel << std::endl;
    if (max_velocity != DBL_MIN)
        m_max_velocity = std::min(max_velocity, m_max_velocity_limit);
    if (max_accel != DBL_MIN)
    {
        if (force)
            m_max_accel_limit = max_accel;
        m_max_accel = std::min(max_accel, m_max_accel_limit);
    }

    if (square_corner_velocity != DBL_MIN)
        m_square_corner_velocity = square_corner_velocity;
    if (requested_accel_to_decel != DBL_MIN)
        m_requested_accel_to_decel = requested_accel_to_decel;
    this->calc_junction_deviation();
    // msg = ("max_velocity: %.6f\n"
    //            "max_accel: %.6f\n"
    //            "max_accel_to_decel: %.6f\n"
    //            "square_corner_velocity: %.6f" % (
    //                self.max_velocity, self.max_accel,
    //                self.requested_accel_to_decel,
    //                self.square_corner_velocity))
    //     self.printer.set_rollover_info("toolhead", "toolhead: %s" % (msg,))
    // if (max_velocity is None and
    //         max_accel is None and
    //         square_corner_velocity is None and
    //         requested_accel_to_decel is None):
    //         gcmd.respond_info(msg, log=False)  //---??---ToolHead
}

void ToolHead::cmd_RESET_PRINTER_PARAM(GCodeCommand &gcmd)
{
    Printer::GetInstance()->m_gcode_io->single_command("M900 P%f", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "pressure_advance", 0.03));
    Printer::GetInstance()->m_gcode_io->single_command("SET_VELOCITY_LIMIT VELOCITY=%.2f ACCEL=%.2f SQUARE_CORNER_VELOCITY=%.2f ACCEL_TO_DECEL=%.2f",
                                                       m_max_velocity_limit, m_max_accel_limit,
                                                       Printer::GetInstance()->m_pconfig->GetDouble("printer", "square_corner_velocity", 0.0),
                                                       Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_accel_to_decel", m_max_accel_limit * 0.5));
}

void ToolHead::cmd_M204(GCodeCommand &gcmd)
{
    // Use S for accel
    double accel = gcmd.get_double("S", DBL_MAX, 0, m_max_accel_limit, .0);
    if (accel == DBL_MAX)
    {
        // Use minimum of P and T for accel
        double p = gcmd.get_double("P", DBL_MAX, 0, m_max_accel_limit, 0.);
        double t = gcmd.get_double("T", DBL_MAX, 0, m_max_accel_limit, 0.);
        if (p == DBL_MAX && t == DBL_MAX)
        {
            gcmd.m_respond_info("Invalid M204 command" + gcmd.get_commandline(), true);
            return;
        }
        accel = std::min(p, t);
    }
    m_max_accel = std::min(m_max_accel_limit, accel);
    this->calc_junction_deviation();
}

void ToolHead::cmd_M222(GCodeCommand &gcmd)
{
    if (Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    m_max_velocity = gcmd.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_velocity"));
}

void ToolHead::cmd_M223(GCodeCommand &gcmd)
{
    if (Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    m_kin->m_max_z_velocity = gcmd.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_z_velocity"));
}

void ToolHead::cmd_M211(GCodeCommand &gcmd)
{
    if (Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int set_enable = gcmd.get_int("S", -1);
    if (set_enable == -1)
    {
        for (int i = 0; i < 3; i++)
        {
            char limits_info[MAX_SERIAL_MSG_LENGTH];
            // sprintf(limits_info, "Axis %c soft endstop limits : MIN = %f  MAX = %f", i + 88, m_kin->m_limits[i][0], m_kin->m_limits[i][1]);
            // serial_info(limits_info);
        }
    }
    else if (set_enable == 0)
    {
        m_kin->is_open_soft_limit = false;
    }
    else if (set_enable == 1)
    {
        m_kin->is_open_soft_limit = true;
    }
    else
    {
        LOG_E("soft endstop limits config error\n");
        return;
    }
}

#define M400_STATE_CALLBACK_SIZE 16
static M400_state_callback_t M400_state_callback[M400_STATE_CALLBACK_SIZE];
int M400_register_state_callback(M400_state_callback_t state_callback)
{
    for (int i = 0; i < M400_STATE_CALLBACK_SIZE; i++)
    {
        if (M400_state_callback[i] == NULL)
        {
            M400_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}
int M400_unregister_state_callback(M400_state_callback_t state_callback)
{
    for (int i = 0; i < M400_STATE_CALLBACK_SIZE; i++)
    {
        if (M400_state_callback[i] == state_callback)
        {
            M400_state_callback[i] = NULL;
            return 0;
        }
    }
    return -1;
}

int M400_state_callback_call(int event)
{
    for (int i = 0; i < M400_STATE_CALLBACK_SIZE; i++)
    {
        if (M400_state_callback[i] != NULL)
        {
            M400_state_callback[i](event);
        }
    }
    return 0;
}

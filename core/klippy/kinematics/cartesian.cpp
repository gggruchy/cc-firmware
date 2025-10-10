#include "cartesian.h"
#include "Define.h"
#include "klippy.h"
#define LOG_TAG "cartesian"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_WARN
#include "log.h"
#define SOFT_LIMIT_ENABLE 1
CartKinematics::CartKinematics(std::string section_name) : Kinematics() // xyz机型
{
    // Setup axis rails
    m_dual_carriage_axis = -1;
    if (Printer::GetInstance()->m_pconfig->GetSection("stepper_x") != nullptr)
    {
        m_rails.push_back(LookupMultiRail("stepper_x"));
    }
    if (Printer::GetInstance()->m_pconfig->GetSection("stepper_y") != nullptr)
    {
        m_rails.push_back(LookupMultiRail("stepper_y"));
    }
    if (Printer::GetInstance()->m_pconfig->GetSection("stepper_z") != nullptr)
    {
        m_rails.push_back(LookupMultiRail("stepper_z"));
    }
    m_rails[0]->setup_itersolve('x');
    m_rails[1]->setup_itersolve('y');
    m_rails[2]->setup_itersolve('z');
    std::vector<std::vector<MCU_stepper *>> steppers = get_steppers();
    for (int i = 0; i < steppers.size(); i++)
    {
        for (int j = 0; j < steppers[i].size(); j++)
        {
            steppers[i][j]->set_trapq(Printer::GetInstance()->m_tool_head->get_trapq());
            Printer::GetInstance()->m_tool_head->register_step_generator(std::bind(&MCU_stepper::generate_steps, steppers[i][j], std::placeholders::_1));
        }
    }
    Printer::GetInstance()->register_event_double_handler("stepper_enable:motor_off:CartKinematics", std::bind(&CartKinematics::motor_off, this, std::placeholders::_1));
    // Setup boundary checks
    std::vector<double> max_velocitys = Printer::GetInstance()->m_tool_head->get_max_velocity();
    double max_velocity = max_velocitys[0];
    double max_accel = max_velocitys[1];
    m_max_z_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_z_velocity", max_velocity, DBL_MIN, max_velocity, 0.);
    m_max_z_accel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_z_accel", max_accel, DBL_MIN, max_accel, 0.);
    m_limits = {{1.0, -1.0}, {1.0, -1.0}, {1.0, -1.0}};
    std::vector<std::vector<double>> ranges;
    for (int i = 0; i < m_rails.size(); i++)
    {
        ranges.push_back(m_rails[i]->get_range());
    }
    for (int i = 0; i < ranges.size(); i++)
    {
        m_axes_min.push_back(ranges[i][0]);
        m_axes_max.push_back(ranges[i][1]);
    }
    m_axes_min.push_back(0.);
    m_axes_max.push_back(0.);
    // Check for dual carriage support
    if (Printer::GetInstance()->m_pconfig->GetSection("dual_carriage") != nullptr)
    {
    std:
        string dc_axis = Printer::GetInstance()->m_pconfig->GetString("dual_carriage", "axis", "");
        PrinterRail *dc_rail;
        if (dc_axis == "x")
        {
            m_dual_carriage_axis = 0;
            dc_rail = LookupMultiRail("dual_carriage");
            m_dual_carriage_rails.push_back(m_rails[0]);
            m_dual_carriage_rails.push_back(dc_rail);
        }
        else if (dc_axis == "y")
        {
            m_dual_carriage_axis = 1;
            dc_rail = LookupMultiRail("dual_carriage");
            m_dual_carriage_rails.push_back(m_rails[1]);
            m_dual_carriage_rails.push_back(dc_rail);
        }
        std::vector<MCU_stepper *> dc_axis_steppers = dc_rail->get_steppers();
        for (int i = 0; i < dc_axis_steppers.size(); i++)
        {
            Printer::GetInstance()->m_tool_head->register_step_generator(std::bind(&MCU_stepper::generate_steps, dc_axis_steppers[i], std::placeholders::_1));
        }
        m_cmd_SET_DUAL_CARRIAGE_help = "Set which carriage is active";
        Printer::GetInstance()->m_gcode->register_command("SET_DUAL_CARRIAGE", std::bind(&CartKinematics::cmd_SET_DUAL_CARRIAGE, this, std::placeholders::_1), false, m_cmd_SET_DUAL_CARRIAGE_help);
    }
}

CartKinematics::~CartKinematics()
{
    for (int i = 0; i < m_rails.size(); i++)
    {
        if (m_rails[i] != nullptr)
        {
            delete m_rails[i];
        }
    }
}

std::vector<std::vector<MCU_stepper *>> CartKinematics::get_steppers()
{
    std::vector<PrinterRail *> rails = m_rails;
    if (m_dual_carriage_axis == 0) // x
    {
        rails.insert(rails.begin() + 1, m_dual_carriage_rails[1]);
    }
    if (m_dual_carriage_axis == 1) // y
    {
        rails.insert(rails.begin() + 2, m_dual_carriage_rails[1]);
    }
    std::vector<std::vector<MCU_stepper *>> all_steppers;
    for (int i = 0; i < rails.size(); i++)
    {
        all_steppers.push_back(rails[i]->get_steppers());
    }
    return all_steppers;
}

std::vector<double> CartKinematics::calc_position(std::map<std::string, double> stepper_positions)
{
    std::vector<double> ret;
    for (int i = 0; i < m_rails.size(); i++)
    {
        if (stepper_positions.find(m_rails[i]->m_steppers[0]->get_name()) != stepper_positions.end())
        {
            ret.push_back(stepper_positions[m_rails[i]->m_steppers[0]->get_name()]);
        }
    }
    return ret;
}

void CartKinematics::set_position(double newpos[3], std::vector<int> homing_axes)
{
    for (int i = 0; i < m_rails.size(); i++)
    {
        m_rails[i]->set_position(newpos);
        // printf("CartKinematics::set_position: %f\n", newpos[i]);
        // printf("axes: %d\n", i);
        if (std::find(homing_axes.begin(), homing_axes.end(), i) != homing_axes.end())
        {
            // printf("当前homing axes: %d\n", i);
            m_limits[i][0] = m_rails[i]->get_range()[0];
            m_limits[i][1] = m_rails[i]->get_range()[1];
        }
    }
}

void CartKinematics::note_z_not_homed()
{
    m_limits[2][0] = 1.0;
    m_limits[2][1] = -1.0;
}

bool CartKinematics::home_axis(Homing &homing_state, int axis, PrinterRail *rail) //--2-home-2task-G-G--UI_control_task--
{
    // Determine movement
    GAM_DEBUG_send_UI_home("2-H2-H906-\n");
    double position_min = rail->get_range()[0];
    double position_max = rail->get_range()[1];
    struct homingInfo hi = rail->get_homing_info();                                             // 某一轴归零信息，position_endstop在里面！
    std::vector<double> homepos = {DO_NOT_MOVE_F, DO_NOT_MOVE_F, DO_NOT_MOVE_F, DO_NOT_MOVE_F}; // 只修改当前归零的轴
    homepos[axis] = hi.position_endstop;                                                        // 归零后的坐标由它决定
    std::vector<double> forcepos = homepos;
    if (hi.positive_dir)
        forcepos[axis] -= 1.5 * (hi.position_endstop - position_min);
    else
        forcepos[axis] += 1.5 * (position_max - hi.position_endstop);
    // Perform homing
    std::vector<double> force_pos = {forcepos[0], forcepos[1], forcepos[2], forcepos[3]};
    std::vector<double> home_pos = {homepos[0], homepos[1], homepos[2], homepos[3]};
    std::vector<PrinterRail *> rails = {rail};
    return homing_state.home_rails(rails, force_pos, home_pos, axis); // 返回是否成功
}

void CartKinematics::home(Homing &homing_state)
{
    std::vector<int> axes = homing_state.get_axes();
    homing_state_callback_call(HOMING_STATE_START);
    for (auto axis : axes)
    {
        homing_state_callback_call(HOMING_STATE_X_BEGIN + axis);
        if (m_dual_carriage_axis == axis)
        {
            PrinterRail *dc1 = m_dual_carriage_rails[0];
            PrinterRail *dc2 = m_dual_carriage_rails[1];
            int altc = (m_rails[axis] == dc2);
            _activate_carriage(0);
            home_axis(homing_state, axis, dc1);
            _activate_carriage(1);
            home_axis(homing_state, axis, dc2);
            _activate_carriage(altc);
        }
        else
        {
            if (home_axis(homing_state, axis, m_rails[axis]) == false)
            {
                homing_state_callback_call(HOMING_STATE_X_FALT + axis);
                throw std::runtime_error("G28 error");
            }
            else
            {
                homing_state_callback_call(HOMING_STATE_X_SUCC + axis);
            }
        }
    }
    homing_state_callback_call(HOMING_STATE_COMPLETE);
}

void CartKinematics::motor_off(double print_time)
{
    m_limits = {{1.0, -1.0}, {1.0, -1.0}, {1.0, -1.0}};
}

bool CartKinematics::check_endstops(Move &move) //-CS-3-move-2task-G-G--UI_control_task-------
{
    // std::vector<double> end_pos{move.m_end_pos[0], move.m_end_pos[1], move.m_end_pos[2], move.m_end_pos[3]};
    // for (int i = 0; i < 3; i++)
    // {
    //     if (move.m_axes_d[i] && (end_pos[i] < m_limits[i][0] || end_pos[i] > m_limits[i][1]))
    //     {
    //         if (m_limits[i][0] > m_limits[i][1])
    //         {
    //             LOG_W("%s\n", move.move_error("Must home axis first").c_str());
    //             // homing_state_callback_call(WARNING_MOVE_HOME_FIRST);
    //             return true;
    //         }
    //         else if (end_pos[i] < m_limits[i][0])
    //         {
    //             std::vector<double> temp_end_pos{move.m_end_pos[0], move.m_end_pos[1], move.m_end_pos[2], move.m_end_pos[3]};
    //             temp_end_pos[i] = m_limits[i][0];
    //             move.reset_endpos(temp_end_pos);
    //             LOG_W("%s\n", move.move_error().c_str());
    //         }
    //         else if (end_pos[i] > m_limits[i][1])
    //         {
    //             std::vector<double> temp_end_pos{move.m_end_pos[0], move.m_end_pos[1], move.m_end_pos[2], move.m_end_pos[3]};
    //             temp_end_pos[i] = m_limits[i][1];
    //             move.reset_endpos(temp_end_pos);
    //             LOG_W("%s\n", move.move_error().c_str());
    //         }
    //         else
    //         {
    //             return false;
    //         }
    //     }
    //     if (m_limits[i][0] == SOFT_ENDSTOPS_MIN || m_limits[i][1] == SOFT_ENDSTOPS_MAX)
    //     {
    //         serial_info("The soft endstops for this axis (MAX or MIN) has been disabled");
    //     }
    // }
    return true;
}

bool CartKinematics::check_move(Move &move)
{
    //  GAM_DEBUG_send_UI("2-80-\n" );
    std::vector<std::vector<double>> limits(3);
    limits = m_limits;

    double xpos = move.m_end_pos[0];
    double ypos = move.m_end_pos[1];
#if SOFT_LIMIT_ENABLE
    if (is_open_soft_limit == true && (xpos < limits[0][0] || xpos > limits[0][1] || ypos < limits[1][0] || ypos > limits[1][1]))
    {
        if (!check_endstops(move))
        {
            return false;
        }
        // GAM_ERR_printf("-ERR-%f---%f--\n",xpos, ypos);
    }
#endif
    if (fabs(move.m_axes_d[2]) <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        return true;
    }
#if SOFT_LIMIT_ENABLE
    if (is_open_soft_limit == true && !check_endstops(move))
    {
        return false;
    }
#endif
    double z_ratio = move.m_move_d / fabs(move.m_axes_d[2]);                     // s/z
    move.limit_speed(move, m_max_z_velocity * z_ratio, m_max_z_accel * z_ratio); //--4-move-2task-G-G--UI_control_task--根据Z轴移动相对距离限速
    return true;
}

std::map<std::string, std::string> CartKinematics::get_status(double eventtime)
{
    std::vector<std::string> axis = {"x", "y", "z"};
    std::vector<std::string> axes;
    for (int i = 0; i < m_limits.size(); i++)
    {
        if (m_limits[i][0] <= m_limits[i][1])
        {
            axes.push_back(axis[i]);
        }
    }
    std::map<std::string, std::string> ret;
    ret["homed_axes"] = "";
    for (int i = 0; i < axes.size(); i++)
    {
        ret["homed_axes"] += axes[i] + ",";
    }
    for (int i = 0; i < m_axes_max.size(); i++)
    {
        ret["axis_minimum"] += std::to_string(m_axes_min[i]) + ",";
        ret["axis_maximum"] += std::to_string(m_axes_max[i]) + ",";
    }
    return ret;
}

void CartKinematics::_activate_carriage(int carriage)
{
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    PrinterRail *dc_rail = m_dual_carriage_rails[carriage];
    int dc_axis = m_dual_carriage_axis;
    m_rails[dc_axis]->set_trapq(NULL);
    dc_rail->set_trapq(Printer::GetInstance()->m_tool_head->get_trapq());
    m_rails[dc_axis] = dc_rail;
    std::vector<double> ret_pos = Printer::GetInstance()->m_tool_head->get_position();
    std::vector<double> pos = {ret_pos[0], ret_pos[1], ret_pos[2], ret_pos[3]};
    pos[dc_axis] = dc_rail->m_steppers[0]->get_commanded_position();
    Printer::GetInstance()->m_tool_head->set_position(pos);
    if (m_limits[dc_axis][0] <= m_limits[dc_axis][1])
        m_limits[dc_axis] = dc_rail->get_range();
}

void CartKinematics::cmd_SET_DUAL_CARRIAGE(GCodeCommand &gcmd)
{
    int carriage = gcmd.get_int("CARRIAGE", INT32_MIN, 0, 1);
    _activate_carriage(carriage);
}
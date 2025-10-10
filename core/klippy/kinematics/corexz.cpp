#include "corexz.h"
#include "Define.h"
#include "klippy.h"

CoreXZKinematics::CoreXZKinematics(std::string section_name) : Kinematics()
{
    // Setup axis rails
    m_rails.push_back(new PrinterRail("stepper_x"));
    m_rails.push_back(new PrinterRail("stepper_y"));
    m_rails.push_back(new PrinterRail("stepper_z"));
    m_rails[0]->get_endstops()[0]->add_stepper(m_rails[2]->get_steppers()[0]);
    m_rails[2]->get_endstops()[0]->add_stepper(m_rails[0]->get_steppers()[0]);
    m_rails[0]->setup_itersolve('+');
    m_rails[1]->setup_itersolve('y');
    m_rails[2]->setup_itersolve('-');
    std::vector<std::vector<MCU_stepper*>> steppers = get_steppers();
    for (int i = 0; i < steppers.size(); i++)
    {
        for (int j = 0; j < steppers[i].size(); j++)
        {
            steppers[i][j]->set_trapq(Printer::GetInstance()->m_tool_head->get_trapq());
            Printer::GetInstance()->m_tool_head->register_step_generator(std::bind(&MCU_stepper::generate_steps, steppers[i][j], std::placeholders::_1));
        }
    }
    Printer::GetInstance()->register_event_double_handler("stepper_enable:motor_off:CoreXZKinematics", std::bind(&CoreXZKinematics::motor_off, this, std::placeholders::_1)); //---??---CoreXZKinematics
    // Setup boundary checks
    std::vector<double> max_velocitys = Printer::GetInstance()->m_tool_head->get_max_velocity();
    double max_velocity = max_velocitys[0];
    double max_accel = max_velocitys[1];
    m_max_z_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_z_velocity", max_velocity, DBL_MIN, max_velocity, 0.);
    m_max_z_accel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_z_accel" , max_accel, DBL_MIN, max_accel, 0.);
    m_limits = {{1.0, -1.0}, {1.0, -1.0}, {1.0, -1.0}};
    std::vector<std::vector<double>> ranges;
    for (int i = 0; i < m_rails.size(); i++)
    {
        ranges.push_back(m_rails[i]->get_range());
    }
    for (int i = 0; i < ranges.size(); i++)
    {
        m_axes_min[i] = ranges[i][0];
        m_axes_max[i] = ranges[i][1];
    }
    m_axes_min.push_back(0.);
    m_axes_max.push_back(0.);
}

CoreXZKinematics::~CoreXZKinematics()
{
    for (int i = 0; i < m_rails.size(); i++)
    {
        if (m_rails[i] != nullptr)
        {
            delete m_rails[i];
        }
    }
}

std::vector<std::vector<MCU_stepper*>> CoreXZKinematics::get_steppers()
{
    std::vector<std::vector<MCU_stepper*>> all_steppers;
    for (int i = 0; i < m_rails.size(); i++)
    {
        all_steppers.push_back(m_rails[i]->get_steppers());
    }
    return all_steppers;
}

std::vector<double> CoreXZKinematics::calc_position(std::map<std::string, double> stepper_positions)
{
    std::vector<double> pos;
    for (int i = 0; i < m_rails.size(); i++)
    {
        pos.push_back(stepper_positions[m_rails[i]->m_steppers[0]->get_name()]);
    }
    std::vector<double> ret = {0.5 * (pos[0] + pos[1]), 0.5 * (pos[0] - pos[1]), pos[2]};
    return ret;
}

void CoreXZKinematics::set_position(double newpos[3], std::vector<int> homing_axes)
{
    for (int i = 0; i < m_rails.size(); i++)
    {
        m_rails[i]->set_position(newpos);
        if (std::find(homing_axes.begin(), homing_axes.end(), homing_axes[i]) != homing_axes.end())
        {
            m_limits[i][0] = m_rails[i]->get_range()[0];
            m_limits[i][1] = m_rails[i]->get_range()[1];
        }
    }
}

void CoreXZKinematics::note_z_not_homed()
{
    m_limits[2][0] = 1.0;
    m_limits[2][1] = -1.0;
}

void CoreXZKinematics::home(Homing &homing_state)
{  
    // Each axis is homed independently and in order
    std::vector<int> axes = homing_state.get_axes();
    for (int axis = 0; axis < axes.size(); axis++)
    {
        PrinterRail* rail = m_rails[axis];
        // Determine movement
        double position_min = rail->get_range()[0];
        double position_max = rail->get_range()[1];
        struct homingInfo hi = rail->get_homing_info();
        std::vector<double> homepos = {DBL_MIN, DBL_MIN, DBL_MIN, DBL_MIN};
        homepos[axis] = hi.position_endstop;
        std::vector<double> forcepos = homepos;
        if (hi.positive_dir)
            forcepos[axis] -= 1.5 * (hi.position_endstop - position_min);
        else
            forcepos[axis] += 1.5 * (position_max - hi.position_endstop);
        // Perform homing
        std::vector<double> force_pos = {forcepos[0], forcepos[1], forcepos[2], forcepos[3]};
        std::vector<double> home_pos = {homepos[0], homepos[1], homepos[2], homepos[3]};
        std::vector<PrinterRail*> rails = {rail};
        homing_state.home_rails(rails, force_pos, home_pos, axis);   //--3-home-2task-G-G--UI_control_task--
    }   
}

void CoreXZKinematics::motor_off(double print_time)
{
    m_limits = {{1.0, -1.0}, {1.0, -1.0}, {1.0, -1.0}};
}

bool CoreXZKinematics::check_endstops(Move& move)      //-CS-3-move-2task-G-G--UI_control_task-------
{
    std::vector<double> end_pos{move.m_end_pos[0], move.m_end_pos[1], move.m_end_pos[2], move.m_end_pos[3]};
    for(int i = 0; i <3; i++)
    {
        if(move.m_axes_d[i] && (end_pos[i] < m_limits[i][0] || end_pos[i] > m_limits[i][1]))
        {
            if(m_limits[i][0] > m_limits[i][1])
            {
                // serial_error(move.move_error("Must home axis first"));
                return false;
            }
            // serial_error(move.move_error());
            return false;
        }
        if (m_limits[i][0] == SOFT_ENDSTOPS_MIN || m_limits[i][1] == SOFT_ENDSTOPS_MAX) {
            // serial_info("The soft endstops for this axis (MAX or MIN) has been disabled");
            return true;
        }
    }
    return true;
}

bool CoreXZKinematics::check_move(Move& move)        
{
    std::vector<std::vector<double>> limits(3);
    limits = m_limits;
     
    double xpos = move.m_end_pos[0];
    double ypos = move.m_end_pos[1];
    if (xpos < limits[0][0] || xpos > limits[0][1] || ypos < limits[1][0] || ypos > limits[1][1])
    {
        if(!check_endstops(move))
        {
            return false;
        }
        // GAM_ERR_printf("-ERR-%f---%f--\n",xpos, ypos);
    }
    if (move.m_axes_d[2] <= 1e-15)       //--IS_DOUBLE_ZERO----------
    {
        return true;
    }  
    if(!check_endstops(move))
    {
        return false;
    }    
    double z_ratio = move.m_move_d / fabs(move.m_axes_d[2]);     //s/z
    move.limit_speed(move, m_max_z_velocity * z_ratio, m_max_z_accel * z_ratio);      //--4-move-2task-G-G--UI_control_task--根据Z轴移动相对距离限速
    return true;
}

std::map<std::string, std::string> CoreXZKinematics::get_status(double eventtime)
{
    std::vector<std::string> axis = {"x", "y", "z"};
    std::vector<std::string> axes;
    for (int i = 0; i < m_limits.size(); i++)
    {
        for (int j = 0; j < m_limits[i].size(); j++)
        {
            if (m_limits[i][0] <= m_limits[i][1])
            {
                axes.push_back(axis[i]);
            }
        }
    }
    std::map<std::string, std::string> ret;
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
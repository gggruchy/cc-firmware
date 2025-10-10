#include "force_move.h"
#include "klippy.h"
#include "Define.h"
struct calc_move_time_ret calc_move_time(double dist, double speed, double accel)
{
    struct calc_move_time_ret ret;
    double axis_r = 1.0;
    if (dist < 0.0)
    {
        axis_r = -1;
        dist = -dist;
    }
    if (!accel || !dist)
    {
        ret = {
            .axis_r = axis_r,
            .accel_t = 0,
            .cruise_t = dist / speed,
            .speed = speed,
        };
        return ret;
    }
    double max_cruise_v2 = dist * accel;
    if (max_cruise_v2 < pow(speed, 2))          //距离太短，速度不能加速到指定速度并减速到0
        speed = sqrt(max_cruise_v2);
    double accel_t = speed / accel;
    double accel_decel_d = accel_t * speed;
    double cruise_t = (dist - accel_decel_d) / speed;

    ret = {
        .axis_r = axis_r,
        .accel_t = accel_t,
        .cruise_t = cruise_t,
        .speed = speed,
    };
    return ret;
}

ForceMove::ForceMove(std::string section_name)
{
    trapq = trapq_alloc();
    sk = cartesian_stepper_alloc('x');
    cmd_SET_KINEMATIC_POSITION_help = "Force a low-level kinematic position";
    cmd_FORCE_MOVE_help = "Manually move a stepper; invalidates kinematics";
    cmd_STEPPER_BUZZ_help = "Oscillate a given stepper to help id it";
    Printer::GetInstance()->m_gcode->register_command("STEPPER_BUZZ", std::bind(&ForceMove::cmd_STEPPER_BUZZ, this, std::placeholders::_1), false, cmd_STEPPER_BUZZ_help);
    if (Printer::GetInstance()->m_pconfig->GetBool(section_name, "enable_force_move", false)) 
    {
        Printer::GetInstance()->m_gcode->register_command("FORCE_MOVE", std::bind(&ForceMove::cmd_FORCE_MOVE, this, std::placeholders::_1), false, cmd_FORCE_MOVE_help);
        Printer::GetInstance()->m_gcode->register_command("SET_KINEMATIC_POSITION", std::bind(&ForceMove::cmd_SET_KINEMATIC_POSITION, this, std::placeholders::_1), false, cmd_SET_KINEMATIC_POSITION_help);
    }
}

ForceMove::~ForceMove()
{
    trapq_free(trapq);
    free(sk);
}


void ForceMove::register_stepper(MCU_stepper *stepper)
{
    std::string name = stepper->get_name();
    if (name == "stepper_x")
    {
        steppers[0] = stepper;
    }
    else if(name == "stepper_y")
    {
        steppers[1] = stepper;
    }
    else if(name == "stepper_z")
    {
        steppers[2] = stepper;
    }
}

MCU_stepper *ForceMove::lookup_stepper(std::string name)
{
    if (name == "stepper_x")
    {
        return steppers[0];
    }
    else if(name == "stepper_y")
    {
        return steppers[1];
    }
    else if(name == "stepper_z")
    {
        return steppers[2];
    }
}

bool ForceMove::force_enable(MCU_stepper *stepper)
{
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    std::string name = stepper->get_name();
    PrinterStepperEnable *stepper_enable;
    EnableTracking* enable = Printer::GetInstance()->m_stepper_enable->lookup_enable(name);
    bool was_enable = enable->is_motor_enabled();
    if (!was_enable)
    {
        enable->motor_enable(print_time);
        Printer::GetInstance()->m_tool_head->dwell(STALL_TIME);
    }
    return was_enable;
}

void ForceMove::restore_enable(MCU_stepper *stepper, bool was_enable)
{
    Printer::GetInstance()->m_tool_head->dwell(STALL_TIME);
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    std::string name = stepper->get_name();
    EnableTracking* enable = Printer::GetInstance()->m_stepper_enable->lookup_enable(name);
    enable->motor_disable(print_time);
    Printer::GetInstance()->m_tool_head->dwell(STALL_TIME);
}

void ForceMove::manual_move(MCU_stepper *stepper, double dist, double speed, double accel)
{
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    struct stepper_kinematics *prev_sk = stepper->set_stepper_kinematics(sk);
    struct trapq *prev_trapq = stepper->set_trapq(trapq);
    double pos[3] = {0, 0, 0};
    stepper->set_position(pos);
    calc_move_time_ret move_time = calc_move_time(dist, speed, accel);
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    trapq_append(trapq, print_time, move_time.accel_t, move_time.cruise_t, move_time.accel_t, 0, 0, 0, move_time.axis_r, 0, 0, 0, move_time.speed, accel);
    print_time = print_time + move_time.accel_t + move_time.cruise_t + move_time.accel_t;
    stepper->generate_steps(print_time);
    trapq_free_moves(trapq, print_time + 99999.9);
    stepper->set_trapq(prev_trapq);
    stepper->set_stepper_kinematics(prev_sk);
    Printer::GetInstance()->m_tool_head->note_kinematic_activity(print_time);
    Printer::GetInstance()->m_tool_head->dwell(move_time.accel_t + move_time.cruise_t + move_time.accel_t);
}

MCU_stepper *ForceMove::_lookup_stepper(GCodeCommand &gcmd)//_lookup_stepper(gcmd) //---??---
{
    std::string step_name = gcmd.get_string("STEPPER", "");
    if (step_name == "stepper_x")
    {
        return Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->m_steppers[0];
    }
    else if(step_name == "stepper_y")
    {
        return Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->m_steppers[1];
    }
    else if(step_name == "stepper_z")
    {
        return Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_steppers[2];
    }
}

void ForceMove::cmd_STEPPER_BUZZ(GCodeCommand &gcmd) //cmd_STEPPER_BUZZ(gcmd)  //---??---
{
    MCU_stepper *stepper = _lookup_stepper(gcmd);
    bool was_enable = force_enable(stepper);
    double dist = BUZZ_DISTANCE;
    double speed = BUZZ_VELOCITY;
    if (stepper->m_units_in_radians)
    {
        dist = BUZZ_RADIANS_DISTANCE;
        speed = BUZZ_RADIANS_VELOCITY;
    }
    for (int i= 0; i < 10; i++)
    {
        manual_move(stepper, dist, speed);
        Printer::GetInstance()->m_tool_head->dwell(0.050);
        manual_move(stepper, -dist, speed);
        Printer::GetInstance()->m_tool_head->dwell(0.450);
    }
    restore_enable(stepper, was_enable);
}

void ForceMove::cmd_FORCE_MOVE(GCodeCommand &gcmd)  //cmd_FORCE_MOVE(gcmd) //---??---
{
    MCU_stepper *stepper = _lookup_stepper(gcmd);
    double distance = gcmd.get_double("DISTANCE", DBL_MIN);
    double speed = gcmd.get_double("VELOCITY", DBL_MIN, DBL_MIN, DBL_MAX, 0);
    double accel = gcmd.get_double("ACCEL", 0.0, DBL_MIN, DBL_MAX, 0);
    force_enable(stepper);
    manual_move(stepper, distance, speed, accel);
}

void ForceMove::cmd_SET_KINEMATIC_POSITION(GCodeCommand &gcmd) //cmd_SET_KINEMATIC_POSITION(gcmd) //---??---
{
    Printer::GetInstance()->m_tool_head->get_last_move_time();
    std::vector<double> curpos = Printer::GetInstance()->m_tool_head->get_position();
    double x = gcmd.get_double("X", curpos[0]);
    double y = gcmd.get_double("Y", curpos[1]);
    double z = gcmd.get_double("Z", curpos[2]);
    std::vector<double> pos = {x, y, z, curpos[3]};
    std::vector<int> axes = {0, 1, 2};
    Printer::GetInstance()->m_tool_head->set_position(pos, axes);
}

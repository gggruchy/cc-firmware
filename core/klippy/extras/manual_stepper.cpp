#include "manual_stepper.h"
#include "klippy.h"

ManualStepper::ManualStepper(std::string section_name)
{
    if (Printer::GetInstance()->m_pconfig->GetString(section_name, "endstop_pin", "") != "")
    {
        m_can_home = true;
        m_rail = new PrinterRail(section_name, false, 0.);
        m_steppers = m_rail->get_steppers();
    }
    else
    {
        m_can_home = false;
        m_rail = new PrinterRail(section_name, false, 0.);
        m_steppers = m_rail->get_steppers();  //
    }
        
    m_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "velocity", 5., DBL_MIN, DBL_MAX, 0.);
    m_accel = m_homing_accel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "accel", 0., 0.);
    m_next_cmd_time = 0.;
    // Setup iterative solver
    m_trapq = trapq_alloc();
    m_rail->setup_itersolve('x');
    m_rail->set_trapq(m_trapq);
    // Register commands
    std::string stepper_name = section_name;
    m_cmd_MANUAL_STEPPER_help = "Command a manually configured stepper";
    Printer::GetInstance()->m_gcode->register_mux_command("MANUAL_STEPPER", "STEPPER", stepper_name, std::bind(&ManualStepper::cmd_MANUAL_STEPPER, this, std::placeholders::_1), m_cmd_MANUAL_STEPPER_help);
}

ManualStepper::~ManualStepper()
{
    if (m_trapq != nullptr)
    {
        trapq_free(m_trapq);
    }
}

void ManualStepper::sync_print_time()
{
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    if (m_next_cmd_time > print_time)
    {
        Printer::GetInstance()->m_tool_head->dwell(m_next_cmd_time - print_time);
    }
    else
    {
        m_next_cmd_time = print_time;
    }
}
    
void ManualStepper::do_enable(bool enable)
{
    sync_print_time();
    EnableTracking*se;
    if (enable)
    {
        for (int i = 0; i < m_steppers.size(); i++)
        {
            se = Printer::GetInstance()->m_stepper_enable->lookup_enable(m_stepper_name);
            se->motor_enable(m_next_cmd_time);
        }
    }
    else
    {
        for (int i = 0; i < m_steppers.size(); i++)
        {
            se = Printer::GetInstance()->m_stepper_enable->lookup_enable(m_stepper_name);
            se->motor_disable(m_next_cmd_time);
        }
    }
    sync_print_time();
}


void ManualStepper::do_set_position(double setpos)
{
    double pos[3] = {setpos, 0.0, 0.0};
    m_rail->set_position(pos);
}

void ManualStepper::do_move(double movepos, double speed, double accel, bool sync)
{
    sync_print_time();
    double cp = m_rail->m_steppers[0]->get_commanded_position();
    double dist = movepos - cp;
    struct calc_move_time_ret ret = calc_move_time(dist, speed, accel);
    trapq_append(m_trapq, m_next_cmd_time, ret.accel_t, ret.cruise_t, ret.accel_t, cp, 0., 0., 
                ret.axis_r, 0., 0.,0., ret.speed, m_accel);
    m_next_cmd_time = m_next_cmd_time + ret.accel_t + ret.cruise_t + ret.accel_t;
    m_steppers[0]->generate_steps(m_next_cmd_time);
    trapq_free_moves(m_trapq, m_next_cmd_time + 99999.9);
    Printer::GetInstance()->m_tool_head->note_kinematic_activity(m_next_cmd_time);
    if (sync)
    {
        sync_print_time();
    }
}

void ManualStepper::do_homing_move(double movepos, double speed, double accel, bool triggered, bool check_trigger)
{
    if (!m_can_home)
    {
        printf("No endstop for this manual stepper\n");
    }
    m_homing_accel = accel;
    std::vector<double> pos = {movepos, 0, 0, 0};
    std::vector<MCU_endstop*> endstops = Printer::GetInstance()->m_tool_head->m_kin->m_rails[m_stepper_index]->m_endstops;
    Printer::GetInstance()->m_printer_homing->manual_home(endstops, pos, speed, triggered, check_trigger);
}

    
void ManualStepper::cmd_MANUAL_STEPPER(GCodeCommand& gcmd)
{
    int enable = gcmd.get_int("ENABLE", DBL_MIN);
    if (enable)
    {
        do_enable(enable);
    }
    double setpos = gcmd.get_double("SET_POSITION", DBL_MIN);
    if (setpos)
    {
        do_set_position(setpos);
    }
    double speed = gcmd.get_double("SPEED", m_velocity, DBL_MIN, DBL_MAX, 0.);
    double accel = gcmd.get_double("ACCEL", m_accel, 0.);
    int homing_move = gcmd.get_int("STOP_ON_ENDSTOP", 0);
    if (homing_move)
    {
        double movepos = gcmd.get_double("MOVE", DBL_MIN);
        bool triggered = homing_move > 0;
        bool check_trigger = abs(homing_move) == 1;
        do_homing_move(movepos, speed, accel, triggered, check_trigger);
    }
    else if(gcmd.get_double("MOVE", DBL_MIN))
    {
        float movepos = gcmd.get_double("MOVE", DBL_MIN);
        int sync = gcmd.get_int("SYNC", 1);
        do_move(movepos, speed, accel, sync);
    }
    else if(gcmd.get_int("SYNC", 0))
    {
        sync_print_time();
    }
    
}
void ManualStepper::flush_step_generation()
{
    sync_print_time();
}
void ManualStepper::get_position()
{
    double pos = m_rail->m_steppers[0]->get_commanded_position();
    std::vector<double> pos_ret = {pos, 0, 0, 0};
}
void ManualStepper::set_position(double newpos[3])
{
    do_set_position(newpos[0]);
}
double ManualStepper::get_last_move_time()
{
    sync_print_time();
    return m_next_cmd_time;
}

void ManualStepper::dwell(double delay)
{
    m_next_cmd_time += std::max(0.0, delay);
}

void ManualStepper::drip_move(double newpos[3], double speed, double drip_completion)
{
    do_move(newpos[0], speed, m_homing_accel);
}

ManualStepper* ManualStepper::get_kinematics()
{
    return this;
}
     
std::vector<MCU_stepper*> ManualStepper::get_steppers()
{
    return m_steppers;
}
        
#include "stepper.h"
#include "klippy.h"

MCU_stepper::MCU_stepper(std::string &stepper_name, pinParams *step_pin_params, pinParams *dir_pin_params, double step_dist, bool units_in_radians)
{
    m_name = stepper_name;
    m_step_dist = step_dist;
    m_units_in_radians = units_in_radians;
    m_mcu = (MCU *)step_pin_params->chip;
    m_oid = m_mcu->create_oid();
    m_mcu->register_config_callback(std::bind(&MCU_stepper::build_config, this, std::placeholders::_1));
    m_step_pin = m_mcu->m_serial->m_msgparser->m_pinMap[step_pin_params->pin]; // m_step_pin = step_pin_params.pin;
    m_invert_step = step_pin_params->invert;
    m_dir_pin = m_mcu->m_serial->m_msgparser->m_pinMap[dir_pin_params->pin]; // m_dir_pin = dir_pin_params.pin;
    m_invert_dir = dir_pin_params->invert;
    m_mcu_position_offset = 0;
    // m_reset_cmd_tag = None;
    // m_get_position_cmd = None;
    // m_active_callbacks = []; //---??---MCU_stepper
    m_step_queue = stepcompress_alloc(m_oid);
    m_mcu->register_stepqueue(m_step_queue);
    m_sk = nullptr;
    is_enable_motor = false;
}
MCU_stepper::~MCU_stepper()
{
    if (m_step_queue != nullptr)
    {
        stepcompress_free(m_step_queue);
    }
    if (m_sk == NULL)
    {
        free(m_sk);
    }
}

MCU *MCU_stepper::get_mcu()
{
    return m_mcu;
}

std::string MCU_stepper::get_name(bool shorter)
{
    return m_name;
}

bool MCU_stepper::units_in_radians()
{
    // Returns true if distances are in radians instead of millimeters
    return m_units_in_radians;
}

double MCU_stepper::dist_to_time(double dist, double start_velocity, double accel)
{
    // Calculate the time it takes to travel a distance with constant accel
    double time_offset = start_velocity / accel;
    return sqrt(2. * dist / accel + time_offset * time_offset) - time_offset;
}

extern struct stepper_kinematics *cartesian_stepper_alloc(char axis);
extern struct stepper_kinematics *corexy_stepper_alloc(char type);
extern struct stepper_kinematics *delta_stepper_alloc(double arm2, double tower_x, double tower_y);

void MCU_stepper::setup_itersolve(char axis)
{
    stepper_kinematics *sk;
    if (axis == '+' || axis == '-')
    {
        sk = corexy_stepper_alloc(axis);
    }
    else
    {
        sk = cartesian_stepper_alloc(axis);
    }
    set_stepper_kinematics(sk);
}

void MCU_stepper::setup_itersolve(double arm2, double tower_x, double tower_y)
{

    stepper_kinematics *sk = delta_stepper_alloc(arm2, tower_x, tower_y);
    set_stepper_kinematics(sk);
}

void MCU_stepper::build_config(int para)
{
    m_step_pulse_duration = .000002;

    if (para & 1)
    {
        std::stringstream config_stepper;
        config_stepper << "config_stepper oid=" << m_oid << " step_pin=" << m_step_pin << " dir_pin=" << m_dir_pin << " invert_step=" << m_invert_step << " step_pulse_ticks=" << m_mcu->seconds_to_clock(m_step_pulse_duration);
        m_mcu->add_config_cmd(config_stepper.str());
        int32_t step_cmd_tag = m_mcu->m_serial->m_msgparser->m_format_to_id.at("queue_step oid=%c interval=%u count=%hu add=%hi");
        int32_t dir_cmd_tag = m_mcu->m_serial->m_msgparser->m_format_to_id.at("set_next_step_dir oid=%c dir=%c");
        m_reset_cmd_tag = m_mcu->m_serial->m_msgparser->m_format_to_id.at("reset_step_clock oid=%c clock=%u");

        double max_error = m_mcu->m_max_stepper_error;
        stepcompress_fill(m_step_queue, m_mcu->seconds_to_clock(max_error), m_invert_dir, step_cmd_tag, dir_cmd_tag);
    }
    if (para & 4)
    {
        std::stringstream reset_step_clock;
        reset_step_clock << "reset_step_clock oid=" << m_oid << " clock=0";
        m_mcu->add_config_cmd(reset_step_clock.str(), false, true);
    }
}

int MCU_stepper::get_oid()
{
    return m_oid;
}

double MCU_stepper::get_step_dist()
{
    return m_step_dist;
}

void MCU_stepper::set_step_dist(double dist)
{
    int64_t mcu_pos = get_mcu_position();
    m_step_dist = dist;
    set_stepper_kinematics(m_sk);
    set_mcu_position(mcu_pos);
}

uint32_t MCU_stepper::is_dir_inverted()
{
    return m_invert_dir;
}

double MCU_stepper::calc_position_from_coord(std::vector<double> &coord)
{
    double pos = itersolve_calc_position_from_coord(m_sk, coord[0], coord[1], coord[2]);
    return pos;
}

void MCU_stepper::set_position(double coord[3])
{
    uint64_t mcu_pos = get_mcu_position();
    stepper_kinematics *sk = m_sk;
    itersolve_set_position(sk, coord[0], coord[1], coord[2]);
    set_mcu_position(mcu_pos);
}

double MCU_stepper::get_commanded_position()
{
    double pos = itersolve_get_commanded_pos(m_sk);
    return pos;
}

int64_t MCU_stepper::get_mcu_position()
{
    double mcu_pos_dist = get_commanded_position() + m_mcu_position_offset;
    // std::cout << "get_commanded_position() = " << get_commanded_position() << std::endl;
    // std::cout << "m_mcu_position_offset = " << m_mcu_position_offset << std::endl;
    double mcu_pos = mcu_pos_dist / m_step_dist;
    if (mcu_pos >= 0)
    {
        return (int64_t)(mcu_pos + 0.5);
    }
    return (int64_t)(mcu_pos - 0.5);
}

void MCU_stepper::set_mcu_position(int64_t mcu_pos)
{
    double mcu_pos_dist = mcu_pos * m_step_dist;
    m_mcu_position_offset = mcu_pos_dist - get_commanded_position();
}

int64_t MCU_stepper::get_past_mcu_position(double print_time)
{
    uint64_t clock = m_mcu->print_time_to_clock(print_time);
    return stepcompress_find_past_position(m_step_queue, clock);
}

double MCU_stepper::get_past_commanded_position(double print_time)
{
    int64_t mcu_pos = get_past_mcu_position(print_time);
    return mcu_pos * m_step_dist - m_mcu_position_offset;
}

double MCU_stepper::mcu_to_command_position(double mcu_pos)
{
    return mcu_pos * m_step_dist - m_mcu_position_offset;
}

std::vector<struct pull_history_steps> MCU_stepper::dump_steps(int count, uint64_t start_clock, uint64_t end_clock)
{

    struct pull_history_steps *data = new pull_history_steps[count];
    count = stepcompress_extract_old(m_step_queue, data, count, start_clock, end_clock);
    std::vector<struct pull_history_steps> vdata;
    for (int i = 0; i < count; i++)
    {
        vdata.push_back(data[i]);
    }
    return vdata;
}

stepper_kinematics *MCU_stepper::set_stepper_kinematics(stepper_kinematics *sk)
{

    stepper_kinematics *old_sk = m_sk;
    uint64_t mcu_pos = 0;

    if (old_sk != NULL)
    {

        mcu_pos = get_mcu_position();
    }
    m_sk = sk;
    itersolve_set_stepcompress(sk, m_step_queue, m_step_dist);
    set_trapq(m_tq);
    set_mcu_position(mcu_pos);
    return old_sk;
}

void MCU_stepper::note_homing_end(bool did_trigger)
{
    stepcompress_reset(m_step_queue, 0);
    uint32_t data[3] = {m_reset_cmd_tag, m_oid, 0}; //"reset_step_clock oid=%c clock=%u";
    int len = sizeof(data) / 4;
    stepcompress_queue_msg(m_step_queue, data, len);
    if (!did_trigger)
    {
        return;
    }

    std::stringstream stepper_get_position;
    stepper_get_position << "stepper_get_position oid=" << m_oid;
    ParseResult params = m_mcu->m_serial->send_with_response(stepper_get_position.str(), "stepper_position", m_mcu->m_serial->m_command_queue, m_oid);
    // std::cout << __func__ << "\t" << m_name << " " << params.PT_uint32_outs.at("pos") << std::endl;
    int32_t last_pos = (int32_t)params.PT_uint32_outs.at("pos");
    // std::cout << __func__ << "\t" << m_name << " last_pos:" << last_pos << std::endl;
    if (m_invert_dir)
    {
        last_pos = 0 - last_pos;
    }
    int ret = stepcompress_set_last_position(m_step_queue, last_pos);
    if (ret)
    {
    }
    set_mcu_position(last_pos);
}

trapq *MCU_stepper::set_trapq(trapq *tq)
{
    itersolve_set_trapq(m_sk, tq);
    trapq *old_tq = m_tq;
    m_tq = tq;
    return old_tq;
}

void MCU_stepper::add_active_callback(std::function<void(double)> cb)
{
    m_active_callbacks.push_back(cb);
}

void MCU_stepper::generate_steps(double flush_time) //--16-move-2task-G-G--UI_control_task--
{
    // Check for activity if necessary
    stepper_kinematics *sk;
    if (m_active_callbacks.size() > 0)
    {
        sk = m_sk;
        double ret_check = itersolve_check_active(sk, flush_time);
        if (ret_check)
        {
            for (int i = 0; i < m_active_callbacks.size(); i++)
            {
                m_active_callbacks[i](ret_check);
            }
            m_active_callbacks = std::vector<std::function<void(double)>>();
        }
    }
    // Generate steps
    sk = m_sk;
    int ret = itersolve_generate_steps(sk, flush_time); //---12-add_move-G-G---
    if (ret)
    {
        printf("Internal error in stepcompress\n");
    }
} //--2022.8.31--

int32_t MCU_stepper::is_active_axis(char axis)
{
    return itersolve_is_active_axis(m_sk, axis);
}

std::string MCU_stepper::stepper_index_to_name(int index)
{
    if (index == 0)
    {
        return "stepper_x";
    }
    else if (index == 1)
    {
        return "stepper_y";
    }
    else if (index == 2)
    {
        return "stepper_z";
    }
}

int MCU_stepper::stepper_index_to_name(std::string name)
{
    if (name == "stepper_x")
    {
        return 0;
    }
    else if (name == "stepper_y")
    {
        return 1;
    }
    else if (name == "stepper_z")
    {
        return 2;
    }
}

pin_info MCU_stepper::get_pin_info()
{
    pin_info info;
    info.dir_pin = m_dir_pin;
    info.step_pin = m_step_pin;
    info.invert_dir = m_invert_dir;
    info.invert_step = m_invert_step;
    return info;
}

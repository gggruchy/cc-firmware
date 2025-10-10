#include "delta.h"
#include "klippy.h"
#include "Define.h"
#include "mathutil.h"

#define SLOW_RATIO 3.
DeltaKinematics::DeltaKinematics(std::string section_name) : Kinematics()          //三角洲机型
{
    // Setup tower rails
    PrinterRail *rail_a = new PrinterRail("stepper_x", false);
    double a_endstop = rail_a->get_homing_info().position_endstop;
    PrinterRail *rail_b = new PrinterRail("stepper_y", false, a_endstop);
    PrinterRail *rail_c = new PrinterRail("stepper_z", false, a_endstop);
    m_rails = {rail_a, rail_b, rail_c};
    
    Printer::GetInstance()->register_event_double_handler("stepper_enable:motor_off", std::bind(&DeltaKinematics::motor_off, this, std::placeholders::_1));
    // Setup max velocity
    m_max_velocity = Printer::GetInstance()->m_tool_head->get_max_velocity()[0];
    m_max_accel = Printer::GetInstance()->m_tool_head->get_max_velocity()[1];
    m_max_z_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_z_velocity", m_max_velocity, m_max_velocity, DBL_MAX, 0.);
    // Read radius and arm lengths
    m_radius = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "delta_radius", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    double print_radius = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "print_radius", m_radius, DBL_MIN, DBL_MAX, 0.);
    double arm_length_a = Printer::GetInstance()->m_pconfig->GetDouble("stepper_a", "arm_length", DBL_MIN, DBL_MIN, DBL_MAX, m_radius);

    m_arm_lengths.push_back(Printer::GetInstance()->m_pconfig->GetDouble("stepper_a", "arm_length", arm_length_a, DBL_MIN, DBL_MAX, m_radius));
    m_arm_lengths.push_back(Printer::GetInstance()->m_pconfig->GetDouble("stepper_b", "arm_length", arm_length_a, DBL_MIN, DBL_MAX, m_radius));
    m_arm_lengths.push_back(Printer::GetInstance()->m_pconfig->GetDouble("stepper_c", "arm_length", arm_length_a, DBL_MIN, DBL_MAX, m_radius));

    m_arm2.push_back(pow(m_arm_lengths[0], 2));
    m_arm2.push_back(pow(m_arm_lengths[1], 2));
    m_arm2.push_back(pow(m_arm_lengths[2], 2));

    m_abs_endstops.push_back(m_rails[0]->get_homing_info().position_endstop + sqrt(m_arm2[0] - pow(m_radius, 2)));
    m_abs_endstops.push_back(m_rails[1]->get_homing_info().position_endstop + sqrt(m_arm2[1] - pow(m_radius, 2)));
    m_abs_endstops.push_back(m_rails[2]->get_homing_info().position_endstop + sqrt(m_arm2[2] - pow(m_radius, 2)));
    // Determine tower locations in cartesian space
    m_angles.push_back(Printer::GetInstance()->m_pconfig->GetDouble("stepper_a", "angle", 210.));
    m_angles.push_back(Printer::GetInstance()->m_pconfig->GetDouble("stepper_b", "angle", 330.));
    m_angles.push_back(Printer::GetInstance()->m_pconfig->GetDouble("stepper_c", "angle", 90.));

    for (int i = 0; i < m_angles.size(); i++)
    {
        std::vector<double> tower = {cos(m_angles[i] *M_PI / 180.0) * m_radius, sin(m_angles[i] *M_PI / 180.0) * m_radius};
        m_towers.push_back(tower);
    }
    for (int i = 0; i < m_rails.size(); i++)
    {
        m_rails[i]->setup_itersolve(m_arm2[i], m_towers[i][0], m_towers[i][1]);
    }
    std::vector<std::vector<MCU_stepper*>> steppers = get_steppers();
    for (int i = 0; i < steppers.size(); i++)
    {
        for (int j = 0; j < steppers[i].size(); j++)
        {
            steppers[i][j]->set_trapq(Printer::GetInstance()->m_tool_head->get_trapq());
            Printer::GetInstance()->m_tool_head->register_step_generator(std::bind(&MCU_stepper::generate_steps, steppers[i][j], std::placeholders::_1));
        }
    }
    // Setup boundary checks
    m_need_home = true;
    m_limit_xy2 = -1.;
    m_home_position = _actuator_to_cartesian(m_abs_endstops);
    m_max_z = std::min(m_rails[0]->get_homing_info().position_endstop, std::min(m_rails[1]->get_homing_info().position_endstop, m_rails[2]->get_homing_info().position_endstop));
    m_min_z = Printer::GetInstance()->m_pconfig->GetDouble("printer", "minimum_z_position", 0, DBL_MIN, m_max_z);
    m_limit_z = std::min(m_abs_endstops[0] - m_arm_lengths[0], std::min(m_abs_endstops[1] - m_arm_lengths[1], m_abs_endstops[2] - m_arm_lengths[2]));

    // logging.info( "Delta max build height %.2fmm (radius tapered above %.2fmm)" % (m_max_z, m_limit_z))
    // Find the point where an XY move could result in excessive
    // tower movement
    for (int i = 0; i < m_rails.size(); i++)
    {
        m_rails[i]->get_steppers()[0]->get_step_dist();
    }
    double half_min_step_dist = std::min(m_rails[0]->get_steppers()[0]->get_step_dist(), std::min(m_rails[1]->get_steppers()[0]->get_step_dist(), m_rails[2]->get_steppers()[0]->get_step_dist())) * 0.5;
    double min_arm_length = std::min(m_arm_lengths[0], std::min(m_arm_lengths[1], m_arm_lengths[2]));
    m_slow_xy2 = pow(ratio_to_xy(SLOW_RATIO, min_arm_length, half_min_step_dist, m_radius), 2);
    m_very_slow_xy2 = pow(ratio_to_xy(2. * SLOW_RATIO, min_arm_length, half_min_step_dist, m_radius), 2);
    m_max_xy2 = std::min(print_radius, std::min(min_arm_length - m_radius, pow(ratio_to_xy(4. * SLOW_RATIO, min_arm_length, half_min_step_dist, m_radius), 2)));
    double max_xy = sqrt(m_max_xy2);
    // logging.info("Delta max build radius %.2fmm (moves slowed past %.2fmm" " and %.2fmm)" % (max_xy, math.sqrt(m_slow_xy2), math.sqrt(m_very_slow_xy2)))
    std::vector<double> m_axes_min = {-max_xy, -max_xy, m_min_z, 0.};
    std::vector<double> m_axes_max = {max_xy, max_xy, m_max_z, 0.};
    double set_pos[3] = {0., 0., 0.};
    set_position(set_pos);
}

DeltaKinematics::~DeltaKinematics()
{
}

double DeltaKinematics::ratio_to_xy(double ratio, double min_arm_length, double half_min_step_dist, double radius)
{
    return (ratio * sqrt(pow(min_arm_length, 2) / (pow(ratio, 2) + 1.) - pow(half_min_step_dist, 2)) + half_min_step_dist - radius);
}
            
std::vector<std::vector<MCU_stepper*>> DeltaKinematics::get_steppers()
{
    std::vector<std::vector<MCU_stepper*>> all_steppers;
    for (int i = 0; i < m_rails.size(); i++)
    {
        all_steppers.push_back(m_rails[i]->get_steppers());
    }
    return all_steppers;
}

std::vector<double> DeltaKinematics::_actuator_to_cartesian(std::vector<double> spos)
{
    std::vector<std::vector<double>> sphere_coords;
    for (int i = 0; i < m_towers.size(); i++)
    {
        std::vector<double> coord = {m_towers[i][0], m_towers[i][1], spos[i]};
        sphere_coords.push_back(coord);
    }
    return trilateration(sphere_coords, m_arm2);
}

std::vector<double> DeltaKinematics::calc_position(std::map<std::string, double> stepper_positions)
{
    std::vector<double> pos;
    for (int i = 0; i < m_rails.size(); i++)
    {
        pos.push_back(stepper_positions[m_rails[i]->m_steppers[0]->get_name()]);
    }
    return _actuator_to_cartesian(pos);
}
        
void DeltaKinematics::set_position(double newpos[3], std::vector<int> homing_axes)
{
    for (int i = 0; i < m_rails.size(); i++)
    {
        m_rails[i]->set_position(newpos);
    }
    m_limit_xy2 = -1.;
    std::vector<int> axes = {0, 1, 2};
    if (homing_axes == axes)
    {
        m_need_home = false;
    }
}
        
void DeltaKinematics::home(Homing &homing_state)
{
    // All axes are homed simultaneously
    std::vector<int> axes = {0, 1, 2};
    homing_state.set_axes(axes);
    std::vector<double> forcepos = m_home_position;
    forcepos[2] = -1.5 * sqrt(*std::max_element(m_arm2.begin(), m_arm2.end())-m_max_xy2);
    homing_state.home_rails(m_rails, forcepos, m_home_position, 0);
}
        
void DeltaKinematics::motor_off(double print_time)
{
    m_limit_xy2 = -1.;
    m_need_home = true;
}
        
bool DeltaKinematics::check_move(Move& move)
{
    std::vector<double> end_pos = move.m_end_pos;
    double end_xy2 = pow(end_pos[0], 2) + pow(end_pos[1], 2);
    if (end_xy2 <= m_limit_xy2 && !move.m_axes_d[2])
    {
        // Normal XY move
        return true;
    }
    if (m_need_home)
    {
        return false;
        // raise move.move_error("Must home first")  //---??---
    }
    double end_z = end_pos[2];
    double limit_xy2 = m_max_xy2;
    if (end_z > m_limit_z)
    {
        limit_xy2 = std::min(limit_xy2, pow((m_max_z - end_z), 2));
    } 
    if (end_xy2 > limit_xy2 || end_z > m_max_z || end_z < m_min_z)
    {
        // Move out of range - verify not a homing move
        if ((end_pos[0] != m_home_position[0] && end_pos[1] != m_home_position[1]) || end_z < m_min_z || end_z > m_home_position[2])
        {
            // raise move.move_error() //---??---
        }
        limit_xy2 = -1.;
    }
    if (move.m_axes_d[2])
    {
        move.limit_speed(move, m_max_z_velocity, move.m_accel);
        limit_xy2 = -1.;
    } 
    // Limit the speed/accel of this move if is is at the extreme
    // end of the build envelope
    double extreme_xy2 = std::max(end_xy2, pow(move.m_start_pos[0], 2) + pow(move.m_start_pos[1], 2));
    if (extreme_xy2 > m_slow_xy2)
    {
        double r = 0.5;
        if (extreme_xy2 > m_very_slow_xy2)
            r = 0.25;
        double max_velocity = m_max_velocity;
        if (move.m_axes_d[2])
            max_velocity = m_max_z_velocity;
        move.limit_speed(move, max_velocity * r, m_max_accel * r);
        limit_xy2 = -1.;
    }
    m_limit_xy2 = std::min(limit_xy2, m_slow_xy2);
    return true;
}
        
std::map<std::string, std::string> DeltaKinematics::get_status(double eventtime)
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

// DeltaCalibration* DeltaKinematics::get_calibration()
// {
//     std::vector<double> endstops;
//     std::vector<double> stepdists;
//     for (int i = 0; i < m_rails.size(); i++)
//     {
//         endstops.push_back(m_rails[i]->get_homing_info().position_endstop);
//         stepdists.push_back(m_rails[i]->get_steppers()[0]->get_step_dist());
//     }
//     return new DeltaCalibration(m_radius, m_angles, m_arm_lengths, endstops, stepdists);
// }
        

// Delta parameter calibration for DELTA_CALIBRATE tool
DeltaCalibration::DeltaCalibration(double radius, std::vector<double> angles, std::vector<double> arms, std::vector<double> endstops, std::vector<double> stepdists)
{
    m_radius = radius;
    m_angles = angles;
    m_arms = arms;
    m_endstops = endstops;
    m_stepdists = stepdists;
    // Calculate the XY cartesian coordinates of the delta towers
    for (int i = 0; i < m_angles.size(); i++)
    {
        std::vector<double> tower = {cos(m_angles[i] *M_PI / 180.0) * m_radius, sin(m_angles[i] *M_PI / 180.0) * m_radius};
        m_towers.push_back(tower);
    }
    // Calculate the absolute Z height of each tower endstop
    double radius2 = pow(radius, 2);
    for (int i = 0; i < endstops.size(); i++)
    {
        m_abs_endstops.push_back(endstops[i] + sqrt(pow(arms[i], 2) - radius2));
    }
}

DeltaCalibration::~DeltaCalibration()
{

}

std::map<std::string, double> DeltaCalibration::coordinate_descent_params(bool is_extended)
{
    //Determine adjustment parameters (for use with coordinate_descent)
    std::vector<std::string> adj_params = {"radius", "angle_a", "angle_b", "endstop_a", "endstop_b", "endstop_c"};
    if (is_extended)
    {
        adj_params.push_back("arm_a");
        adj_params.push_back("arm_b");
        adj_params.push_back("arm_c");
    }
    std::map<std::string, double> params;
    params["radius"] =  m_radius;
    std::vector<std::string> abc = {"a", "b", "c"};
    for (int i = 0; i < 3; i++)
    {
        params["angle_"+ abc[i]] = m_angles[i];
        params["arm_"+ abc[i]] = m_arms[i];
        params["endstop_"+ abc[i]] = m_endstops[i];
        params["stepdist_"+ abc[i]] = m_stepdists[i];
    }
    return params;
}
       
DeltaCalibration *DeltaCalibration::new_calibration(std::map<std::string, double> params)
{
    // Create a new calibration object from coordinate_descent params
    double radius = params["radius"];
    std::vector<double> angles;
    std::vector<double> arms;
    std::vector<double> endstops;
    std::vector<double> stepdists;
    std::vector<std::string> abc = {"a", "b", "c"};
    for (int i = 0; i < 3; i++)
    {
        angles.push_back(params["angle_"+ abc[i]]);
        arms.push_back(params["arm_"+ abc[i]]);
        endstops.push_back(params["endstop_"+ abc[i]]);
        stepdists.push_back(params["stepdist_"+ abc[i]]);
    }
    return new DeltaCalibration(radius, angles, arms, endstops, stepdists);
}
        
std::vector<double> DeltaCalibration::get_position_from_stable(std::vector<double> stable_position)
{
    //Return cartesian coordinates for the given stable_position
    std::vector<std::vector<double>>  sphere_coords;
    for (int i = 0; i < 3; i++)
    {
        std::vector<double> coord = {m_towers[i][0], m_towers[i][1], m_abs_endstops[i] - stable_position[i] * m_stepdists[i]};
        sphere_coords.push_back(coord);
    }
    std::vector<double> arm2 = {pow(m_arms[0], 2), pow(m_arms[1], 2), pow(m_arms[2], 2)};
    return trilateration(sphere_coords, arm2);
}
        
std::vector<double> DeltaCalibration::calc_stable_position(std::vector<double> coord)
{
    //Return a stable_position from a cartesian coordinate
    std::vector<double> ret;
    for (int i = 0; i < 3; i++)
    {
        double pos_value = sqrt(pow(m_arms[i], 2) - pow((m_towers[i][0]-coord[0]), 2) - pow((m_towers[i][1]-coord[1]), 2)) + coord[2];
        double ret_value = (m_abs_endstops[i] - pos_value) / m_stepdists[i];
        ret.push_back(ret_value);
    }
    return ret;
}
        
void DeltaCalibration::save_state()
{
    // Save the current parameters (for use with SAVE_CONFIG)
    Printer::GetInstance()->m_pconfig->SetDouble("printer", "delta_radius", m_radius);
    std::vector<std::string> abc = {"a", "b", "c"};
    for (int i = 0; i < 3; i++)
    {
        Printer::GetInstance()->m_pconfig->SetDouble("stepper_"+ abc[i], "angle", m_angles[i]);
        Printer::GetInstance()->m_pconfig->SetDouble("stepper_"+ abc[i], "arm_length", m_arms[i]);
        Printer::GetInstance()->m_pconfig->SetDouble("stepper_"+ abc[i], "position_endstop", m_endstops[i]);
    }
    // gcode.respond_info(
    //     "stepper_a: position_endstop: %.6f angle: %.6f arm: %.6f\n"
    //     "stepper_b: position_endstop: %.6f angle: %.6f arm: %.6f\n"
    //     "stepper_c: position_endstop: %.6f angle: %.6f arm: %.6f\n"
    //     "delta_radius: %.6f"
    //     % (m_endstops[0], m_angles[0], m_arms[0],
    //         m_endstops[1], m_angles[1], m_arms[1],
    //         m_endstops[2], m_angles[2], m_arms[2],
    //         m_radius))  //---??---
}
        
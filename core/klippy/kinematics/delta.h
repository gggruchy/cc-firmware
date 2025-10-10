#ifndef DELTAKINEMATICS_H
#define DELTAKINEMATICS_H
#include "kinematics_base.h"

class DeltaKinematics : public Kinematics
{
private:
    
public:
    DeltaKinematics(std::string section_name);
    ~DeltaKinematics();

    double ratio_to_xy(double ratio, double min_arm_length, double half_min_step_dist, double radius);
    std::vector<std::vector<MCU_stepper*>> get_steppers();
    std::vector<double> _actuator_to_cartesian(std::vector<double> spos);
    std::vector<double> calc_position(std::map<std::string, double> stepper_positions);
    void set_position(double newpos[3], std::vector<int> homing_axes = std::vector<int>());
    void home(Homing &homing_state);
    void motor_off(double print_time);
    bool check_move(Move& move);
    bool check_endstops(std::vector<double> &end_pos){}
    std::map<std::string, std::string> get_status(double eventtime);
    // DeltaCalibration* get_calibration();

    void _activate_carriage(int carriage){};
    void cmd_SET_DUAL_CARRIAGE(GCodeCommand& gcmd){};
    bool home_axis(Homing &homing_state, int axis, PrinterRail *rail){};
    void note_z_not_homed(){};
    bool check_endstops(Move& move){};
};

class DeltaCalibration{
    private:

    public:
        DeltaCalibration(double radius, std::vector<double> angles, std::vector<double> arms, std::vector<double> endstops, std::vector<double> stepdists);
        ~DeltaCalibration();

        double m_radius;
        std::vector<double> m_angles;
        std::vector<double> m_arms;
        std::vector<double> m_endstops;
        std::vector<double> m_stepdists;
        std::vector<std::vector<double>> m_towers;
        std::vector<double> m_abs_endstops;

        std::map<std::string, double> coordinate_descent_params(bool is_extended);
        DeltaCalibration *new_calibration(std::map<std::string, double> params);
        std::vector<double> get_position_from_stable(std::vector<double> stable_position);
        std::vector<double> calc_stable_position(std::vector<double> coord);
        void save_state();
};

#endif 
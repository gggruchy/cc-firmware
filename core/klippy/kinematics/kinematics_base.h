#ifndef KINEMATICS_BASE_H
#define KINEMATICS_BASE_H
#include "print_rail.h"
#include "move.h"
#include "gcode.h"
#include "homing.h"

#define SOFT_ENDSTOPS_MIN -9999
#define SOFT_ENDSTOPS_MAX 9999
class Kinematics{
private:

public:
    int m_dual_carriage_axis;
    std::vector<PrinterRail*> m_dual_carriage_rails;
    std::vector<PrinterRail*> m_rails;
    double m_max_z_velocity; //5 Z轴最大速度
    double m_max_z_accel; //100  Z轴最大加速度
    std::vector<std::vector<double>> m_limits; //X Y Z 尺寸
    std::vector<double> m_axes_min;
    std::vector<double> m_axes_max;
    std::string m_cmd_SET_DUAL_CARRIAGE_help; 

    double m_max_velocity;
    double m_max_accel;
    double m_radius;
    std::vector<double> m_arm_lengths;
    std::vector<double> m_arm2;
    std::vector<double> m_abs_endstops;
    std::vector<double> m_angles;
    std::vector<std::vector<double>> m_towers;
    bool m_need_home;
    double m_limit_xy2;
    std::vector<double> m_home_position;
    double m_max_z;
    double m_min_z;
    double m_limit_z;
    double m_slow_xy2;
    double m_very_slow_xy2;
    double m_max_xy2;
    double max_xy;
    bool is_open_soft_limit=true;
public:
    Kinematics(){};
    ~Kinematics(){};
    
    virtual std::vector<std::vector<MCU_stepper*>> get_steppers()=0;
    virtual std::vector<double> calc_position(std::map<std::string, double> stepper_positions)=0;
    virtual void set_position(double newpos[3], std::vector<int> homing_axes = std::vector<int>())=0;
    virtual void note_z_not_homed()=0;
    virtual bool home_axis(Homing &homing_state, int axis, PrinterRail *rail)=0;
    virtual void home(Homing &homing_state)=0;
    virtual void motor_off(double print_time)=0;
    virtual bool check_endstops(Move& move)=0;
    virtual bool check_endstops(std::vector<double> &end_pos)=0;
    virtual bool check_move(Move& move)=0; 
    virtual std::map<std::string, std::string> get_status(double eventtime)=0;
    virtual void _activate_carriage(int carriage)=0;
    virtual void cmd_SET_DUAL_CARRIAGE(GCodeCommand& gcmd)=0;
    virtual std::vector<double> _actuator_to_cartesian(std::vector<double> spos)=0;
};
#endif
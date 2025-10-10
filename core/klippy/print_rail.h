#ifndef PRINT_RAIL_H
#define PRINT_RAIL_H
#include <string>
#include <iostream>
#include <vector>
#include <math.h>
#include <sstream>
#include <functional>

#include "stepper.h"
#include "mcu_io.h"

struct homingInfo{
    double speed;
    double position_endstop;
    double retract_speed;
    double retract_dist;
    double force_retract;
    bool positive_dir; 
    double second_homing_speed;
    double homing_accel;
    double homing_current;
};

class PrinterRail{

public:
    PrinterRail(std::string rail_name, bool need_position_minmax = true, double default_position_endstop = DBL_MAX, bool units_in_radians = false);
    ~PrinterRail();
    
    std::vector<MCU_stepper*> m_steppers;
    std::vector<MCU_endstop*> m_endstops;
    std::vector<std::string> m_endstops_name;
    bool m_stepper_units_in_radians;
    double m_position_min;
    double m_position_max;

    int cur_stepper_slect;
    double m_homing_speed;
    double m_position_endstop;
    double m_position_endstop_extra;
    double m_homing_accel;
    double m_homing_current;
    double m_homing_retract_speed; 
    double m_homing_retract_dist;
    double m_homing_force_retract;
    bool m_homing_positive_dir; 
    double m_second_homing_speed;

    std::vector<double> get_range();
    homingInfo get_homing_info();
    std::vector<MCU_stepper*> get_steppers();
    std::vector<MCU_endstop*> get_endstops();
    void add_extra_stepper(std::string section_name);
    void setup_itersolve(char axis);
    void setup_itersolve(double arm2, double tower_x, double tower_y);
    void set_trapq(trapq* trapq);
    void set_position(double coord[3]);
private:
};

MCU_stepper* PrinterStepper(std::string section_name, bool units_in_radians = false);
double parse_step_distance(std::string section_name, bool units_in_radians, bool note_valid);
double parse_gear_ratio(std::string section_name, bool note_valid);
PrinterRail* LookupMultiRail(std::string section_name);
#endif
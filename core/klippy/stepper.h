#ifndef __STEPPER_H__
#define __STEPPER_H__
#include <string>
#include <iostream>
#include <vector>
#include <math.h>
#include <sstream>
#include <functional>
#include "mcu.h"
#include "pins.h"

extern "C"
{
    #include "chelper/itersolve.h"
    #include "chelper/stepcompress.h"
    #include "chelper/kin_cartesian.h"
    #include "chelper/kin_corexy.h"
    #include "chelper/kin_delta.h"
}

typedef struct pin_info_t
{
    int dir_pin;
    int step_pin;
    uint32_t invert_dir;
    uint32_t invert_step;
}pin_info;

class MCU_stepper{
private:
    
public:
    std::string m_name;
    double m_step_dist;     //步距
    bool m_units_in_radians;
    MCU *m_mcu;
    int m_oid;
    int m_step_pin;
    uint32_t m_invert_step;
    int m_dir_pin;
    uint32_t m_invert_dir;
    double m_step_pulse_duration;
    stepcompress *m_step_queue;
    int32_t m_reset_cmd_tag;
    std::string m_get_position_cmd;
    bool is_enable_motor;
    stepper_kinematics *m_sk;           //input_shaper_alloc
    trapq *m_tq;                                        //Printer::GetInstance()->m_tool_head->m_trapq   m_trapq
    double m_mcu_position_offset;
    std::vector<std::function<void(double)>> m_active_callbacks;
    
public:
    MCU_stepper(std::string &stepper_name, pinParams *step_pin_params, pinParams *dir_pin_params, double step_dist, bool units_in_radians);
    ~MCU_stepper();
    
    MCU* get_mcu();
    std::string get_name(bool shorter=false);
    bool units_in_radians();
    double dist_to_time(double dist, double start_velocity, double accel);
    void setup_itersolve(char axis);
    void setup_itersolve(double arm2, double tower_x, double tower_y);
    void build_config(int para);
    int get_oid();
    double get_step_dist();   
    void set_step_dist(double dist);     
    uint32_t is_dir_inverted();
    double calc_position_from_coord(std::vector<double> &coord);
    void set_position(double coord[3]);
    double get_commanded_position();
    int64_t get_mcu_position();
    void set_mcu_position(int64_t mcu_pos);
    int64_t get_past_mcu_position(double print_time); 
    double mcu_to_command_position(double mcu_pos);
    std::vector<struct pull_history_steps> dump_steps(int count, uint64_t start_clock, uint64_t end_clock);
    double get_past_commanded_position(double print_time);
    stepper_kinematics* set_stepper_kinematics(stepper_kinematics *sk);
    void note_homing_end(bool did_trigger=false);
    trapq* set_trapq(trapq *tq);
    void add_active_callback(std::function<void(double)> cb);
    void generate_steps(double flush_time);
    int32_t is_active_axis(char axis);
    std::string stepper_index_to_name(int index);
    int stepper_index_to_name(std::string name);
    pin_info get_pin_info();
    // PrinterStepperEnable * m_stepper_enable; //--2022.8.31-yds-
};
#endif
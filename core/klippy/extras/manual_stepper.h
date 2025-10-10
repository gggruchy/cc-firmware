#ifndef MANUAL_STEPPER_H
#define MANUAL_STEPPER_H
#include "mcu.h"
#include "stepper.h"
#include "force_move.h"
#include "gcode.h"
#include "print_rail.h"

class ManualStepper{
private:
    
public:
    ManualStepper(std::string section_name);
    ~ManualStepper();
    bool m_can_home;
    std::vector<MCU_stepper*> m_steppers;
    PrinterRail *m_rail;
    double m_velocity;
    double m_accel;
    double m_homing_accel;
    double m_next_cmd_time;
    int m_stepper_index;
    std::string m_stepper_name;
    trapq *m_trapq;
    std::string m_cmd_MANUAL_STEPPER_help;

    void sync_print_time();
    void do_enable(bool enable);
    void do_set_position(double setpos);
    void do_move(double movepos, double speed, double accel, bool sync = true);
    void do_homing_move(double movepos, double speed, double accel, bool triggered, bool check_trigger);

    void flush_step_generation();
    void get_position();
    void set_position(double newpos[3]);
    double get_last_move_time();
    void dwell(double delay);
    void drip_move(double newpos[3], double speed, double drip_completion);
    ManualStepper* get_kinematics();   
    std::vector<MCU_stepper*> get_steppers();
    void cmd_MANUAL_STEPPER(GCodeCommand& gcmd);
};

#endif
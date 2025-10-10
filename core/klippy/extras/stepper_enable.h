
#ifndef __STEPPERENABLE_H__
#define __STEPPERENABLE_H__

#include <cstdlib>
#include <iostream>
#include <vector>
#include <math.h>
#include <sstream>
#include <functional>

#include "pins.h"
#include "mcu_io.h"
#include "stepper.h"
#include "gcode.h"

#define DISABLE_STALL_TIME 0.100

// class StepperEnable
// {
// private:
// public:
//     MCU_digital_out *m_mcu_digital_out;
//     StepperEnable(std::string pin);
//     ~StepperEnable();
    
// };  //--8.30--

class StepperEnablePin{
private:

public:
    
    MCU_digital_out *m_mcu_enable;
    int m_enable_count;
    bool m_is_dedicated;

public:
    StepperEnablePin(MCU_digital_out *mcu_enable, int enable_count);
    ~StepperEnablePin();
    void set_enable(double print_time);
    void set_disable(double print_time);
};

class EnableTracking{
private:

public:
    MCU_stepper *m_stepper;
    StepperEnablePin* m_stepper_enable;
    std::vector<std::function<void(double, bool)>> m_callbacks;
    bool m_is_enabled;
public:
    EnableTracking(MCU_stepper *stepper, std::string pin);
    ~EnableTracking();
    void register_state_callback(std::function<void(double, bool)> cb);
    void motor_enable(double print_time);
    void motor_disable(double print_time);
    bool is_motor_enabled();
    bool has_dedicated_enable();
};

class PrinterStepperEnable
{
private:
    std::string m_section_name;
public:
    PrinterStepperEnable(std::string section_name);
    ~PrinterStepperEnable();
    std::map<std::string, EnableTracking*> m_enable_lines;
    std::string cmd_SET_STEPPER_ENABLE_help;

public:
    void register_stepper(MCU_stepper *mcu_stepper, std::string pin);
    void motor_off();
    void motor_debug_enable(std::string stepper_name, int enable);
    void handle_request_restart(double print_time);
    bool get_stepper_state(std::string stepper_name);
    void cmd_M18(GCodeCommand &gcmd);
    void cmd_SET_STEPPER_ENABLE(GCodeCommand &gcmd);
    EnableTracking* lookup_enable(std::string name);
    std::vector<string> get_steppers();
};

#endif
#ifndef TMC_H
#define TMC_H
#include "my_math.h"
#include "stepper.h"
#include <algorithm>
#include <iostream>
#include <string>
#include "bus.h"
#include "stepper.h"
#include "gcode.h"
#include "stepper_enable.h"

class FieldHelper
{
private:
public:
    FieldHelper(std::map<std::string, std::map<std::string, int>> &all_fields, std::vector<std::string> &signed_fields, std::map<std::string, std::function<std::string(int)>> &field_formatters,std::map<std::string, int>registers=std::map<std::string, int>());
    ~FieldHelper();
    std::string lookup_register(std::string field_name);
    int get_field(std::string field_name, int reg_value = 0, std::string reg_name = "");
    int set_field(std::string field_name, int field_value, int reg_value = 0, std::string reg_name = "");
    int set_config_field(std::string section_name, std::string field_name, int default_value);

    double threshold_velocity;
    double irun;
    double ihold;
    int driver_sgthrs;
    // int vsense;

    double irun_home;
    double ihold_home;
    int driver_sgthrs_home;

    std::string pretty_format(std::string reg_name, int reg_value);
    std::map<std::string, int> get_reg_fields(std::string reg_name, int reg_value);

    std::map<std::string, std::map<std::string, int>> m_all_fields;
    std::map<std::string, int> m_signed_fields;
    std::map<std::string, std::function<std::string(int)>> m_field_formatters;
    std::map<std::string, int> m_registers;
    std::map<std::string, std::string> m_field_to_register;
};

class MCU_TMC
{
private:
public:
    MCU_TMC(){};
    ~MCU_TMC(){};
    FieldHelper *get_fields()
    {
        return fields;
    }
    virtual uint32_t _do_get_register(std::string reg_name)=0;
    virtual uint32_t get_register(std::string reg_name)=0;
    virtual void set_register(std::string reg_name, uint32_t val, double print_time = DBL_MAX)=0;

    std::string name;
    uint8_t addr;
    std::vector<int> instance_id;
    uint32_t ifcnt;
    ReactorMutex *mutex;
    std::map<std::string, uint8_t> name_to_reg;
    FieldHelper *fields;
};



class TMCCurrentHelper
{
private:
public:
    TMCCurrentHelper(std::string section_name, MCU_TMC *mcu_tmc);
    ~TMCCurrentHelper();
    int _calc_current_bits(double current, int vsense);
    bool _calc_current(double run_current, double hold_current, int &irun, int &ihold);
    double get_current(double &run_current, double &hold_current);
    void set_current(double run_current, double hold_current, double print_time);
    double _calc_current_from_field(std::string field_name);

    std::string m_name;
    MCU_TMC *m_mcu_tmc;
    FieldHelper *m_fields;
    // double hold_current;
    double m_sense_resistor;
};
// #include "tmc2208.h"
class TMCErrorCheck
{
private:
public:
    TMCErrorCheck(std::string section_name, MCU_TMC *mcu_tmc);
    ~TMCErrorCheck();
    void start_checks(){};
};

class TMCCommandHelper
{
private:
public:
    TMCCommandHelper(std::string section_name, MCU_TMC *mcu_tmc, TMCCurrentHelper *current_helper);
    ~TMCCommandHelper();
    void _init_registers(double print_time = DBL_MAX);
    void cmd_INIT_TMC(GCodeCommand &gcmd);
    void cmd_SET_TMC_FIELD(GCodeCommand &gcmd);
    void cmd_SET_TMC_CURRENT(GCodeCommand &gcmd);
    void _do_enable(double print_time);
    void _do_disable(double print_time);
    void handle_stepper_enable(double print_time, bool is_enable);
    void _handle_connect();
    void setup_register_dump(std::vector<std::string> read_registers, std::vector<std::string> read_translate = std::vector<std::string>());
    void cmd_DUMP_TMC(GCodeCommand &gcmd);
    std::string m_stepper_name;
    std::string m_name;
    MCU_TMC *m_mcu_tmc;
    TMCCurrentHelper *m_current_helper;
    TMCErrorCheck *m_echeck_helper;
    PrinterStepperEnable *m_stepper_enable;
    FieldHelper *m_fields;
    std::vector<std::string> m_read_registers;
    std::vector<std::string> m_read_translate;
    int m_toff;
    //     m_mcu_phase_offset = None
    //         m_stepper = None

    //             std::string cmd_INIT_TMC_help;
    std::string cmd_SET_TMC_FIELD_help;
    std::string cmd_SET_TMC_CURRENT_help;
    std::string cmd_DUMP_TMC_help;
};

class TMCVirtualPinHelper : public McuChip
{
private:
public:
    TMCVirtualPinHelper(std::string section_name, MCU_TMC *mcu_tmc, TMCCurrentHelper *current_helper);
    ~TMCVirtualPinHelper() {}
    void *setup_pin(std::string pin_type, pinParams *pin_params);
    void handle_homing_move_begin(HomingMove *hmove);
    void handle_homing_move_end(HomingMove *hmove);
    void set_silent_mode(bool silent_mode);

    MCU_TMC *m_mcu_tmc;
    FieldHelper *m_fields;
    std::string m_section_name;
    std::string m_diag_pin;
    std::string m_diag_pin_field;
    MCU_endstop *m_mcu_endstop;
    TMCCurrentHelper *m_current_helper;
    int m_en_pwm;
    int m_pwmthrs;
};
class TMCMicrostepHelper
{
private:
public:
    TMCMicrostepHelper(std::string section_name, MCU_TMC *mcu_tmc);
    ~TMCMicrostepHelper() {}
    int get_microsteps();
    int get_phase();
    MCU_TMC *m_mcu_tmc;
    FieldHelper *m_fields;
};
void TMCStealthchopHelper(std::string section_name, MCU_TMC *mcu_tmc,double tmc_freq);
#endif

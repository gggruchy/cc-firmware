#ifndef __HEATER_H__
#define __HEATER_H__

#include "pins.h"
#include "mcu_io.h"
#include "gcode.h"
#include <mutex>
#include "temperature_sensors_base.h"

struct temp_state{
    double smoothed_temp;
    double target_temp;
    double last_pwm_value;
};

class Control
{
private:
    
public:
    Control(){};
    ~Control(){};
    virtual void temperature_update(double read_time, double temp, double target_temp) = 0;
    virtual bool check_busy(double eventtime, double smoothed_temp, double target_temp) = 0;
    virtual void reset_pid()= 0;
    virtual void set_pid(double Kp, double Ki, double Kd) = 0;
    virtual std::vector<double> get_pid() = 0;
};


class Heater
{
private:
    
public:
    double m_min_temp;
    double m_max_temp;
    double m_next_pwm_time;
    double m_last_pwm_value;
    double m_pwm_delay;
    double m_last_temp;
    double m_last_temp_time;
    double m_smoothed_temp;
    double m_smooth_time;
    double m_inv_smooth_time;
    double m_min_extrude_temp;
    double last_min_extrude_temp;
    double m_target_temp;
    bool m_can_extrude;
    bool m_can_extrude_switch;
    double m_max_power;
    Control* m_control;
    std::string m_name;
    std::string cmd_SET_HEATER_TEMPERATURE_help;
    MCU_pwm* m_mcu_pwm;
    std::mutex m_lock;
    std::function<void(double, double, double)> m_updata_temp_callback;

    Heater(std::string section_name, TemperatureSensors *sensor);
    ~Heater();
    void set_pwm(double read_time, double value);
    void temperature_callback(double read_time, double temp);
    double get_pwm_delay();
    double get_max_power();
    double get_smooth_time();
    double set_temp(double degrees);
    std::vector<double> get_temp(double eventtime);
    bool check_busy(double eventtime);
    Control* set_control(Control* control);
    void alter_target(double target_temp);
    bool stats(double eventtime);
    struct temp_state get_status(double eventtime);
    void cmd_SET_HEATER_TEMPERATURE(GCodeCommand& gcmd);
};

class ControlBangBang : public Control
{
private:
    double m_heater_max_power;
    double m_max_delta;
    bool m_heating;
    Heater *m_heater;
public:
    ControlBangBang(Heater* heater, std::string section_name);
    ~ControlBangBang();
    void temperature_update(double read_time, double temp, double target_temp);
    bool check_busy(double eventtime, double smoothed_temp, double target_temp);
    void reset_pid(){};
    void set_pid(double Kp, double Ki, double Kd){};
    std::vector<double> get_pid(){};
};

#define PID_SETTLE_DELTA 1.0
#define PID_SETTLE_SLOPE 0.1

class ControlPID : public Control
{
private:
    double m_heater_max_power;
    std::string m_section_name;
    double m_Kp;
    double m_Ki;
    double m_Kd;
    double m_min_deriv_time;
    double m_temp_integ_max;
    double m_prev_temp;
    double m_prev_temp_time;
    double m_prev_temp_deriv;
    double m_prev_temp_integ;
    Heater *m_heater;
public:
    
    ControlPID(Heater* heater, std::string section_name);
    ~ControlPID();
    void temperature_update(double read_time, double temp, double target_temp);
    bool check_busy(double eventtime, double smoothed_temp, double target_temp);
    void reset_pid();
    void set_pid(double Kp, double Ki, double Kd);
    std::vector<double> get_pid(){};
};

struct Available_status
{
    std::vector<std::string> available_heaters;
    std::vector<std::string> available_sensors;
};

class PrinterHeaters
{
private:
    std::string m_section_name;
    bool m_has_started = false;
    bool m_have_load_sensors = false;
    std::map<std::string, TemperatureSensors*> m_sensor_factories;
    std::vector<std::string> m_available_sensors;
    std::map<std::string, Heater*> m_gcode_id_to_sensor;
    std::map<std::string, Heater*> m_heaters;
    std::vector<std::string> m_available_heaters;
public:

    Heater *heater_bed;

    PrinterHeaters(std::string section_name);
    ~PrinterHeaters();
    void add_sensor_factory(std::string sensor_type, TemperatureSensors* sensor_factory);
    Heater* setup_heater(std::string section_name, std::string gcode_id = "");
    std::vector<std::string> get_all_heaters();
    Heater* lookup_heater(std::string heater_name);
    TemperatureSensors* setup_sensor(std::string section_name);
    void register_sensor(std::string section_name, Heater* psensor, std::string gcode_id = "");
    void load_config();
    struct Available_status get_status(double eventtime);
    void turn_off_all_heaters();
    void cmd_TURN_OFF_HEATERS(GCodeCommand &gcmd);
    void handle_ready();
    std::string get_temp(double eventtime);
    void cmd_M105(GCodeCommand &gcmd);
    void cmd_M301(GCodeCommand &gcmd);
    void cmd_M130(GCodeCommand &gcmd);
    void cmd_M131(GCodeCommand &gcmd);
    void cmd_M132(GCodeCommand &gcmd);
    void cmd_M573(GCodeCommand &gcmd);
    void cmd_M136(GCodeCommand &gcmd);
    
    void wait_for_temperature(Heater *heater);
    void set_temperature(Heater *heater, double temp, bool wait = false);
    void cmd_TEMPERATURE_WAIT(GCodeCommand &gcmd);

};



#endif
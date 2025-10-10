#ifndef PID_CALIBRATE_H
#define PID_CALIBRATE_H
#include "gcode.h"
#include "heaters.h"
#include <vector>

class PIDCalibrate
{
private:
    
public:
    bool m_finish_flag;
    PIDCalibrate(std::string section_name);
    ~PIDCalibrate();
    std::string m_cmd_PID_CALIBRATE_help;
    void cmd_PID_CALIBRATE(GCodeCommand &gcmd);
    void cmd_PID_CALIBRATE_ALL(GCodeCommand &gcmd);
    void cmd_PID_CALIBRATE_HOTBED(GCodeCommand &gcmd);
    void cmd_PID_CALIBRATE_NOZZLE(GCodeCommand &gcmd);
    std::vector<double> m_test_points;
    double m_extruder_target_temp;
    double m_heater_bed_target_temp;
};

class ControlAutoTune : public Control
{
private:
    
public:
    ControlAutoTune(Heater* heater, double target);
    ~ControlAutoTune();
    void set_pwm(double read_time, double value);
    void temperature_update(double read_time, double temp, double target_temp);
    bool check_busy(double eventtime, double smoothed_temp, double target_temp);
    void reset_pid(){};
    void set_pid(double Kp, double Ki, double Kd){};
    std::vector<double> get_pid(){};
    void check_peaks();
    std::vector<double> calc_pid(int pos);
    std::vector<double> calc_final_pid();
    void write_file(std::string filename);

public:
    Heater *m_heater;
    double m_heater_max_power = 0;
    double m_calibrate_temp = 0;
    // Heating control
    bool m_heating = false;
    double m_peak = 0.;
    double m_peak_time = 0.;
    // Peak recording
    std::vector<std::pair<double, double>> m_peaks;
    // Sample recording
    double m_last_pwm = 0.;
    std::vector<std::pair<double, double>> m_pwm_samples;
    std::vector<std::pair<double, double>> m_temp_samples;
};

enum
{
    PID_CALIBRATE_STATE_ERROR = 0,
    PID_CALIBRATE_STATE_START = 1,
    PID_CALIBRATE_STATE_PREHEAT_DONE = 2,
    PID_CALIBRATE_STATE_FINISH = 3,
};
typedef void (*pid_calibrate_state_callback_t)(int state);
int pid_calibrate_register_state_callback(pid_calibrate_state_callback_t state_callback);
int pid_calibrate_unregister_state_callback(pid_calibrate_state_callback_t state_callback);
int pid_calibrate_state_callback_call(int state);






#endif
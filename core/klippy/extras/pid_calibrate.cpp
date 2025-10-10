#include "pid_calibrate.h"
#include "klippy.h"
#include "utility"
#include "Define_config_path.h"
#include "my_string.h"
#define LOG_TAG "pid_calibrate"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_ERROR
#include "log.h"

#define TUNE_PID_DELTA 5.0

PIDCalibrate::PIDCalibrate(std::string section_name)
{
    std::string point_str = Printer::GetInstance()->m_pconfig->GetString("pid_calibrate", "test_points", "110,110,20");
    point_str = strip(point_str);
    std::vector<string> temp = split(point_str, ",");
    for (auto coord : temp)
    {
        coord = strip(coord);
        m_test_points.push_back(std::stod(coord));
    }
    m_extruder_target_temp = Printer::GetInstance()->m_pconfig->GetDouble("pid_calibrate", "extruder_target_temp", 210);
    m_heater_bed_target_temp = Printer::GetInstance()->m_pconfig->GetDouble("pid_calibrate", "heater_bed_target_temp", 60);
    std::string m_cmd_PID_CALIBRATE_help = "Run PID calibration test";
    Printer::GetInstance()->m_gcode->register_command("PID_CALIBRATE", std::bind(&PIDCalibrate::cmd_PID_CALIBRATE, this, std::placeholders::_1), false, m_cmd_PID_CALIBRATE_help);
    Printer::GetInstance()->m_gcode->register_command("PID_CALIBRATE_ALL", std::bind(&PIDCalibrate::cmd_PID_CALIBRATE_ALL, this, std::placeholders::_1), false, m_cmd_PID_CALIBRATE_help);
    Printer::GetInstance()->m_gcode->register_command("PID_CALIBRATE_HOTBED", std::bind(&PIDCalibrate::cmd_PID_CALIBRATE_HOTBED, this, std::placeholders::_1), false, m_cmd_PID_CALIBRATE_help);
    Printer::GetInstance()->m_gcode->register_command("PID_CALIBRATE_NOZZLE", std::bind(&PIDCalibrate::cmd_PID_CALIBRATE_NOZZLE, this, std::placeholders::_1), false, m_cmd_PID_CALIBRATE_help);
    m_finish_flag = false;
}

PIDCalibrate::~PIDCalibrate()
{
}

void PIDCalibrate::cmd_PID_CALIBRATE(GCodeCommand &gcmd)
{
    std::string heater_name = gcmd.get_string("HEATER", "");
    double target = gcmd.get_double("TARGET", DBL_MIN);
    int write_file = gcmd.get_int("WRITE_FILE", 0);
    Heater *heater;
    if (heater_name == "extruder")
        heater = Printer::GetInstance()->m_printer_extruder->m_heater;
    else if (heater_name == "heater_bed")
        heater = Printer::GetInstance()->m_bed_heater->m_heater;
    if (heater == nullptr)
    {
        return;
    }
    Printer::GetInstance()->m_tool_head->get_last_move_time();
    ControlAutoTune calibrate(heater, target);
    Control *old_control = heater->set_control(&calibrate);
    Printer::GetInstance()->m_pheaters->set_temperature(heater, target, true);
    heater->set_control(old_control);
    if (write_file)
    {
    }
    if (calibrate.check_busy(0., 0., 0.))
    {
        std::cout << "pid_calibrate interrupted" << std::endl;
        pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_ERROR);
        return;
    }
    // Log and report results
    std::cout << "Log and report results" << std::endl;
    std::vector<double> pid = calibrate.calc_final_pid();
    std::cout << "Autotune: final: Kp=" << pid[0] << " Ki=" << pid[1] << " Kd=" << pid[2] << std::endl;
    // Store results for SAVE_CONFIG
    Printer::GetInstance()->m_pconfig->SetValue(heater_name, "control", "pid");
    Printer::GetInstance()->m_pconfig->SetDouble(heater_name, "pid_Kp", pid[0]);
    Printer::GetInstance()->m_pconfig->SetDouble(heater_name, "pid_Ki", pid[1]);
    Printer::GetInstance()->m_pconfig->SetDouble(heater_name, "pid_Kd", pid[2]);
    Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
    if (heater_name == "extruder")
    {
        std::vector<string> keys;
        keys.push_back("pid_Kp");
        keys.push_back("pid_Ki");
        keys.push_back("pid_Kd");
        Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, heater_name, keys);
    }
}

void PIDCalibrate::cmd_PID_CALIBRATE_ALL(GCodeCommand &gcmd)
{
    Printer::GetInstance()->m_gcode_io->single_command("G28");
    std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(get_monotonic());
    if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos || kin_status["homed_axes"].find("z") == std::string::npos)
    {
        LOG_E("G28 failed\n");
        return;
    }
    std::vector<string> script;
    script.push_back("G90");
    script.push_back("G1 X" + to_string(m_test_points[0]) + " Y" + to_string(m_test_points[1]) + " Z" + to_string(m_test_points[2]) + " F1800");
    script.push_back("M106 S255");
    script.push_back("PID_CALIBRATE HEATER=extruder TARGET=" + to_string(m_extruder_target_temp));
    script.push_back("M106 S255");
    script.push_back("PID_CALIBRATE HEATER=heater_bed TARGET=" + to_string(m_heater_bed_target_temp));
    script.push_back("M104 S0");
    script.push_back("M140 S0");
    script.push_back("M107");
    script.push_back("M18");
    pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_START);
    Printer::GetInstance()->m_gcode->run_script(script);
    if(Printer::GetInstance()->is_shutdown())
    {
        return;
    }
    pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_FINISH);
    m_finish_flag = true;
}

void PIDCalibrate::cmd_PID_CALIBRATE_HOTBED(GCodeCommand &gcmd)
{
    pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_START);
    Printer::GetInstance()->m_gcode_io->single_command("G28");
    std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(get_monotonic());
    if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos || kin_status["homed_axes"].find("z") == std::string::npos)
    {
        LOG_E("G28 failed\n");
        return;
    }
    Printer::GetInstance()->m_gcode_io->single_command("G90");
    Printer::GetInstance()->m_gcode_io->single_command("G1 X" + to_string(m_test_points[0]) + " Y" + to_string(m_test_points[1]) + " Z" + to_string(m_test_points[2]) + " F1800");
    Printer::GetInstance()->m_gcode_io->single_command("M106 S255");
    Printer::GetInstance()->m_gcode_io->single_command("PID_CALIBRATE HEATER=heater_bed TARGET=" + to_string(m_heater_bed_target_temp));
    Printer::GetInstance()->m_gcode_io->single_command("M104 S0");
    Printer::GetInstance()->m_gcode_io->single_command("M140 S0");
    Printer::GetInstance()->m_gcode_io->single_command("M107");
    Printer::GetInstance()->m_gcode_io->single_command("M18");
    if(Printer::GetInstance()->is_shutdown())
    {
        return;
    }
    pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_FINISH);
    m_finish_flag = true;
}

void PIDCalibrate::cmd_PID_CALIBRATE_NOZZLE(GCodeCommand &gcmd)
{
    double target_temp = gcmd.get_double("TARGET_TEMP", m_extruder_target_temp, 180 ,335);
    pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_START);
    Printer::GetInstance()->m_gcode_io->single_command("G28");
    std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(get_monotonic());
    if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos || kin_status["homed_axes"].find("z") == std::string::npos)
    {
        LOG_E("G28 failed\n");
        return;
    }
    Printer::GetInstance()->m_gcode_io->single_command("G90");
    Printer::GetInstance()->m_gcode_io->single_command("G1 X" + to_string(m_test_points[0]) + " Y" + to_string(m_test_points[1]) + " Z" + to_string(m_test_points[2]) + " F1800");
    Printer::GetInstance()->m_gcode_io->single_command("M106 S255");
    Printer::GetInstance()->m_gcode_io->single_command("M109 S100");
    pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_PREHEAT_DONE);
    Printer::GetInstance()->m_gcode_io->single_command("PID_CALIBRATE HEATER=extruder TARGET=" + to_string(target_temp));
    Printer::GetInstance()->m_gcode_io->single_command("M104 S0");
    Printer::GetInstance()->m_gcode_io->single_command("M140 S0");
    Printer::GetInstance()->m_gcode_io->single_command("M107");
    Printer::GetInstance()->m_gcode_io->single_command("M18");
    if(Printer::GetInstance()->is_shutdown())
    {
        return;
    }
    pid_calibrate_state_callback_call(PID_CALIBRATE_STATE_FINISH);
    m_finish_flag = true;
}

#define PID_CALIBRATE_STATE_CALLBACK_SIZE 16
static pid_calibrate_state_callback_t pid_calibrate_state_callback[PID_CALIBRATE_STATE_CALLBACK_SIZE];

int pid_calibrate_register_state_callback(pid_calibrate_state_callback_t state_callback)
{
    for (int i = 0; i < PID_CALIBRATE_STATE_CALLBACK_SIZE; i++)
    {
        if (pid_calibrate_state_callback[i] == NULL)
        {
            pid_calibrate_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}

int pid_calibrate_unregister_state_callback(pid_calibrate_state_callback_t state_callback)
{
    for (int i = 0; i < PID_CALIBRATE_STATE_CALLBACK_SIZE; i++)
    {
        if (pid_calibrate_state_callback[i] == state_callback)
        {
            pid_calibrate_state_callback[i] = NULL;
            return 0;
        }
    }
    return -1;
}

int pid_calibrate_state_callback_call(int state)
{
    // printf("homing_state_callback_call state:%d\n", state);
    for (int i = 0; i < PID_CALIBRATE_STATE_CALLBACK_SIZE; i++)
    {
        if (pid_calibrate_state_callback[i] != NULL)
        {
            pid_calibrate_state_callback[i](state);
        }
    }
    return 0;
}

ControlAutoTune::ControlAutoTune(Heater *heater, double target)
{
    m_heater = heater;
    m_heater_max_power = heater->get_max_power();
    m_calibrate_temp = target;
    m_heating = false;
    std::vector<std::pair<double, double>>().swap(m_peaks);
}

ControlAutoTune::~ControlAutoTune()
{
}

void ControlAutoTune::set_pwm(double read_time, double value)
{
    if (value != m_last_pwm)
    {
        m_pwm_samples.push_back(std::pair<double, double>(read_time + m_heater->get_pwm_delay(), value));
        m_last_pwm = value;
    }
    m_heater->set_pwm(read_time, value);
}

void ControlAutoTune::temperature_update(double read_time, double temp, double target_temp)
{
    m_temp_samples.push_back(std::pair<double, double>(read_time, temp));
    // Check if the temperature has crossed the target and
    // enable/disable the heater if so.
    if (m_heating && temp >= target_temp)
    {
        m_heating = false;
        this->check_peaks();
        m_heater->alter_target(m_calibrate_temp - TUNE_PID_DELTA);
    }
    else if (!m_heating && temp <= target_temp)
    {
        m_heating = true;
        this->check_peaks();
        m_heater->alter_target(m_calibrate_temp);
    }
    // Check if this temperature is a peak and record it if so
    if (m_heating)
    {
        this->set_pwm(read_time, m_heater_max_power);
        if (temp < m_peak)
        {
            m_peak = temp;
            m_peak_time = read_time;
        }
    }
    else
    {
        this->set_pwm(read_time, 0.);
        if (temp > m_peak)
        {
            m_peak = temp;
            m_peak_time = read_time;
        }
    }
}

bool ControlAutoTune::check_busy(double eventtime, double smoothed_temp, double target_temp)
{
    if (m_heating || m_peaks.size() < 12)
        return true;
    return false;
}

// Analysis
void ControlAutoTune::check_peaks()
{
    m_peaks.push_back(std::pair<double, double>(m_peak, m_peak_time));
    if (m_heating)
        m_peak = 9999999.;
    else
        m_peak = -9999999.;
    if (m_peaks.size() < 4)
        return;
    this->calc_pid(m_peaks.size() - 1);
}

std::vector<double> ControlAutoTune::calc_pid(int pos)
{
    double temp_diff = m_peaks[pos].first - m_peaks[pos - 1].first;
    double time_diff = m_peaks[pos].second - m_peaks[pos - 2].second;
    // Use Astrom-Hagglund method to estimate Ku and Tu
    double amplitude = .5 * fabs(temp_diff);
    double Ku = 4. * m_heater_max_power / (M_PI * amplitude);
    double Tu = time_diff;
    // Use Ziegler-Nichols method to generate PID parameters
    double Ti = 0.5 * Tu;
    double Td = 0.125 * Tu;
    double Kp = 0.6 * Ku * PID_PARAM_BASE;
    double Ki = Kp / Ti;
    double Kd = Kp * Td;
    std::vector<double> ret = {Kp, Ki, Kd};
    return ret;
}

std::vector<double> ControlAutoTune::calc_final_pid()
{
    std::vector<std::pair<double, int>> cycle_times;
    for (int pos = 4; pos < m_peaks.size(); pos++)
    {
        cycle_times.push_back(std::pair<double, int>(m_peaks[pos].second - m_peaks[pos - 2].second, pos));
    }
    std::sort(cycle_times.begin(), cycle_times.end(), [](std::pair<double, int> &pair1, std::pair<double, int> &pair2) -> bool
              { return pair1.first < pair2.first; });
    int midpoint_pos = cycle_times[cycle_times.size() / 2].second;
    return this->calc_pid(midpoint_pos);
}

// Offline analysis helper
void ControlAutoTune::write_file(std::string filename)
{
    //----??----
}
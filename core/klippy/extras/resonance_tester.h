#ifndef RESONANCE_TESTER_H
#define RESONANCE_TESTER_H
#include <string>
#include <math.h>
#include <vector>
#include <limits>
#include "adxl345.h"


class CalibrationData;
class ShaperCalibrate;

class TestAxis
{
private:
public: 
    std::string m_name;
    std::pair<double, double> m_vib_dir;

public:
    TestAxis(std::string axis = "", std::pair<double, double> vib_dir = {0, 0});
    ~TestAxis();
    bool matches(std::string chip_axis);
    std::string get_name();
    std::pair<double, double> get_point(double l);
};

class VibrationPulseTest
{
private:

public:
    double m_min_freq;
    double m_max_freq;
    double m_accel_per_hz;
    std::vector<std::vector<double>> m_probe_points;
    double m_freq_start = 0;
    double m_freq_end = 0;
    double m_hz_per_sec = 0;
    
public:
    VibrationPulseTest(std::string section_name);
    ~VibrationPulseTest();
    std::vector<std::vector<double>> get_start_test_points();
    void prepare_test(GCodeCommand &gcmd);
    double run_test_time();
    void run_test(TestAxis* axis, GCodeCommand &gcmd);
};


class ResonanceTester
{
private:
    
public:
    double m_move_speed = 50;
    VibrationPulseTest* m_test = nullptr;
    std::vector<std::vector<std::string>> m_accel_chip_names;
    double m_max_smoothing;
    std::vector<std::pair<std::string, ADXL345*>> m_accel_chips;
    bool m_test_finish;
public:
    ResonanceTester(std::string section_name);
    ~ResonanceTester();
    void connect();
    std::map<std::string, CalibrationData*> _run_test(GCodeCommand &gcmd, std::vector<TestAxis*>axes, ShaperCalibrate* helper, std::string raw_name_suffix = "");
    void cmd_TEST_RESONANCES(GCodeCommand &gcmd);
    void cmd_SHAPER_CALIBRATE(GCodeCommand &gcmd);
    void cmd_SHAPER_CALIBRATE_SCRIPT(GCodeCommand &gcmd);
    void cmd_MEASURE_AXES_NOISE(GCodeCommand &gcmd);
    bool is_valid_name_suffix(std::string name_suffix);
    std::string get_filename(std::string base, std::string name_suffix, std::string axis_name, std::vector<double> point);
    // void save_calibration_data(self, base_name, name_suffix, shaper_calibrate,
    //                           axis, calibration_data, all_shapers=None);
};

enum
{
    RESONANCE_TESTER_STATE_ERROR_X = 0,
    RESONANCE_TESTER_STATE_ERROR_Y = 1,
    RESONANCE_TESTER_STATE_START = 2,
    RESONANCE_TESTER_STATE_START_X = 3,
    RESONANCE_TESTER_STATE_FINISH_X = 4,
    RESONANCE_TESTER_STATE_START_Y = 5,
    RESONANCE_TESTER_STATE_FINISH_Y = 6,
    RESONANCE_TESTER_STATE_FINISH = 7,
    RESONANCE_MEASURE_AXES_NOISE_FINISH = 8,
};
typedef void (*resonace_tester_state_callback_t)(int state);
int resonace_tester_register_state_callback(resonace_tester_state_callback_t state_callback);
int resonace_tester_unregister_state_callback(resonace_tester_state_callback_t state_callback);
int resonace_tester_state_callback_call(int state);



#endif
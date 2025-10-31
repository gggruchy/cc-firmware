
#include "shaper_calibrate.h"
#include "resonance_tester.h"
#include "klippy.h"
#include "my_string.h"
#define LOG_TAG "resonance_tester"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_ERROR
#include "log.h"
// #include "shaper_defs.h"
// #include "adxl345.h"

static std::string cmd_TEST_RESONANCES_help = ("Runs the resonance test for a specifed axis");
static std::string cmd_MEASURE_AXES_NOISE_help = ("Measures noise of all enabled accelerometer chips");
static std::string cmd_SHAPER_CALIBRATE_help = ("Simular to TEST_RESONANCES but suggest input shaper config");
std::vector<std::vector<double>> parse_probe_points(std::string section_name)
{
    std::string point_str = Printer::GetInstance()->m_pconfig->GetString(section_name, "probe_points");
    std::vector<std::string> points_str = split(point_str, "\n");
    std::vector<std::vector<double>> points;
    for (auto line : points_str)
    {
        line = strip(line);
        std::vector<string> temp = split(line, ",");
        std::vector<double> point;
        for (auto coord : temp)
        {
            coord = strip(coord);
            point.push_back(std::stod(coord));
        }
        points.push_back(point);
    }
    return points;
}

TestAxis::TestAxis(std::string axis, std::pair<double, double> vib_dir)
{
    if (axis == "")
        m_name = "axis=" + std::to_string(vib_dir.first) + "," + std::to_string(vib_dir.second);
    else
        m_name = axis;
    if (vib_dir.first == 0 && vib_dir.second == 0)
    {
        if (axis == "x")
        {
            m_vib_dir.first = 1;
            m_vib_dir.second = 0;
        }
        else
        {
            m_vib_dir.first = 0;
            m_vib_dir.second = 1;
        }
    }
    else
    {
        double s = sqrt(pow(vib_dir.first, 2) + pow(vib_dir.second, 2));
        m_vib_dir.first = vib_dir.first / s;
        m_vib_dir.second = vib_dir.second / s;
    }
}

TestAxis::~TestAxis()
{
}

bool TestAxis::matches(std::string chip_axis)
{
    if (m_vib_dir.first && chip_axis.find("x") != std::string::npos)
        return true;
    if (m_vib_dir.second && chip_axis.find("y") != std::string::npos)
        return true;
    return false;
}

std::string TestAxis::get_name()
{
    return m_name;
}

std::pair<double, double> TestAxis::get_point(double l)
{
    return std::pair<double, double>(m_vib_dir.first * l, m_vib_dir.second * l);
}

TestAxis *parse_axis(GCodeCommand &gcmd, std::string raw_axis)
{
    if (raw_axis == "")
        return nullptr;
    for (auto alph : raw_axis)
    {
        alph = tolower(alph);
    }
    if (raw_axis == "x" || raw_axis == "y")
    {
        // cbd_new_mem("------------------------------------------------new_mem test:TestAxis 1 ",0);
        return new TestAxis(raw_axis); //------new---??-----
    }
    std::vector<std::string> dirs = split(raw_axis, ",");
    if (dirs.size() != 2)
    {
        std::cout << "Invalid format of axis" << std::endl;
    }
    std::pair<double, double> vib_dir;
    vib_dir.first = std::stod(strip(dirs[0]));
    vib_dir.second = std::stod(strip(dirs[1]));
    // cbd_new_mem("------------------------------------------------new_mem test:TestAxis 2",0);
    return new TestAxis("", vib_dir); //------new---??-----
}

VibrationPulseTest::VibrationPulseTest(std::string section_name)
{
    m_min_freq = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "min_freq", 5, 1);
    // Defaults are such that max_freq * accel_per_hz == 10000 (max_accel)
    m_max_freq = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_freq", 10000. / 75., m_min_freq, 200);
    m_accel_per_hz = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "accel_per_hz", 75., std::numeric_limits<double>::min(), std::numeric_limits<double>::max(), 0);
    m_hz_per_sec = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "hz_per_sec", 1., 0.1, 2);
    m_probe_points = parse_probe_points(section_name);
}

VibrationPulseTest::~VibrationPulseTest()
{
}

std::vector<std::vector<double>> VibrationPulseTest::get_start_test_points()
{
    return m_probe_points;
}

void VibrationPulseTest::prepare_test(GCodeCommand &gcmd) //-SC-ADXL345-G-G-2-1-0----
{
    m_freq_start = gcmd.get_double("FREQ_START", m_min_freq, 1);
    m_freq_end = gcmd.get_double("FREQ_END", m_max_freq, m_freq_start, 200);
    m_hz_per_sec = gcmd.get_double("HZ_PER_SEC", m_hz_per_sec, std::numeric_limits<double>::min(), std::numeric_limits<double>::max(), 0, 2);
}

#define TEST 1
double VibrationPulseTest::run_test_time() //-SC-ADXL345-G-G-2-3-0----
{
    double time = 0;
    double freq = m_freq_start;
    while (freq <= m_freq_end + 0.000001)
    {
        double t_seg = (0.25 / freq); // t=0.25*T
        freq += 2 * t_seg * m_hz_per_sec;
        time += 2 * t_seg;
    }
    return time;
}
void VibrationPulseTest::run_test(TestAxis *axis, GCodeCommand &gcmd) //-SC-ADXL345-G-G-2-3-0----
{
    std::vector<double> pos;
    pos = Printer::GetInstance()->m_tool_head->get_position();
    double sign = 1;
    double freq = m_freq_start;
    // Override maximum acceleration and acceleration to
    // deceleration based on the maximum test frequency
    double systime = get_monotonic();
    std::map<std::string, std::string> toolhead_info;
    toolhead_info = Printer::GetInstance()->m_tool_head->get_status(systime);
    double old_max_accel = std::stod(toolhead_info["max_accel"]);
    double old_max_accel_to_decel = std::stod(toolhead_info["max_accel_to_decel"]);
    double max_accel = m_freq_end * m_accel_per_hz;
    std::vector<std::string> commands = {"SET_VELOCITY_LIMIT ACCEL=" + to_string(max_accel) + " ACCEL_TO_DECEL=" + to_string(max_accel) + " FORCE=1"};
    Printer::GetInstance()->m_gcode->run_script_from_command(commands);
    if (Printer::GetInstance()->m_input_shaper != nullptr && !gcmd.get_int("INPUT_SHAPING", 0))
    {
        Printer::GetInstance()->m_input_shaper->disable_shaping();
        gcmd.m_respond_info("Disabled [input_shaper] for resonance testing", true);
    }
    else
    {
        Printer::GetInstance()->m_input_shaper == nullptr;
    }
    double old_freq = freq;
    double eventtime = get_monotonic();
    gcmd.m_respond_info("Testing frequency " + to_string(freq) + " Hz to " + to_string(m_freq_end) + " Hz start:" + to_string(eventtime), true);
    while (freq <= m_freq_end + 0.000001)
    {
        double t_seg = (0.25 / freq);                // t=0.25*T
        double accel = m_accel_per_hz * freq * TEST; // a
        double max_v = accel * t_seg;                // Vt=a*t
        // std::map<std::string, std::string> params;
        // params["S"] = to_string(accel);
        // Printer::GetInstance()->m_tool_head->cmd_M204(Printer::GetInstance()->m_gcode->create_gcode_command("M204", "M204", params));
        Printer::GetInstance()->m_gcode_io->single_command("M204 S" + to_string(accel));
        double L = 0.5 * accel * t_seg * t_seg; // 0.5*a*t*t
        std::pair<double, double> dxy = axis->get_point(L);
        double nX = pos[0] + sign * dxy.first;
        double nY = pos[1] + sign * dxy.second;
        double move_pos[4] = {nX, nY, pos[2], pos[3]};
        Printer::GetInstance()->m_tool_head->move1(move_pos, max_v);
        move_pos[0] = pos[0];
        move_pos[1] = pos[1];
        move_pos[2] = pos[2];
        move_pos[3] = pos[3];
        Printer::GetInstance()->m_tool_head->move1(move_pos, max_v);
        sign = -sign;

        freq += 2 * t_seg * m_hz_per_sec;
        if (floor(freq) > floor(old_freq + 10))
        {
            old_freq = freq;
            gcmd.m_respond_info("frequency " + to_string(freq) + " Hz", true);
        }
    }
    eventtime = get_monotonic();
    gcmd.m_respond_info("Testing frequency " + to_string(freq) + " Hz end:" + to_string(eventtime), true);
    std::cout << "finish move " << std::endl;
    // Restore the original acceleration values
    std::vector<std::string> set_velocity_limit_command = {"SET_VELOCITY_LIMIT ACCEL=" + to_string(old_max_accel) + " ACCEL_TO_DECEL=" + to_string(old_max_accel_to_decel) + " FORCE=1"};
    Printer::GetInstance()->m_gcode->run_script_from_command(set_velocity_limit_command);
    // Restore input shaper if it was disabled for resonance testing
    if (Printer::GetInstance()->m_input_shaper != nullptr)
    {
        Printer::GetInstance()->m_input_shaper->enable_shaping();
        gcmd.m_respond_info("Re-enabled [input_shaper]", true);
    }
}

ResonanceTester::ResonanceTester(std::string section_name)
{
    m_move_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "move_speed", 50, std::numeric_limits<double>::min(), std::numeric_limits<double>::max(), 0);
    m_test = new VibrationPulseTest(section_name); //----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:VibrationPulseTest",0);
    if (Printer::GetInstance()->m_pconfig->GetString(section_name, "accel_chip_x") == "")
    {
        std::vector<std::string> chip;
        chip.push_back("xy");
        chip.push_back(strip(Printer::GetInstance()->m_pconfig->GetString(section_name, "accel_chip")));
        m_accel_chip_names.push_back(chip);
    }
    else
    {
        std::vector<std::string> chip_x;
        chip_x.push_back("x");
        chip_x.push_back(strip(Printer::GetInstance()->m_pconfig->GetString(section_name, "accel_chip_x")));
        m_accel_chip_names.push_back(chip_x);
        std::vector<std::string> chip_y;
        chip_y.push_back("y");
        chip_y.push_back(strip(Printer::GetInstance()->m_pconfig->GetString(section_name, "accel_chip_y")));
        m_accel_chip_names.push_back(chip_y);
        if (m_accel_chip_names[0][1] == m_accel_chip_names[1][1])
        {
            std::string chip_name = m_accel_chip_names[0][1];
            std::vector<std::vector<std::string>>().swap(m_accel_chip_names);
            std::vector<std::string> chip;
            chip.push_back("xy");
            chip.push_back(chip_name);
            m_accel_chip_names.push_back(chip);
        }
    }
    m_max_smoothing = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_smoothing", 0., 0);

    Printer::GetInstance()->m_gcode->register_command("MEASURE_AXES_NOISE", std::bind(&ResonanceTester::cmd_MEASURE_AXES_NOISE, this, std::placeholders::_1), false, cmd_MEASURE_AXES_NOISE_help);
    Printer::GetInstance()->m_gcode->register_command("TEST_RESONANCES", std::bind(&ResonanceTester::cmd_TEST_RESONANCES, this, std::placeholders::_1), false, cmd_TEST_RESONANCES_help);
    Printer::GetInstance()->m_gcode->register_command("SHAPER_CALIBRATE", std::bind(&ResonanceTester::cmd_SHAPER_CALIBRATE, this, std::placeholders::_1), false, cmd_SHAPER_CALIBRATE_help);
    Printer::GetInstance()->m_gcode->register_command("SHAPER_CALIBRATE_SCRIPT", std::bind(&ResonanceTester::cmd_SHAPER_CALIBRATE_SCRIPT, this, std::placeholders::_1), false, cmd_SHAPER_CALIBRATE_help);
    Printer::GetInstance()->register_event_handler("klippy:connect:ResonanceTester", std::bind(&ResonanceTester::connect, this));
    m_test_finish = false;
}

ResonanceTester::~ResonanceTester()
{
}

void ResonanceTester::connect()
{
    for (auto chip : m_accel_chip_names)
    {
        std::pair<std::string, ADXL345 *> accel_chip;
        // std::cout << "chip[0] = " << chip[0] << std::endl;
        // std::cout << "chip[1] = " << chip[1] << std::endl;
        accel_chip.first = chip[0];
        if (Printer::GetInstance()->m_adxl345s.find(chip[1]) != Printer::GetInstance()->m_adxl345s.end())
        {
            accel_chip.second = Printer::GetInstance()->m_adxl345s[chip[1]];
        }
        else
        {
            std::cout << "----------------------------chip[1] = " << chip[1] << std::endl;
        }
        m_accel_chips.push_back(accel_chip);
    }
}

std::map<std::string, CalibrationData *> ResonanceTester::_run_test(GCodeCommand &gcmd, std::vector<TestAxis *> axes, ShaperCalibrate *helper, std::string raw_name_suffix)
{ //-SC-ADXL345-G-G-2-0----
    std::map<std::string, CalibrationData *> calibration_data;
    ADXL345Results *results = nullptr;
    m_test->prepare_test(gcmd);                                                     //-SC-ADXL345-G-G-2-1----
    std::vector<std::vector<double>> test_points = m_test->get_start_test_points(); // 可以有多个测试点
    for (auto point : test_points)                                                  // 在多个位置依次测试
    {
        std::vector<double> coord = {point[0], point[1], point[2]};
        Printer::GetInstance()->m_tool_head->manual_move(coord, m_move_speed);
        if (test_points.size() > 1)
        {
            gcmd.m_respond_info("Probing point (%.3f, %.3f, %.3f)", true);
        }
        for (auto axis : axes) // 测试多个轴
        {
            Printer::GetInstance()->m_tool_head->wait_moves();
            // sleep(5);
            Printer::GetInstance()->m_tool_head->dwell(0.5);
            if (axes.size() > 1)
            {
                gcmd.m_respond_info("Testing axis ", true);
            }
            int measure_time = (int)m_test->run_test_time();
            for (auto accel_chip : m_accel_chips) // 有多个采样的加速度计 同时采样
            {
                if (axis->matches(accel_chip.first))
                {
                    if (accel_chip.second->start_measurements(-1, measure_time + 20))
                    {
                        if (axis->m_name == "x")
                        {
                            resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_ERROR_X);
                        }
                        else if (axis->m_name == "y")
                        {
                            resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_ERROR_Y);
                        }
                        throw std::runtime_error("resonance_tester error");
                    } //-开始采样--SC-ADXL345-G-G-2-2----
                }
            }
            // Generate moves
            m_test->run_test(axis, gcmd); //---振动 ---SC-ADXL345-G-G-2-3----
            std::vector<std::pair<std::string, ADXL345Results *>> raw_values;
            for (auto accel_chip : m_accel_chips)
            {
                if (axis->matches(accel_chip.first))
                {
                    results = accel_chip.second->finish_measurements(); //-结束采样-SC-ADXL345-G-G-2-4----
                    if (raw_name_suffix != "")
                    {
                        results.write_to_file("/app/acceldata-" + axis->m_name + std::to_string(point) + ".csv");
                        gcmd.m_respond_info("Writing raw accelerometer data to ", true);
                    }
                    raw_values.push_back(make_pair(accel_chip.first, results));
                    gcmd.m_respond_info("-axis accelerometer stats: ", true);
                }
            }
            if (helper == nullptr)
            {
                continue;
            }
            for (auto value : raw_values) // 依次处理各个加速度计采样的值
            {
                if (value.second == nullptr)
                    std::cout << "axis acceleromter measured no data" << std::endl;
                CalibrationData *new_data = helper->process_accelerometer_data(value.second); //-处理采样数据 --SC-ADXL345-G-G-2-5----
                if (value.second)
                    delete value.second; // 内存泄漏
                value.second = nullptr;
                std::cout << "axis :" << value.first << axis->get_name() << std::endl;
                if (calibration_data.find(axis->get_name()) == calibration_data.end())
                {
                    calibration_data[axis->get_name()] = new_data;
                }
                else
                {
                    calibration_data[axis->get_name()]->add_data(new_data);
                }
            }
        }
    }
    return calibration_data;
}

void ResonanceTester::cmd_TEST_RESONANCES(GCodeCommand &gcmd)
{
    TestAxis *axis = parse_axis(gcmd, gcmd.get_string("AXIS", ""));
    std::vector<std::string> outputs = split(gcmd.get_string("OUTPUT", "resonances"), ",");
    if (outputs.size() == 0)
    {
        std::cout << "No output specified, at least one of 'resonances' or 'raw_data' must be set in OUTPUT parameter" << std::endl;
    }
    for (auto output : outputs)
    {
        if (output != "resonances" && output != "raw_data")
        {
            std::cout << "Unsupported output , only 'resonances' and 'raw_data' are supported" << std::endl;
        }
    }
    std::string name_suffix = gcmd.get_string("NAME", "time"); // strftime time
    is_valid_name_suffix(name_suffix);
    bool csv_output, raw_output;
    for (auto output : outputs)
    {
        csv_output = output == "resonances" ? 1 : 0;
        raw_output = output == "raw_data" ? 1 : 0;
    }
    // Setup calculation of resonances
    ShaperCalibrate *helper = nullptr;
    if (csv_output)
    {
        helper = new ShaperCalibrate(); //----------new---??-----
        // cbd_new_mem("------------------------------------------------new_mem test:ShaperCalibrate",0);
    }
    else
    {
        helper = nullptr;
    }
    std::vector<TestAxis *> axes = {axis};
    std::map<std::string, CalibrationData *> calibration_data = _run_test(gcmd, axes, helper, name_suffix);
    
    std::string csv_name = save_calibration_data("resonances", name_suffix, helper, axis, data);
    std::cout << "Resonances data written to file" << std::endl;
    
    if (helper != nullptr)
        delete helper;
    for (auto &data : calibration_data)
    {
        if (data.second != nullptr)
            delete data.second;
    }
    std::map<std::string, CalibrationData *>().swap(calibration_data);
}

void ResonanceTester::cmd_SHAPER_CALIBRATE_SCRIPT(GCodeCommand &gcmd)
{
    std::map<std::string, std::string> params_measure_noise;
    GCodeCommand params_measure_noise_cmd = Printer::GetInstance()->m_gcode->create_gcode_command("MEASURE_AXES_NOISE", "MEASURE_AXES_NOISE", params_measure_noise);
    cmd_MEASURE_AXES_NOISE(params_measure_noise_cmd);
    // Printer::GetInstance()->m_gcode_io->single_command("MEASURE_AXES_NOISE");
    resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_START);
    Printer::GetInstance()->m_gcode_io->single_command("G28");
    std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(get_monotonic());
    if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos || kin_status["homed_axes"].find("z") == std::string::npos)
    {
        LOG_E("G28 failed\n");
        return;
    }
    resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_START_X);
    std::map<std::string, std::string> params_x;
    params_x["AXIS"] = "x";
    GCodeCommand shaper_calibrate_x_cmd = Printer::GetInstance()->m_gcode->create_gcode_command("SHAPER_CALIBRATE", "SHAPER_CALIBRATE", params_x);
    cmd_SHAPER_CALIBRATE(shaper_calibrate_x_cmd);
    // Printer::GetInstance()->m_gcode_io->single_command("SHAPER_CALIBRATE AXIS=x");
    resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_FINISH_X);
    resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_START_Y);
    std::map<std::string, std::string> params_y;
    params_y["AXIS"] = "y";
    GCodeCommand shaper_calibrate_y_cmd = Printer::GetInstance()->m_gcode->create_gcode_command("SHAPER_CALIBRATE", "SHAPER_CALIBRATE", params_y);
    cmd_SHAPER_CALIBRATE(shaper_calibrate_y_cmd);
    // Printer::GetInstance()->m_gcode_io->single_command("SHAPER_CALIBRATE AXIS=y");
    resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_FINISH_Y);
    resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_FINISH);
}

void ResonanceTester::cmd_SHAPER_CALIBRATE(GCodeCommand &gcmd) //-SC-ADXL345-G-G-0-----
{
    // system("echo 1 > /proc/sys/vm/compact_memory");
    // system("echo 3 > /proc/sys/vm/drop_caches");
    std::string axis = gcmd.get_string("AXIS", "");
    std::vector<TestAxis *> calibrate_axes;
    if (axis == "")
    {
        TestAxis *axis_x = new TestAxis("x"); //------new---??-----
                                              // cbd_new_mem("------------------------------------------------new_mem test:TestAxis",0);
        TestAxis *axis_y = new TestAxis("y");
        calibrate_axes.emplace_back(axis_x);
        calibrate_axes.emplace_back(axis_y);
    }
    else if (axis != "x" && axis != "y")
    {
        std::cout << "unsupported axis " << axis << std::endl;
    }
    else
    {
        TestAxis *ax = new TestAxis(axis); //----------new---??-----
                                           // cbd_new_mem("------------------------------------------------new_mem test:TestAxis 2",0);
        calibrate_axes.emplace_back(ax);
    }
    double max_smoothing = gcmd.get_double("MAX_SMOOTHING", m_max_smoothing, 0.05);
    // std::string name_suffix = gcmd.get_string("NAME");
    // if(is_valid_name_suffix(name_suffix))
    // {

    // }

    // Setup shaper calibration
    ShaperCalibrate *helper = new ShaperCalibrate; //-SC-ADXL345-G-G-1-----       //----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:ShaperCalibrate",0);
    std::map<std::string, CalibrationData *> calibration_data = _run_test(gcmd, calibrate_axes, helper); //--振动采样计算-SC-ADXL345-G-G-2-----
    for (auto axis : calibrate_axes)
    {
        std::string axis_name = axis->get_name();
        std::cout << "Calculating the best input shaper parameters for " << axis_name << " axis" << std::endl;
        calibration_data[axis->get_name()]->normalize_to_frequencies();                                     //-进一步处理数据----SC-ADXL345-G-G-3-----
        // shaper_t best_shaper = helper->find_best_shaper(calibration_data[axis->get_name()], max_smoothing); //-找到最佳整形器---SC-ADXL345-G-G-4-----
        auto res = helper->find_best_shaper(calibration_data[axis_name], max_smoothing); //-找到最佳整形器---SC-ADXL345-G-G-4-----
        shaper_t best_shaper = std::get<0>(res);
        std::vector<shaper_t> all_shapers = std::get<1>(res);

        //save_calibration_data
        // std::string name_suffix = gcmd.get_string("NAME", "");
        // std::string file_name = get_filename("resonances", name_suffix, axis_name, m_test->m_probe_points[0]); //单个测量点
        // std::cout << "file name : " << file_name << std::endl;
        // helper->save_calibration_data(file_name, calibration_data[axis_name], all_shapers, m_test->m_freq_end);

        helper->save_params(axis_name, best_shaper.name, best_shaper.freq); //-SC-ADXL345-G-G-5-----
    }
    for (auto &axis : calibrate_axes)
    {
        delete axis;
    }
    std::vector<TestAxis *>().swap(calibrate_axes);
    delete helper;
    for (auto &data : calibration_data)
    {
        delete data.second;
    }
    std::map<std::string, CalibrationData *>().swap(calibration_data);
    //  gcmd.respond_info(
    //         "The SAVE_CONFIG command will update the printer config file\n"
    //         "with these parameters and restart the printer.");
    m_test_finish = true;
    // system("echo 1 > /proc/sys/vm/compact_memory");
    // system("echo 3 > /proc/sys/vm/drop_caches");
}

void ResonanceTester::cmd_MEASURE_AXES_NOISE(GCodeCommand &gcmd) //--ADXL345-G-G-0-----
{
    double meas_time = gcmd.get_double("MEAS_TIME", 3.);
    for (auto accel_chip : m_accel_chips)
    {
        if (accel_chip.second->start_measurements(-1, (int)meas_time))
        {
            resonace_tester_state_callback_call(RESONANCE_TESTER_STATE_ERROR_X);
            throw std::runtime_error("resonance_tester error");
        }
    }
    Printer::GetInstance()->m_tool_head->dwell(meas_time);
    std::vector<std::pair<std::string, ADXL345Results *>> raw_values;
    for (auto accel_chip : m_accel_chips)
    {
        raw_values.push_back(make_pair(accel_chip.first, accel_chip.second->finish_measurements()));
    }
    ShaperCalibrate helper;
    for (auto raw_data : raw_values)
    {
        CalibrationData *data = helper.process_accelerometer_data(raw_data.second);
        if (raw_data.second)
            delete raw_data.second; // 内存泄漏
        raw_data.second = nullptr;
        nc::NdArray<double> vx = nc::mean(data->m_psd_x);
        nc::NdArray<double> vy = nc::mean(data->m_psd_y);
        nc::NdArray<double> vz = nc::mean(data->m_psd_z);
        std::stringstream ss;
        ss << "Axes noise for " << raw_data.first << "-axis accelerometer: " << vx.front() << " (x), " << vy.front() << " (y), " << vz.front() << " (z)";
        gcmd.m_respond_raw(ss.str());
    }
    resonace_tester_state_callback_call(RESONANCE_MEASURE_AXES_NOISE_FINISH);
}

bool ResonanceTester::is_valid_name_suffix(std::string name_suffix)
{
}

std::string ResonanceTester::get_filename(std::string base, std::string name_suffix, std::string axis_name, std::vector<double> point)
{
    std::stringstream name;
    name << base << "_" << axis_name;

    if (!point.empty())
        name << "_" << std::fixed << std::setprecision(3) << point[0]
             << "_" << std::fixed << std::setprecision(3) << point[1]
             << "_" << std::fixed << std::setprecision(3) << point[2];

    if (!name_suffix.empty())
        name << "_" << name_suffix;

    name << ".csv";

    return "/tmp/" + name.str();
}

#define RESONANCE_TESTER_STATE_CALLBACK_SIZE 16
static resonace_tester_state_callback_t resonace_tester_state_callback[RESONANCE_TESTER_STATE_CALLBACK_SIZE];

int resonace_tester_register_state_callback(resonace_tester_state_callback_t state_callback)
{
    for (int i = 0; i < RESONANCE_TESTER_STATE_CALLBACK_SIZE; i++)
    {
        if (resonace_tester_state_callback[i] == NULL)
        {
            resonace_tester_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}

int resonace_tester_unregister_state_callback(resonace_tester_state_callback_t state_callback)
{
    for (int i = 0; i < RESONANCE_TESTER_STATE_CALLBACK_SIZE; i++)
    {
        if (resonace_tester_state_callback[i] == state_callback)
        {
            resonace_tester_state_callback[i] = NULL;
            return 0;
        }
    }
    return -1;
}

int resonace_tester_state_callback_call(int state)
{
    for (int i = 0; i < RESONANCE_TESTER_STATE_CALLBACK_SIZE; i++)
    {
        if (resonace_tester_state_callback[i] != NULL)
        {
            resonace_tester_state_callback[i](state);
        }
    }
    return 0;
}

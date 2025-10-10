#ifndef PROBE_H
#define PROBE_H

#include <iostream>
#include <algorithm>

#include "gcode.h"
#include "homing.h"
#include "pins.h"
#include "probe_endstop_wrapper_base.h"
#include "manual_probe.h"

struct probe_state
{
    bool last_query;
    double last_z_result;
};

class ProbeEndstopWrapper : public ProbeEndstopWrapperBase
{
private:
public:
    ProbeEndstopWrapper(std::string section_name);
    ~ProbeEndstopWrapper();

public:
    void handle_mcu_identify();

    void raise_probe();

    void lower_probe();

    void multi_probe_begin();
    void multi_probe_end();

    void probe_prepare(HomingMove *hmove);

    void probe_finish(HomingMove *hmove);

    double get_position_endstop();
};

class PrinterProbe
{
private:
public:
    PrinterProbe(std::string section_name, ProbeEndstopWrapperBase *mcu_probe);
    ~PrinterProbe();

    std::string m_name;
    ProbeEndstopWrapperBase *m_mcu_probe;
    // ProbeEndstopWrapperBase *m_bed_mesh_mcu_probe;
    double m_speed;
    double m_final_speed;
    double m_lift_speed;
    double m_x_offset;
    double m_y_offset;
    double m_z_offset;
    double m_probe_calibrate_z;
    bool m_multi_probe_pending;
    bool m_last_state;
    double m_last_z_result;        //
    double move_after_each_sample; // 探针触发后，移动的距离，有符号，正数为向上移动，负数为向下移动
    double m_z_offset_adjust; // z_offset的偏移量，用于校准z_offset
    double m_z_position;

    int m_sample_count;
    int m_edge_sample_count;
    double m_edge_compensation;
    int m_error;
    double m_sample_retract_dist;
    std::string m_samples_result;
    double m_samples_tolerance;
    double m_samples_tolerance_step;
    double m_edge_samples_tolerance;
    double m_edge_samples_tolerance_step;
    double m_fast_autoleveling_threshold;
    int m_samples_retries;

    std::string cmd_PROBE_help;
    std::string cmd_QUERY_PROBE_help;
    std::string cmd_PROBE_CALIBRATE_help;
    std::string cmd_PROBE_ACCURACY_help;

public:
    void _handle_homing_move_begin(HomingMove *hmove);

    void _handle_homing_move_end(HomingMove *hmove);

    void _handle_home_rails_begin(Homing *homing_state, std::vector<PrinterRail *> rails);

    void _handle_home_rails_end(Homing *homing_state, std::vector<PrinterRail *> rails);

    void _handle_command_error();

    void multi_probe_begin();

    void multi_probe_end();
    ProbeEndstopWrapperBase *setup_pin(std::string pin_type, pinParams pin_params);

    double get_lift_speed(GCodeCommand *gcmd = NULL);

    std::vector<double> get_offsets();

    std::vector<double> _probe(double speed);

    void _move(std::vector<double> coord, double speed);

    std::vector<double> _calc_mean(std::vector<std::vector<double>> positions);
    std::vector<double> _calc_median(std::vector<std::vector<double>> positions);
    std::vector<double> _calc_weighted_average(std::vector<std::vector<double>> positions);

    std::vector<double> run_probe(GCodeCommand &gcmd);

    void cmd_PROBE(GCodeCommand &gcmd);

    void cmd_QUERY_PROBE(GCodeCommand &gcmd);

    struct probe_state get_status(double eventtime);

    void cmd_PROBE_ACCURACY(GCodeCommand &gcmd);

    void probe_calibrate_finalize();

    void cmd_PROBE_CALIBRATE(GCodeCommand &gcmd);
};

class ProbePointsHelper
{
private:
public:
    ProbePointsHelper(std::string section_name, std::function<std::string(std::vector<double>, std::vector<std::vector<double>>)>, std::vector<std::vector<double>> default_points = std::vector<std::vector<double>>());
    ~ProbePointsHelper();

public:
    double m_horizontal_move_z;
    bool m_use_offsets;
    double m_speed;
    double m_lift_speed;
    std::vector<double> m_probe_offsets = {0, 0, 0};
    std::vector<std::vector<double>> m_probe_points;
    std::vector<std::vector<double>> m_fast_probe_points;                   //快速调平的点
    std::vector<double> m_last_fast_probe_points_z;                         //上一次快速调平的点结果值
    std::vector<std::vector<double>> m_results;
    std::string m_name;
    std::function<std::string(std::vector<double>, std::vector<std::vector<double>>)> m_finalize_callback;

public:
    void minimum_points(int n);

    void update_probe_points(std::vector<std::vector<double>> points, int min_points);

    void use_xy_offsets(bool use_offsets);

    double get_lift_speed();

    bool _move_next(int move_type);

    void start_probe(GCodeCommand &gcmd);

    void _manual_probe_start();

    bool _fast_probe_start(PrinterProbe *probe, GCodeCommand &gcmd);

    void _manual_probe_finalize(std::vector<double> kin_pos);

    void _fast_probe_finalize(std::vector<double> kin_pos);
};
enum
{
    CMD_BEDMESH_PROBE_SUCC = 0,
    CMD_BEDMESH_PROBE_FALT,
    CMD_BEDMESH_PROBE_EXCEPTION,    //连续探点误差大于阈值：传感器损坏或收环境影响
    CMD_PROBE_SUCC,
    CMD_PROBE_FALT,
    AUTO_LEVELING_PROBEING
};
struct probe_data_t
{
    int state;
    double value;
};
typedef void (*probe_state_callback_t)(probe_data_t state);
int probe_register_state_callback(probe_state_callback_t state_callback);
int probe_unregister_state_callback(probe_state_callback_t state_callback);
int probe_state_callback_call(probe_data_t state);
#endif

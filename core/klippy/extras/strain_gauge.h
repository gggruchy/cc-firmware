#ifndef STRAIN_GAUGE_H
#define STRAIN_GAUGE_H

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <random>
#include <tuple>
#include "dirzctl.h"
#include "filter.h"
#include "hx711s.h"
#include "hx711_sample.h"
#include "bed_mesh.h"
#include "alg.h"
#include "extruder.h"
#include "toolhead.h"
#include "kinematics_base.h"
#include "heater_bed.h"

class StrainGaugeCFG
{
private:
public:
    StrainGaugeCFG(std::string section_name);
    ~StrainGaugeCFG();

    int m_base_count;
    bool m_enable_z_home;
    int m_pi_count; // 数组大小
    int m_g28_min_hold;
    int m_g28_max_hold;
    int m_g29_min_hold;
    int m_g29_max_hold;
    // int m_min_hold;
    // int m_max_hold;
    double m_hot_min_temp;
    double m_hot_max_temp;
    double m_bed_max_temp;
    int m_pa_fil_len_mm;
    int m_pa_fil_dis_mm;
    int m_pa_clr_dis_mm;
    double m_pa_clr_down_mm;
    double m_clr_noz_start_x;
    double m_clr_noz_start_y;
    double m_clr_noz_len_x;
    double m_clr_noz_len_y;
    int m_bed_max_err;
    double m_g28_max_err;
    double m_g28_max_try;
    double m_g29_max_detection;
    double m_max_z;
    double m_g29_xy_speed;
    double m_fix_z_offset;
    std::string m_platform_material;
    double m_mesh_max;
    double m_max_dis_bef_g28;
    double m_dead_zone_bef_g28;
    double m_g28_sta0_speed;
    double m_g28_sta1_speed;
    double m_g29_rdy_speed;
    double m_g29_speed;
    bool m_show_msg;
    double m_best_above_z;
    bool m_g28_wait_cool_down;
    int m_shake_cnt;
    double m_shake_range;
    double m_shake_max_velocity;
    double m_shake_max_accel;
    int m_g28_sta0_min_hold;
    bool m_need_measure_gap;
    double m_gap_dis_range;
    double m_z_gap_0;
    double m_z_gap_1;
    double m_z_gap_2;
    double m_z_gap_11;
    double m_check_bed_mesh_max_err;
    bool m_enable_ts_compense;
    bool m_use_probe_by_step;
    std::string m_tri_wave_ip;
    double m_self_z_offset;
    std::map<std::string, std::string> m_stored_profs;
};

class StrainGaugeVAL
{
private:
public:
    StrainGaugeVAL(std::string section_name);
    ~StrainGaugeVAL();

    int m_out_index;
    double m_out_val_mm;
    double m_end_z_mm;
    std::vector<std::vector<double>> m_rdy_pos;
    std::vector<std::vector<double>> m_gap_pos;
    int m_g29_cnt;
    int m_re_probe_cnt;
    std::vector<double> m_home_xy;
    bool m_jump_probe_ready;
};

class StrainGaugeOBJ
{
private:
public:
    StrainGaugeOBJ(std::string section_name);
    ~StrainGaugeOBJ();
    void find_objs();

    DirZCtl *m_dirzctl;
    BedMesh *m_bed_mesh;
    Filter *m_filter;
    HX711S *m_hx711s;
    ToolHead *m_tool_head;
    PrinterHeaters *m_pheaters;
    PrinterExtruder *m_printer_extruder;
    PrinterHeaterBed *m_bed_heater;
    MCU *m_mcu;
    Kinematics *m_kin;
};

class StrainGaugeWrapper
{
private:
public:
    StrainGaugeWrapper(std::string section_name);
    ~StrainGaugeWrapper();
    void _handle_mcu_identify();
    void multi_probe_begin();
    void multi_probe_end();
    void probe_prepare();
    bool home_start();
    void add_stepper();
    void get_steppers();
    bool ck_sys_sta();
    void _ck_g28ed();
    void _move(std::vector<double> pos, double speed, bool wait = true);
    bool _check_index(int index);
    std::vector<double> _get_linear2(std::vector<double> &p1, std::vector<double> &p2, std::vector<double> &po, bool is_base_x);
    void _pnt_tri_msg();
    bool _check_trigger(int arg_index, std::vector<double> fit_vals, double min_hold, double max_hold);
    bool _check_trigger(int arg_index, std::vector<double> fit_vals, const std::vector<double> &fusion_values, double min_hold, double max_hold);
    void _set_hot_temps(double temp, double fan_spd, bool wait = false, int err = 5);
    void _set_bed_temps(double temp, bool wait = false, int err = 5);
    double _probe_times(int max_times, std::vector<double> rdy_pos, double speed_mm, double min_dis_mm, double max_z_err, double min_hold, double max_hold);
    double get_best_rdy_z(double rdy_x, double rdy_y, std::vector<std::vector<double>> base_pos);
    void shake_motor(int cnt);
    double measure_gap(double zero_z);
    double _gap_times(int max_times, double zero_pos);
    bool probe_ready();
    void check_bed_mesh(bool auto_g29 = true);
    void raise_z_bef_g28();
    std::tuple<int, int, bool> cal_min_z(double start_z, const std::vector<double> &hx711_vals);
    double cal_z(double start_z, double start_time, double end_z, double end_time);
    std::tuple<int, double, bool> probe_by_step(std::vector<double> rdy_pos, double speed_mm, double min_dis_mm, double min_hold, double max_hold, bool up_after = true, double up_dis_mm = 0);
    bool run_probe(bool need_shake = true, bool need_force_retract = true, bool need_cool_down = true);
    bool run_G28_Z(Homing &homing_state, bool need_shake = true, bool need_force_retract = true, bool need_cool_down = true);
    std::vector<double> run_G29_Z(HomingMove *hmove);
    bool wait_home();

    void cmd_STRAINGAUGE_Z_HOME(GCodeCommand &gcmd);
    void cmd_STRAINGAUGE_TEST(GCodeCommand &gcmd);
    void cmd_PRTOUCH_READY(GCodeCommand &gcmd);
    void cmd_CHECK_BED_MESH(GCodeCommand &gcmd);
    void cmd_MEASURE_GAP_TEST(GCodeCommand &gcmd);
    void cmd_NOZZLE_CLEAR(GCodeCommand &gcmd);

    void change_hot_min_temp(double temp);

    // std::vector<double> m_compensation;
    StrainGaugeCFG *m_cfg;
    StrainGaugeVAL *m_val;
    StrainGaugeOBJ *m_obj;

    Alg *m_AlgSg;

    double m_step_base;
    std::vector<double> m_tri_val;
};

#endif